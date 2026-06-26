import os

BASE = os.getcwd()

def w(path, content):
    full = os.path.join(BASE, path)
    os.makedirs(os.path.dirname(full), exist_ok=True)
    with open(full, 'w', encoding='utf-8') as f:
        f.write(content)
    lc = content.count('\n')
    print(f"  {path}: {lc} lines")
    return lc

total = 0
