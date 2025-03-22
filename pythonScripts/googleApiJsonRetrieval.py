import requests
import json

def get_book_json(title, api_key=None):
    base_url = "https://www.googleapis.com/books/v1/volumes"
    params = {"q": title}

    if api_key:
        params["key"] = api_key

    response = requests.get(base_url, params=params)

    if response.status_code == 200:
        return response.json() 
    else:
        return {"error": f"Failed to fetch data (Status Code: {response.status_code})"}

# Example usage
book_title = "The Great Gatsby"
api_key = "AIzaSyAQytIQrXZqLMv69IH-hE8GHipZ5iJx_eM"
book_json = get_book_json(book_title, api_key)

# Print the full JSON response formatted nicely
print(json.dumps(book_json, indent=2))