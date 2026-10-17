/* fifa14_obb.h -- the game's expansion file (main.136.com.ea.game.fifa14_row.obb,
 * 1.39 GB), located, opened and readable (fifa14_obb.c).
 *
 * WHERE IT IS: the runtime's path map (dcr_path.c) sends the Android path the
 * game itself asks for,
 *     /storage/emulated/0/Android/obb/com.ea.game.fifa14_row/<name>
 * to <game folder>/obb/<name> -- and /sdcard/<name> to <game folder>/sdcard/.
 * This module looks in the folders a user may reasonably have used, so the
 * game's own open() succeeds whichever one it is in.
 *
 * WHO READS IT: the engine. It opens the OBB through its EA::IO / AssetManager
 * layer (the Java->native EAIO.StartupNativeImpl path, fifa14_blast_stubs.c),
 * so the port does not have to serve the bytes: bionic_io.c + dcr_path.c give
 * the engine a real file. What this module adds is (a) the boot log line that
 * says whether the data is there, which folder, how big, and whether the first
 * block reads -- the first thing to look at when the game boots to a black
 * screen -- and (b) fifa14_obb_read() with a shared handle and a 64 KiB block
 * cache, for the next round (a hook over the engine's reads, the way
 * tasm2_nx's tasm2_obb.c serves its own data).
 *
 * MIT.
 */
#ifndef FIFA14_OBB_H
#define FIFA14_OBB_H

#include <stddef.h>
#include <stdint.h>

/* Look for the OBB (the paths in the comment above), open it, and read one
 * 64 KiB block through the cache to prove it works. Every path tried and the
 * result are logged. Returns 0 when the OBB is open and readable, -1 when it
 * is missing (logged with what to do about it). */
int fifa14_obb_init(void);

/* 1 once fifa14_obb_init() succeeded. */
int fifa14_obb_ready(void);
/* The path it was found at ("" before / when missing). */
const char *fifa14_obb_path(void);
/* Its size in bytes (the real one on the card, 0 when not found). */
uint64_t fifa14_obb_size(void);

/* `n` bytes at `off` through the cache: 0, or -1 (past the end / not open /
 * an I/O error, logged). */
int fifa14_obb_read(uint64_t off, void *dst, size_t n);

/* One log line: blocks read, cache hits and misses, the handle's position. */
void fifa14_obb_stats(void);

#endif /* FIFA14_OBB_H */
