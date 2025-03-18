import requests
import pandas as pd
import time

# Load your dataset
file_path = "goodreads_cleaned.csv"  # Adjust the path if needed
df = pd.read_csv(file_path)

# Function to fetch book details from Open Library API
def get_book_details(title, author):
    base_url = "https://openlibrary.org/search.json"
    params = {"title": title, "author": author, "limit": 1}

    response = requests.get(base_url, params=params)
    if response.status_code == 200:
        data = response.json()
        if data["docs"]:
            book_data = data["docs"][0]
            publication_date = book_data.get("first_publish_year", "Unknown")
            genres = book_data.get("subject", [])
            genres = ", ".join(genres[:3]) if genres else "Unknown"  # Limit to top 3 genres
            return publication_date, genres
    return "Unknown", "Unknown"

# Fetch details for the books
df["publication_date"] = ""
df["genres"] = ""

for idx, row in df.iterrows():
    title, author = row["bookTitle"], row["authorName"]
    pub_date, genre = get_book_details(title, author)
    df.at[idx, "publication_date"] = pub_date
    df.at[idx, "genres"] = genre
    time.sleep(1)  # To avoid rate limiting

# Save the updated dataset
df.to_csv("goodreads_updated.csv", index=False)
print("Updated dataset saved as goodreads_updated.csv")