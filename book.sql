CREATE TABLE books (
    book_id SERIAL PRIMARY KEY,
    title VARCHAR(500) NOT NULL,
    author VARCHAR(500) NOT NULL,
    genre VARCHAR(100),
    publication_year INT,
    isbn VARCHAR(20) UNIQUE,
    copies_available INT DEFAULT 1
);

CREATE TABLE borrowed_books (
    borrow_id SERIAL PRIMARY KEY,
    book_id INT REFERENCES books(book_id) ON DELETE CASCADE,
    user_id INT REFERENCES users(id) ON DELETE CASCADE,
    borrow_date DATE DEFAULT CURRENT_DATE,
    return_date DATE
);

CREATE TABLE reserved_books (
    reserve_id SERIAL PRIMARY KEY,
    book_id INT REFERENCES books(book_id) ON DELETE CASCADE,
    user_id INT REFERENCES users(id) ON DELETE CASCADE,
    reserve_date DATE DEFAULT CURRENT_DATE,
    status VARCHAR(20) DEFAULT 'Pending' CHECK (status IN ('Pending', 'Cancelled', 'Completed'))
);