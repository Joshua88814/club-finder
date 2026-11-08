#include "routes.h"
#include "helpers.h"
#include "email.h"

void register_post_routes(App& app) {
    // Post Announcement
    CROW_ROUTE(app, "/api/clubs/<int>/announcements").methods(crow::HTTPMethod::Post)(
        [&app](const crow::request& req, int id) {
        Session me;
        if (!current_session(app, req, me)) return json_reply(401, error_json("Please log in"));
        auto body = crow::json::load(req.body);
        if (!body) return json_reply(400, error_json("Invalid request"));

        std::string title = field(body, "title"), message = field(body, "body"), category = field(body, "category");
        bool pinned = body.has("is_pinned") && body["is_pinned"].t() == crow::json::type::True;
        if (title.empty() || message.empty()) return json_reply(400, error_json("Please enter a title and a message"));
        if (title.size() > 150) return json_reply(400, error_json("Title is too long (150 characters max)"));
        if (category != "recruitment" && category != "event") category = "general";

        try {
            Conn con = open_db();
            if (!can_manage_club(*con, me.user_id, id))
                return json_reply(403, error_json("You are not allowed to post for this club"));
            std::unique_ptr<sql::PreparedStatement> ps(con->prepareStatement(
                "INSERT INTO announcements (club_id, author_id, title, body, category, is_pinned) "
                "VALUES (?, ?, ?, ?, ?, ?)"));
            ps->setInt(1, id);
            ps->setInt(2, me.user_id);
            ps->setString(3, title);
            ps->setString(4, message);
            ps->setString(5, category);
            ps->setBoolean(6, pinned);
            ps->executeUpdate();
            int new_id = last_insert_id(*con);
            notify_followers(*con, id, me.user_id, "announcement", title, new_id, 0);
        } catch (sql::SQLException& e) {
            std::cerr << "DB error: " << e.what() << std::endl;
            return json_reply(500, error_json("Database error"));
        }
        crow::json::wvalue ok;
        ok["ok"] = true;
        return json_reply(200, std::move(ok));
    });

    // Post Event
    CROW_ROUTE(app, "/api/clubs/<int>/events").methods(crow::HTTPMethod::Post)(
        [&app](const crow::request& req, int id) {
        Session me;
        if (!current_session(app, req, me)) return json_reply(401, error_json("Please log in"));
        auto body = crow::json::load(req.body);
        if (!body) return json_reply(400, error_json("Invalid request"));

        std::string title = field(body, "title"), venue = field(body, "venue"), desc = field(body, "description");
        std::string start = to_datetime(field(body, "start")), end = to_datetime(field(body, "end"));
        if (title.empty()) return json_reply(400, error_json("Please enter an event title"));
        if (title.size() > 150) return json_reply(400, error_json("Title is too long (150 characters max)"));
        if (start.size() != 19 || end.size() != 19) return json_reply(400, error_json("Please choose a start and end time"));
        if (end <= start) return json_reply(400, error_json("The end time must be after the start time"));

        try {
            Conn con = open_db();
            if (!can_manage_club(*con, me.user_id, id))
                return json_reply(403, error_json("You are not allowed to post for this club"));
            std::unique_ptr<sql::PreparedStatement> ps(con->prepareStatement(
                "INSERT INTO events (club_id, title, description, venue, start_time, end_time, created_by) "
                "VALUES (?, ?, ?, ?, ?, ?, ?)"));
            ps->setInt(1, id);
            ps->setString(2, title);
            if (desc.empty()) ps->setNull(3, sql::DataType::VARCHAR); else ps->setString(3, desc);
            if (venue.empty()) ps->setNull(4, sql::DataType::VARCHAR); else ps->setString(4, venue);
            ps->setString(5, start);
            ps->setString(6, end);
            ps->setInt(7, me.user_id);
            ps->executeUpdate();
            int new_id = last_insert_id(*con);
            notify_followers(*con, id, me.user_id, "event", title, 0, new_id);
        } catch (sql::SQLException& e) {
            std::cerr << "DB error: " << e.what() << std::endl;
            return json_reply(500, error_json("Database error"));
        }
        crow::json::wvalue ok;
        ok["ok"] = true;
        return json_reply(200, std::move(ok));
    });

    // Delete Announcement / Event
    CROW_ROUTE(app, "/api/clubs/<int>/announcements/<int>").methods(crow::HTTPMethod::Delete)(
        [&app](const crow::request& req, int cid, int rid) {
        return remove_row(app, req, cid, rid, "DELETE FROM announcements WHERE announcement_id = ? AND club_id = ?");
    });
    CROW_ROUTE(app, "/api/clubs/<int>/events/<int>").methods(crow::HTTPMethod::Delete)(
        [&app](const crow::request& req, int cid, int rid) {
        return remove_row(app, req, cid, rid, "DELETE FROM events WHERE event_id = ? AND club_id = ?");
    });
}