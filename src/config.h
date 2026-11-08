#ifndef CONFIG_H
#define CONFIG_H

#include <string>

// Database configuration
const std::string DB_HOST = "localhost";
const std::string DB_USER = "root";          // Replace with your MySQL username if different
const std::string DB_PASS = "123456789"; // Replace with your MySQL password
const std::string DB_NAME = "dlsl_club_finder";

// School email domain restriction
const std::string EMAIL_DOMAIN = "@dlsl.edu.ph";

// Gmail SMTP settings for email notifications (leave as placeholders if not using email yet)
const std::string SMTP_USER = "dave_evan_mercader@dlsl.edu.ph";
const std::string SMTP_PASS = "vituxetbxghtjuul";

#endif