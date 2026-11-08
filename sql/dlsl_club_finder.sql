DROP DATABASE IF EXISTS dlsl_club_finder;
CREATE DATABASE dlsl_club_finder CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE dlsl_club_finder;



CREATE TABLE users (
    user_id         INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    email           VARCHAR(150) NOT NULL UNIQUE,
    password_hash   VARCHAR(255) NOT NULL,         
    first_name      VARCHAR(60)  NOT NULL,
    last_name       VARCHAR(60)  NOT NULL,
    user_type       ENUM('student','teacher','alumni') NOT NULL,
    system_role     ENUM('user','super_admin') NOT NULL DEFAULT 'user',
    grade_level     VARCHAR(20) NULL,               
    is_active       BOOLEAN NOT NULL DEFAULT TRUE,
    failed_attempts INT DEFAULT 0,                 
    locked_until    DATETIME NULL,                  
    created_at      TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT chk_dlsl_email CHECK (email LIKE '%@dlsl.edu.ph')
) ENGINE=InnoDB;

CREATE TABLE club_categories (
    category_id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    name        VARCHAR(80) NOT NULL UNIQUE          
) ENGINE=InnoDB;

CREATE TABLE clubs (
    club_id           INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    category_id       INT UNSIGNED NULL,
    name              VARCHAR(120) NOT NULL UNIQUE,
    description       TEXT NOT NULL,
    status            ENUM('active','inactive','on_hiatus') NOT NULL DEFAULT 'active',
    meeting_schedule  VARCHAR(150) NULL,             
    meeting_place     VARCHAR(150) NULL,
    contact_email     VARCHAR(150) NULL,
    created_at        TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at        TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    FOREIGN KEY (category_id) REFERENCES club_categories(category_id) ON DELETE SET NULL,
    FULLTEXT KEY ft_club_search (name, description)  
) ENGINE=InnoDB;

CREATE TABLE club_staff (
    club_id     INT UNSIGNED NOT NULL,
    user_id     INT UNSIGNED NOT NULL,
    staff_role  ENUM('adviser','officer') NOT NULL,
    position    VARCHAR(80) NULL,                    
    assigned_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (club_id, user_id),
    FOREIGN KEY (club_id) REFERENCES clubs(club_id) ON DELETE CASCADE,
    FOREIGN KEY (user_id) REFERENCES users(user_id) ON DELETE CASCADE
) ENGINE=InnoDB;

CREATE TABLE club_members (
    club_id   INT UNSIGNED NOT NULL,
    user_id   INT UNSIGNED NOT NULL,
    status    ENUM('active','inactive') NOT NULL DEFAULT 'active',
    joined_at DATE NOT NULL DEFAULT (CURRENT_DATE),
    PRIMARY KEY (club_id, user_id),
    FOREIGN KEY (club_id) REFERENCES clubs(club_id) ON DELETE CASCADE,
    FOREIGN KEY (user_id) REFERENCES users(user_id) ON DELETE CASCADE
) ENGINE=InnoDB;

CREATE TABLE club_requirements (
    requirement_id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    club_id        INT UNSIGNED NOT NULL,
    description    VARCHAR(255) NOT NULL,            
    is_mandatory   BOOLEAN NOT NULL DEFAULT TRUE,
    sort_order     TINYINT UNSIGNED NOT NULL DEFAULT 0,
    FOREIGN KEY (club_id) REFERENCES clubs(club_id) ON DELETE CASCADE
) ENGINE=InnoDB;

CREATE TABLE recruitment_periods (
    period_id    INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    club_id      INT UNSIGNED NOT NULL,
    school_year  VARCHAR(9) NOT NULL,               
    start_date   DATE NOT NULL,
    end_date     DATE NOT NULL,
    notes        VARCHAR(255) NULL,                 
    created_by   INT UNSIGNED NULL,
    FOREIGN KEY (club_id)    REFERENCES clubs(club_id) ON DELETE CASCADE,
    FOREIGN KEY (created_by) REFERENCES users(user_id) ON DELETE SET NULL,
    CONSTRAINT chk_recruit_dates CHECK (end_date >= start_date)
) ENGINE=InnoDB;

CREATE TABLE announcements (
    announcement_id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    club_id         INT UNSIGNED NULL,
    author_id       INT UNSIGNED NOT NULL,
    title           VARCHAR(150) NOT NULL,
    body            TEXT NOT NULL,
    category        ENUM('recruitment','event','general') NOT NULL DEFAULT 'general',
    is_pinned       BOOLEAN NOT NULL DEFAULT FALSE,
    created_at      TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (club_id)   REFERENCES clubs(club_id) ON DELETE CASCADE,
    FOREIGN KEY (author_id) REFERENCES users(user_id)  ON DELETE CASCADE
) ENGINE=InnoDB;

CREATE TABLE events (
    event_id    INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    club_id     INT UNSIGNED NOT NULL,
    title       VARCHAR(150) NOT NULL,
    description TEXT NULL,
    venue       VARCHAR(150) NULL,
    start_time  DATETIME NOT NULL,
    end_time    DATETIME NOT NULL,
    created_by  INT UNSIGNED NULL,
    created_at  TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (club_id)    REFERENCES clubs(club_id) ON DELETE CASCADE,
    FOREIGN KEY (created_by) REFERENCES users(user_id) ON DELETE SET NULL,
    CONSTRAINT chk_event_time CHECK (end_time > start_time)
) ENGINE=InnoDB;

