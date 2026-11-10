PROPONENTS
Malisa, Joshua Q.
Cezar, Jared Andrei S.
Mercader, Dave Evan

App modules and functions


Accounts and login	
- src/routes_auth.cpp, static/login.html
- Sign up with a DLSL email, log in, log out. Passwords must be at least 8 characters with no spaces and are stored as hashes. Three wrong passwords in a row lock the account for 48 hours.
-----------------------------------------------------------------------------------------------------------------------------------------------------
Club directory	

- src/routes_clubs.cpp, static/index.html, static/club.html, static/announcement.html, static/event.html
- Club list with search, filter (Academic or Non-academic) and sort (A-Z or Z-A). Club page with description, schedule, requirements, recruitment periods, announcements, events and questions. Full pages for each announcement and event.

-----------------------------------------------------------------------------------------------------------------------------------------------------

Announcements and events	

- src/routes_posts.cpp
- Club managers post and delete announcements (optionally pinned) and events. Followers are notified.

-----------------------------------------------------------------------------------------------------------------------------------------------------

Club settings	

- src/routes_club_settings.cpp	
- Club managers add and delete requirements and recruitment periods, and edit the club's description, schedule, place and contact email.

-----------------------------------------------------------------------------------------------------------------------------------------------------

Questions and answers	

- src/routes_qa.cpp	
- Any logged-in user asks a question on a club page. Club managers answer.

-----------------------------------------------------------------------------------------------------------------------------------------------------

Follow and notifications	

 - src/routes_notifications.cpp, static/notifications.html, src/email.cpp	

-----------------------------------------------------------------------------------------------------------------------------------------------------

Follow or unfollow clubs. 

- Followers get a notification on the site (with an unread count) and an email through Gmail when the club posts an announcement or event.

-----------------------------------------------------------------------------------------------------------------------------------------------------

Admin and members

- src/routes_admin.cpp, static/admin.html	
 - The super admin adds clubs, changes a club's status (active, inactive, on hiatus) and assigns or removes advisers and officers. Club managers add members and take attendance for each event.

-----------------------------------------------------------------------------------------------------------------------------------------------------

Shared tools	

- src/helpers.cpp, src/helpers.h, src/email.h, src/main.cpp, src/routes.h, src/config.h
- Database connection JSON helpers, login sessions, permission checks, sending email, server start-up and settings.

-----------------------------------------------------------------------------------------------------------------------------------------------------

Database	

- sql/dlsl_club_finder.sql
- Creates the database with 14 tables and one view (users, clubs, club staff, members, requirements, recruitment periods, announcements, events, attendance, questions, answers, follows, notifications).

-----------------------------------------------------------------------------------------------------------------------------------------------------

Build	

- CMakeLists.txt
- Tells CMake which files to compile and which libraries to link.e clubs. 
Only students and teachers with DLSL emails can access the website.

