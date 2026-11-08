#include "routes.h"
#include "helpers.h"

void register_qa_routes(App& app) {
    // Post a question to a club
    CROW_ROUTE(app, "/api/clubs/<int>/questions").methods(crow::HTTPMethod::Post)(
        [&app](const crow::request& req, int id) {
        Session me;
        if (!current_session(app, req, me)) return json_reply(401, error_json("Please log in"));
        auto body = crow::json::load(req.body);
        if (!body) return json_reply(400, error_json("Invalid request"));
        std::string question = field(body, "question");
        if (question.empty()) return json_reply(400, error_json("Please type your question"));
        if (question.size() > 500) return json_reply(400, error_json("Question is too long (500 characters max)"));
        try {
            Conn con = open_db();
            std::unique_ptr<sql::PreparedStatement> ps(con->prepareStatement(
                "INSERT INTO club_questions (club_id, asker_id, question) VALUES (?, ?, ?)"));
            ps->setInt(1, id);
            ps->setInt(2, me.user_id);
            ps->setString(3, question);
            ps->executeUpdate();
        } catch (sql::SQLException& e) {
            std::cerr << "DB error: " << e.what() << std::endl;
            return json_reply(500, error_json("Database error"));
        }
        return ok_reply();
    });

    // Answer a club question
    CROW_ROUTE(app, "/api/questions/<int>/answers").methods(crow::HTTPMethod::Post)(
        [&app](const crow::request& req, int qid) {
        Session me;
        if (!current_session(app, req, me)) return json_reply(401, error_json("Please log in"));
        auto body = crow::json::load(req.body);
        if (!body) return json_reply(400, error_json("Invalid request"));
        std::string answer = field(body, "answer");
        if (answer.empty()) return json_reply(400, error_json("Please type an answer"));
        if (answer.size() > 1000) return json_reply(400, error_json("Answer is too long (1000 characters max)"));
        try {
            Conn con = open_db();
            int club_id = 0;
            {
                std::unique_ptr<sql::PreparedStatement> ps(con->prepareStatement(
                    "SELECT club_id FROM club_questions WHERE question_id = ?"));
                ps->setInt(1, qid);
                std::unique_ptr<sql::ResultSet> rs(ps->executeQuery());
                if (!rs->next()) return json_reply(404, error_json("Question not found"));
                club_id = rs->getInt("club_id");
            }
            if (!can_manage_club(*con, me.user_id, club_id))
                return json_reply(403, error_json("Only this club's adviser or officers can answer"));
            std::unique_ptr<sql::PreparedStatement> ps(con->prepareStatement(
                "INSERT INTO club_answers (question_id, answerer_id, answer) VALUES (?, ?, ?)"));
            ps->setInt(1, qid);
            ps->setInt(2, me.user_id);
            ps->setString(3, answer);
            ps->executeUpdate();
        } catch (sql::SQLException& e) {
            std::cerr << "DB error: " << e.what() << std::endl;
            return json_reply(500, error_json("Database error"));
        }
        return ok_reply();
    });
}