import re, os

def clean_file(filepath):
    if not os.path.exists(filepath):
        return
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
    original = content
    # Remove typedef struct { ... } Name; blocks and typedef enum { ... } Name; blocks
    # Also remove #define macros that are now in headers
    patterns_to_remove = [
        # typedef struct with multiline body
        r'typedef struct \{[^}]*?\} \w+;\s*\n',
        # typedef enum  
        r'typedef enum \{[^}]*?\} \w+;\s*\n',
    ]
    for pat in patterns_to_remove:
        content = re.sub(pat, '', content, flags=re.DOTALL)
    # Also remove specific defines
    define_patterns = [
        r'#define KV_CACHE_MAX_SEQ\s+\d+\s*\n',
        r'#define KV_CACHE_MAX_LAYERS\s+\d+\s*\n',
        r'#define NGRAM_MAX_VOCAB\s+\d+\s*\n',
        r'#define NGRAM_MAX_ORDER\s+\d+\s*\n',
        r'#define BEAM_WIDTH\s+\d+\s*\n',
        r'#define BEAM_MAX_LEN\s+\d+\s*\n',
        r'#define NER_MAX_ENTITIES\s+\d+\s*\n',
        r'#define NER_MAX_TEXT_LEN\s+\d+\s*\n',
        r'#define TEXTRANK_MAX_SENT\s+\d+\s*\n',
        r'#define TEXTRANK_MAX_ITER\s+\d+\s*\n',
        r'#define TOT_MAX_BRANCHES\s+\d+\s*\n',
        r'#define TOT_MAX_DEPTH\s+\d+\s*\n',
        r'#define TOT_BEAM_WIDTH\s+\d+\s*\n',
    ]
    for pat in define_patterns:
        content = re.sub(pat, '', content)
    if content != original:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"  Cleaned {filepath}: {len(original)} -> {len(content)} bytes")
    else:
        print(f"  {filepath}: no changes")

for f in ['src/inference_server.c', 'src/nlp_pipeline.c', 'src/recommender.c', 'src/ai_platform.c']:
    clean_file(f)
print("Done cleaning!")
