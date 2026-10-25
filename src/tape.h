/* Load the game from the player's own tape image.
 *
 * Accepts the C64 raw tape (H.E.R.O.tap: standard CBM ROM-loader encoding) or
 * an already decrypted 64 KB memory image. The tape is decoded and the
 * loader's three decryption stages are replayed (see docs/c64_memory_map.md),
 * giving the 64 KB RAM image the game runs from (game at $8000-$BFFF).
 */
#ifndef TAPE_H
#define TAPE_H
#include <stddef.h>
#include <stdint.h>

/* 0 on success; on failure a message in err. */
int tape_load_file(const char *path, uint8_t image[0x10000], char *err, size_t errlen);
int tape_load_mem(const uint8_t *data, size_t len, uint8_t image[0x10000], char *err, size_t errlen);

/* Decode a raw C64 TAP into the payloads of its (first-copy) blocks.
 * Calls fn for each block with a valid checksum. Returns the number of blocks
 * seen, or -1 if data is not a C64 TAP. */
typedef void (*tape_block_fn)(void *user, const uint8_t *payload, size_t len, int repeat);
int tape_decode(const uint8_t *data, size_t len, tape_block_fn fn, void *user);

#endif
