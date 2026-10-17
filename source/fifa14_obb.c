/* fifa14_obb.c -- FIFA 14's expansion file: where it is, how big, whether it
 * reads (fifa14_obb.h has the why; the paths and the engine's own reads).
 *
 * The read path is tasm2_nx's (source/tasm2_obb.c): ONE shared handle behind a
 * mutex, and a small LRU of 64 KiB blocks, so a run of small reads around each
 * other costs one real read per block instead of one per call. The engine does
 * not go through it yet -- it opens the OBB itself through EA::IO, which
 * bionic_io.c + dcr_path.c serve from the card -- so this is (a) the boot
 * proof that the data is there and (b) the tool the next round needs to hook
 * or serve the engine's own reads.
 *
 * MIT.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>

#include "config.h"
#include "dcr_path.h"
#include "dcr_setup.h"
#include "fifa14_ctlcfg.h"
#include "fifa14_external.h"
#include "fifa14_obb.h"
#include "util.h"

#define OBB_BLOCK_BYTES (64u * 1024u)
#define OBB_BLOCKS      8u /* 512 KiB of cache */

typedef struct {
  uint64_t base;
  uint64_t used;    /* LRU stamp */
  uint32_t bytes;   /* real bytes in it (the file's last block is short) */
  int valid;
} Block;

static FILE *g_f;
static Mutex g_lock;
static int g_ready;
static char g_path[512];
static uint64_t g_size;
static uint8_t *g_arena; /* OBB_BLOCKS * OBB_BLOCK_BYTES */
static Block g_blocks[OBB_BLOCKS];
static uint64_t g_clock;
static uint32_t g_hits, g_misses, g_freads, g_reads;

int fifa14_obb_ready(void) { return g_ready; }
const char *fifa14_obb_path(void) { return g_path; }
uint64_t fifa14_obb_size(void) { return g_size; }

/* The block holding `base`, filling it when it is not cached. The caller holds
 * g_lock (the buffer it returns is evictable by another thread otherwise). */
static const uint8_t *block_locked(uint64_t base) {
  int slot = -1, empty = -1;
  for (unsigned i = 0; i < OBB_BLOCKS; i++) {
    if (g_blocks[i].valid && g_blocks[i].base == base) {
      slot = (int)i;
      break;
    }
    if (!g_blocks[i].valid && empty < 0)
      empty = (int)i;
  }
  if (slot >= 0) {
    g_blocks[slot].used = ++g_clock;
    g_hits++;
    return g_arena + (size_t)slot * OBB_BLOCK_BYTES;
  }
  if (empty >= 0) {
    slot = empty;
  } else { /* evict the least recently used */
    slot = 0;
    for (unsigned i = 1; i < OBB_BLOCKS; i++)
      if (g_blocks[i].used < g_blocks[slot].used)
        slot = (int)i;
  }
  size_t want = OBB_BLOCK_BYTES;
  if (g_size - base < want)
    want = (size_t)(g_size - base);
  uint8_t *buf = g_arena + (size_t)slot * OBB_BLOCK_BYTES;
  /* newlib's off_t in this toolchain is 32 bits; the OBB (1.39 GB) fits. */
  if (base > 0x7fffffffull || fseeko(g_f, (off_t)base, SEEK_SET) != 0) {
    debugPrintf("[obb] seek to %llu failed\n", (unsigned long long)base);
    return NULL;
  }
  const size_t got = fread(buf, 1, want, g_f);
  g_freads++;
  if (got < want)
    memset(buf + got, 0, OBB_BLOCK_BYTES - got);
  g_blocks[slot].base = base;
  g_blocks[slot].bytes = (uint32_t)got;
  g_blocks[slot].valid = 1;
  g_blocks[slot].used = ++g_clock;
  g_misses++;
  if (got != want)
    debugPrintf("[obb] short read at %llu: %u of %u B (end of the file?)\n",
                (unsigned long long)base, (unsigned)got, (unsigned)want);
  return buf;
}

