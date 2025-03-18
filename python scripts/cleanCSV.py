import sys
from langdetect import detect, LangDetectException
import pandas as pd

# Check if file path is passed as an argument
if len(sys.argv) < 3:
    print("Usage: python script.py <input_file_path> <output_file_path>")
    sys.exit(1)

input_file_path = sys.argv[1]  # Get the input file path from command line argument
output_file_path = sys.argv[2]  # Get the output file path from command line argument

# Read the CSV file into a DataFrame
df = pd.read_csv(input_file_path)

# Function to detect language and filter out non-English books
def filter_english_books(df):
    def safe_detect(text):
        try:
            if not text or text.isspace():
                return 'unknown'  # Treat empty or whitespace-only text as 'unknown'
            return detect(text)
        except LangDetectException:
            return 'unknown'  # Handle detection errors gracefully
    
    # Apply the safe language detection function
    df['language'] = df['bookTitle'].apply(safe_detect)
    return df[df['language'] == 'en']  # Keep only English books

# Apply the filter
df_english = filter_english_books(df)

# Save the filtered DataFrame to a new CSV file
df_english.to_csv(output_file_path, index=False)

print(f"Filtered data saved to {output_file_path}")