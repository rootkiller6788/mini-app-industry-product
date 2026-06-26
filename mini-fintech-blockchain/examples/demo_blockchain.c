#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "fintech_core.h"

/* Demo: Blockchain with proof-of-work
 * Showcases: Genesis block, block addition, Merkle tree, chain validation */

int main(void) {
    printf("=== Blockchain Demo ===

");

    /* Create Merkle tree for transactions */
    MerkleTree* mt = merkle_create();
    merkle_add_leaf(mt, "Alice pays Bob 50 BTC");
    merkle_add_leaf(mt, "Charlie pays Dave 30 BTC");
    merkle_add_leaf(mt, "Eve pays Frank 15 BTC");
    merkle_add_leaf(mt, "Grace pays Henry 25 BTC");

    merkle_compute_root(mt);
    printf("Merkle Tree: %d transactions
", mt->leaf_count);
    printf("  Merkle Root: %.32s...
", mt->merkle_root);

    /* Verify individual transaction */
    bool valid_txn = merkle_verify_leaf(mt, 0, "Alice pays Bob 50 BTC");
    printf("  Txn 0 verified: %s

", valid_txn ? "YES" : "NO");

    /* Create blockchain */
    Blockchain* bc = blockchain_create(1);
    printf("Genesis Block created (difficulty=%d):
", bc->difficulty);
    printf("  Hash: %.32s...
", bc->blocks[0].hash);
    printf("  Data: %s

", bc->blocks[0].data);

    /* Add blocks */
    blockchain_add_block(bc, "Block 1: Batch payment processed");
    blockchain_add_block(bc, "Block 2: Smart contract executed");
    blockchain_add_block(bc, "Block 3: Token transfer completed");

    printf("Chain after %d blocks:
", bc->block_count);
    for (int i = 0; i < bc->block_count; i++) {
        printf("  Block %d: %.16s... nonce=%llu
",
               bc->blocks[i].index, bc->blocks[i].hash,
               (unsigned long long)bc->blocks[i].nonce);
    }

    /* Chain integrity check */
    bool integrity = true;
    for (int i = 1; i < bc->block_count; i++) {
        if (strncmp(bc->blocks[i].prev_hash, bc->blocks[i-1].hash, 64) != 0) {
            integrity = false;
            break;
        }
    }
    printf("
Chain integrity: %s
", integrity ? "INTACT" : "BROKEN");

    merkle_destroy(mt);
    blockchain_destroy(bc);
    printf("
Demo complete.
");
    return 0;
}
