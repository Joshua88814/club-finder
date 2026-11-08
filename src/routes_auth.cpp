#include "routes.h"
#include "helpers.h"
#include "email.h"
#include <sodium.h>
#include "config.h"


// Lets loggin in users view the app and redirects unauthenticated users to login 
void register_auth_routes(App& app) {
    CROW_ROUTE(app, "/")([&app](const crow::request& req, crow::response& res) {
        Session s;
        if (!current_session(app, req, s)) {
            res.redirect("/static/login.html");
            res.end();
            return;
        }
        res.set_static_file_info("static/index.html");
        res.end();
    });

    // Signup
    CROW_ROUTE(app, "/api/signup").methods(crow::HTTPMethod::Post)([](const crow::request& req) {
        auto body = crow::json::load(req.body);
        if (!body) return json_reply(400, error_json("Invalid request"));

        std::string email = lower(field(body, "email")), pw = raw_field(body, "password");
        std::string first = field(body, "first_name"), last = field(body, "last_name");
        std::string type = field(body, "user_type"), grade = field(body, "grade_level");

        if (first.empty() || last.empty()) return json_reply(400, error_json("Please enter your first and last name"));
        if (!ends_with(email, EMAIL_DOMAIN)) return json_reply(400, error_json("Please use your DLSL email (" + EMAIL_DOMAIN + ")"));
        if (has_whitespace(email)) return json_reply(400, error_json("Email cannot contain spaces"));
        if (has_whitespace(pw)) return json_reply(400, error_json("Password cannot contain spaces"));
        if (pw.size() < 8) return json_reply(400, error_json("Password must be at least 8 characters"));
        if (type != "student" && type != "teacher" && type != "alumni")
            return json_reply(400, error_json("Please choose student, teacher or alumni"));

        char hash[crypto_pwhash_STRBYTES];
        if (crypto_pwhash_str(hash, pw.c_str(), pw.size(), crypto_pwhash_OPSLIMIT_INTERACTIVE,
                              crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0)
            return json_reply(500, error_json("Could not process password"));

        try {
            Conn con = open_db();
            std::unique_ptr<sql::PreparedStatement> ps(con->prepareStatement(
                "INSERT INTO users (email, password_hash, first_name, last_name, user_type, grade_level) "
                "VALUES (?, ?, ?, ?, ?, ?)"));
            ps->setString(1, email);
            ps->setString(2, hash);
            ps->setString(3, first);
            ps->setString(4, last);
            ps->setString(5, type);
            if (grade.empty()) ps->setNull(6, sql::DataType::VARCHAR);
            else ps->setString(6, grade);
            ps->executeUpdate();
        } catch (sql::SQLException& e) {
            if (e.getErrorCode() == 1062) return json_reply(409, error_json("That email is already registered"));
            std::cerr << "DB error: " << e.what() << std::endl;
            return json_reply(500, error_json("Database error"));
        }
        crow::json::wvalue ok;
        ok["ok"] = true;
        return json_reply(200, std::move(ok));
    });

    // Login 
   CROW_ROUTE(app, "/api/login").methods(crow::HTTPMethod::Post)([&app](const crow::request& req) {
    auto body = crow::json::load(req.body);
    if (!body) return json_reply(400, error_json("Invalid request"));

    std::string email = lower(field(body, "email"));
    std::string password = raw_field(body, "password");   

    if (email.empty() || password.empty()) 
        return json_reply(400, error_json("Please enter your email and password"));

    try {
        Conn con = open_db();
        
        //check if the user exists and get the password hash and failed attempts
        std::unique_ptr<sql::PreparedStatement> ps(con->prepareStatement(
            "SELECT user_id, password_hash, failed_attempts, locked_until, "
            "first_name, last_name, user_type, system_role, "
            "NOW() < locked_until AS is_locked FROM users WHERE email = ?"));
        ps->setString(1, email);
        std::unique_ptr<sql::ResultSet> rs(ps->executeQuery());

        if (!rs->next()) {
            return json_reply(401, error_json("Invalid email or password"));
        }

        int user_id = rs->getInt("user_id");
        std::string db_hash = rs->getString("password_hash");
        bool is_locked = rs->getBoolean("is_locked");
        int failed_attempts = rs->getInt("failed_attempts");
        std::string first_name = rs->getString("first_name");
        std::string last_name = rs->getString("last_name");
        std::string user_type = rs->getString("user_type");
        std::string system_role = rs->getString("system_role");

        //check if the account is locked
        if (is_locked) {
            return json_reply(403, error_json("Account is locked due to multiple failed login attempts. Try again in 48 hours."));
        }

        //verify the password
        bool password_match = (crypto_pwhash_str_verify(db_hash.c_str(), password.c_str(), password.size()) == 0);

        if (!password_match) {
            failed_attempts++;
            if (failed_attempts >= 3) {
                
                std::unique_ptr<sql::PreparedStatement> lock_ps(con->prepareStatement(
                    "UPDATE users SET failed_attempts = 0, locked_until = DATE_ADD(NOW(), INTERVAL 48 HOUR) WHERE user_id = ?"));
                lock_ps->setInt(1, user_id);
                lock_ps->executeUpdate();
                return json_reply(403, error_json("Too many failed attempts. Your account has been locked for 48 hours."));
            } else {
                    std::unique_ptr<sql::PreparedStatement> fail_ps(con->prepareStatement(
                    "UPDATE users SET failed_attempts = ? WHERE user_id = ?"));
                fail_ps->setInt(1, failed_attempts);
                fail_ps->setInt(2, user_id);
                fail_ps->executeUpdate();
                
                int attempts_left = 3 - failed_attempts;
                return json_reply(401, error_json("Invalid email or password. " + std::to_string(attempts_left) + " attempts remaining before lockout."));
            }
        }

        
        {
            std::unique_ptr<sql::PreparedStatement> reset_ps(con->prepareStatement(
                "UPDATE users SET failed_attempts = 0, locked_until = NULL WHERE user_id = ?"));
            reset_ps->setInt(1, user_id);
            reset_ps->executeUpdate();
        }

        auto& ctx = app.get_context<crow::CookieParser>(req);
        std::string sid = new_token();
        {
            std::lock_guard<std::mutex> lock(sessions_mutex);
            sessions[sid] = Session{user_id, first_name + " " + last_name, user_type, system_role};
        }
        ctx.set_cookie("sid", sid).path("/").max_age(86400);

    } catch (sql::SQLException& e) {
        std::cerr << "DB error: " << e.what() << std::endl;
        return json_reply(500, error_json("Database error"));
    }

    return ok_reply();
});

    // Logout
    CROW_ROUTE(app, "/api/logout").methods(crow::HTTPMethod::Post)([&app](const crow::request& req) {
        auto& ctx = app.get_context<crow::CookieParser>(req);
        std::string sid = ctx.get_cookie("sid");
        { std::lock_guard<std::mutex> lock(sessions_mutex); sessions.erase(sid); }
        ctx.set_cookie("sid", "").path("/").max_age(0);
        crow::json::wvalue ok;
        ok["ok"] = true;
        return json_reply(200, std::move(ok));
    });

    CROW_ROUTE(app, "/api/me")([&app](const crow::request& req) {
        Session s;
        if (!current_session(app, req, s)) return json_reply(401, error_json("Not logged in"));
        crow::json::wvalue me;
        me["id"] = s.user_id;
        me["name"] = s.name;
        me["user_type"] = s.user_type;
        me["system_role"] = s.system_role;
        return json_reply(200, std::move(me));
    });
}