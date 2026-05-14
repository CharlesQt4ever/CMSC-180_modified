import os
from pypdf import PdfReader

with open('extracted_texts.md', 'w', encoding='utf-8') as f:
    for file in os.listdir('.'):
        if file.endswith('.pdf'):
            f.write(f"--- START OF {file} ---\n")
            try:
                reader = PdfReader(file)
                for page in reader.pages:
                    text = page.extract_text()
                    if text:
                        f.write(text + "\n")
            except Exception as e:
                f.write(f"Error reading {file}: {e}\n")
            f.write(f"--- END OF {file} ---\n\n")
