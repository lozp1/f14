/* fifa14_assets.c -- serving the APK's assets/ to the game's own file layer.
 *
 * THE FAILURE THIS FIXES (hardware, first console run): a data abort at
 *   FileStream::Open+0x1dc  (EA::IO::FileStream::Open)
 *   lr AssetStream::CreateMemoryStreamFromFile+0x9c
 *   ldr r1,[r0] with r0 = 0  -- the open FAILED and the NULL stream was
 *   dereferenced to death.
 * The stack dump carries the path the game was opening: "assets/" +
 * "assets/adview.html" and "assets/adview.js", two of the 19 entries in the
 * APK's assets/ that the port's index counts but never served.
 *
 * WHY IT WAS NOT SERVED: the engine opens its data by PATH, and dcr_path.c's
 * rule for an Android "assets/..." path is <game folder>/assets/... -- a file
 * that exists INSIDE the APK's zip, not on the SD card. A deflated entry (16 of
 * FIFA's 19) has no usable byte range on the card at all, so the rule could
 * only ever point at nothing: open() -> ENOENT -> NULL -> the abort above. The
 * JNI AssetManager in fifa14_blast_stubs.c (the engine's Java-side reads) did
 * inflate, but a FileStream/AssetStream open never goes through it.
 *
 * WHAT THIS DOES: on the FIRST open of a path inside assets/, the entry is
 * looked up in the index built at boot (name -> local header / sizes /
 * compression), read from the APK (the runtime's block cache,
 * dcr_apkcache_read), inflated with tinfl when it is deflated (STORE entries
 * are copied as they are) and written to <game folder>/assets/<name>
 * (rt_write_atomic: <dst>.part + rename, folders made). Only then is the
 * translated path returned. Every way the engine opens it afterwards -- open,
 * open64, openat, fopen, stat, access, even mmap of a real file -- finds an
 * ordinary file, and the NEXT runs find it already extracted (size checked
 * against the index, so a different APK re-extracts).
 *
 * The hook: port_path_fixup() is the last step of dcr_translate_path(), which
 * bionic_io.c (b_open/b_stat) and bionic_stdio.c (b_fopen) all call -- the one
 * place every path-based open of the 32-bit ABI passes through.
 *
 * EVERY open is logged with its result, and counted:
 *   [assets] open(adview.html) -> served 1517 B from the APK (deflated,
 *            inflated now) to sdmc:/switch/fifa14/assets/adview.html
 *   [assets] open(adview.html) -> already extracted (1517 B in ...)
 *   [assets] open(x.ini) -> NOT in the APK's assets/ (19 entries there; the
 *            game gets an open failure)
 *   [assets] open(x.js) -> ERROR: it is in the APK (9839 B) and could not be
 *            written to ...
 * plus, at the end of the run, fifa14_assets_report(): every distinct path the
 * game asked for, how many times, and what happened to it. A console run then
 * says by itself what was served and what was not.
 * MIT. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>

#include "config.h"
#include "dcr_path.h"
#include "dcr_setup.h"
#include "fifa14_assets.h"
#include "fifa14_external.h"
#include "util.h"

#define ASSETS_DIR "assets"

/* The served path is <game root> + "/assets/" + the name after "assets/",
 * and that name may use all of DCR_PATH_MAX (dcr_path.c's own buffer):
 * size the destination so it can never be cut short. A silently truncated
 * path would extract an entry to the WRONG file and the next open would read
 * somebody else's bytes -- gcc's -Wformat-truncation flags exactly this.
 * asset_dst() logs and refuses when a path does not fit. */
#define ASSET_DST_MAX (DCR_PATH_MAX + 320)
#define TRACK_NAME    (DCR_PATH_MAX + 32)

/* <game root>/assets/<rel> into `out` (cap >= ASSET_DST_MAX): 1 when it fit.
 * A path that does not fit is a bug worth seeing: it is logged and refused. */
