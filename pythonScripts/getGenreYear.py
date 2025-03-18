import requests
import pandas as pd

def get_book_metadata(title, author):
    query = f"{title} {author}"
    url = f"https://www.googleapis.com/books/v1/volumes?q={query}&key=AIzaSyAQytIQrXZqLMv69IH-hE8GHipZ5iJx_eM"
    response = requests.get(url)
    data = response.json()

    if "items" in data:
        book = data["items"][0]["volumeInfo"]
        pub_year = book.get("publishedDate", "Unknown")[:4]  # Extracts the year
        genre = book.get("categories", ["Unknown"])[0]
        return pub_year, genre
    return "Unknown", "Unknown"

# Load your CSV
df = pd.read_csv("C:\\Users\\natea\\InventorySystem\\book_csvs\\goodreads_cleaned.csv")


# Apply function to each row
df[['Publication Year', 'Genre']] = df.apply(lambda x: pd.Series(get_book_metadata(x['bookTitle'], x['authorName'])), axis=1)

# Save updated CSV
df.to_csv("C:\\Users\\natea\\InventorySystem\\book_csvs\\books_with_metadata.csv", index=False)
