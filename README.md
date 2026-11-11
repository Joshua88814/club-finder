PROPONENTS
Malisa, Joshua Q.
Cezar, Jared Andrei S.
Mercader, Dave Evan

APP MODULES


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




FUNCTIONS
-----------------------------------------------------------------------------------------------------------------------------------------------------

Not logged in	

- Only the login and sign-up page

-----------------------------------------------------------------------------------------------------------------------------------------------------

Student, teacher, alumni	

- Browse and search clubs, read announcements and events, follow clubs, ask questions, see notifications

-----------------------------------------------------------------------------------------------------------------------------------------------------

Club officer or adviser	

- Everything above, plus manage their own club: announcements, events, requirements, recruitment, club info, members, attendance, answers

-----------------------------------------------------------------------------------------------------------------------------------------------------

Super admin

= Everything above for every club, plus add clubs, change club status and assign staff

-----------------------------------------------------------------------------------------------------------------------------------------------------

SET UP

These steps are for Windows.

1. Install the prerequisites
 - Git
 - Visual Studio with the Desktop development with C++ workload 
 - vcpkg, installed at C:\vcpkg using cmd
 - MySQL Server 8.0.16 or newer
 - VS Code with the C/C++, CMake Tools and SQLTools extensions
   
2. Get the code

 - git clone https://github.com/(your-username)/(your-repo).git
 - cd  (your-repo)

4. Create the database

 - mysql -u root -p < sql/dlsl_club_finder.sql

4. Add your settings

 - copy src\config.example.h src\config.h

 - Open src/config.h and set your MySQL password (DB_PASS). The Gmail settings (SMTP_USER, SMTP_PASS) are optional because they keep their placeholder values no emails are sent, and the notifications on the site still work. For Gmail, use an App password by going to Google Account Security, 2-Step Verification, App passwords , not your normal password.


5. Install the libraries
(u can use cmd for this )
 - cd C:\vcpkg
 - vcpkg install crow:x64-windows-static
 - vcpkg install libsodium:x64-windows-static
 - vcpkg install curl:x64-windows-static
 - vcpkg install "mysql-connector-cpp[jdbc]:x64-windows-static"

6. Build

From the project folder:

 - cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static
 - cmake --build build --config Debug

In VS Code  run CMake: Configure, then press F7. If you move the project, delete the build folder first.

7. Run

Run it from the project folder so it can find the static folder:

 - .\build\Debug\club_finder.exe

 - Open http://localhost:8080. You are sent to the login page.

8. Create the first accounts

The sample users in the SQL file cannot log in. Sign up on the website with a DLSL email, then give the account a role in MySQL.

 - UPDATE dlsl_club_finder.users SET system_role = 'super_admin' WHERE email = 'you@dlsl.edu.ph';

 - to use this you can make a text file with the exetension of sql then right click and then attach file and then run query to update the mysql

