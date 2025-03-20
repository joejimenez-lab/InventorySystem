import barcode
from barcode.writer import ImageWriter
import os
import sys

# Ensure the correct number of arguments
if len(sys.argv) != 2:
    print("Usage: python generate_barcode.py <book_id>")
    sys.exit(1)

book_id = sys.argv[1]

BARCODE_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "barcodes")
os.makedirs(BARCODE_DIR, exist_ok=True)  
def generate_barcode(book_id):
    book_id_str = str(book_id).zfill(12)
    ean = barcode.get('ean13', book_id_str, writer=ImageWriter())
    file_path = os.path.join(BARCODE_DIR, f"barcode_{book_id}")

    ean.save(file_path)

    print(f"Barcode saved at: {file_path}")

generate_barcode(book_id)