static int asset_dst(char *out, size_t cap, const char *rel) {
  const int n = snprintf(out, cap, "%s/" ASSETS_DIR "/%s", dcr_game_root(), rel);
  if (n < 0 || (size_t)n >= cap) {
    debugPrintf("[assets] the destination path for \"%s\" does not fit in %u B: refusing it\n", rel,
                (unsigned)cap);
    out[0] = 0;
    return 0;
  }
  return 1;
}

/* The open lines one run may spend (the report has the rest): an asset a frame
 * loop asks for again and again must not flood debug.log. Past this, only a
 * path not seen before is logged. */
#define LOG_MAX 400
/* Distinct asset paths the report lists. */
#define TRACK_MAX 96

/* What an open did. */
enum {
  ST_NONE = 0,
  ST_SERVED,  /* taken from the APK now (inflated if it was deflated) */
  ST_CACHED,  /* already extracted by an earlier open (this run or another) */
  ST_MISSING, /* not in the APK's assets/ -- the open fails, as before */
  ST_FAILED,  /* in the APK but could not be read / written */
  ST_EARLY,   /* asked before the index was built */
};

typedef struct {
  char name[TRACK_NAME]; /* the path after "assets/", as the game asked for it */
  unsigned requests;
  int last;
} Track;

static Track g_track[TRACK_MAX];
static int g_ntrack;
static unsigned g_requests, g_served, g_cached, g_missing, g_failed, g_early;
static int g_logged;
static int g_ready; /* the index is built: an asset may be served */
static Mutex g_lock;
static int g_lock_up;

/* The asset the open being logged is about (set by serve_locked). */
static uint32_t g_cur_size, g_cur_comp;
static int g_cur_stored;
static char g_cur_dst[ASSET_DST_MAX];

void fifa14_asset_log_init(void) {
  mutexInit(&g_lock);
  g_lock_up = 1;
}

void fifa14_asset_index_ready(void) { g_ready = 1; }

/* ---------------------------------------------------------------- paths ---
 * The name after "assets/", when `real` is a path into it: the translated form
 * <game folder>/assets/... first (what dcr_path.c makes of "assets/..."), then
 * the forms it LEAVES ALONE -- EA::IO asks for assets with "assets:/" and
 * "appbundle:/assets/", whose ':' has no separator for dcr_path.c's rules, so
 * they reach here untranslated (and unopenable). */
static int asset_rel(const char *real, char *out, size_t cap) {
  if (!real || !real[0])
    return 0;
  const char *p = real;
  while (!strncmp(p, "file://", 7))
    p += 7;
  const char *root = dcr_game_root();
  const size_t rl = strlen(root);
  if (rl && !strncmp(p, root, rl)) {
    const char *t = p + rl;
    if (strncmp(t, "/" ASSETS_DIR "/", sizeof("/" ASSETS_DIR "/") - 1) != 0)
      return 0; /* the folder itself, or another folder of the game's */
    p = t + sizeof("/" ASSETS_DIR "/") - 1;
  } else if (!strncmp(p, "appbundle:/", 11)) {
    p += 11;
    if (!strncmp(p, ASSETS_DIR "/", sizeof(ASSETS_DIR "/") - 1))
      p += sizeof(ASSETS_DIR "/") - 1;
  } else if (!strncmp(p, ASSETS_DIR ":/", sizeof(ASSETS_DIR ":/") - 1)) {
    p += sizeof(ASSETS_DIR ":/") - 1;
  } else if (!strncmp(p, "/" ASSETS_DIR "/", sizeof("/" ASSETS_DIR "/") - 1)) {
    p += sizeof("/" ASSETS_DIR "/") - 1;
  } else if (!strncmp(p, "./" ASSETS_DIR "/", sizeof("./" ASSETS_DIR "/") - 1)) {
    p += sizeof("./" ASSETS_DIR "/") - 1;
  } else {
    return 0;
  }
  while (*p == '/')
    p++;
  if (!*p)
    return 0;
  snprintf(out, cap, "%s", p);
  return 1;
}

/* ---------------------------------------------------------------- the work */
/* The APK entry -> <game folder>/assets/<name>, once. The caller holds g_lock
 * (two threads extracting the same entry would share rt_write_atomic's .part). */
