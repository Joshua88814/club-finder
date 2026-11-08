#include "crow.h"
#include <sodium.h>
#include <curl/curl.h>
#include <iostream>
#include "routes.h"

int main() {
    if (sodium_init() < 0) { 
        std::cerr << "Could not start libsodium" << std::endl; 
        return 1; 
    }
    curl_global_init(CURL_GLOBAL_DEFAULT);
    App app;

    register_routes(app);

    std::cout << "Open http://localhost:8080 in your browser" << std::endl;
    app.port(8080).multithreaded().run();
    
    curl_global_cleanup();
    return 0;
}