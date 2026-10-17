/* fifa14_external.c -- the game's external storage served from the OBB.
 * (fifa14_external.h has the failure this fixes: the engine opens
 * <external>/data/sprites/fifa14_splash_wide.sm2 by path, dcr_path.c turns it
 * into <game folder>/external/..., the file is not on the card, open() gives
 * ENOENT, the engine keeps a NULL handle and dies on `str r0,[r5,#8]`.)
 *
 * ============================================================ the container
 * main.136.com.ea.game.fifa14_row.obb, 1,386,236,976 B, is EA's "EB" container
 * (NOT a zip and NOT encrypted). Read out of the real file:
 *
 *   000000  45 42 00 03   magic "EB" + version 3
 *   000004  00 00 04 c1   u32 BE, the number of entries              1217
 *   00000c  00 00 51 10   u32 BE, the name table's byte offset      0x5110
 *   00001c  52 a0 4c 30   u32 BE, the whole file's size = 1,386,236,976
 *   000030                the entry table: 1217 x 16 B, ASCENDING by field 3:
 *                           +0  u32 BE  the data offset, in 16-byte units
 *                           +4  u32 BE  0
 *                           +8  u32 BE  the size in bytes
 *                           +12 u32 BE  djb2 (H = H*33 + c, H0 = 5381) of the
 *                                       entry's path INSIDE the container
 *   005110                the name table: 1217 x 40 B, same order as the
 *                           entries: u16 0, then the BASENAME (NUL-padded,
 *                           38 B; the longest in this file is 37 chars)
 *   010f38                a table of the 137 directory paths (66 B records),
 *                         the per-directory name lists, and
 *   01fd70                THE FILE DATA. NOT compressed, NOT encrypted: the
 *                         JPEG of "data/sprites/fifa14_splash_wide_en.jpg"
 *                         (entry 250, offset 0x03710ca5 * 16) starts with
 *                         ff d8 ff e0 00 10 4a 46 49 46, and
 *                         "data/sprites/hud_fui.png" (entry 198) with
 *                         89 50 4e 47 0d 0a 1a 0a. The first data offset is
 *                         exactly 0x1fd70, where the index ends, and the 1217
 *                         entries' sizes sum to 1,386,098,458 B = the file
 *                         minus that 0x1fd70-byte index.
 *
 * The paths are container-relative, forward slashes, no leading slash, and it
 * is exactly that string the hash covers:
 *     djb2("data/sprites/fifa14_splash_wide.sm2") = 0xa1aab474
 *     entry 768's field 3                        = 0xa1aab474  (92 B, entry 768's
 *     name table row is "fifa14_splash_wide.sm2")                <- what the game asks for
 * Verified for 400 of the 400 single-basename entries against the directory
 * table's own paths (data/gui/..., textures/..., skin.bin at the root, ...).
 * So the key is the game's own path with the package prefix removed -- what
 * dcr_path.c already leaves after <root>/external/.
 *
 * ============================================================= what it does
 * At boot: the header, the entry table and the name table are read into RAM
 * (1217 x (16 + 40) = 68 KB, never the 1.39 GB), hashed and checked (the
 * table's order, every offset inside the file).
 * Then, as the LAST step of dcr_translate_path() -- port_path_fixup(), the one
 * place every path-based open of the 32-bit ABI passes through -- a path in
 * <root>/external/ that is not on the card is looked up in the table (djb2 +
 * binary search) and the entry is written to <root>/external/<path>.part and
 * renamed into place, STREAMED through a 256 KiB buffer (a 36 MB .big costs
 * 256 KiB of RAM, not 36 MB of it). The engine then opens an ordinary file, so
 * open/open64/openat/fopen/stat/access/mmap all work as they would on the
 * phone. The next run finds the file already extracted (size checked against
 * the table), and a file the game itself wrote there is left alone.
 *
 * Not in the table and not on the card -> nothing is changed and the open
 * fails exactly as before (errno 2): the port never invents a file. Every
 * served path is logged once, counted, and reported at the end of the run.
 * MIT. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#include <switch.h>

#include "config.h"
#include "dcr_dircache.h"
#include "dcr_path.h"
#include "dcr_setup.h"
#include "fifa14_external.h"
#include "fifa14_obb.h"
#include "rt_cfg.h"
#include "util.h"

/* ------------------------------------------------------- the container ---
 * (fifa14_external.h and the header comment above have the evidence.) */
#define OBB_MAGIC        0x45420003u /* "EB" + version 3 */
#define OBB_COUNT_OFF    0x04u       /* u32 BE: the number of entries */
#define OBB_NAMES_OFF    0x0cu       /* u32 BE: the name table's offset */
#define OBB_SIZE_OFF     0x1cu       /* u32 BE: the whole file's size */
#define OBB_HDR_BYTES    0x30u       /* the header, up to the entry table */
#define OBB_ENTRY_OFF    0x30u
#define OBB_ENTRY_BYTES  16u
#define OBB_HASH_FIELD   12u         /* the entry's djb2(path) */
#define OBB_SIZE_FIELD   8u          /* the entry's size in bytes */
#define OBB_NAME_BYTES   40u         /* the name table's record */
#define OBB_NAME_SKIP    2u          /* u16 before the basename */
#define OBB_NAME_MAX     (OBB_NAME_BYTES - OBB_NAME_SKIP) /* 38: 37 chars + NUL */
#define OBB_OFF_UNIT     16u         /* the data offset's unit */
/* A table bigger than this is not this container: refuse it instead of
 * allocating whatever a corrupt header says. */
#define OBB_MAX_ENTRIES  (1u << 20)

/* The folder dcr_path.c sends <shared storage>/Android/data/<pkg>/... to. */
#define EXT_DIR          "external"
/* The extraction's stream buffer: never a whole entry in RAM (the largest
 * here is 36 MB). */
#define EXT_COPY_CHUNK   (256u * 1024u)
/* The destination is <game root> + "/external/" + the path in the container,
 * which may use all of DCR_PATH_MAX: size it so it can never be cut short
 * (a truncated path would extract to the WRONG file). */
#define EXT_DST_MAX      (DCR_PATH_MAX + 320)

/* open lines one run may spend; past it only a path not seen before is
 * logged (the report has the counts). */
#define EXT_LOG_MAX      400
/* Distinct paths the report lists. */
#define EXT_TRACK_MAX    96

typedef struct {
  uint32_t hash; /* djb2 of the path in the container */
  uint32_t size; /* its size in bytes */
  uint32_t name; /* where its basename sits in the OBB (name table) */
  uint64_t off;  /* its data */
} Entry;

