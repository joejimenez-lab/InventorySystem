import sys
from langdetect import detect, LangDetectException
import pandas as pd

# Check if file path is passed as an argument
if len(sys.argv) < 3:
    print("Usage: python script.py <input_file_path> <output_file_path>")
    sys.exit(1)

input_file_path = sys.argv[1] 
output_file_path = sys.argv[2]  

# Read the CSV file into a DataFrame
df = pd.read_csv(input_file_path)


def filter_english_books(df):
    def safe_detect(text):
        try:
            if not text or text.isspace():
                return 'unknown'  
            return detect(text)
        except LangDetectException:
            return 'unknown'  
    
    df['language'] = df['bookTitle'].apply(safe_detect)
    return df[df['language'] == 'en']  

df_english = filter_english_books(df)


df_english.to_csv(output_file_path, index=False)

print(f"Filtered data saved to {output_file_path}")