static int serve_locked(const char *rel) {
  uint32_t size = 0, comp = 0;
  int stored = 0;

  g_requests++;
  g_cur_size = g_cur_comp = 0;
  g_cur_stored = 0;
  g_cur_dst[0] = 0;
  if (!g_ready) {
    g_early++;
    return ST_EARLY;
  }
  if (!fifa14_asset_info(rel, &size, &comp, &stored)) {
    g_missing++;
    return ST_MISSING;
  }
  g_cur_size = size;
  g_cur_comp = comp;
  g_cur_stored = stored;
  asset_dst(g_cur_dst, sizeof g_cur_dst, rel);
  if (!g_cur_dst[0]) {
    /* The path did not fit (asset_dst() logged it): writing the entry to a
     * cut-short path would put it where nothing looks for it. */
    g_failed++;
    return ST_FAILED;
  }
  if (rt_file_size(g_cur_dst) == (long)size) {
    g_cached++;
    return ST_CACHED;
  }
  uint32_t got = 0;
  uint8_t *bytes = fifa14_asset_bytes(rel, &got);
  if (!bytes || got != size) {
    debugPrintf("[assets] open(%s): in the APK (%u B, %s) but its bytes did not come out "
                "(%u B%s)\n",
                rel, (unsigned)size, stored ? "stored" : "deflated", (unsigned)got,
                bytes ? "" : ", no memory or a bad entry");
    free(bytes);
    g_failed++;
    return ST_FAILED;
  }
  const int wrote = rt_write_atomic(g_cur_dst, bytes, got);
  free(bytes);
  if (!wrote) {
    g_failed++;
    return ST_FAILED;
  }
  g_served++;
  return ST_SERVED;
}

/* Which entry of the report's table `rel` is, adding it when it is new; -1 when
 * the table is full (the line is still logged). */
static int tracked(const char *rel) {
  for (int i = 0; i < g_ntrack; i++)
    if (!strcmp(g_track[i].name, rel))
      return i;
  if (g_ntrack >= TRACK_MAX)
    return -1;
  snprintf(g_track[g_ntrack].name, TRACK_NAME, "%s", rel);
  g_track[g_ntrack].requests = 0;
  g_track[g_ntrack].last = ST_NONE;
  return g_ntrack++;
}

static void note_locked(const char *rel, int st) {
  const int i = tracked(rel);
  const int first = i >= 0 && g_track[i].requests == 0;
  if (i >= 0) {
    g_track[i].requests++;
    g_track[i].last = st;
  }
  /* One line per open while the budget lasts, then only a path not seen before
   * (the report has the counts either way). */
  if (g_logged >= LOG_MAX && !first)
    return;
  g_logged++;
  switch (st) {
  case ST_SERVED:
    debugPrintf("[assets] open(%s) -> SERVED %u B from the APK (%s) to %s\n", rel,
                (unsigned)g_cur_size, g_cur_stored ? "stored" : "deflated, inflated now", g_cur_dst);
    break;
  case ST_CACHED:
    debugPrintf("[assets] open(%s) -> already extracted (%u B in %s)\n", rel, (unsigned)g_cur_size,
                g_cur_dst);
    break;
  case ST_MISSING:
    debugPrintf("[assets] open(%s) -> NOT in the APK's assets/ (%d entries there: this open "
                "fails)\n",
                rel, fifa14_assets_count());
    break;
  case ST_FAILED:
    debugPrintf("[assets] open(%s) -> ERROR: in the APK (%u B%s) and could not be written to %s\n",
                rel, (unsigned)g_cur_size, g_cur_stored ? "" : ", deflated", g_cur_dst);
    break;
  default:
    debugPrintf("[assets] open(%s) -> asked before the APK's assets/ was indexed: this open "
                "fails\n",
                rel);
    break;
  }
  if (g_logged == LOG_MAX)
    debugPrintf("[assets] %d asset open lines logged; a path not seen before is still logged\n",
                LOG_MAX);
}

/* --------------------------------------------------------------- the hook */
static const char *why(int st); /* the report's wording, below */