/* What an open did. */
enum {
  ST_NONE = 0,
  ST_SERVED,  /* taken from the OBB now */
  ST_CACHED,  /* already extracted (this run or an earlier one) */
  ST_MISSING, /* not in the container: the open fails, as before */
  ST_FAILED,  /* in the container but could not be written / read out */
  ST_KEPT,    /* on the card, larger than the entry: left alone (a save?) */
  ST_OWNER,   /* a path the PLAYER owns: never written over once it exists */
  ST_EARLY,   /* asked before the table was read */
};

typedef struct {
  char name[DCR_PATH_MAX + 32];
  unsigned requests;
  int last;
} Track;

static Entry *g_e;
static uint32_t g_n;
static int g_ready;  /* the table is in RAM: an external path may be served */
static int g_sorted; /* the table is ascending by hash (binary search) */
static Mutex g_lock;
static int g_lock_up;
static uint8_t *g_copy; /* the extraction buffer (EXT_COPY_CHUNK) */

static Track g_track[EXT_TRACK_MAX];
static int g_ntrack;
static unsigned g_requests, g_served, g_cached, g_missing, g_failed, g_kept, g_owner, g_early,
    g_badname;
static uint64_t g_bytes; /* written out of the OBB */
static unsigned g_logged;

/* The path the open being logged is about (set by serve_locked). */
static uint32_t g_cur_size;
static char g_cur_dst[EXT_DST_MAX];

int fifa14_external_count(void) { return (int)g_n; }

uint32_t fifa14_external_hash(const char *path) {
  uint32_t h = 5381u;
  for (const unsigned char *p = (const unsigned char *)path; p && *p; p++)
    h = h * 33u + *p;
  return h;
}

