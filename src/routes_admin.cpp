#include "routes.h"
#include "helpers.h"
#include "email.h"
#include "config.h"

void register_admin_routes(App& app) {
    // super admin dashboard 
    CROW_ROUTE(app, "/api/admin/data")([&app](const crow::request& req) {
        try {
            Session me; Conn con; crow::response err;
            if (!admin_check(app, req, con, me, err)) return err;
            crow::json::wvalue out;
            std::unique_ptr<sql::Statement> st(con->createStatement());

            // Load club categories
            {
                crow::json::wvalue::list items;
                std::unique_ptr<sql::ResultSet> rs(st->executeQuery("SELECT category_id, name FROM club_categories ORDER BY name"));
                while (rs->next()) {
                    crow::json::wvalue i;
                    i["id"] = rs->getInt("category_id");
                    i["name"] = text(*rs, "name");
                    items.push_back(std::move(i));
                }
                out["categories"] = std::move(items);
            }

            // Load all clubs
            {
                crow::json::wvalue::list items;
                std::unique_ptr<sql::ResultSet> rs(st->executeQuery("SELECT club_id, name, status FROM clubs ORDER BY name"));
                while (rs->next()) {
                    crow::json::wvalue i;
                    i["id"] = rs->getInt("club_id");
                    i["name"] = text(*rs, "name");
                    i["status"] = text(*rs, "status");
                    items.push_back(std::move(i));
                }
                out["clubs"] = std::move(items);
            }

            // Load all staff
            {
                crow::json::wvalue::list items;
                std::unique_ptr<sql::ResultSet> rs(st->executeQuery(
                    "SELECT s.club_id, c.name AS club, s.user_id, "
                    "CONCAT(u.first_name, ' ', u.last_name) AS person, u.email, s.staff_role, s.position "
                    "FROM club_staff s JOIN clubs c ON c.club_id = s.club_id "
                    "JOIN users u ON u.user_id = s.user_id ORDER BY c.name, s.staff_role"));
                while (rs->next()) {
                    crow::json::wvalue i;
                    i["club_id"] = rs->getInt("club_id");
                    i["club"] = text(*rs, "club");
                    i["user_id"] = rs->getInt("user_id");
                    i["person"] = text(*rs, "person");
                    i["email"] = text(*rs, "email");
                    i["role"] = text(*rs, "staff_role");
                    i["position"] = text(*rs, "position");
                    items.push_back(std::move(i));
                }
                out["staff"] = std::move(items);
            }
            return json_reply(200, std::move(out));
        } catch (sql::SQLException& e) {
            std::cerr << "DB error: " << e.what() << std::endl;
            return json_reply(500, error_json("Database error"));
        }
    });

    // registers clubs into the system
    CROW_ROUTE(app, "/api/admin/clubs").methods(crow::HTTPMethod::Post)([&app](const crow::request& req) {
        auto body = crow::json::load(req.body);
        if (!body) return json_reply(400, error_json("Invalid request"));
        std::string name = field(body, "name"), desc = field(body, "description");
        std::string schedule = field(body, "schedule"), place = field(body, "place"), email = field(body, "email");
        int category = int_field(body, "category_id");
        if (name.empty() || desc.empty()) return json_reply(400, error_json("Please enter the club name and description"));
        if (name.size() > 120) return json_reply(400, error_json("Club name is too long (120 characters max)"));
        if (schedule.size() > 150 || place.size() > 150 || email.size() > 150)
            return json_reply(400, error_json("Schedule, place and email must be 150 characters or less"));
        try {
            Session me; Conn con; crow::response err;
            if (!admin_check(app, req, con, me, err)) return err;
            std::unique_ptr<sql::PreparedStatement> ps(con->prepareStatement(
                "INSERT INTO clubs (category_id, name, description, meeting_schedule, meeting_place, contact_email) "
                "VALUES (?, ?, ?, ?, ?, ?)"));
            if (category > 0) ps->setInt(1, category); else ps->setNull(1, sql::DataType::INTEGER);
            ps->setString(2, name);
            ps->setString(3, desc);
            if (schedule.empty()) ps->setNull(4, sql::DataType::VARCHAR); else ps->setString(4, schedule);
            if (place.empty()) ps->setNull(5, sql::DataType::VARCHAR); else ps->setString(5, place);
            if (email.empty()) ps->setNull(6, sql::DataType::VARCHAR); else ps->setString(6, email);
            ps->executeUpdate();
        } catch (sql::SQLException& e) {
            if (e.getErrorCode() == 1062) return json_reply(409, error_json("A club with that name already exists"));
            std::cerr << "DB error: " << e.what() << std::endl;
            return json_reply(500, error_json("Database error"));
        }
        return ok_reply();
    });

    //club status update route
    CROW_ROUTE(app, "/api/admin/clubs/<int>/status").methods(crow::HTTPMethod::Post)(
        [&app](const crow::request& req, int id) {
        auto body = crow::json::load(req.body);
        if (!body) return json_reply(400, error_json("Invalid request"));
        std::string status = field(body, "status");
        if (status != "active" && status != "inactive" && status != "on_hiatus")
            return json_reply(400, error_json("Invalid status"));
        try {
            Session me; Conn con; crow::response err;
            if (!admin_check(app, req, con, me, err)) return err;
            std::unique_ptr<sql::PreparedStatement> ps(con->prepareStatement("UPDATE clubs SET status = ? WHERE club_id = ?"));
            ps->setString(1, status);
            ps->setInt(2, id);
            ps->executeUpdate();
        } catch (sql::SQLException& e) {
            std::cerr << "DB error: " << e.what() << std::endl;
            return json_reply(500, error_json("Database error"));
        }
        return ok_reply();
    });

    // assign staff to clubs or register a specific person as staff for a club
    CROW_ROUTE(app, "/api/admin/staff").methods(crow::HTTPMethod::Post)([&app](const crow::request& req) {
        auto body = crow::json::load(req.body);
        if (!body) return json_reply(400, error_json("Invalid request"));
        int club_id = int_field(body, "club_id");
        std::string email = lower(field(body, "email")), role = field(body, "staff_role"), position = field(body, "position");
        if (club_id <= 0 || email.empty()) return json_reply(400, error_json("Please choose a club and enter the person's email"));
        if (role != "adviser" && role != "officer") return json_reply(400, error_json("Role must be adviser or officer"));
        if (position.size() > 80) return json_reply(400, error_json("Position is too long (80 characters max)"));
        try {
            Session me; Conn con; crow::response err;
            if (!admin_check(app, req, con, me, err)) return err;
            int user_id = 0;
            {
                std::unique_ptr<sql::PreparedStatement> ps(con->prepareStatement("SELECT user_id FROM users WHERE email = ?"));
                ps->setString(1, email);
                std::unique_ptr<sql::ResultSet> rs(ps->executeQuery());
                if (!rs->next()) return json_reply(404, error_json("No account with that email. They need to sign up first."));
                user_id = rs->getInt("user_id");
            }
            std::unique_ptr<sql::PreparedStatement> ps(con->prepareStatement(
                "INSERT INTO club_staff (club_id, user_id, staff_role, position) VALUES (?, ?, ?, ?) "
                "ON DUPLICATE KEY UPDATE staff_role = ?, position = ?"));
            ps->setInt(1, club_id);
            ps->setInt(2, user_id);
            ps->setString(3, role);
            if (position.empty()) ps->setNull(4, sql::DataType::VARCHAR); else ps->setString(4, position);
            ps->setString(5, role);
            if (position.empty()) ps->setNull(6, sql::DataType::VARCHAR); else ps->setString(6, position);
            ps->executeUpdate();
        } catch (sql::SQLException& e) {
            if (e.getErrorCode() == 1452) return json_reply(404, error_json("Club not found"));
            std::cerr << "DB error: " << e.what() << std::endl;
            return json_reply(500, error_json("Database error"));
        }
        return ok_reply();
    });

    // remove a staff member from a club
    CROW_ROUTE(app, "/api/admin/staff/<int>/<int>").methods(crow::HTTPMethod::Delete)(
        [&app](const crow::request& req, int club_id, int user_id) {
        try {
            Session me; Conn con; crow::response err;
            if (!admin_check(app, req, con, me, err)) return err;
            std::unique_ptr<sql::PreparedStatement> ps(con->prepareStatement("DELETE FROM club_staff WHERE club_id = ? AND user_id = ?"));
            ps->setInt(1, club_id);
            ps->setInt(2, user_id);
            ps->executeUpdate();
        } catch (sql::SQLException& e) {
            std::cerr << "DB error: " << e.what() << std::endl;
            return json_reply(500, error_json("Database error"));
        }
        return ok_reply();
    });

    // members and attendance 
    CROW_ROUTE(app, "/api/clubs/<int>/members").methods(crow::HTTPMethod::Get, crow::HTTPMethod::Post)(
        [&app](const crow::request& req, int id) {
        try {
            Session me; Conn con; crow::response err;
            if (!manager_check(app, req, id, con, me, err)) return err;

            if (req.method == crow::HTTPMethod::Post) {
                auto body = crow::json::load(req.body);
                if (!body) return json_reply(400, error_json("Invalid request"));
                std::string email = lower(field(body, "email"));
                if (email.empty()) return json_reply(400, error_json("Please enter the member's email"));
                int user_id = 0;
                {
                    std::unique_ptr<sql::PreparedStatement> ps(con->prepareStatement("SELECT user_id FROM users WHERE email = ?"));
                    ps->setString(1, email);
                    std::unique_ptr<sql::ResultSet> rs(ps->executeQuery());
                    if (!rs->next()) return json_reply(404, error_json("No account with that email. They need to sign up first."));
                    user_id = rs->getInt("user_id");
                }
                std::unique_ptr<sql::PreparedStatement> ps(con->prepareStatement(
                    "INSERT INTO club_members (club_id, user_id) VALUES (?, ?) ON DUPLICATE KEY UPDATE status = 'active'"));
                ps->setInt(1, id);
                ps->setInt(2, user_id);
                ps->executeUpdate();
                return ok_reply();
            }

            crow::json::wvalue out;
            out["members"] = rows(*con,
                "SELECT m.user_id, CONCAT(u.first_name, ' ', u.last_name) AS name, u.email "
                "FROM club_members m JOIN users u ON u.user_id = m.user_id "
                "WHERE m.club_id = ? AND m.status = 'active' ORDER BY u.last_name, u.first_name", id,
                {{"user_id", "user_id", false}, {"name", "name", false}, {"email", "email", false}});
            return json_reply(200, std::move(out));
        } catch (sql::SQLException& e) {
            std::cerr << "DB error: " << e.what() << std::endl;
            return json_reply(500, error_json("Database error"));
        }
    });

    CROW_ROUTE(app, "/api/clubs/<int>/members/<int>").methods(crow::HTTPMethod::Delete)(
        [&app](const crow::request& req, int cid, int uid) {
        return remove_row(app, req, cid, uid, "DELETE FROM club_members WHERE user_id = ? AND club_id = ?");
    });

    // event tracker and attendance
    CROW_ROUTE(app, "/api/clubs/<int>/events/<int>/attendance").methods(crow::HTTPMethod::Get, crow::HTTPMethod::Post)(
        [&app](const crow::request& req, int club_id, int event_id) {
        try {
            Session me; Conn con; crow::response err;
            if (!manager_check(app, req, club_id, con, me, err)) return err;
            {
                std::unique_ptr<sql::PreparedStatement> ps(con->prepareStatement(
                    "SELECT 1 FROM events WHERE event_id = ? AND club_id = ?"));
                ps->setInt(1, event_id);
                ps->setInt(2, club_id);
                std::unique_ptr<sql::ResultSet> rs(ps->executeQuery());
                if (!rs->next()) return json_reply(404, error_json("Event not found"));
            }

            if (req.method == crow::HTTPMethod::Post) {
                auto body = crow::json::load(req.body);
                if (!body || !body.has("records") || body["records"].t() != crow::json::type::List)
                    return json_reply(400, error_json("Invalid request"));
                std::unique_ptr<sql::PreparedStatement> ps(con->prepareStatement(
                    "INSERT INTO event_attendance (event_id, user_id, status, recorded_by) "
                    "SELECT ?, user_id, ?, ? FROM club_members WHERE club_id = ? AND user_id = ? "
                    "ON DUPLICATE KEY UPDATE status = ?, recorded_by = ?, recorded_at = CURRENT_TIMESTAMP"));
                for (auto& r : body["records"]) {
                    int uid = int_field(r, "user_id");
                    std::string status = field(r, "status");
                    bool valid = status == "present" || status == "late" || status == "absent" || status == "excused";
                    if (uid <= 0 || !valid) return json_reply(400, error_json("Invalid attendance record"));
                    ps->setInt(1, event_id);
                    ps->setString(2, status);
                    ps->setInt(3, me.user_id);
                    ps->setInt(4, club_id);
                    ps->setInt(5, uid);
                    ps->setString(6, status);
                    ps->setInt(7, me.user_id);
                    ps->executeUpdate();
                }
                return ok_reply();
            }

            crow::json::wvalue out;
            out["members"] = rows(*con,
                "SELECT m.user_id, CONCAT(u.first_name, ' ', u.last_name) AS name, COALESCE(a.status, '') AS status "
                "FROM club_members m JOIN users u ON u.user_id = m.user_id "
                "LEFT JOIN event_attendance a ON a.user_id = m.user_id AND a.event_id = " + std::to_string(event_id) + " "
                "WHERE m.club_id = ? AND m.status = 'active' ORDER BY u.last_name, u.first_name", club_id,
                {{"user_id", "user_id", false}, {"name", "name", false}, {"status", "status", false}});
            return json_reply(200, std::move(out));
        } catch (sql::SQLException& e) {
            std::cerr << "DB error: " << e.what() << std::endl;
            return json_reply(500, error_json("Database error"));
        }
    });

}