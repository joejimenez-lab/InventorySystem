import csv
import pyodbc
import sys
import itertools
# Get the CSV file path from command line argument
csv_file = sys.argv[1]
print(f"Received file path: {sys.argv[1]}")
# ODBC connection parameters
dsn = 
user = 
password = 
database = 

try:
    conn = pyodbc.connect(
        f'DSN={dsn};UID={user};PWD={password};DATABASE={database}'
    )
    cursor = conn.cursor()
    print("Connected to the database via ODBC.")

    with open(csv_file, newline='', encoding='utf-8') as file:
        reader = csv.DictReader(itertools.islice(file, 4, None))
        for row in reader:
            print(row)
            if row['book_id'] == '------WebKitFormBoundaryVjQZ4BxZxQiPICXS--' or None in row.values():
                continue  
            cursor.execute("""
                INSERT INTO books (title, author, genre, publication_year, isbn)
                VALUES (?, ?, ?, ?, ?)
            """, (
                row['title'],
                row['author'],
                row['genre'],
                int(row['publication_year']),
                row['isbn']
            ))

    conn.commit()
    print("CSV data inserted successfully.")

except Exception as e:
    print("Error:", e)

finally:
    if conn:
        cursor.close()
        conn.close()
        print("Database connection closed.")