static uint32_t be32(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

/* The entry's basename, read from the OBB's name table. */
static void entry_name(const Entry *e, char *out, size_t cap) {
  uint8_t b[OBB_NAME_BYTES];
  size_t k = 0;
  out[0] = 0;
  if (!cap || fifa14_obb_read(e->name, b, sizeof b) != 0)
    return;
  while (k < OBB_NAME_MAX - 1 && k + 1 < cap && b[k]) {
    out[k] = (char)b[k];
    k++;
  }
  out[k] = 0;
}

/* The entry whose path hashes to `h`, or -1. */
static int find_by_hash(uint32_t h) {
  if (!g_n)
    return -1;
  if (g_sorted) {
    uint32_t lo = 0, hi = g_n;
    while (lo < hi) {
      const uint32_t mid = lo + (hi - lo) / 2;
      if (g_e[mid].hash < h)
        lo = mid + 1;
      else
        hi = mid;
    }
    return lo < g_n && g_e[lo].hash == h ? (int)lo : -1;
  }
  for (uint32_t i = 0; i < g_n; i++)
    if (g_e[i].hash == h)
      return (int)i;
  return -1;
}

/* ------------------------------------------------------------------ init */
int fifa14_external_init(void) {
  uint8_t hdr[OBB_HDR_BYTES];
  uint8_t *raw = NULL;
  Entry *e = NULL;

  if (!g_lock_up) {
    mutexInit(&g_lock);
    g_lock_up = 1;
  }
  if (!fifa14_obb_ready()) {
    debugPrintf("[obb] the OBB is not open: its file table is not read, and the game's "
                "external data stays unserved\n");
    return -1;
  }
  if (fifa14_obb_read(0, hdr, sizeof hdr) != 0) {
    debugPrintf("[obb] the container's header does not read\n");
    return -1;
  }
  const uint32_t magic = be32(hdr);
  const uint32_t count = be32(hdr + OBB_COUNT_OFF);
  const uint64_t names = (uint64_t)be32(hdr + OBB_NAMES_OFF);
  const uint64_t size = fifa14_obb_size();
  debugPrintf("[obb] container: magic %02x %02x %02x %02x%s, %u entries, names at 0x%llx, "
              "%llu B\n",
              hdr[0], hdr[1], hdr[2], hdr[3],
              magic == OBB_MAGIC ? " (EA \"EB\" v3, not a zip)" : " (UNKNOWN)", (unsigned)count,
              (unsigned long long)names, (unsigned long long)size);
  if (magic != OBB_MAGIC) {
    debugPrintf("[obb] not an EA \"EB\" container: no file table is built (the game's external "
                "data is left to fail as it did)\n");
    return -1;
  }
  if (!count || count > OBB_MAX_ENTRIES ||
      OBB_ENTRY_OFF + (uint64_t)count * OBB_ENTRY_BYTES > size ||
      names + (uint64_t)count * OBB_NAME_BYTES > size) {
    debugPrintf("[obb] the header's table does not fit in %llu B (count %u, names 0x%llx): "
                "refusing it\n",
                (unsigned long long)size, (unsigned)count, (unsigned long long)names);
    return -1;
  }

  e = calloc(count, sizeof *e);
  raw = malloc((size_t)count * OBB_ENTRY_BYTES);
  if (!e || !raw) {
    debugPrintf("[obb] no memory for the file table (%u entries)\n", (unsigned)count);
    free(e);
    free(raw);
    return -1;
  }
  if (fifa14_obb_read(OBB_ENTRY_OFF, raw, (size_t)count * OBB_ENTRY_BYTES) != 0) {
    debugPrintf("[obb] the entry table (0x%x, %u x %u B) does not read\n", (unsigned)OBB_ENTRY_OFF,
                (unsigned)count, (unsigned)OBB_ENTRY_BYTES);
    free(e);
    free(raw);
    return -1;
  }

  uint32_t inside = 0, bad = 0;
  uint64_t min_off = ~0ull, max_end = 0;
  int sorted = 1;
  for (uint32_t i = 0; i < count; i++) {
    const uint8_t *r = raw + (size_t)i * OBB_ENTRY_BYTES;
    e[i].off = (uint64_t)be32(r) * OBB_OFF_UNIT;
    e[i].size = be32(r + OBB_SIZE_FIELD);
    e[i].hash = be32(r + OBB_HASH_FIELD);
    e[i].name = (uint32_t)(names + (uint64_t)i * OBB_NAME_BYTES + OBB_NAME_SKIP);
    if (i && e[i].hash < e[i - 1].hash)
      sorted = 0;
    if (e[i].off < min_off)
      min_off = e[i].off;
    if (e[i].off + e[i].size > max_end)
      max_end = e[i].off + e[i].size;
    if (e[i].off < (uint64_t)count * OBB_ENTRY_BYTES || e[i].off + e[i].size > size ||
        e[i].off > 0xffffffffull)
      bad++;
    else
      inside++;
  }
  free(raw);

  g_e = e;
  g_n = count;
  g_sorted = sorted;
  g_copy = malloc(EXT_COPY_CHUNK);
  g_ready = 1;

  /* The first and last rows are printed as read: what proves the layout is
   * that the offsets inside them hold the files the names say (the self-check
   * below reads one back). */
  debugPrintf("[obb] the table is sorted by the path's hash=%s, %u entr%s inside the file, %u "
              "outside it\n",
              g_sorted ? "yes (binary search)" : "NO (linear search)", (unsigned)inside,
              inside == 1 ? "y" : "ies", (unsigned)bad);
  char nm[OBB_NAME_MAX];
  for (uint32_t k = 0; k < (count < 3 ? count : 3); k++) {
    entry_name(&g_e[k], nm, sizeof nm);
    debugPrintf("[obb]   #%u  hash %08x  off 0x%08llx  size %-9u  %s\n", (unsigned)k,
                g_e[k].hash, (unsigned long long)g_e[k].off, (unsigned)g_e[k].size, nm);
  }
  entry_name(&g_e[count - 1], nm, sizeof nm);
  debugPrintf("[obb]   #%u  hash %08x  off 0x%08llx  size %-9u  %s (last)\n", (unsigned)(count - 1),
              g_e[count - 1].hash, (unsigned long long)g_e[count - 1].off,
              (unsigned)g_e[count - 1].size, nm);
  debugPrintf("[obb]   %llu B of data in %u entries; the first one starts at 0x%llx (the index "
              "ends there) and the last ends at 0x%llx of %llu B\n",
              (unsigned long long)(size - min_off), (unsigned)count,
              (unsigned long long)min_off, (unsigned long long)max_end,
              (unsigned long long)size);
  fifa14_external_selfcheck("the boot's fifa14_obb_init");
  return (int)count;
}

/* ----------------------------------------------------------------- paths */
/* `real` -> the path the container's hash covers ("data/sprites/x.sm2").
 * Two forms arrive at the fixup: the TRANSLATED one
 * (<game root>/external/<path>, dcr_path.c's rule for
 * <shared storage>/Android/data/<pkg>/...) and -- when no rule matched the
 * engine's path at all -- the engine's own Android path. 0 when it is
 * neither. */
static int ext_rel(const char *real, char *out, size_t cap) {
  if (!real || !real[0] || !cap)
    return 0;
  const char *p = real;
  while (!strncmp(p, "file://", 7))
    p += 7;

  const char *root = dcr_game_root();
  const size_t rl = strlen(root);
  const char *nodev = strchr(root, ':');
  nodev = nodev ? nodev + 1 : root;
  const char *r = NULL;
  if (rl && !strncmp(p, root, rl))
    r = p + rl;
  else if (*nodev && !strncmp(p, nodev, strlen(nodev)))
    r = p + strlen(nodev);

  if (r) { /* ours: only <root>/external/ is this module's */
    const size_t n = sizeof("/" EXT_DIR "/") - 1;
    if (strncmp(r, "/" EXT_DIR "/", n) != 0)
      return 0;
    p = r + n;
  } else { /* the engine's own Android path, untranslated */
    static const char *const k_ext[] = {
        "/storage/emulated/0/Android/data/" PORT_PACKAGE "/",
        "/storage/emulated/legacy/Android/data/" PORT_PACKAGE "/",
        "/storage/sdcard0/Android/data/" PORT_PACKAGE "/",
        "/sdcard/Android/data/" PORT_PACKAGE "/",
        "/mnt/sdcard/Android/data/" PORT_PACKAGE "/",
        "/mnt/extSdCard/Android/data/" PORT_PACKAGE "/",
    };
    const char *t = NULL;
    for (unsigned i = 0; i < ARRAY_SIZE(k_ext) && !t; i++) {
      const size_t n = strlen(k_ext[i]);
      if (!strncmp(p, k_ext[i], n))
        t = p + n;
    }
    if (!t)
      return 0;
    p = t;
  }
  while (*p == '/')
    p++;
  if (!*p)
    return 0;
  /* Collapse separators and '\' the way dcr_path.c's clean() does, so the
   * string that reaches the hash is the container's own spelling. */
  size_t o = 0;
  int prev_sep = 0;
  for (; *p && o + 1 < cap; p++) {
    char c = *p == '\\' ? '/' : *p;
    if (c == '/') {
      if (prev_sep)
        continue;
      prev_sep = 1;
    } else {
      prev_sep = 0;
    }
    out[o++] = c;
  }
  out[o] = 0;
  return o != 0;
}

/* <game root>/external/<rel> into `out` (cap >= EXT_DST_MAX): 1 when it fits.
 * A path that does not fit is a bug worth seeing: logged and refused. */
static int ext_dst(char *out, size_t cap, const char *rel) {
  const int n = snprintf(out, cap, "%s/" EXT_DIR "/%s", dcr_game_root(), rel);
  if (n < 0 || (size_t)n >= cap) {
    debugPrintf("[obb] the destination path for \"%s\" does not fit in %u B: refusing it\n", rel,
                (unsigned)cap);
    out[0] = 0;
    return 0;
  }
  return 1;
}

/* The container's hash of a path, case-folded (the container's paths are all
 * lower case; an engine that spells one differently still finds it). */
static uint32_t hash_lower(const char *path) {
  char low[DCR_PATH_MAX];
  size_t k = 0;
  for (; path[k] && k + 1 < sizeof low; k++) {
    const char c = path[k];
    low[k] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
  }
  low[k] = 0;
  return fifa14_external_hash(low);
}

static const char *base_name(const char *rel) {
  const char *s = strrchr(rel, '/');
  return s ? s + 1 : rel;
}

/* ------------------------------------------------------ the player's own ====
 * The paths inside the game's external storage that belong to the PLAYER (or
 * to a pack the player installed) and not to the expansion file. Once one of
 * them is on the card the port never writes the container's copy over it,
 * whatever the sizes are -- the ordinary rule only protects a file LARGER than
 * the entry, which is not enough for a text the user is asked to edit or for a
 * pack the game itself would replace.
 *
 *   data/cmn/fe/about_ES.txt   Help -> About, Spanish  (the user's own text)
 *   data/cmn/fe/about.txt      Help -> About, English  (the same, English)
 *   ini/na/locale.ini          the audio language, patched by the port
 *                              (fifa14_locale_patch) -- its size no longer
 *                              matches the entry once patched, so without this
 *                              rule every run would re-extract the OBB's copy
 *                              and undo the patch.
 *
 *   Speech/...                 THE COMMENTARY PACK (added 2026-10-13). FIFA 14
 *                              Android has no commentary in its data: the
 *                              engine's speech is a DOWNLOAD, served by
 *                              EA::ContentManager out of
 *                              <external>/Speech/ (workingDirectory "Speech"
 *                              in the app's own contentmanagerSpeech.cmconfig,
 *                              serverUrl "http://127.0.0.1" in every build
 *                              that has been read). The engine asks for
 *                              Speech/HiRes.cmconfig, ...cmserver,
 *                              Speech/dat_ENG_US.big and its .cmmetadata, etc.
 *                              -- the names a pack the player installs has.
 *                              With the whole prefix owned by the player, a
 *                              pack copied to <external>/Speech/ is served as
 *                              it is and can never be replaced by anything the
 *                              port does. (The OBB has no Speech/ path at
 *                              all, so this is a promise, not a rescue: the
 *                              port would serve those files anyway, as "on the
 *                              card already", but now the log says whose they
 *                              are.)
 *
 * The paths are container-relative and compared case-insensitively (the
 * container's are lower case). */
/* Whole directories the player owns: every path under one of these. */
static const char *const k_owner_prefix[] = {
    "Speech/", /* the commentary pack (see above) */
    NULL,
};

static int is_user_override(const char *rel) {
  static const char *const k_owner[] = {
      "data/cmn/fe/about_ES.txt",
      "data/cmn/fe/about.txt",
      "ini/na/locale.ini",
      /* THE GAME'S CONTROL SCHEME (added 2026-10-13, fifa14_ctlcfg.c): the
       * [CONTROL_CONFIG] block of ini/na/controls.ini (SCHEME_ID_P0/P1,
       * LAYOUT_ID, RENDER_DEBUG, USE_SIDE_BUTTONS) and the control options of
       * ini/na/common.ini's [SETTINGS] block (FLOATING_DPAD, BASIC_HAND,
       * BIG_BUTTONS, BIG_DPAD, ALTERNATE_CONTROLS) are the levers the port's
       * config.ini [controls] moves. Both are patched on the card, so the
       * container's copy must never be extracted over them -- the same reason
       * locale.ini is here. */
      "ini/na/controls.ini",
      "ini/na/common.ini",
      NULL,
  };
  if (!rel)
    return 0;
  for (int i = 0; k_owner[i]; i++)
    if (!strcasecmp(rel, k_owner[i]))
      return 1;
  for (int i = 0; k_owner_prefix[i]; i++) {
    const size_t n = strlen(k_owner_prefix[i]);
    if (!strncasecmp(rel, k_owner_prefix[i], n))
      return 1;
  }
  return 0;
}

/* ------------------------------------------------------ the speech pack ====
 * THE NAMES, AND WHERE THEY COME FROM. FIFA 14 Android ships no commentary:
 * the engine's speech is EA::ContentManager content under the working
 * directory the app's own asset says (contentmanagerSpeech.cmconfig:
 * "workingDirectory": "Speech"), and the only speech file names the engine
 * itself carries are, read out of libFIFA14.so's string table,
 *
 *     Speech/hdr_ENG_US.big   Speech/sth_ENG_US.big
 *     Speech/dat_ENG_US.bh    Speech/pbp_ENG_US.evt
 *
 * (four consecutive strings, and no other language's). Those are the names of
 * a PACK, and the pack the engine then looked for on the user's card was
 * Speech/HiRes.cmconfig and its .cmserver/.cmdownload siblings (the run's log:
 * "[assets] open(\"Speech/HiRes.cmconfig\") -> NOT in the APK's assets/", "the
 * engine gets FileNotFoundException"). The sizes below are the ones the pack's
 * own .cmmetadata files declare ({"size": N}) and, for dat_ENG_US.big, the
 * real file's -- the engine checks the SIZE, not a hash: both
 * contentmanager*.cmconfig files ship "checksumValidationEnabled": false and
 * "eTagEnabled": false, so there is nothing a port would have to forge.
 *
 * This function is the one line the next log needs: which of the pack's files
 * the port can see, with their sizes. It writes nothing. */
int fifa14_speech_pack_report(void) {
  static const struct {
    const char *rel;
    long long size;
  } k_pack[] = {
      {"Speech/dat_ENG_US.big", 497643440},
      {"Speech/dat_ENG_US.bh", 127984},
      {"Speech/hdr_ENG_US.big", 332880},
      {"Speech/sth_ENG_US.big", 373492},
      {"Speech/pbp_ENG_US.evt", 57484},
      {"Speech/HiRes.cmconfig", 5129},         /* the content description */
      {"Speech/HiRes.cmconfig.cmserver", 4476},/* the same, as downloaded */
  };
  char dst[EXT_DST_MAX];
  int seen = 0;
  debugPrintf("[obb] ==== the commentary pack (%s) ====\n", k_owner_prefix[0]);
  for (size_t i = 0; i < sizeof k_pack / sizeof k_pack[0]; i++) {
    if (!ext_dst(dst, sizeof dst, k_pack[i].rel)) {
      debugPrintf("[obb]   %s: the path does not fit\n", k_pack[i].rel);
      continue;
    }
    const long have = rt_file_size(dst);
    if (have < 0) {
      debugPrintf("[obb]   %s: MISSING on the card (the engine will ask for it and "
                  "fail, as it does now; the pack declares %lld B)\n",
                  k_pack[i].rel, k_pack[i].size);
      continue;
    }
    seen++;
    debugPrintf("[obb]   %s: %ld B on the card (the pack declares %lld B)%s\n",
                k_pack[i].rel, have, k_pack[i].size,
                have == (long)k_pack[i].size ? "" : "  <- SIZE DOES NOT MATCH");
  }
  if (seen == 0)
    debugPrintf("[obb] the commentary pack is NOT installed: that is why the game's "
                "language screen offers the commentators as a DOWNLOAD. The engine's "
                "speech is content the phone downloaded (its own contentmanagerSpeech."
                "cmconfig points at http://127.0.0.1, i.e. no server in the shipped "
                "build), and no .mp3 in this game is commentary: data/audio/speech/"
                "*.mp3 are the 10-13 s language SAMPLES the menu plays.\n");
  else
    debugPrintf("[obb] the commentary pack: %d of %d files present (its whole "
                "directory is the player's: is_user_override covers \"%s\")\n",
                seen, (int)(sizeof k_pack / sizeof k_pack[0]), k_owner_prefix[0]);
  /* THE INCOMPLETE-DOWNLOAD MARKER. <file>.cmdownload is the TEMPORARY file
   * EA::ContentManager::FileDownloader writes a download into (the .so's
   * FileDownloader::SetTemporaryFileExtension / PrepareResume /
   * RemoveIncompleteDownloads / GetTmpDownloadFilePath). The engine created a
   * 0-byte one on this card by itself -- the run's log has
   * `open(.../Speech/HiRes.cmconfig.cmserver.cmdownload) flags=0x242` where
   * 0x242 is O_RDWR|O_CREAT|O_TRUNC and the returned value is a FILE
   * DESCRIPTOR (11), not an error. The pack the player copies in carries one
   * too (the author's own copy had it): it is the residue of a description
   * download that never finished.
   *
   * With the pack's files in place there is nothing left to download, and a
   * leftover temp file is worse than useless: the CM stats and READS it when it
   * prepares a resume (`[fs] obb asked="Speech/HiRes.cmconfig.cmserver.
   * cmdownload"` appears in the same log right before the description download
   * starts), i.e. an unfinished download is exactly what the language screen
   * then offers to finish. So the port removes it -- the same thing the CM's own
   * TemporaryFilesUtils::RemoveIncompleteDownloads does with a stale temp --
   * and says so. Nothing else under Speech/ is ever touched (is_user_override
   * still owns every byte of the pack). */
  if (ext_dst(dst, sizeof dst, "Speech/HiRes.cmconfig.cmserver.cmdownload")) {
    const long have = rt_file_size(dst);
    if (have == 0) {
      if (seen == (int)(sizeof k_pack / sizeof k_pack[0]) && remove(dst) == 0)
        debugPrintf("[obb] the pack is complete and %s was a 0-byte INCOMPLETE-DOWNLOAD "
                    "MARKER (the engine's own temp file, one the pack itself carried): "
                    "REMOVED -- an unfinished description download is the state that makes "
                    "the language screen offer the pack as a download\n",
                    dst);
      else if (seen == (int)(sizeof k_pack / sizeof k_pack[0]))
        debugPrintf("[obb] %s is a 0-byte INCOMPLETE-DOWNLOAD MARKER and could not be "
                    "removed: delete it by hand (the pack is complete, so nothing is left to "
                    "download)\n",
                    dst);
      else
        debugPrintf("[obb] %s is a 0-byte INCOMPLETE-DOWNLOAD MARKER and %d of the pack's "
                    "files are missing: left alone (it is the state of a real, unfinished "
                    "download)\n",
                    dst, (int)(sizeof k_pack / sizeof k_pack[0]) - seen);
    } else if (have > 0) {
      debugPrintf("[obb] NOTE: %s holds %ld B: it is a download IN PROGRESS or a finished one, "
                  "and the port leaves it alone\n",
                  dst, have);
    }
  }
  return seen;
}

/* -------------------------------------------------------------- serving */
/* The entry's bytes -> `dst`, streamed (fifa14_external.c's header: 256 KiB
 * of RAM whatever the entry's size). Returns 1 when `dst` holds them. */
static int extract_entry(const char *dst, uint64_t off, uint32_t size) {
  char tmp[EXT_DST_MAX + 8];
  const int n = snprintf(tmp, sizeof tmp, "%s.part", dst);
  if (n < 0 || (size_t)n >= sizeof tmp) {
    debugPrintf("[obb] the temporary path for \"%s\" does not fit\n", dst);
    return 0;
  }
  rt_mkdirs_for(dst);
  FILE *f = fopen(tmp, "wb");
  if (!f) {
    debugPrintf("[obb] cannot create %s (out of space?)\n", tmp);
    return 0;
  }
  uint32_t done = 0;
  int ok = 1;
  while (done < size) {
    uint32_t want = size - done;
    if (want > EXT_COPY_CHUNK)
      want = EXT_COPY_CHUNK;
    if (!g_copy || fifa14_obb_read(off + done, g_copy, want) != 0) {
      debugPrintf("[obb] %s: the OBB does not read at 0x%llx (%u B)\n", dst,
                  (unsigned long long)(off + done), (unsigned)want);
      ok = 0;
      break;
    }
    if (fwrite(g_copy, 1, want, f) != want) {
      debugPrintf("[obb] %s: writing %u B at 0x%x failed\n", tmp, (unsigned)want, (unsigned)done);
      ok = 0;
      break;
    }
    done += want;
  }
  if (fclose(f) != 0)
    ok = 0;
  if (ok) {
    unlink(dst); /* rename() over an existing file is not everywhere */
    ok = rename(tmp, dst) == 0;
  }
  if (!ok) {
    unlink(tmp);
    return 0;
  }
  /* dcr_dircache.c answers "this file does not exist" out of a directory
   * listing, and <game folder>/external/ is inside its scope. The file (and
   * its directories) were just created with newlib's open/rename -- not
   * through the bionic shims that tell the cache to forget -- so the listing
   * it took BEFORE this extraction does not have the name and every later
   * open of it would be answered ENOENT without the card being asked. */
  dcr_dircache_forget();
  return 1;
}

/* What an open of `rel` does. The caller holds g_lock (two threads extracting
 * the same entry would share the .part file). */
static int serve_locked(const char *rel, char *dst, size_t cap) {
  g_requests++;
  g_cur_size = 0;
  g_cur_dst[0] = 0;
  if (!g_ready) {
    g_early++;
    return ST_EARLY;
  }
  if (!ext_dst(dst, cap, rel)) {
    g_failed++;
    return ST_FAILED;
  }
  /* The log's subject: the path on the card this open is about, whatever the
   * lookup below decides. */
  snprintf(g_cur_dst, sizeof g_cur_dst, "%s", dst);
  const long have = rt_file_size(dst);
  /* THE PLAYER'S OWN FILES COME FIRST, AND STAY. A handful of paths belong to
   * the user, not to the container, and the port must NEVER write the OBB's
   * copy over them once they exist: about_ES.txt / about.txt are the Help ->
   * About text the player replaces with his own, and ini/na/locale.ini carries
   * the audio language the port patches (fifa14_locale_patch() below). The
   * ordinary rule (a file SMALLER than the entry is re-extracted) would destroy
   * both, so this rule is checked before any size comparison. */
  if (have >= 0 && is_user_override(rel)) {
    g_owner++;
    /* the entry's size is wanted for the log line only (a bsearch) */
    int io = find_by_hash(fifa14_external_hash(rel));
    if (io < 0)
      io = find_by_hash(hash_lower(rel));
    if (io >= 0)
      g_cur_size = g_e[io].size;
    return ST_OWNER;
  }
  uint32_t h = fifa14_external_hash(rel);
  int i = find_by_hash(h);
  if (i < 0) {
    /* Not the exact spelling: the container's paths are lower case. */
    const uint32_t hl = hash_lower(rel);
    if (hl != h) {
      i = find_by_hash(hl);
      if (i >= 0) {
        char nm[OBB_NAME_MAX];
        entry_name(&g_e[i], nm, sizeof nm);
        if (strcasecmp(nm, base_name(rel)) != 0)
          i = -1; /* a different file: not this one */
      }
    }
    if (i < 0) {
      /* Not in the container. A file already on the card (the game's own save
       * or cache) is left exactly as it is; a missing one keeps failing the
       * way it did before this module existed (errno 2, the engine decides). */
      if (have >= 0) {
        g_kept++;
        return ST_KEPT;
      }
      g_missing++;
      return ST_MISSING;
    }
  } else {
    /* The hash is the container's key; the name is the proof. A hash that
     * matches under a DIFFERENT basename is a collision, not the file. */
    char nm[OBB_NAME_MAX];
    entry_name(&g_e[i], nm, sizeof nm);
    if (strcasecmp(nm, base_name(rel)) != 0) {
      g_badname++;
      debugPrintf("[obb] \"%s\" hashes to an entry named \"%s\": refusing it (a hash collision?)\n",
                  rel, nm);
      g_missing++;
      return ST_MISSING;
    }
  }
  g_cur_size = g_e[i].size;
  if (g_e[i].off < (uint64_t)g_n * OBB_ENTRY_BYTES || g_e[i].off + g_e[i].size > fifa14_obb_size()) {
    debugPrintf("[obb] \"%s\": the table puts it at 0x%llx (%u B), past the container\n", rel,
                (unsigned long long)g_e[i].off, (unsigned)g_e[i].size);
    g_failed++;
    return ST_FAILED;
  }
  if (have == (long)g_e[i].size) {
    g_cached++;
    return ST_CACHED;
  }
  if (have > (long)g_e[i].size) {
    /* Bigger than the entry: the game wrote something of its own there (a
     * save, a cache) -- it is NOT the container's file, and destroying it
     * would lose the player's data. */
    g_kept++;
    return ST_KEPT;
  }
  if (!extract_entry(dst, g_e[i].off, g_e[i].size)) {
    g_failed++;
    return ST_FAILED;
  }
  g_served++;
  g_bytes += g_e[i].size;
  return ST_SERVED;
}

/* Which entry of the report `rel` is, adding it when new; -1 table full. */
static int tracked(const char *rel) {
  for (int i = 0; i < g_ntrack; i++)
    if (!strcmp(g_track[i].name, rel))
      return i;
  if (g_ntrack >= EXT_TRACK_MAX)
    return -1;
  snprintf(g_track[g_ntrack].name, sizeof g_track[0].name, "%s", rel);
  g_track[g_ntrack].requests = 0;
  g_track[g_ntrack].last = ST_NONE;
  return g_ntrack++;
}

static const char *why(int st) {
  switch (st) {
  case ST_SERVED:
    return "served from the OBB now";
  case ST_CACHED:
    return "already extracted";
  case ST_MISSING:
    return "NOT in the OBB (the open failed)";
  case ST_FAILED:
    return "IN the OBB but could not be served";
  case ST_KEPT:
    return "on the card already (left alone)";
  case ST_OWNER:
    return "the player's own file (never written over)";
  default:
    return "asked before the table was read";
  }
}

static void note_locked(const char *rel, int st) {
  const int i = tracked(rel);
  const int first = i >= 0 && g_track[i].requests == 0;
  if (i >= 0) {
    g_track[i].requests++;
    g_track[i].last = st;
  }
  if (g_logged >= EXT_LOG_MAX && !first)
    return;
  g_logged++;
  switch (st) {
  case ST_SERVED:
    debugPrintf("[obb] open(%s) -> SERVED %u B from the OBB to %s\n", rel, (unsigned)g_cur_size,
                g_cur_dst);
    break;
  case ST_CACHED:
    debugPrintf("[obb] open(%s) -> already extracted (%u B in %s)\n", rel, (unsigned)g_cur_size,
                g_cur_dst);
    break;
  case ST_MISSING:
    debugPrintf("[obb] open(%s) -> NOT in the OBB (%u entries there: this open fails as before)\n",
                rel, (unsigned)g_n);
    break;
  case ST_FAILED:
    debugPrintf("[obb] open(%s) -> ERROR: in the OBB (%u B) and could not be written to %s\n", rel,
                (unsigned)g_cur_size, g_cur_dst);
    break;
  case ST_KEPT:
    if (g_cur_size)
      debugPrintf("[obb] open(%s) -> the card already holds one of its own (%ld B, the OBB's is "
                  "%u B): left alone\n",
                  rel, rt_file_size(g_cur_dst), (unsigned)g_cur_size);
    else
      debugPrintf("[obb] open(%s) -> not in the OBB and the card has a file there (%ld B): left "
                  "alone\n",
                  rel, rt_file_size(g_cur_dst));
    break;
  case ST_OWNER:
    debugPrintf("[obb] open(%s) -> THE PLAYER'S OWN FILE (%ld B on the card%s): served as it is, "
                "the container's copy is NOT written over it\n",
                rel, rt_file_size(g_cur_dst),
                g_cur_size ? " where the OBB's entry is a different size" : "");
    break;
  default:
    debugPrintf("[obb] open(%s) -> asked before the OBB's file table was read: the open fails\n",
                rel);
    break;
  }
  if (g_logged == EXT_LOG_MAX)
    debugPrintf("[obb] %d external open lines logged; a path not seen before is still logged\n",
                EXT_LOG_MAX);
}

/* --------------------------------------------------------------- the hook */
const char *fifa14_external_fixup(const char *real, char *out, size_t cap) {
  char rel[DCR_PATH_MAX];
  char dst[EXT_DST_MAX];
  if (!real || !out || !cap || !g_lock_up || !ext_rel(real, rel, sizeof rel))
    return real;
  mutexLock(&g_lock);
  const int st = serve_locked(rel, dst, sizeof dst);
  note_locked(rel, st);
  mutexUnlock(&g_lock);
  const int served = st == ST_SERVED || st == ST_CACHED || st == ST_KEPT || st == ST_OWNER;
  debugPrintf("[fs] obb     asked=\"%s\" real=\"%s\" status=%s served=%d\n", rel, served ? dst : real,
              why(st), served);
  if (!served)
    return real;
  if (strlen(dst) >= cap) {
    /* The caller's buffer (DCR_PATH_MAX) is a contract: never return a path
     * it cannot hold. */
    debugPrintf("[obb] the served path for \"%s\" needs %u B and the caller's buffer is %u\n", rel,
                (unsigned)strlen(dst), (unsigned)cap);
    return real;
  }
  if (!strcmp(dst, real))
    return real; /* the ordinary translated path: exactly this file */
  snprintf(out, cap, "%s", dst);
  return out;
}

/* ------------------------------------------------------------- self-check */
/* The engine's own path (the crash log's) and one a directory deeper, asked
 * the way the engine asks them: a run's log then says whether the table can
 * find the data BEFORE the engine asks for it. Nothing is written out. */
void fifa14_external_selfcheck(const char *why) {
  static const char *const k_paths[] = {
      "data/sprites/fifa14_splash_wide.sm2",  /* the file the engine crashed on */
      "data/sprites/fifa14_splash_wide_en.jpg",
      "data/sprites/loading_sprite.sm2",
      "data/cmn/trivia.db",
      "skin.bin",
  };
  debugPrintf("[obb] self-check (%s): the game's own paths, asked of the table\n", why);
  char nm[OBB_NAME_MAX];
  for (unsigned k = 0; k < ARRAY_SIZE(k_paths); k++) {
    const uint32_t h = fifa14_external_hash(k_paths[k]);
    const int i = find_by_hash(h);
    if (i < 0) {
      debugPrintf("[obb]   %-42s hash %08x -> NOT in the table\n", k_paths[k], (unsigned)h);
      continue;
    }
    entry_name(&g_e[i], nm, sizeof nm);
    /* The data is read back at its offset: the entry's bytes are the file the
     * name says (a JPEG's SOI, a PNG's signature). */
    uint8_t probe[8] = {0};
    const int read = fifa14_obb_read(g_e[i].off, probe, sizeof probe) == 0;
    debugPrintf("[obb]   %-42s hash %08x -> entry #%-4d \"%s\" at 0x%llx (%u B)  %s\n",
                k_paths[k], (unsigned)h, i, nm, (unsigned long long)g_e[i].off,
                (unsigned)g_e[i].size,
                read ? "reads" : "DOES NOT READ");
  }
}

/* --------------------------------------------------------------- report */
void fifa14_external_report(void) {
  if (!g_lock_up || !g_requests)
    return;
  debugPrintf("[obb] ==== the game's external storage, opened by the engine ====\n");
  debugPrintf("[obb] the OBB's table has %u entries; %d distinct path(s) were asked for: %u "
              "open(s), %u served from the OBB (%llu MB written), %u already extracted, %u FAILED, "
              "%u not in the OBB, %u left to the card's own, %u the player's own (never written "
              "over), %u before the table\n",
              (unsigned)g_n, g_ntrack, g_requests, g_served, (unsigned long long)(g_bytes >> 20),
              g_cached, g_failed, g_missing, g_kept, g_owner, g_early);
  for (int i = 0; i < g_ntrack; i++)
    debugPrintf("[obb]   %-56s %5u open(s)  %s\n", g_track[i].name, g_track[i].requests,
                why(g_track[i].last));
  if (g_ntrack >= EXT_TRACK_MAX)
    debugPrintf("[obb]   (only the first %d distinct paths are listed)\n", EXT_TRACK_MAX);
  if (g_badname)
    debugPrintf("[obb] %u path(s) hashed to an entry with a different name (refused)\n",
                (unsigned)g_badname);
}

/* ================================== THE AUDIO LANGUAGE (the commentators) ====
 * MEASURED, from the OBB's own ini/na/locale.ini (862 B, entry 837 of 1217)
 * and the file the port already extracts to
 * <game folder>/external/ini/na/locale.ini:
 *
 *   [OPTIONS]  LANG_OVERRIDE = 1          (use the device's language -- the
 *                                         TEXT already comes out Spanish and
 *                                         the log proves it: SPA_ES.loc loads)
 *              DEFAULT_TEXT_LANGUAGE = ENG_US
 *   [TEXT_LANGUAGES]  ENGLISH = ENG_US    SPANISH = SPA_MX   <-- the TEXT map,
 *                                                              NOT touched here
 *   []         AUDIO_LANGUAGE = ENG_US    <-- THE AUDIO language
 *   [AUDIO_LANGUAGES] ENGLISH = eng       SPANISH = mex      <-- 'mex' has no
 *                                                              file in the OBB
 *
 * The OBB's audio/speech folder holds ENG_US.mp3, SPA_ES.mp3, GER_DE.mp3,
 * ITA_IT.mp3 and FRE_FR.mp3 (checked in the container's 1217-entry table), so
 * the Spanish commentators exist and nothing but this key was pointing the
 * engine at English. The keys are rewritten here, ON THE CARD, and
 * is_user_override() keeps the container's copy from being re-extracted over
 * them on later runs.
 *
 * WHAT IS DELIBERATELY NOT TOUCHED: LANG_OVERRIDE and DEFAULT_TEXT_LANGUAGE.
 * The text language already resolves to Spanish (the log's SPA_ES.loc) and the
 * brief for this build is explicit: the text is not to be changed. Only the
 * AUDIO keys move.
 *
 * The patch is idempotent (a file that already has the wanted values is left
 * byte for byte alone) and logs both effective values, so the next log says
 * what the engine is being served. */
#define FIFA14_LOCALE_REL "ini/na/locale.ini"
#define FIFA14_LOCALE_MAX 65536

/* One `key = value` line of `text`, patched in place inside `buf` (which the
 * caller writes back). `want_key`: the key to find; `want_val`, when not NULL,
 * restricts the match to a line whose value is exactly that (so SPANISH = SPA_MX
 * in [TEXT_LANGUAGES] is never mistaken for SPANISH = mex in
 * [AUDIO_LANGUAGES]). Returns the number of lines changed. */
static int locale_patch_line(char *buf, const char *want_key, const char *want_val,
                             const char *new_val, char *seen, size_t seen_cap) {
  int changed = 0;
  char *p = buf;
  while (p && *p) {
    char *nl = strchr(p, '\n');
    char *end = nl ? nl : p + strlen(p);
    char *eq = memchr(p, '=', (size_t)(end - p));
    if (eq) {
      /* the key: the text before '=', blanks trimmed at BOTH ends */
      char *ks = p;
      while (ks < eq && (*ks == ' ' || *ks == '\t'))
        ks++;
      char *ke = eq;
      while (ke > ks && (ke[-1] == ' ' || ke[-1] == '\t'))
        ke--;
      /* the value token: after the '=', up to the next blank */
      char *vs = eq + 1;
      while (vs < end && (*vs == ' ' || *vs == '\t'))
        vs++;
      char *ve = vs;
      while (ve < end && *ve != ' ' && *ve != '\t' && *ve != '\r')
        ve++;
      const size_t kl = (size_t)(ke - ks), vl = (size_t)(ve - vs);
      if (kl == strlen(want_key) && !strncmp(ks, want_key, kl) &&
          (!want_val || (vl == strlen(want_val) && !strncmp(vs, want_val, vl)))) {
        char old[64];
        snprintf(old, sizeof old, "%.*s", (int)vl, vs);
        if (strcmp(old, new_val) != 0) {
          const size_t nl_len = strlen(new_val), rest = strlen(ve);
          memmove(vs + nl_len, ve, rest + 1); /* +1: the terminator */
          memcpy(vs, new_val, nl_len);
          changed++;
        }
        if (seen && seen[0] == 0)
          snprintf(seen, seen_cap, "%s = %s (was %s)", want_key, new_val, old);
      }
    }
    if (!nl)
      break;
    p = nl + 1;
  }
  return changed;
}

int fifa14_locale_patch(void) {
  char dst[EXT_DST_MAX];
  if (!g_lock_up || !g_ready) {
    debugPrintf("[locale] %s: the OBB is not open yet: the audio language was NOT patched\n",
                FIFA14_LOCALE_REL);
    return -1;
  }
  if (!ext_dst(dst, sizeof dst, FIFA14_LOCALE_REL)) {
    debugPrintf("[locale] the path for %s does not fit: not patched\n", FIFA14_LOCALE_REL);
    return -1;
  }
  /* on the card? if not, take it out of the container first */
  if (rt_file_size(dst) < 0) {
    mutexLock(&g_lock);
    int i = find_by_hash(fifa14_external_hash(FIFA14_LOCALE_REL));
    if (i < 0)
      i = find_by_hash(hash_lower(FIFA14_LOCALE_REL));
    const int ok = i >= 0 && extract_entry(dst, g_e[i].off, g_e[i].size);
    mutexUnlock(&g_lock);
    if (!ok) {
      debugPrintf("[locale] %s: neither on the card nor in the OBB: the commentators stay in "
                  "whatever language the file already picks\n",
                  FIFA14_LOCALE_REL);
      return -1;
    }
    debugPrintf("[locale] %s: taken out of the OBB for the patch\n", FIFA14_LOCALE_REL);
  }
  FILE *f = fopen(dst, "rb");
  if (!f) {
    debugPrintf("[locale] %s: cannot read it: not patched\n", FIFA14_LOCALE_REL);
    return -1;
  }
  static char buf[FIFA14_LOCALE_MAX];
  const size_t n = fread(buf, 1, sizeof buf - 1, f);
  fclose(f);
  buf[n] = 0;
  if (n == 0 || n >= sizeof buf - 1) {
    debugPrintf("[locale] %s: %u B is not a locale.ini the port can patch\n", FIFA14_LOCALE_REL,
                (unsigned)n);
    return -1;
  }

  char audio[96], spanish[96];
  audio[0] = spanish[0] = 0;
  /* The AUDIO language to serve: config.ini [audio] language (fifa14_config.c
   * has the note). "keep" (the default) is the port's own choice, SPA_ES --
   * data/audio/speech/SPA_ES.mp3 is the file the OBB really has. ENG_US is the
   * one other value worth a run: MEASURED in libFIFA14.so, the engine's speech
   * system reads this very key when it initialises its banks (the init at
   * 0x7e6f1c loads "AUDIO_LANGUAGE" -- 0x10196a4 -- at 0x7e6f88 and 0x7e70f8)
   * and the four voice file names it asks the engine for are hardcoded ENG_US.
   * A value the port does not know is ignored (the row's own choices are
   * parsed by the ini engine; this is the belt to that braces). */
  const char *want_audio = rt_config_get("audio", "language");
  if (!want_audio || !*want_audio || !strcmp(want_audio, "keep"))
    want_audio = "SPA_ES";
  else if (strcmp(want_audio, "SPA_ES") && strcmp(want_audio, "ENG_US")) {
    debugPrintf("[locale] config.ini [audio] language is \"%s\": not a language this port knows "
                "(keep, SPA_ES, ENG_US), serving SPA_ES\n",
                want_audio);
    want_audio = "SPA_ES";
  }
  debugPrintf("[locale] the audio language this run wants: AUDIO_LANGUAGE = %s (config.ini [audio] "
              "language = %s)\n",
              want_audio, rt_config_get("audio", "language"));
  /* 1) AUDIO_LANGUAGE = <want_audio> (SPA_ES: the file the OBB really has) */
  int changed = locale_patch_line(buf, "AUDIO_LANGUAGE", NULL, want_audio, audio, sizeof audio);
  /* 2) the [AUDIO_LANGUAGES] entry that pointed at a file which does not exist
   *    ('mex'): SPANISH = mex -> SPANISH = <want_audio>. The [TEXT_LANGUAGES]
   *    entry (SPANISH = SPA_MX) is matched by its value and left alone. */
  changed += locale_patch_line(buf, "SPANISH", "mex", want_audio, spanish, sizeof spanish);

  if (changed) {
    FILE *w = fopen(dst, "wb");
    if (!w || fwrite(buf, 1, strlen(buf), w) != strlen(buf) || fclose(w) != 0) {
      if (w)
        fclose(w);
      debugPrintf("[locale] %s: the patch could not be written back (the file is unchanged on "
                  "the card, or half written)\n",
                  FIFA14_LOCALE_REL);
      return -1;
    }
    debugPrintf("[locale] %s PATCHED on the card (%d line(s)): %s; %s\n", FIFA14_LOCALE_REL,
                changed, audio[0] ? audio : "(AUDIO_LANGUAGE not found)",
                spanish[0] ? spanish : "(the AUDIO_LANGUAGES SPANISH = mex entry was not found)");
  } else {
    debugPrintf("[locale] %s: already patched, left byte for byte alone: %s; %s\n",
                FIFA14_LOCALE_REL, audio[0] ? audio : "(AUDIO_LANGUAGE not found)",
                spanish[0] ? spanish : "(the AUDIO_LANGUAGES SPANISH entry was not found)");
  }
  /* The effective values, always: the log then says what the engine is served
   * without having to read the card. */
  debugPrintf("[locale] the audio language served: AUDIO_LANGUAGE -> %s, AUDIO_LANGUAGES SPANISH "
              "-> %s (%s). The TEXT language is untouched (LANG_OVERRIDE/DEFAULT_TEXT_LANGUAGE as "
              "the OBB has them)\n",
              audio[0] ? audio : "?", spanish[0] ? spanish : "?",
              dst);
  return changed;
}

/* ================ ANY external file, ready for the port to read or patch ====
 * (fifa14_ctlcfg.c's two files: the game's own control scheme.) The path is
 * <game>/external/<rel>; when it is not on the card yet, its entry is taken
 * out of the OBB first -- exactly the dance fifa14_locale_patch() does, so a
 * caller that wants to WRITE a file the engine will later read does not have
 * to know about the container. 0 on success (dst holds the path), -1 otherwise
 * (logged: the table is not up, the path does not fit, or the file is neither
 * on the card nor in the OBB). */
int fifa14_external_prepare(const char *rel, char *dst, size_t cap) {
  if (!g_lock_up || !g_ready) {
    debugPrintf("[obb] %s: the OBB is not open yet: it can neither be patched nor served\n", rel);
    return -1;
  }
  if (!ext_dst(dst, cap, rel))
    return -1;
  if (rt_file_size(dst) >= 0)
    return 0; /* on the card already */
  mutexLock(&g_lock);
  int i = find_by_hash(fifa14_external_hash(rel));
  if (i < 0)
    i = find_by_hash(hash_lower(rel));
  const int ok = i >= 0 && extract_entry(dst, g_e[i].off, g_e[i].size);
  mutexUnlock(&g_lock);
  if (!ok) {
    debugPrintf("[obb] %s: neither on the card nor in the OBB\n", rel);
    return -1;
  }
  debugPrintf("[obb] %s: taken out of the OBB for the port to read\n", rel);
  return 0;
}

/* The same path WITHOUT taking anything out of the container: for a watcher
 * that only wants to stat() a file that may not be there yet. 1 when the path
 * fits (dst filled). */
int fifa14_external_path(const char *rel, char *dst, size_t cap) {
  if (!g_lock_up || !g_ready)
    return 0;
  return ext_dst(dst, cap, rel);
}
