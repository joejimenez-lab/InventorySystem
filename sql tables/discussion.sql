CREATE TABLE discussion_threads (
    thread_id INT PRIMARY KEY,
    book_id INT,
    user_id INT,  -- The user who started the thread
    title VARCHAR(255),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (book_id) REFERENCES books(book_id)
);

CREATE TABLE comments (
    comment_id INT PRIMARY KEY,
    thread_id INT,
    user_id INT,  -- The user who made the comment
    comment TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (thread_id) REFERENCES discussion_threads(thread_id)
);