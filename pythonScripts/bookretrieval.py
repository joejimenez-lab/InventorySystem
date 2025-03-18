import requests
import pandas as pd
import re
import time


API_URL = "https://openlibrary.org/search.json"

# Function to fetch books from Open Library
def fetch_books(query, max_books=2500, max_pages=50):
    books = []
    seen_isbns = set()
    page = 1

    while len(books) < max_books and page <= max_pages:
        print(f"Fetching page {page} for query: {query}...")
        response = requests.get(API_URL, params={"q": query, "page": page, "limit": 100})
        
        if response.status_code != 200:
            print(f"Error fetching data: {response.status_code}")
            break

        data = response.json()
        docs = data.get("docs", [])

        if not docs:
            break  

        for doc in docs:
            title = doc.get("title", "").strip()
            author = ", ".join(doc.get("author_name", ["Unknown"])).strip()
            genre = doc.get("subject", ["Unknown"])[0] if "subject" in doc else "Unknown"
            year = doc.get("first_publish_year", "Unknown")
            isbn_list = doc.get("isbn", [])


            for isbn in isbn_list:
                if isbn not in seen_isbns and is_valid_text(title) and is_valid_text(author):
                    books.append([title, author, genre, year, isbn])
                    seen_isbns.add(isbn)  
                    break  

        page += 1
        time.sleep(0.5)  

        if len(books) >= max_books:
            break

    return books

# Function to validate English-only text
def is_valid_text(text):
    return bool(re.match(r'^[A-Za-z0-9\s,.\'-]*$', text))

# Fetch books from different genres
queries = [
    "fiction", "mystery", "science fiction", "fantasy", "romance",
    "history", "biography", "technology", "self-help", "horror",
    "adventure", "philosophy", "psychology", "cooking", "health"
]

all_books = []
seen_isbns = set() 

for query in queries:
    books = fetch_books(query, max_books=2500, max_pages=100)
    for book in books:
        if book[4] not in seen_isbns:  
            all_books.append(book)
            seen_isbns.add(book[4])


df = pd.DataFrame(all_books, columns=["title", "author", "genre", "publication_year", "isbn"])
df = df.sample(n=10000, random_state=42) if len(df) > 10000 else df

df.to_csv("books_utf8_unique.csv", encoding="utf-8", index=False)
print(f"✅ Scraped and saved {len(df)} unique books to 'books_utf8_unique.csv'")
