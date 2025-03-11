CREATE TABLE users_restrictions (
    user_id INT PRIMARY KEY REFERENCES users(id) ON DELETE CASCADE,
    borrow BOOLEAN NOT NULL ON DEFAULT 'false',
    reservation BOOLEAN NOT NULL ON DEFAULT 'false',
    resource BOOLEAN NOT NULL ON DEFAULT 'false',
    updated_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);