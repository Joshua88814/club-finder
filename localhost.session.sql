USE dlsl_club_finder;

INSERT INTO club_categories (name) VALUES ('Arts');

UPDATE users 
SET failed_attempts = 0, locked_until = NULL 
WHERE email = 'Malisa@dlsl.edu.ph';