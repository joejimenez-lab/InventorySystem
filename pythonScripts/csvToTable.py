import argparse
import pyodbc
import csv

# Function to connect to PostgreSQL using ODBC
def connect_to_db(dsn, user, password, database):
    return pyodbc.connect(f'DSN={dsn};UID={user};PWD={password};DATABASE={database}')

# Function to load CSV into the database (filtering out unnecessary columns)
def load_csv_to_db(csv_file, conn):
    cursor = conn.cursor()
    with open(csv_file, 'r', encoding='utf-8') as file:
        reader = csv.DictReader(file)  # Use DictReader to access columns by name
        
        for row in reader:
            # Extract only the required columns from each row
            book_title = row['bookTitle']  # Adjust the column names based on your CSV
            author_name = row['authorName']
            genres = row['genres']
            
            # You can use default values for missing columns
            publication_year = None  # Assuming not present in CSV, set to None
            isbn = None  # Assuming not present in CSV, set to None
            copies_available = 1  # Default value for copies_available
            
            # Insert into the table
            cursor.execute("""
                INSERT INTO books (title, author, genre, publication_year, isbn, copies_available)
                VALUES (?, ?, ?, ?, ?, ?)
            """, book_title, author_name, genres, publication_year, isbn, copies_available)

    conn.commit()
    cursor.close()

# Main function to parse arguments and load CSV
def main():
    # Set up argument parser
    parser = argparse.ArgumentParser(description='Load CSV data into PostgreSQL via ODBC')
    
    # Add arguments
    parser.add_argument('--dsn', required=True, help='ODBC Data Source Name')
    parser.add_argument('--user', required=True, help='PostgreSQL username')
    parser.add_argument('--password', required=True, help='PostgreSQL password')
    parser.add_argument('--database', required=True, help='PostgreSQL database name')
    parser.add_argument('--csv', required=True, help='Path to the CSV file')
    
    # Parse arguments
    args = parser.parse_args()
    
    # Connect to the database
    conn = connect_to_db(args.dsn, args.user, args.password, args.database)
    
    # Load the CSV file into the database
    load_csv_to_db(args.csv, conn)
    
    # Close the connection
    conn.close()
    print("CSV data loaded successfully!")

if __name__ == '__main__':
    main()