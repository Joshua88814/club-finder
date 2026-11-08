#include "routes.h"
#include "helpers.h"

void register_notification_routes(App& app) {
    // Follow / unfollow club
    CROW_ROUTE(app, "/api/clubs/<int>/follow").methods(crow::HTTPMethod::Post, crow::HTTPMethod::Delete)(
        [&app](const crow::request& req, int id) {
        Session me;
        if (!current_session(app, req, me)) return json_reply(401, error_json("Please log in"));
        try {
            Conn con = open_db();
            std::unique_ptr<sql::PreparedStatement> ps;
            if (req.method == crow::HTTPMethod::Post)
                ps.reset(con->prepareStatement("INSERT IGNORE INTO club_follows (user_id, club_id) VALUES (?, ?)"));
            else
                ps.reset(con->prepareStatement("DELETE FROM club_follows WHERE user_id = ? AND club_id = ?"));
            ps->setInt(1, me.user_id);
            ps->setInt(2, id);
            ps->executeUpdate();
        } catch (sql::SQLException& e) {
            std::cerr << "DB error: " << e.what() << std::endl;
            return json_reply(500, error_json("Database error"));
        }
        return ok_reply();
    });

    // Get notifications
    CROW_ROUTE(app, "/api/notifications")([&app](const crow::request& req) {
        Session me;
        if (!current_session(app, req, me)) return json_reply(401, error_json("Please log in"));
        try {
            Conn con = open_db();
            crow::json::wvalue out;
            {
                std::unique_ptr<sql::PreparedStatement> ps(con->prepareStatement(
                    "SELECT COUNT(*) AS n FROM notifications WHERE user_id = ? AND is_read = FALSE"));
                ps->setInt(1, me.user_id);
                std::unique_ptr<sql::ResultSet> rs(ps->executeQuery());
                out["unread"] = rs->next() ? rs->getInt("n") : 0;
            }
            out["items"] = rows(*con,
                "SELECT n.notification_id, n.message, n.is_read, n.created_at, n.announcement_id, COALESCE(a.club_id, e.club_id) AS club_id "
                "FROM notifications n "
                "LEFT JOIN announcements a ON a.announcement_id = n.announcement_id "
                "LEFT JOIN events e ON e.event_id = n.event_id "
                "WHERE n.user_id = ? ORDER BY n.created_at DESC, n.notification_id DESC LIMIT 30", me.user_id,
                {{"id", "notification_id", false}, {"message", "message", false}, {"read", "is_read", true},
                 {"date", "created_at", false}, {"club_id", "club_id", false},
                 {"announcement_id", "announcement_id", false}});
            return json_reply(200, std::move(out));
        } catch (sql::SQLException& e) {
            std::cerr << "DB error: " << e.what() << std::endl;
            return json_reply(500, error_json("Database error"));
        }
    });

    // Mark notifications as read
    CROW_ROUTE(app, "/api/notifications/read").methods(crow::HTTPMethod::Post)([&app](const crow::request& req) {
        Session me;
        if (!current_session(app, req, me)) return json_reply(401, error_json("Please log in"));
        try {
            Conn con = open_db();
            std::unique_ptr<sql::PreparedStatement> ps(con->prepareStatement(
                "UPDATE notifications SET is_read = TRUE WHERE user_id = ?"));
            ps->setInt(1, me.user_id);
            ps->executeUpdate();
        } catch (sql::SQLException& e) {
            std::cerr << "DB error: " << e.what() << std::endl;
            return json_reply(500, error_json("Database error"));
        }
        return ok_reply();
    });
}