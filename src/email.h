#ifndef EMAIL_H
#define EMAIL_H

#include <string>
#include <vector>
#include <mysql/jdbc.h>

bool smtp_enabled();
std::string one_line(std::string s);
std::string send_email(const std::string& to, const std::string& subject, const std::string& text);
void send_emails_in_background(std::vector<std::string> emails, std::string message);
void notify_followers(sql::Connection& con, int club_id, int poster_id, const std::string& what,
                      const std::string& title, int announcement_id, int event_id);

#endif