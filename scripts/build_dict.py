import sqlite3
import os
import re

def remove_tones(pinyin):
    """
    Remove tones from pinyin string.
    e.g. 'hǎo' -> 'hao'
    """
    tone_map = {
        'ā': 'a', 'á': 'a', 'ǎ': 'a', 'à': 'a',
        'ē': 'e', 'é': 'e', 'ě': 'e', 'è': 'e',
        'ī': 'i', 'í': 'i', 'ǐ': 'i', 'ì': 'i',
        'ō': 'o', 'ó': 'o', 'ǒ': 'o', 'ò': 'o',
        'ū': 'u', 'ú': 'u', 'ǔ': 'u', 'ù': 'u',
        'ǖ': 'v', 'ǘ': 'v', 'ǚ': 'v', 'ǜ': 'v', 'ü': 'v',
        'n': 'n', 'g': 'g'
    }
    
    result = ""
    i = 0
    while i < len(pinyin):
        char = pinyin[i]
        # Check simplified tone marks
        if char in tone_map:
            result += tone_map[char]
        elif ord(char) < 128:
            result += char
        else:
            # Handle multibyte chars manually if not in map
            result += char # Should not happen for standard pinyin
        i += 1
    return result

def build_db(pinyin_file, db_file):
    if os.path.exists(db_file):
        os.remove(db_file)
        
    conn = sqlite3.connect(db_file)
    c = conn.cursor()
    
    # Create table
    c.execute('''CREATE TABLE pinyin_map
                 (id INTEGER PRIMARY KEY AUTOINCREMENT,
                  pinyin TEXT NOT NULL,
                  hanzi TEXT NOT NULL)''')
                  
    # Create index for faster query
    c.execute('CREATE INDEX idx_pinyin ON pinyin_map (pinyin)')
    
    print(f"Reading {pinyin_file}...")
    
    count = 0
    with open(pinyin_file, 'r', encoding='utf-8') as f:
        c.execute('BEGIN TRANSACTION')
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
                
            # Format: U+3007: líng,yuán,xīng  # 〇
            parts = line.split('#')
            if len(parts) < 2:
                continue
                
            hanzi = parts[1].strip()
            data_part = parts[0].split(':')
            if len(data_part) < 2:
                continue
                
            pinyin_str = data_part[1].strip()
            pinyins = pinyin_str.split(',')
            
            for p in pinyins:
                clean_pinyin = remove_tones(p.strip())
                # Normalize ü to v
                clean_pinyin = clean_pinyin.replace('ü', 'v')
                # Filter non-alpha
                clean_pinyin = ''.join([ch for ch in clean_pinyin if 'a' <= ch <= 'z'])
                
                if clean_pinyin:
                    c.execute("INSERT INTO pinyin_map (pinyin, hanzi) VALUES (?, ?)", 
                              (clean_pinyin, hanzi))
                    count += 1
                    
        c.execute('COMMIT')
        
    print(f"Inserted {count} records.")
    conn.close()
    print(f"Database generated at {db_file}")

if __name__ == '__main__':
    project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    pinyin_txt = os.path.join(project_root, 'src', 'pinyin.txt')
    db_path = os.path.join(project_root, 'src', 'utime.db')
    
    build_db(pinyin_txt, db_path)