int fifa14_obb_read(uint64_t off, void *dst, size_t n) {
  if (!g_ready || !dst)
    return -1;
  if (off >= g_size)
    return -1;
  if (n > (size_t)(g_size - off))
    n = (size_t)(g_size - off);
  mutexLock(&g_lock);
  uint8_t *out = dst;
  while (n) {
    const uint64_t base = (off / OBB_BLOCK_BYTES) * OBB_BLOCK_BYTES;
    const size_t inblk = (size_t)(off - base);
    size_t take = OBB_BLOCK_BYTES - inblk;
    if (take > n)
      take = n;
    const uint8_t *src = block_locked(base);
    if (!src) {
      mutexUnlock(&g_lock);
      return -1;
    }
    memcpy(out, src + inblk, take);
    out += take;
    off += take;
    n -= take;
  }
  g_reads++;
  mutexUnlock(&g_lock);
  return 0;
}

void fifa14_obb_stats(void) {
  debugPrintf("[obb] cache: %u reads from the card, %u hits, %u misses, %u API calls "
              "(%u blocks of %u KiB)\n",
              (unsigned)g_freads, (unsigned)g_hits, (unsigned)g_misses, (unsigned)g_reads,
              (unsigned)OBB_BLOCKS, (unsigned)(OBB_BLOCK_BYTES / 1024));
}

/* The folders to try, in order. 1 = found (path in g_path). */
static int try_paths(void) {
  static const char *const k_dirs[] = {
      "obb",                                /* Android/obb/<pkg> -> <root>/obb */
      "sdcard",                             /* /sdcard/<name>    -> <root>/sdcard */
      "",                                   /* next to the APK (port_apk_help's text) */
      "Android/obb/" PORT_PACKAGE,          /* as the user's card has it */
  };
  char path[448];
  for (unsigned i = 0; i < sizeof k_dirs / sizeof k_dirs[0]; i++) {
    if (k_dirs[i][0])
      rt_root_path(path, sizeof path, k_dirs[i]);
    else
      snprintf(path, sizeof path, "%s", dcr_game_root());
    char full[512];
    snprintf(full, sizeof full, "%s/%s", path, FIFA14_OBB_NAME);
    const long size = rt_file_size(full);
    debugPrintf("[obb] %s: %s\n", size >= 0 ? "found" : "no file", full);
    /* [fs] the exact path each candidate was looked for at, and its size. */
    debugPrintf("[fs] obb     try=\"%s\" bytes=%ld\n", full, size);
    if (size >= 0) {
      snprintf(g_path, sizeof g_path, "%s", full);
      return 1;
    }
  }
  /* One more: whatever the game's own ask translates to. */
  {
    char xlat[DCR_PATH_MAX];
    const char *real = dcr_translate_path("/storage/emulated/0/Android/obb/" PORT_PACKAGE
                                          "/" FIFA14_OBB_NAME,
                                          xlat, sizeof xlat);
    debugPrintf("[obb] the game's own path translates to: %s\n", real);
    debugPrintf("[fs] obb     asked=\"%s\" real=\"%s\"\n",
                "/storage/emulated/0/Android/obb/" PORT_PACKAGE "/" FIFA14_OBB_NAME, real);
  }
  return 0;
}

