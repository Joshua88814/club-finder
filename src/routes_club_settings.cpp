#include "routes.h"
#include "helpers.h"

void register_club_settings_routes(App& app) {
    // Add Requirement
    CROW_ROUTE(app, "/api/clubs/<int>/requirements").methods(crow::HTTPMethod::Post)(
        [&app](const crow::request& req, int id) {
        Session me;
        if (!current_session(app, req, me)) return json_reply(401, error_json("Please log in"));
        auto body = crow::json::load(req.body);
        if (!body) return json_reply(400, error_json("Invalid request"));
        std::string desc = field(body, "description");
        bool mandatory = !(body.has("mandatory") && body["mandatory"].t() == crow::json::type::False);
        if (desc.empty()) return json_reply(400, error_json("Please enter the requirement"));
        if (desc.size() > 255) return json_reply(400, error_json("Requirement is too long (255 characters max)"));
        try {
            Conn con = open_db();
            if (!can_manage_club(*con, me.user_id, id))
                return json_reply(403, error_json("You are not allowed to change this club"));
            int next_order = 1;
            {
                std::unique_ptr<sql::PreparedStatement> ps(con->prepareStatement(
                    "SELECT COALESCE(MAX(sort_order), 0) + 1 AS n FROM club_requirements WHERE club_id = ?"));
                ps->setInt(1, id);
                std::unique_ptr<sql::ResultSet> rs(ps->executeQuery());
                if (rs->next()) next_order = rs->getInt("n");
            }
            std::unique_ptr<sql::PreparedStatement> ps(con->prepareStatement(
                "INSERT INTO club_requirements (club_id, description, is_mandatory, sort_order) VALUES (?, ?, ?, ?)"));
            ps->setInt(1, id);
            ps->setString(2, desc);
            ps->setBoolean(3, mandatory);
            ps->setInt(4, next_order);
            ps->executeUpdate();
        } catch (sql::SQLException& e) {
            std::cerr << "DB error: " << e.what() << std::endl;
            return json_reply(500, error_json("Database error"));
        }
        return ok_reply();
    });

    // Delete Requirement / Recruitment Period
    CROW_ROUTE(app, "/api/clubs/<int>/requirements/<int>").methods(crow::HTTPMethod::Delete)(
        [&app](const crow::request& req, int cid, int rid) {
        return remove_row(app, req, cid, rid, "DELETE FROM club_requirements WHERE requirement_id = ? AND club_id = ?");
    });
    CROW_ROUTE(app, "/api/clubs/<int>/recruitment/<int>").methods(crow::HTTPMethod::Delete)(
        [&app](const crow::request& req, int cid, int rid) {
        return remove_row(app, req, cid, rid, "DELETE FROM recruitment_periods WHERE period_id = ? AND club_id = ?");
    });

    // Add Recruitment Period
    CROW_ROUTE(app, "/api/clubs/<int>/recruitment").methods(crow::HTTPMethod::Post)(
        [&app](const crow::request& req, int id) {
        Session me;
        if (!current_session(app, req, me)) return json_reply(401, error_json("Please log in"));
        auto body = crow::json::load(req.body);
        if (!body) return json_reply(400, error_json("Invalid request"));
        std::string year = field(body, "school_year"), start = field(body, "start"), end = field(body, "end"), notes = field(body, "notes");
        if (year.empty() || year.size() > 9) return json_reply(400, error_json("Enter the school year, like 2026-2027"));
        if (start.size() != 10 || end.size() != 10) return json_reply(400, error_json("Please choose a start and end date"));
        if (end < start) return json_reply(400, error_json("The end date must not be before the start date"));
        if (notes.size() > 255) return json_reply(400, error_json("Notes are too long (255 characters max)"));
        try {
            Conn con = open_db();
            if (!can_manage_club(*con, me.user_id, id))
                return json_reply(403, error_json("You are not allowed to change this club"));
            std::unique_ptr<sql::PreparedStatement> ps(con->prepareStatement(
                "INSERT INTO recruitment_periods (club_id, school_year, start_date, end_date, notes, created_by) "
                "VALUES (?, ?, ?, ?, ?, ?)"));
            ps->setInt(1, id);
            ps->setString(2, year);
            ps->setString(3, start);
            ps->setString(4, end);
            if (notes.empty()) ps->setNull(5, sql::DataType::VARCHAR); else ps->setString(5, notes);
            ps->setInt(6, me.user_id);
            ps->executeUpdate();
        } catch (sql::SQLException& e) {
            std::cerr << "DB error: " << e.what() << std::endl;
            return json_reply(500, error_json("Database error"));
        }
        return ok_reply();
    });

    // Update Club Description & Schedule Info
    CROW_ROUTE(app, "/api/clubs/<int>/info").methods(crow::HTTPMethod::Post)([&app](const crow::request& req, int id) {
        auto body = crow::json::load(req.body);
        if (!body) return json_reply(400, error_json("Invalid request"));
        std::string desc = field(body, "description"), schedule = field(body, "schedule");
        std::string place = field(body, "place"), email = field(body, "email");
        if (desc.empty()) return json_reply(400, error_json("Please enter a description"));
        if (schedule.size() > 150 || place.size() > 150 || email.size() > 150)
            return json_reply(400, error_json("Schedule, place and email must be 150 characters or less"));
        try {
            Session me; Conn con; crow::response err;
            if (!manager_check(app, req, id, con, me, err)) return err;
            std::unique_ptr<sql::PreparedStatement> ps(con->prepareStatement(
                "UPDATE clubs SET description = ?, meeting_schedule = ?, meeting_place = ?, contact_email = ? "
                "WHERE club_id = ?"));
            ps->setString(1, desc);
            if (schedule.empty()) ps->setNull(2, sql::DataType::VARCHAR); else ps->setString(2, schedule);
            if (place.empty()) ps->setNull(3, sql::DataType::VARCHAR); else ps->setString(3, place);
            if (email.empty()) ps->setNull(4, sql::DataType::VARCHAR); else ps->setString(4, email);
            ps->setInt(5, id);
            ps->executeUpdate();
        } catch (sql::SQLException& e) {
            std::cerr << "DB error: " << e.what() << std::endl;
            return json_reply(500, error_json("Database error"));
        }
        return ok_reply();
    });
}