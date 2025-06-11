import os
import json
import zlib
from PIL import Image
import io

def get_artist_and_title(filename):
    base = os.path.splitext(filename)[0]
    if '-' in base:
        artist, name = base.split('-', 1)
        artist = artist.strip() or "Unknown"
        name = name.strip() or "Untitled"
    else:
        artist = "Unknown"
        name = base.strip() or "Untitled"
        
    # Fallback if both are empty
    if not artist:
        artist = "Unknown"
    if not name:
        name = "Untitled"
    return artist, name

def resize_image_keep_aspect(img, max_w, max_h):
    w, h = img.size
    aspect = w / h
    if w > max_w or h > max_h:
        if aspect > (max_w / max_h):
            new_w = max_w
            new_h = int(max_w / aspect)
        else:
            new_h = max_h
            new_w = int(max_h * aspect)
        return img.resize((new_w, new_h), Image.LANCZOS)
    return img

puzzles = []
data_chunks = []
offset = 0

for fname in os.listdir('.'):
    if fname.lower().endswith(('.jpg', '.jpeg', '.png')) and os.path.isfile(fname):
        artist, name = get_artist_and_title(fname)
        with Image.open(fname) as img:
            img = img.convert('RGB')
            img = resize_image_keep_aspect(img, 1280, 720)
            buf = io.BytesIO()
            img.save(buf, format='JPEG', quality=95)
            img_bytes = buf.getvalue()
        compressed = zlib.compress(img_bytes)
        puzzles.append({
            'name': name.replace('_', ' '),
            'artist': artist.replace('_', ' '),
            'offset': offset,
            'length': len(compressed)
        })
        data_chunks.append(compressed)
        offset += len(compressed)

with open('puzzles.dat', 'wb') as f:
    for chunk in data_chunks:
        f.write(chunk)

with open('puzzles_meta.json', 'w', encoding='utf-8') as f:
    json.dump({'puzzles': puzzles}, f, indent=2)

print(f"Processed {len(puzzles)} images. Created puzzles.dat and puzzles_meta.json.")
