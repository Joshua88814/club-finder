#include "email.h"
#include <curl/curl.h>
#include <thread>
#include <iostream>
#include <cstring>
#include "config.h"

// Check if SMTP is enabled by verifying that the SMTP_USER and SMTP_PASS are set and not default values
bool smtp_enabled() {
    return !SMTP_USER.empty() && !SMTP_PASS.empty() &&
           SMTP_USER != "your_gmail@gmail.com" && SMTP_PASS != "your_app_password";
}

std::string one_line(std::string s) {
    for (char& c : s) if (c == '\r' || c == '\n') c = ' ';
    return s;
}

struct EmailBody { std::string data; size_t pos; };
size_t email_read_callback(char* buffer, size_t size, size_t items, void* user) {
    EmailBody* body = static_cast<EmailBody*>(user);
    size_t room = size * items;
    size_t left = body->data.size() - body->pos;
    size_t count = room < left ? room : left;
    std::memcpy(buffer, body->data.data() + body->pos, count);
    body->pos += count;
    return count;
}

// Send an email using SMTP via libcurl. Returns an empty string on success, or an error message on failure.
std::string send_email(const std::string& to, const std::string& subject, const std::string& text) {
    for (char c : to)
        if (c == '\r' || c == '\n' || c == '<' || c == '>' || c == ' ') return "Invalid email address";

    CURL* curl = curl_easy_init();
    if (!curl) return "Could not start the email library";

    std::string body_text;
    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] == '\n' && (i == 0 || text[i - 1] != '\r')) body_text += '\r';
        body_text += text[i];
    }
    EmailBody body{"To: <" + to + ">\r\n"
                   "From: DLSL Club Finder <" + SMTP_USER + ">\r\n"
                   "Subject: " + subject + "\r\n"
                   "MIME-Version: 1.0\r\n"
                   "Content-Type: text/plain; charset=UTF-8\r\n"
                   "Content-Transfer-Encoding: 8bit\r\n\r\n" + body_text + "\r\n", 0};

    std::string rcpt = "<" + to + ">", from = "<" + SMTP_USER + ">";
    struct curl_slist* recipients = curl_slist_append(nullptr, rcpt.c_str());
    curl_easy_setopt(curl, CURLOPT_URL, "smtps://smtp.gmail.com:465");
    curl_easy_setopt(curl, CURLOPT_USERNAME, SMTP_USER.c_str());
    curl_easy_setopt(curl, CURLOPT_PASSWORD, SMTP_PASS.c_str());
    curl_easy_setopt(curl, CURLOPT_MAIL_FROM, from.c_str());
    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, email_read_callback);
    curl_easy_setopt(curl, CURLOPT_READDATA, &body);
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    CURLcode result = curl_easy_perform(curl);
    curl_slist_free_all(recipients);
    curl_easy_cleanup(curl);
    return result == CURLE_OK ? "" : curl_easy_strerror(result);
}

// Notifies members of a club about a new announcement or event by sending them an email in the background
void send_emails_in_background(std::vector<std::string> emails, std::string message) {
    if (emails.empty()) return;
    std::thread([emails, message]() {
        for (const std::string& to : emails) {
            std::string error = send_email(to, "New notification from DLSL Club Finder",
                                           message + "\n\nLog in to DLSL Club Finder to see the details.");
            if (!error.empty()) std::cerr << "Email to " << to << " failed: " << error << std::endl;
        }
    }).detach();
}

void notify_followers(sql::Connection& con, int club_id, int poster_id, const std::string& what,
                      const std::string& title, int announcement_id, int event_id) {
    std::string club_name;
    {
        std::unique_ptr<sql::PreparedStatement> ps(con.prepareStatement("SELECT name FROM clubs WHERE club_id = ?"));
        ps->setInt(1, club_id);
        std::unique_ptr<sql::ResultSet> rs(ps->executeQuery());
        if (rs->next()) club_name = rs->getString("name").asStdString();
    }
    std::string message = "New " + what + " from " + club_name + ": " + title;
    if (message.size() > 250) {
        size_t cut = 250;
        while (cut > 0 && (static_cast<unsigned char>(message[cut]) & 0xC0) == 0x80) cut--;
        message = message.substr(0, cut) + "...";
    }
    std::unique_ptr<sql::PreparedStatement> ps(con.prepareStatement(
        "INSERT INTO notifications (user_id, announcement_id, event_id, message) "
        "SELECT user_id, ?, ?, ? FROM club_follows WHERE club_id = ? AND user_id <> ?"));
    if (announcement_id > 0) ps->setInt(1, announcement_id); else ps->setNull(1, sql::DataType::INTEGER);
    if (event_id > 0) ps->setInt(2, event_id); else ps->setNull(2, sql::DataType::INTEGER);
    ps->setString(3, message);
    ps->setInt(4, club_id);
    ps->setInt(5, poster_id);
    ps->executeUpdate();

    if (smtp_enabled()) {
        std::vector<std::string> emails;
        std::unique_ptr<sql::PreparedStatement> who(con.prepareStatement(
            "SELECT u.email FROM club_follows f JOIN users u ON u.user_id = f.user_id "
            "WHERE f.club_id = ? AND f.user_id <> ? AND f.email_alerts = TRUE AND u.is_active = TRUE"));
        who->setInt(1, club_id);
        who->setInt(2, poster_id);
        std::unique_ptr<sql::ResultSet> rs(who->executeQuery());
        while (rs->next()) emails.push_back(rs->getString("email").asStdString());
        send_emails_in_background(emails, one_line(message));
    }
}