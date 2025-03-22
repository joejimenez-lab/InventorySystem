import cloudscraper
import sys

def fetch_json(url):
    scraper = cloudscraper.create_scraper()
    response = scraper.get(url)
    if response.status_code == 200:
        return response.text
    else:
        return None

if __name__ == "__main__":
    url = sys.argv[1] 
    json_data = fetch_json(url)
    if json_data:
        print(json_data)
    else:
        print("Error: Failed to fetch JSON data", file=sys.stderr)