import argparse
import pyodbc
import csv


def connect_to_db(dsn, user, password, database):
    return pyodbc.connect(f'DSN={dsn};UID={user};PWD={password};DATABASE={database}')


def load_csv_to_db(csv_file, conn):
    cursor = conn.cursor()
    with open(csv_file, 'r', encoding='utf-8') as file:
        reader = csv.DictReader(file) 
        
        for row in reader:
            book_title = row['bookTitle'] 
            author_name = row['authorName']
            genres = row['genres']
            
            publication_year = None  
            isbn = None  
            copies_available = 1
            
            cursor.execute("""
                INSERT INTO books (title, author, genre, publication_year, isbn, copies_available)
                VALUES (?, ?, ?, ?, ?, ?)
            """, book_title, author_name, genres, publication_year, isbn, copies_available)

    conn.commit()
    cursor.close()

def main():

    parser = argparse.ArgumentParser(description='Load CSV data into PostgreSQL via ODBC')
    

    parser.add_argument('--dsn', required=True, help='ODBC Data Source Name')
    parser.add_argument('--user', required=True, help='PostgreSQL username')
    parser.add_argument('--password', required=True, help='PostgreSQL password')
    parser.add_argument('--database', required=True, help='PostgreSQL database name')
    parser.add_argument('--csv', required=True, help='Path to the CSV file')
    
    args = parser.parse_args()
    
    conn = connect_to_db(args.dsn, args.user, args.password, args.database)
    
    load_csv_to_db(args.csv, conn)
    
    conn.close()
    print("CSV data loaded successfully!")

if __name__ == '__main__':
    main()