int fifa14_obb_init(void) {
  mutexInit(&g_lock);
  g_arena = malloc(OBB_BLOCKS * OBB_BLOCK_BYTES);
  if (!g_arena) {
    debugPrintf("[obb] no memory for the %u KiB cache: the OBB is opened but not "
                "cached (reads go straight to the card)\n",
                (unsigned)(OBB_BLOCKS * OBB_BLOCK_BYTES / 1024));
    return -1;
  }
  debugPrintf("[obb] ==== the expansion file (%s) ====\n", FIFA14_OBB_NAME);
  if (!try_paths()) {
    debugPrintf("[obb] NOT FOUND. The game reads its data (1.39 GB) from it and\n"
                "[obb] will fail its own file opens; put the file in the game folder,\n"
                "[obb] in its obb/ folder, or in Android/obb/%s/.\n",
                PORT_PACKAGE);
    return -1;
  }
  g_f = fopen(g_path, "rb");
  debugPrintf("[fs] obb     open=\"%s\" rc=%s%s\n", g_path, g_f ? "ok" : "FAILED",
              g_f ? "" : " (the expansion file is unreadable where the port looks)");
  if (!g_f) {
    debugPrintf("[obb] %s: cannot open it\n", g_path);
    return -1;
  }
  g_size = (uint64_t)rt_file_size(g_path);
  debugPrintf("[obb] open: %s\n", g_path);
  debugPrintf("[obb] %llu B (expected %llu B)%s\n", (unsigned long long)g_size,
              (unsigned long long)FIFA14_OBB_BYTES,
              g_size == FIFA14_OBB_BYTES ? "" : "  <- different version/build?");
  g_ready = 1;

  /* Read one block through the cache: the data is really there. */
  static uint8_t probe[OBB_BLOCK_BYTES];
  if (fifa14_obb_read(0, probe, sizeof probe) != 0) {
    debugPrintf("[obb] the first %u bytes do NOT read -- the file is unreadable\n",
                (unsigned)sizeof probe);
    g_ready = 0;
    return -1;
  }
  uint32_t hash = 2166136261u;
  for (size_t i = 0; i < sizeof probe; i++)
    hash = (hash ^ probe[i]) * 16777619u;
  debugPrintf("[obb] first %u B read: %02x %02x %02x %02x ('%c%c%c%c') fnv32 %08x\n",
              (unsigned)sizeof probe, probe[0], probe[1], probe[2], probe[3],
              probe[0] >= 32 && probe[0] < 127 ? probe[0] : '.',
              probe[1] >= 32 && probe[1] < 127 ? probe[1] : '.',
              probe[2] >= 32 && probe[2] < 127 ? probe[2] : '.',
              probe[3] >= 32 && probe[3] < 127 ? probe[3] : '.', (unsigned)hash);
  if (probe[0] == 'P' && probe[1] == 'K')
    debugPrintf("[obb] it is a ZIP (EA's OBB: the patch/version info is its first entry)\n");
  else if (probe[0] == 'E' && probe[1] == 'B')
    debugPrintf("[obb] it is EA's \"EB\" container (v%d): a file table of %u x 16 B at 0x30, the "
                "names at 0x5110, and the data (uncompressed) from 0x1fd70 -- "
                "fifa14_external.c reads the table\n",
                (int)probe[3], 1217);
  else
    debugPrintf("[obb] neither a ZIP nor EA's \"EB\" container -- is this the right file?\n");
  fifa14_obb_stats();

  /* The container's own file table: what every open under the game's external
   * storage is served from (fifa14_external.c). It is read here, before the
   * engine's modules are loaded, so the very first open the engine makes is
   * already answerable. A failure is not fatal: the game's external data then
   * fails exactly as it did before. */
  fifa14_external_init();
  /* ...and the one file whose CONTENTS the port has to correct before the
   * engine reads it: the audio language (the commentators). It needs the table
   * the line above just read, and it writes to the card -- never to the OBB. */
  fifa14_locale_patch();
  /* ...and THE GAME'S OWN CONTROL SCHEME (fifa14_ctlcfg.c): the [CONTROL_CONFIG]
   * of ini/na/controls.ini and the control options of ini/na/common.ini's
   * [SETTINGS], patched with whatever config.ini [controls] asks for (default:
   * "keep" -- nothing is written) and logged as served. Same place, same
   * reason: the engine loads those files by name later in its own init. */
  fifa14_ctlcfg_patch();
  fifa14_ctlcfg_report("boot: what the engine will be served");
  /* ...and whether the player's commentary pack is on the card at all. The
   * engine's speech is a download (Speech/), so the port's job is to serve the
   * pack the player installs there and to say in the log what it sees. */
  fifa14_speech_pack_report();
  return 0;
}