/* The last step of dcr_translate_path() (dcr_path.h): `real` is the path the
 * SD card would be asked for, and it may be `out` itself. Two halves, one
 * hook: the game's EXTERNAL storage (the OBB; fifa14_external.c) and the APK's
 * assets/ (fifa14_assets.c's index; a file inside a zip has no usable bytes on
 * the card). */
const char *port_path_fixup(const char *real, char *out, size_t cap) {
  char rel[DCR_PATH_MAX];
  char dst[ASSET_DST_MAX];
  if (!real || !out || !cap)
    return real;
  /* The external storage first: it is what the engine opens its data with
   * (<game folder>/external/data/...), and a path into assets/ is never one of
   * these, so the two hooks cannot both claim a path. */
  {
    const char *ext = fifa14_external_fixup(real, out, cap);
    if (ext != real)
      return ext;
  }
  if (!asset_rel(real, rel, sizeof rel))
    return real;
  if (!g_lock_up) {
    /* Before the index (an asset open before boot's fifa14_assets_init()):
     * nothing can be served, and the log has no lock yet -- say it, limited. */
    static int n;
    if (n < 8) {
      n++;
      debugPrintf("[assets] open(%s) -> the APK's assets/ is not indexed yet (this open fails)\n", rel);
    }
    return real;
  }
  if (!asset_dst(dst, sizeof dst, rel))
    return real; /* a path that does not fit must not be served: see asset_dst */
  mutexLock(&g_lock);
  const int st = serve_locked(rel);
  note_locked(rel, st);
  mutexUnlock(&g_lock);
  const int served = st == ST_SERVED || st == ST_CACHED;
  /* The ContentManager's speech config exists on the card too as soon as an
   * open extracted it, and an EA::IO::FileStream open of it would read THAT
   * file. Keep the two copies saying the same thing (fifa14_cm_config_patch()
   * has the measurement); once per process, and only when it is really there. */
  if (served && !strcmp(rel, "contentmanagerSpeech.cmconfig")) {
    static int done;
    if (!done) {
      done = 1;
      size_t len = 0;
      uint8_t *buf = rt_read_whole(dst, &len);
      if (buf) {
        if (fifa14_cm_config_patch(buf, (uint32_t)len) &&
            rt_write_if_changed(dst, buf, len))
          debugPrintf("[speech] the extracted copy on the card (%s, %u B) was rewritten the "
                      "same way, so an EA::IO::FileStream open and the AssetManager's "
                      "InputStream serve the same bytes\n",
                      dst, (unsigned)len);
        free(buf);
      }
    }
  }
  /* [fs] the assets hook's own line: which path the engine asked for, which
   * path the open is being sent to, and what the hook did about it. The
   * caller's buffer (DCR_PATH_MAX) is a contract: never return a path it
   * cannot hold. */
  if (served && strlen(dst) >= cap) {
    debugPrintf("[fs] assets  asked=\"%s\" real=\"%s\" status=%s NOT served: the path needs %u B "
                "and the caller's buffer is %u\n",
                rel, dst, why(st), (unsigned)strlen(dst), (unsigned)cap);
    return real;
  }
  debugPrintf("[fs] assets  asked=\"%s\" real=\"%s\" status=%s served=%d\n", rel, served ? dst : real,
              why(st), served);
  if (!strcmp(dst, real))
    return real; /* the ordinary translated path: exactly this file */
  snprintf(out, cap, "%s", dst);
  return out;
}

