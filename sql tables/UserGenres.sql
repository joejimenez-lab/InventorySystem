CREATE TABLE userGenres (
    user_id INT PRIMARY KEY REFERENCES users(id) ON DELETE CASCADE,
    genres VARCHAR(256),
    updated_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP
)