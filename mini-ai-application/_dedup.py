import re, os, sys

# Patterns to remove from each .c file (types now in headers)
# We'll be conservative and remove only typedef/define blocks that match header declarations

files_to_clean = {
    "src/ai_platform.c": [
        # FeatureDef, OnlineFeature, FeatureStore typedefs
        (r'typedef struct \{\s*\n\s+char\s+name\[64\];[^}]*?\} FeatureDef;\s*\n', ''),
        (r'typedef struct \{\s*\n\s+char\s+feature_name\[64\];[^}]*?\} OnlineFeature;\s*\n', ''),
        (r'typedef struct \{\s*\n\s+FeatureDef\s+definitions\[256\];[^}]*?\} FeatureStore;\s*\n', ''),
        # FeatureImportance
        (r'typedef struct \{\s*\n\s+char\s+feature_name\[64\];[^}]*?\} FeatureImportance;\s*\n', ''),
        # FederatedConfig
        (r'typedef struct \{\s*\n\s+double\s+\*weights;[^}]*?\} FederatedConfig;\s*\n', ''),
        # FederatedState
        (r'typedef struct \{\s*\n\s+double\s+\*global_weights;[^}]*?\} FederatedState;\s*\n', ''),
        # NASOperation enum
        (r'typedef enum \{ NAS_OP_CONV3, NAS_OP_CONV5, NAS_OP_MAXPOOL, NAS_OP_AVGPOOL,\s*\n\s+N' + 'AS_OP_SKIP, NAS_OP_NONE \} NASOperation;\s*\n', ''),
        # ArchCandidate
        (r'typedef struct \{\s*\n\s+NASOperation ops\[8\];[^}]*?\} ArchCandidate;\s*\n', ''),
    ],
}

print("Starting dedup...")

for filepath, replacements in files_to_clean.items():
    if not os.path.exists(filepath):
        print(f"  {filepath}: not found, skipping")
        continue
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
    
    original_len = len(content)
    for pattern, replacement in replacements:
        content = re.sub(pattern, replacement, content)
    
    if len(content) != original_len:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"  {filepath}: cleaned ({original_len} -> {len(content)} bytes)")
    else:
        print(f"  {filepath}: no changes needed")

print("Dedup complete!")
