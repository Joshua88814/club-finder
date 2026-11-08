#pragma once
#include "crow.h"
#include "crow/middlewares/cookie_parser.h"

using App = crow::App<crow::CookieParser>;

// Function declarations for each modular route file
void register_auth_routes(App& app);
void register_club_routes(App& app);
void register_post_routes(App& app);
void register_club_settings_routes(App& app);
void register_qa_routes(App& app);
void register_admin_routes(App& app);
void register_notification_routes(App& app);

// Function called by main.cpp to register all routes at once
inline void register_routes(App& app) {
    register_auth_routes(app);
    register_club_routes(app);
    register_post_routes(app);
    register_club_settings_routes(app);
    register_qa_routes(app);
    register_admin_routes(app);
    register_notification_routes(app);
}