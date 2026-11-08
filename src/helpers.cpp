#include "helpers.h"
#include "config.h"
#include <random>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <sodium.h>

std::unordered_map<std::string, Session> sessions;
std::mutex sessions_mutex;

// establish connection to the database and return a unique_ptr to it
Conn open_db() {
    sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
    Conn con(driver->connect(DB_HOST, DB_USER, DB_PASS));
    con->setSchema(DB_NAME);
    return con;
}

std::string text(sql::ResultSet& rs, const std::string& col) {
    return rs.isNull(col) ? "" : rs.getString(col).asStdString();
}

crow::response json_reply(int code, crow::json::wvalue&& body) {
    crow::response res(code, body.dump());
    res.add_header("Content-Type", "application/json");
    return res;
}

crow::json::wvalue error_json(const std::string& msg) {
    crow::json::wvalue res;
    res["error"] = msg;
    return res;
}

// removes spaces, tabs and line breaks from the start and the end of a text
std::string trim(const std::string& s) {
    size_t start = 0, end = s.size();
    while (start < end && std::isspace(static_cast<unsigned char>(s[start]))) start++;
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) end--;
    return s.substr(start, end - start);
}

// true if the text has any space, tab or line break anywhere inside it
// (also catches the non-breaking space, which looks like a space but is a different character)
bool has_whitespace(const std::string& s) {
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char ch = static_cast<unsigned char>(s[i]);
        if (std::isspace(ch)) return true;
        if (ch == 0xC2 && i + 1 < s.size() && static_cast<unsigned char>(s[i + 1]) == 0xA0) return true;
    }
    return false;
}

// reads a text value exactly as it was sent (used for passwords, which are never trimmed)
std::string raw_field(crow::json::rvalue& body, const char* key) {
    if (body.has(key) && body[key].t() == crow::json::type::String) {
        return std::string(body[key].s());
    }
    return "";
}

// reads a text value and trims it, so a value made only of spaces counts as empty
std::string field(crow::json::rvalue& body, const char* key) {
    return trim(raw_field(body, key));
}

int int_field(crow::json::rvalue& body, const char* key) {
    if (body.has(key) && body[key].t() == crow::json::type::Number) {
        return static_cast<int>(body[key].i());
    }
    return -1;
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
    return s;
}

bool ends_with(const std::string& s, const std::string& suffix) {
    if (s.length() < suffix.length()) return false;
    return s.compare(s.length() - suffix.length(), suffix.length(), suffix) == 0;
}

// generates a token of 64 hex characters (32 bytes) using libsodium's randombytes_buf
std::string new_token() {
    unsigned char raw[32];
    randombytes_buf(raw, sizeof raw);
    char hex[sizeof raw * 2 + 1];
    sodium_bin2hex(hex, sizeof hex, raw, sizeof raw);
    return hex;
}

bool current_session(App& app, const crow::request& req, Session& out) {
    std::string sid = app.get_context<crow::CookieParser>(req).get_cookie("sid");
    if (sid.empty()) return false;
    std::lock_guard<std::mutex> lock(sessions_mutex);
    auto it = sessions.find(sid);
    if (it == sessions.end()) return false;
    out = it->second;
    return true;
}

// check if super admin by querying the database for the user's system_role
bool is_super_admin(sql::Connection& con, int user_id) {
    std::unique_ptr<sql::PreparedStatement> pstmt(con.prepareStatement("SELECT system_role FROM users WHERE user_id = ?"));
    pstmt->setInt(1, user_id);
    std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
    if (res->next()) {
        return res->getString("system_role").asStdString() == "super_admin";
    }
    return false;
}

bool can_manage_club(sql::Connection& con, int user_id, int club_id) {
    if (is_super_admin(con, user_id)) return true;
    std::unique_ptr<sql::PreparedStatement> pstmt(con.prepareStatement("SELECT 1 FROM club_staff WHERE user_id = ? AND club_id = ?"));
    pstmt->setInt(1, user_id);
    pstmt->setInt(2, club_id);
    std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
    return res->next();
}

// converts html datetime string (e.g., "2024-06-01T12:30") to SQL datetime format (e.g., "2024-06-01 12:30:00")
std::string to_datetime(std::string s) {
    std::replace(s.begin(), s.end(), 'T', ' ');
    if (s.size() == 16) s += ":00";
    return s;
}


crow::response ok_reply() {
    crow::json::wvalue res;
    res["ok"] = true;
    return json_reply(200, std::move(res));
}

// helper function to get the last inserted ID from the database after an insert operation
int last_insert_id(sql::Connection& con) {
    std::unique_ptr<sql::Statement> stmt(con.createStatement());
    std::unique_ptr<sql::ResultSet> res(stmt->executeQuery("SELECT LAST_INSERT_ID() AS id"));
    if (res->next()) {
        return res->getInt("id");
    }
    return 0;
}

// helper function to execute a query with one integer parameter and return a list of JSON objects based on the provided columns
crow::json::wvalue::list rows(sql::Connection& con, const std::string& query, int param, const std::vector<Col>& cols) {
    crow::json::wvalue::list list;
    std::unique_ptr<sql::PreparedStatement> pstmt(con.prepareStatement(query));
    if (param != 0) pstmt->setInt(1, param);
    std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
    while (res->next()) {
        crow::json::wvalue item;
        for (const auto& c : cols) {
            if (c.is_bool) {
                item[c.key] = res->getBoolean(c.column);
            } else {
                item[c.key] = text(*res, c.column);
            }
        }
        list.push_back(std::move(item));
    }
    return list;
}

// Ensures that the user is logged in and is a super admin. If not, sets the error response and returns false.
bool admin_check(App& app, const crow::request& req, Conn& con, Session& me, crow::response& err) {
    if (!current_session(app, req, me)) {
        err = json_reply(401, error_json("Please log in"));
        return false;
    }
    con = open_db();
    if (!is_super_admin(*con, me.user_id)) {
        err = json_reply(403, error_json("Super admin only"));
        return false;
    }
    return true;
}

bool manager_check(App& app, const crow::request& req, int club_id, Conn& con, Session& me, crow::response& err) {
    if (!current_session(app, req, me)) {
        err = json_reply(401, error_json("Not logged in"));
        return false;
    }
    con = open_db();
    if (!can_manage_club(*con, me.user_id, club_id)) {
        err = json_reply(403, error_json("You are not allowed to change this club"));
        return false;
    }
    return true;
}

crow::response remove_row(App& app, const crow::request& req, int club_id, int row_id, const char* sql_query) {
    Session me;
    Conn con;
    crow::response err;
    if (!manager_check(app, req, club_id, con, me, err)) return err;

    std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement(sql_query));
    pstmt->setInt(1, row_id);
    pstmt->setInt(2, club_id);
    pstmt->executeUpdate();
    return ok_reply();
}