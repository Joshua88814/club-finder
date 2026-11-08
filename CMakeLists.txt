cmake_minimum_required(VERSION 3.20)
project(club_finder CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The x64-windows-static libraries use the static C++ runtime
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

find_package(Crow CONFIG REQUIRED)
find_package(unofficial-mysql-connector-cpp CONFIG REQUIRED)
find_package(unofficial-sodium CONFIG REQUIRED)
find_package(CURL REQUIRED)

add_executable(club_finder 
    src/main.cpp 
    src/helpers.cpp 
    src/routes_auth.cpp 
    src/routes_clubs.cpp 
    src/routes_posts.cpp 
    src/routes_club_settings.cpp 
    src/routes_qa.cpp 
    src/routes_admin.cpp 
    src/routes_notifications.cpp 
    src/email.cpp
)

target_include_directories(club_finder PRIVATE . src)

# Needed when linking the static libraries
target_compile_definitions(club_finder PRIVATE STATIC_CONCPP SODIUM_STATIC CURL_STATICLIB)

target_link_libraries(club_finder PRIVATE
    Crow::Crow
    unofficial::mysql-connector-cpp::connector
    unofficial::mysql-connector-cpp::connector-jdbc
    unofficial-sodium::sodium
    CURL::libcurl)