/* ============================================================================
 * THE CONTENT MANAGER'S SPEECH CONFIG, AS THE ENGINE READS IT (2026-10-13)
 *
 * WHAT THE CONSOLE LOG SAYS, and why the game's language screen kept offering
 * the commentator pack as a DOWNLOAD even with the pack's 476 MB on the card
 * and served ([obb] ... status=the player's own file ... served=1):
 *
 *   [assets] open("contentmanagerSpeech.cmconfig"): found in the index -- 901 B
 *   [fs] obb asked="Speech/HiRes.cmconfig" ... served=1        (the pack's own
 *                                                              description)
 *   ... the five data files, each stat'ed and served
 *   [fs] obb asked="Speech/timestamp.cmconfig" ... served=0     <- MISSING
 *   [fs] open .../Speech/HiRes.cmconfig.cmserver.cmdownload flags=0x242 rc=11
 *        (0x242 = O_RDWR|O_CREAT|O_TRUNC: EA::ContentManager::FileDownloader
 *         starting a DOWNLOAD of the description file)
 *
 * THE DECISION IS IN THE ENGINE, AND IT IS A CONFIG VALUE. Read instruction by
 * instruction, EA::ContentManager::FlowInitialization::GetRecommendedSyncAction
 * @0x00dd3a94 (arm-none-eabi-objdump over libFIFA14.so) is:
 *
 *   dd3aa8  GetContentDescFile(0); IsContentDescFileUsable(desc)
 *   dd3ac0  if (!usable)                            -> return 1   (sync)
 *   dd3ac8  if (!SyncList::IsSyncListEmpty(...))     -> return 1   (sync: files
 *                                                                  missing)
 *   dd3adc  r7 = Configuration+20                     <- updateCheckPeriodDays
 *   dd3ae0  if (r7 < 0)                              -> return 0   ** NO SYNC **
 *   dd3b9c  period_ns = r7 * 86400000000000 (days*86400*1e9: umull/mla over the
 *           64-bit constant 0x4E94914F0000, i.e. EA::StdC::GetTime()'s unit)
 *   dd3bbc  TimeStampUtils::IsTimeStampExpired(path, period_ns)
 *   dd3bf8  return expired ? 2 : 0                    (2 = a check is due)
 *
 * and the APK's own contentmanagerSpeech.cmconfig ships
 *
 *     "updateCheckPeriodDays":     0,   <- 0 and its own comment says
 *                                          "0 = always suggest an update check"
 *
 * so on every single run the flow is "a check is due" -> the CM goes to
 * serverUrl ("http://127.0.0.1" in this build: NO SERVER) -> the description
 * download fails -> the game reports SyncSpeechFailure -> the popup offers the
 * pack as a download. The timestamp file's absence (WriteTimeStamp @0xde17cc
 * writes 8 bytes = GetTime() into the path GetAbsLocalPath makes; the pack
 * never shipped one) is what makes IsTimeStampExpired answer 1 even before the
 * server is contacted.
 *
 * WHAT THE PORT DOES: it serves that one config with the value the game's own
 * comment documents for exactly this situation -- "-1 = unlimited", i.e. no
 * update check at all -- because this port has no EA server to check against,
 * and the pack's files are already on the card. The change is 1 byte for 1 byte
 * (-1 written over one space and the 0), so the file the engine measures with
 * openFd().getLength() is still its 901 bytes: nothing is forged, no hash or
 * size is invented, and the ONLY thing that changes is a value the game itself
 * documents. The APK on the card is not touched: this is the copy the port
 * serves to the engine's AssetManager (fifa14_blast_stubs.c).
 *
 * The function edits `bytes` IN PLACE and returns 1 when it changed something.
 * The original bytes are logged (before -> after) so one run's log proves the
 * port served the patched config and not the APK's.
 * ==========================================================================*/
