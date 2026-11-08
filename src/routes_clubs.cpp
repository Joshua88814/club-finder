#include "routes.h"
#include "helpers.h"
#include "config.h"

void register_club_routes(App& app) {
    // brows all clubs in the system
    CROW_ROUTE(app, "/api/clubs")([&app](const crow::request& req) {
        Session me;
        if (!current_session(app, req, me)) return json_reply(401, error_json("Please log in"));
        crow::json::wvalue::list clubs;
        try {
            Conn con = open_db();
            std::unique_ptr<sql::Statement> st(con->createStatement());
            std::unique_ptr<sql::ResultSet> rs(st->executeQuery(
                "SELECT club_id, name, category, status, meeting_schedule, recruitment_status "
                "FROM v_club_directory ORDER BY name"));
            while (rs->next()) {
                crow::json::wvalue club;
                club["id"] = rs->getInt("club_id");
                club["name"] = text(*rs, "name");
                club["category"] = text(*rs, "category");
                club["status"] = text(*rs, "status");
                club["schedule"] = text(*rs, "meeting_schedule");
                club["recruitment"] = text(*rs, "recruitment_status");
                clubs.push_back(std::move(club));
            }
        } catch (sql::SQLException& e) {
            std::cerr << "DB error: " << e.what() << std::endl;
            return json_reply(500, error_json("Database error"));
        }
        return json_reply(200, crow::json::wvalue(std::move(clubs)));
    });

    // Club details route 
    CROW_ROUTE(app, "/api/clubs/<int>")([&app](const crow::request& req, int id) {
        Session me;
        if (!current_session(app, req, me)) return json_reply(401, error_json("Please log in"));
        try {
            Conn con = open_db();
            crow::json::wvalue out;
            {
                std::unique_ptr<sql::PreparedStatement> ps(con->prepareStatement(
                    "SELECT c.name, c.description, c.status, c.meeting_schedule, "
                    "c.meeting_place, c.contact_email, cc.name AS category "
                    "FROM clubs c LEFT JOIN club_categories cc ON cc.category_id = c.category_id "
                    "WHERE c.club_id = ?"));
                ps->setInt(1, id);
                std::unique_ptr<sql::ResultSet> rs(ps->executeQuery());
                if (!rs->next()) return json_reply(404, error_json("Club not found"));
                out["id"] = id;
                out["name"] = text(*rs, "name");
                out["description"] = text(*rs, "description");
                out["status"] = text(*rs, "status");
                out["category"] = text(*rs, "category");
                out["schedule"] = text(*rs, "meeting_schedule");
                out["place"] = text(*rs, "meeting_place");
                out["email"] = text(*rs, "contact_email");
            }

            // Load requirements, recruitment, announcements, and events list
            out["requirements"] = rows(*con,
                "SELECT requirement_id, description, is_mandatory FROM club_requirements "
                "WHERE club_id = ? ORDER BY sort_order, requirement_id", id,
                {{"id", "requirement_id", false}, {"description", "description", false}, {"mandatory", "is_mandatory", true}});

            out["recruitment"] = rows(*con,
                "SELECT period_id, school_year, start_date, end_date, notes, "
                "(CURRENT_DATE BETWEEN start_date AND end_date) AS is_open "
                "FROM recruitment_periods WHERE club_id = ? ORDER BY start_date DESC", id,
                {{"id", "period_id", false}, {"school_year", "school_year", false}, {"start", "start_date", false},
                 {"end", "end_date", false}, {"notes", "notes", false}, {"open", "is_open", true}});

            out["announcements"] = rows(*con,
                "SELECT announcement_id, title, body, category, is_pinned, created_at FROM announcements "
                "WHERE club_id = ? ORDER BY is_pinned DESC, created_at DESC LIMIT 10", id,
                {{"id", "announcement_id", false}, {"title", "title", false}, {"body", "body", false}, {"category", "category", false},
                 {"pinned", "is_pinned", true}, {"date", "created_at", false}});

            out["events"] = rows(*con,
                "SELECT event_id, title, description, venue, start_time, end_time FROM events "
                "WHERE club_id = ? ORDER BY start_time DESC LIMIT 10", id,
                {{"id", "event_id", false}, {"title", "title", false}, {"description", "description", false},
                 {"venue", "venue", false}, {"start", "start_time", false}, {"end", "end_time", false}});

            // Load questions & answers list
            std::vector<std::pair<int, crow::json::wvalue>> q_items;
            {
                std::unique_ptr<sql::PreparedStatement> ps(con->prepareStatement(
                    "SELECT q.question_id, q.question, q.created_at, "
                    "CONCAT(u.first_name, ' ', LEFT(u.last_name, 1), '.') AS asker "
                    "FROM club_questions q JOIN users u ON u.user_id = q.asker_id "
                    "WHERE q.club_id = ? ORDER BY q.created_at DESC"));
                ps->setInt(1, id);
                std::unique_ptr<sql::ResultSet> rs(ps->executeQuery());
                crow::json::wvalue::list items;
                while (rs->next()) {
                    int qid = rs->getInt("question_id");
                    crow::json::wvalue item;
                    item["id"] = qid;
                    item["question"] = text(*rs, "question");
                    item["asker"] = text(*rs, "asker");
                    item["date"] = text(*rs, "created_at");
                    item["answers"] = rows(*con,
                        "SELECT a.answer, a.created_at, "
                        "CONCAT(u.first_name, ' ', LEFT(u.last_name, 1), '.') AS answerer "
                        "FROM club_answers a JOIN users u ON u.user_id = a.answerer_id "
                        "WHERE a.question_id = ? ORDER BY a.created_at", qid,
                        {{"answer", "answer", false}, {"answerer", "answerer", false}, {"date", "created_at", false}});
                    items.push_back(std::move(item));
                }
                out["questions"] = std::move(items);
            }

            out["can_manage"] = can_manage_club(*con, me.user_id, id);
            {
                std::unique_ptr<sql::PreparedStatement> ps(con->prepareStatement(
                    "SELECT 1 FROM club_follows WHERE user_id = ? AND club_id = ?"));
                ps->setInt(1, me.user_id);
                ps->setInt(2, id);
                std::unique_ptr<sql::ResultSet> rs(ps->executeQuery());
                out["following"] = rs->next();
            }
            return json_reply(200, std::move(out));
        } catch (sql::SQLException& e) {
            std::cerr << "DB error: " << e.what() << std::endl;
            return json_reply(500, error_json("Database error"));
        }
    });

    // single announcement lookup 
    CROW_ROUTE(app, "/api/announcements/<int>")([&app](const crow::request& req, int id) {
        Session me;
        if (!current_session(app, req, me)) return json_reply(401, error_json("Please log in"));
        try {
            Conn con = open_db();
            std::unique_ptr<sql::PreparedStatement> ps(con->prepareStatement(
                "SELECT a.club_id, c.name AS club, a.title, a.body, a.category, a.is_pinned, a.created_at, "
                "CONCAT(u.first_name, ' ', LEFT(u.last_name, 1), '.') AS author "
                "FROM announcements a LEFT JOIN clubs c ON c.club_id = a.club_id "
                "JOIN users u ON u.user_id = a.author_id WHERE a.announcement_id = ?"));
            ps->setInt(1, id);
            std::unique_ptr<sql::ResultSet> rs(ps->executeQuery());
            if (!rs->next()) return json_reply(404, error_json("Announcement not found"));
            crow::json::wvalue out;
            out["club_id"] = text(*rs, "club_id");
            out["club"] = text(*rs, "club");
            out["title"] = text(*rs, "title");
            out["body"] = text(*rs, "body");
            out["category"] = text(*rs, "category");
            out["pinned"] = rs->getBoolean("is_pinned");
            out["date"] = text(*rs, "created_at");
            out["author"] = text(*rs, "author");
            return json_reply(200, std::move(out));
        } catch (sql::SQLException& e) {
            std::cerr << "DB error: " << e.what() << std::endl;
            return json_reply(500, error_json("Database error"));
        }
    });
}