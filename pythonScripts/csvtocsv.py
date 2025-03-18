import pandas as pd

input_csv = 'cleaned_books.csv'  
output_csv = 'cleaned_books1.csv' 

# Read the CSV file
df = pd.read_csv(input_csv, encoding='utf-8')

df['author'].fillna('Unknown', inplace=True)


df.to_csv(output_csv, index=False, encoding='utf-8')

print(f"Cleaned data saved to {output_csv}")