#define CM_CONFIG_KEY "\"updateCheckPeriodDays\""
int fifa14_cm_config_patch(uint8_t *bytes, uint32_t size) {
  if (!bytes || size < 64)
    return 0;
  /* find the key */
  const size_t klen = sizeof(CM_CONFIG_KEY) - 1;
  size_t at = (size_t)-1;
  for (size_t i = 0; i + klen <= size; i++) {
    if (bytes[i] == '"' && !memcmp(bytes + i, CM_CONFIG_KEY, klen)) {
      at = i;
      break;
    }
  }
  if (at == (size_t)-1) {
    debugPrintf("[speech] the ContentManager's config: no %s in the %u B the engine reads: "
                "the port cannot say whether an update check is due (the game will ask for a "
                "server this port does not have)\n",
                CM_CONFIG_KEY, (unsigned)size);
    return 0;
  }
  size_t i = at + klen;
  while (i < size && bytes[i] != ':')
    i++;
  if (i >= size) {
    debugPrintf("[speech] the ContentManager's config: %s has no ':' after it\n", CM_CONFIG_KEY);
    return 0;
  }
  i++; /* the colon */
  size_t s = i;
  while (s < size && (bytes[s] == ' ' || bytes[s] == '\t'))
    s++;
  size_t e = s;
  while (e < size && ((bytes[e] >= '0' && bytes[e] <= '9') || bytes[e] == '-'))
    e++;
  const size_t len = e - s;
  if (len == 0) {
    debugPrintf("[speech] the ContentManager's config: %s's value is not a number\n",
                CM_CONFIG_KEY);
    return 0;
  }
  if (len == 1 && bytes[s] == '-' ) {
    debugPrintf("[speech] the ContentManager's config: %s is a lone '-'\n", CM_CONFIG_KEY);
    return 0;
  }
  if (len == 2 && bytes[s] == '-' && bytes[s + 1] == '1') {
    debugPrintf("[speech] the ContentManager's config already says -1 (%s, %u B): the engine "
                "reads it as \"no update check\" (the port's own copy)\n",
                CM_CONFIG_KEY, (unsigned)size);
    return 0;
  }
  if (len == 2) {
    bytes[s] = '-';
    bytes[s + 1] = '1';
  } else if (len == 1 && s > 0 && bytes[s - 1] == ' ') {
    /* one space shorter, so the 901 B the engine measured stay 901 B */
    bytes[s - 1] = '-';
    bytes[s] = '1';
  } else {
    debugPrintf("[speech] the ContentManager's config: %s is %u characters long and the port "
                "only rewrites 1 or 2 of them in place (the file must keep the %u B the "
                "engine measured with openFd): left as it is\n",
                CM_CONFIG_KEY, (unsigned)len, (unsigned)size);
    return 0;
  }
  debugPrintf("[speech] the ContentManager's config (contentmanagerSpeech.cmconfig, %u B): "
              "%s was a value the game reads as \"always suggest an update check\" (0) and is "
              "now -1, the value its own comment documents as \"unlimited\"; measured in "
              "FlowInitialization::GetRecommendedSyncAction @0x00dd3a94: a negative period "
              "makes it return 0 = NO SYNC, so the flow that needs EA's server (and fails: "
              "serverUrl is http://127.0.0.1) is never started, and the pack on the card is "
              "the pack the CM has\n",
              (unsigned)size, CM_CONFIG_KEY);
  return 1;
}

/* --------------------------------------------------------------- report */
static const char *why(int st) {
  switch (st) {
  case ST_SERVED:
    return "served from the APK now";
  case ST_CACHED:
    return "already extracted";
  case ST_MISSING:
    return "NOT in the APK (the open failed)";
  case ST_FAILED:
    return "IN the APK but could not be served";
  default:
    return "asked before the index";
  }
}

void fifa14_assets_report(void) {
  if (!g_lock_up || !g_requests)
    return;
  int in_apk = 0;
  for (int i = 0; i < g_ntrack; i++)
    if (g_track[i].last == ST_SERVED || g_track[i].last == ST_CACHED || g_track[i].last == ST_FAILED)
      in_apk++;
  debugPrintf("[assets] ==== the APK's assets/, opened by the game ====\n");
  debugPrintf("[assets] the index has %d entries; %d distinct path(s) were asked for: %u open(s), "
              "%u served now, %u already extracted, %u FAILED, %u NOT in the APK, %u before the "
              "index\n",
              fifa14_assets_count(), in_apk, g_requests, g_served, g_cached, g_failed, g_missing,
              g_early);
  for (int i = 0; i < g_ntrack; i++)
    debugPrintf("[assets]   %-56s %5u open(s)  %s\n", g_track[i].name, g_track[i].requests,
                why(g_track[i].last));
  if (g_ntrack >= TRACK_MAX)
    debugPrintf("[assets]   (only the first %d distinct paths are listed)\n", TRACK_MAX);
}
