#ifndef HELPERS_H
#define HELPERS_H

#include "crow.h"
#include "crow/middlewares/cookie_parser.h"
#include <mysql/jdbc.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>

using App = crow::App<crow::CookieParser>;
using Conn = std::unique_ptr<sql::Connection>;

struct Session { int user_id; std::string name, user_type, system_role; };
struct Col { const char* key; const char* column; bool is_bool; };

extern std::unordered_map<std::string, Session> sessions;
extern std::mutex sessions_mutex;

Conn open_db();
std::string text(sql::ResultSet& rs, const std::string& col);
crow::response json_reply(int code, crow::json::wvalue&& body);
crow::json::wvalue error_json(const std::string& msg);
std::string trim(const std::string& s);
bool has_whitespace(const std::string& s);
std::string raw_field(crow::json::rvalue& body, const char* key);
std::string field(crow::json::rvalue& body, const char* key);
std::string lower(std::string s);
bool ends_with(const std::string& s, const std::string& suffix);
crow::json::wvalue::list rows(sql::Connection& con, const std::string& query, int param, const std::vector<Col>& cols);
std::string new_token();
bool current_session(App& app, const crow::request& req, Session& out);
bool is_super_admin(sql::Connection& con, int user_id);
bool can_manage_club(sql::Connection& con, int user_id, int club_id);
std::string to_datetime(std::string s);
crow::response ok_reply();
crow::response remove_row(App& app, const crow::request& req, int club_id, int row_id, const char* sql);
int int_field(crow::json::rvalue& body, const char* key);
bool admin_check(App& app, const crow::request& req, Conn& con, Session& me, crow::response& err);
bool manager_check(App& app, const crow::request& req, int club_id, Conn& con, Session& me, crow::response& err);
int last_insert_id(sql::Connection& con);

#endif