CREATE TABLE event_attendance (
    event_id    INT UNSIGNED NOT NULL,
    user_id     INT UNSIGNED NOT NULL,
    status      ENUM('present','late','absent','excused') NOT NULL DEFAULT 'present',
    recorded_by INT UNSIGNED NULL,                  
    recorded_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (event_id, user_id),
    FOREIGN KEY (event_id)    REFERENCES events(event_id) ON DELETE CASCADE,
    FOREIGN KEY (user_id)     REFERENCES users(user_id)   ON DELETE CASCADE,
    FOREIGN KEY (recorded_by) REFERENCES users(user_id)   ON DELETE SET NULL
) ENGINE=InnoDB;

CREATE TABLE club_questions (
    question_id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    club_id     INT UNSIGNED NOT NULL,
    asker_id    INT UNSIGNED NOT NULL,
    question    VARCHAR(500) NOT NULL,
    created_at  TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (club_id)  REFERENCES clubs(club_id) ON DELETE CASCADE,
    FOREIGN KEY (asker_id) REFERENCES users(user_id) ON DELETE CASCADE
) ENGINE=InnoDB;

CREATE TABLE club_answers (
    answer_id   INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    question_id INT UNSIGNED NOT NULL,
    answerer_id INT UNSIGNED NOT NULL,              
    answer      VARCHAR(1000) NOT NULL,
    created_at  TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (question_id) REFERENCES club_questions(question_id) ON DELETE CASCADE,
    FOREIGN KEY (answerer_id) REFERENCES users(user_id) ON DELETE CASCADE
) ENGINE=InnoDB;

CREATE TABLE club_follows (
    user_id     INT UNSIGNED NOT NULL,
    club_id     INT UNSIGNED NOT NULL,
    email_alerts BOOLEAN NOT NULL DEFAULT TRUE,
    followed_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (user_id, club_id),
    FOREIGN KEY (user_id) REFERENCES users(user_id) ON DELETE CASCADE,
    FOREIGN KEY (club_id) REFERENCES clubs(club_id) ON DELETE CASCADE
) ENGINE=InnoDB;

CREATE TABLE notifications (
    notification_id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    user_id         INT UNSIGNED NOT NULL,
    announcement_id INT UNSIGNED NULL,
    event_id        INT UNSIGNED NULL,
    message         VARCHAR(255) NOT NULL,
    is_read         BOOLEAN NOT NULL DEFAULT FALSE,
    email_sent_at   DATETIME NULL,                  
    created_at      TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id)         REFERENCES users(user_id) ON DELETE CASCADE,
    FOREIGN KEY (announcement_id) REFERENCES announcements(announcement_id) ON DELETE CASCADE,
    FOREIGN KEY (event_id)        REFERENCES events(event_id) ON DELETE CASCADE
) ENGINE=InnoDB;


CREATE VIEW v_club_directory AS
SELECT c.club_id, c.name, cc.name AS category, c.status, c.meeting_schedule,
       rp.start_date AS recruit_start, rp.end_date AS recruit_end,
       CASE WHEN CURRENT_DATE BETWEEN rp.start_date AND rp.end_date
            THEN 'Open' ELSE 'Closed' END AS recruitment_status
FROM clubs c
LEFT JOIN club_categories cc ON cc.category_id = c.category_id
LEFT JOIN recruitment_periods rp
       ON rp.period_id = (SELECT MAX(p.period_id) FROM recruitment_periods p
                          WHERE p.club_id = c.club_id);



INSERT INTO users (email, password_hash, first_name, last_name, user_type, system_role, grade_level) VALUES
('admin@dlsl.edu.ph',   'HASH_PLACEHOLDER', 'System', 'Admin',  'teacher', 'super_admin', NULL),
('adviser1@dlsl.edu.ph','HASH_PLACEHOLDER', 'Maria',  'Santos', 'teacher', 'user', NULL),
('officer1@dlsl.edu.ph','HASH_PLACEHOLDER', 'Juan',   'Cruz',   'student', 'user', '3rd Year'),
('student1@dlsl.edu.ph','HASH_PLACEHOLDER', 'Ana',    'Reyes',  'student', 'user', '1st Year'),


INSERT INTO club_categories (name) VALUES ('Sleeping'), ('Academic'), ('Sports'), ('Gaming');

INSERT INTO clubs (category_id, name, description, meeting_schedule, meeting_place, contact_email) VALUES
(4, 'Ignis Club', 'Gaming.', 'Fridays 3-5 PM', 'Computer Lab 2', 'compsoc@dlsl.edu.ph'),
(1, 'Batman Claub',    'Being good at not sleeping.',          'Wednesdays 4-6 PM', 'Auditorium', 'theater@dlsl.edu.ph');

INSERT INTO club_staff (club_id, user_id, staff_role, position) VALUES
(1, 2, 'adviser', NULL),
(1, 3, 'officer', 'President');

INSERT INTO club_requirements (club_id, description, is_mandatory, sort_order) VALUES
(1, 'Fill out the interest form', TRUE, 1),
(1, 'Attend the orientation', TRUE, 2);

INSERT INTO recruitment_periods (club_id, school_year, start_date, end_date, notes, created_by) VALUES
(1, '2026-2027', '2026-10-01', '2026-10-15', 'Orientation on Oct 5 at Computer Lab 2', 3);

INSERT INTO announcements (club_id, author_id, title, body, category, is_pinned) VALUES
(1, 3, 'Recruitment is open!', 'Batman is accepting new members until Oct 15.', 'recruitment', TRUE);

INSERT INTO events (club_id, title, venue, start_time, end_time, created_by) VALUES
(1, 'Orientation Day', 'Computer Lab 2', '2026-10-05 15:00:00', '2026-10-05 17:00:00', 3);

INSERT INTO club_questions (club_id, asker_id, question) VALUES
(1, 4, 'Do I need programming experience to join?');

