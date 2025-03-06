# import csv
# import re

# input_file = "books_utf8.csv"       # Your original CSV file
# output_file = "books_utf8_clean.csv"  # Cleaned CSV file

# # Regex pattern: Keep only rows that contain ONLY English letters and numbers
# valid_pattern = re.compile(r'^[A-Za-z0-9\s,.\'-]*$')

# with open(input_file, "r", encoding="utf-8", errors="replace") as infile, open(output_file, "w", encoding="utf-8", newline="") as outfile:
#     reader = csv.reader(infile)
#     writer = csv.writer(outfile)

#     for row in reader:
#         if all(valid_pattern.match(cell) for cell in row):  # Check if ALL cells match the pattern
#             writer.writerow(row)  # Keep the row

# print(f"Cleaned CSV saved as: {output_file}")

import pandas as pd

# Load CSV
df = pd.read_csv("books_utf8_clean.csv", encoding="utf-8")

# Remove duplicate ISBNs, keeping the first occurrence
df = df.drop_duplicates(subset=["isbn"], keep="first")

# Save cleaned CSV
df.to_csv("books_utf8_unique.csv", encoding="utf-8", index=False)

print("Cleaned CSV saved as books_utf8_unique.csv (duplicates removed).")
