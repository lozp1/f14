/* fifa14_ctlcfg.c -- the game's CONTROL SCHEME, served and measured.
 * (fifa14_ctlcfg.h has the measured chain: which keys the engine reads, the
 * 12-file ini list inside the .so, and the blocks the OBB ships.)
 *
 * WHAT THIS FILE DOES, and what it does NOT:
 *
 *   1. PATCH  (<game>/external/ini/na/controls.ini and .../common.ini): the
 *      values config.ini's [controls] asks for, written into the very lines the
 *      engine loads. Same mechanism as the audio language
 *      (fifa14_locale_patch): the file is taken out of the OBB if it is not on
 *      the card yet, patched in place, line by line, and both files are in
 *      is_user_override() so the container's copy is never extracted over the
 *      patch. "keep" (the default of every row) writes nothing at all.
 *
 *   2. LOG the effective values, twice:
 *        [ctlcfg] served   -- read back from the two files on the card: what
 *                             the engine is being handed this run.
 *        [ctlcfg] engine   -- asked of libFIFA14.so's own readers: what the
 *                             engine RESOLVED once it had loaded them.
 *
 *   3. NOT hide anything by itself. The keys in these two files are the
 *      game's own options (Floating Joystick, Large Buttons, Large Joystick,
 *      Left Handed, Alternate Controls, Control Scheme, LAYOUT). Whether any
 *      of them removes the on-screen buttons is exactly what the log is for:
 *      set one, read the run. A round that finds they only resize/move them
 *      has to go to the draw path (the port's notes say where).
 *
 * MIT.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <switch.h>

#include "fifa14_ctlcfg.h"
#include "dcr_path.h"
#include "fifa14_external.h"
#include "rt_cfg.h"
#include "so_util.h"
#include "util.h"

/* The module the engine is in (fifa14_boot.c loads it), for the symbol lookup
 * the engine read below needs. */
extern so_module game_mod;

/* ------------------------------------------------------------------ files */
#define CTL_INI_CONTROLS "ini/na/controls.ini" /* [CONTROL_CONFIG] */
#define CTL_INI_COMMON   "ini/na/common.ini"   /* [SETTINGS] */
/* controls.ini is 2,700 B and common.ini 14,447 B as the OBB ships them; the
 * buffer is this big so a file the player has edited (or one the engine has
 * grown) is still patched, and so a file that does NOT fit is refused rather
 * than half read. */
#define CTL_FILE_MAX (192u * 1024u)
#define CTL_DST_MAX  (DCR_PATH_MAX + 320)

/* One lever: a config.ini [controls] row, the file it moves and the line in
 * it. scheme_id moves BOTH player slots (the game has one per player). */
typedef struct {
  const char *cfg_key; /* config.ini [controls] key */
  const char *rel;     /* the ini, container-relative */
  const char *section; /* its [section] */
  const char *key;     /* the line */
} CtlPatch;

static const CtlPatch k_patch[] = {
    {"layout_id", CTL_INI_CONTROLS, "CONTROL_CONFIG", "LAYOUT_ID"},
    {"scheme_id", CTL_INI_CONTROLS, "CONTROL_CONFIG", "SCHEME_ID_P0"},
    {"scheme_id", CTL_INI_CONTROLS, "CONTROL_CONFIG", "SCHEME_ID_P1"},
    {"render_debug", CTL_INI_CONTROLS, "CONTROL_CONFIG", "RENDER_DEBUG"},
    {"big_buttons", CTL_INI_COMMON, "SETTINGS", "BIG_BUTTONS"},
    {"big_dpad", CTL_INI_COMMON, "SETTINGS", "BIG_DPAD"},
    {"floating_dpad", CTL_INI_COMMON, "SETTINGS", "FLOATING_DPAD"},
    {"alternate_controls", CTL_INI_COMMON, "SETTINGS", "ALTERNATE_CONTROLS"},
    {"basic_hand", CTL_INI_COMMON, "SETTINGS", "BASIC_HAND"},
};
#define CTL_PATCH_COUNT (int)(sizeof k_patch / sizeof k_patch[0])

/* What the log names, per file: every control-scheme key the port knows, the
 * ones it patches first (a value it did not move is just as interesting). */
static const char *const k_keys_controls_ini[] = {
    "SCHEME_ID_P0", "SCHEME_ID_P1", "LAYOUT_ID", "RENDER_DEBUG", "RENDER_DEBUG_SETPIECE",
    "USE_SIDE_BUTTONS",
};
static const char *const k_keys_common_ini[] = {
    "FLOATING_DPAD",          "BASIC_HAND",       "BIG_BUTTONS",
    "BIG_DPAD",               "ALTERNATE_CONTROLS", "ACTION_TAKEN_TEXT_ENABLED",
    "ASSISTED_TACKLE",        "FIRST_TOUCH",      "TUTORIAL_DPAD",
    "TUTORIAL_TOUCH",         "AUTO_SWITCH_LEVEL0", "AUTO_SWITCH_LEVEL1",
};

/* ------------------------------------------------------------- tiny helpers */

/* The section a line opens, or NULL. Only a real header counts: a commented
 * one ("// [SETTINGS]") does not. */
static const char *line_section(char *line, size_t len, char *out, size_t cap) {
  const char *p = line;
  const char *end = line + len;
  while (p < end && (*p == ' ' || *p == '\t'))
    p++;
  if (*p != '[')
    return NULL;
  const char *close = memchr(p, ']', (size_t)(end - p));
  if (!close)
    return NULL;
  const size_t n = (size_t)(close - p - 1);
  if (n + 1 > cap)
    return NULL;
  memcpy(out, p + 1, n);
  out[n] = 0;
  return out;
}

/* The `key = value` line the pointer sits on, when it is a real assignment in
 * the given section: writes the value token's bounds into vs/ve (the caller
 * owns the buffer) and returns 1. A commented line is never an assignment. */
static int line_key_value(char *line, size_t len, const char *want_section,
                          const char *want_key, char **vs, char **ve) {
  char *p = line;
  char *end = line + len;
  while (p < end && (*p == ' ' || *p == '\t'))
    p++;
  if (p + 1 < end && p[0] == '/' && p[1] == '/')
    return 0; /* commented out */
  if (*p == ';')
    return 0;
  char *eq = memchr(p, '=', (size_t)(end - p));
  if (!eq)
    return 0;
  /* the section this line belongs to is the caller's business (it walks the
   * buffer); here only the key and the value are extracted. */
  (void)want_section;
  char *ks = p;
  char *ke = eq;
  while (ke > ks && (ke[-1] == ' ' || ke[-1] == '\t'))
    ke--;
  const size_t kl = (size_t)(ke - ks);
  if (kl != strlen(want_key) || strncmp(ks, want_key, kl) != 0)
    return 0;
  char *v = eq + 1;
  while (v < end && (*v == ' ' || *v == '\t'))
    v++;
  char *e = v;
  while (e < end && *e != ' ' && *e != '\t' && *e != '\r')
    e++;
  *vs = v;
  *ve = e;
  return 1;
}

/* Read a file into `buf` (NUL-terminated). Returns its size, or -1. */
static long read_file(const char *path, char *buf, size_t cap) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return -1;
  const size_t n = fread(buf, 1, cap - 1, f);
  fclose(f);
  if (n == 0 || n >= cap - 1) {
    debugPrintf("[ctlcfg] %s: %u B does not look like one of the game's ini files\n", path,
                (unsigned)n);
    return -1;
  }
  buf[n] = 0;
  return (long)n;
}

/* The value of `key` inside `[section]`, as the file has it, into `out`.
 * Returns 1 (and also reports the section's presence in *found_section when
 * that pointer is given). */
static int value_in_section(const char *buf, const char *section, const char *key, char *out,
                            size_t cap) {
  char cur[64];
  cur[0] = 0;
  const char *p = buf;
  while (p && *p) {
    const char *nl = strchr(p, '\n');
    const size_t len = nl ? (size_t)(nl - p) : strlen(p);
    char line[512];
    const size_t n = len < sizeof line ? len : sizeof line - 1;
    memcpy(line, p, n);
    line[n] = 0;
    char sec[64];
    if (line_section(line, n, sec, sizeof sec))
      snprintf(cur, sizeof cur, "%s", sec);
    else if (!strcmp(cur, section)) {
      char *vs, *ve;
      if (line_key_value(line, n, section, key, &vs, &ve)) {
        snprintf(out, cap, "%.*s", (int)(ve - vs), vs);
        return 1;
      }
    }
    if (!nl)
      break;
    p = nl + 1;
  }
  out[0] = 0;
  return 0;
}

/* Replace `key`'s value inside `[section]` with `val`, in place. Returns the
 * number of lines changed (0 or 1: the keys are unique in their section). */
static int patch_in_section(char *buf, const char *section, const char *key, const char *val) {
  char cur[64];
  cur[0] = 0;
  char *p = buf;
  while (p && *p) {
    char *nl = strchr(p, '\n');
    const size_t len = nl ? (size_t)(nl - p) : strlen(p);
    char sec[64];
    if (line_section(p, len, sec, sizeof sec)) {
      snprintf(cur, sizeof cur, "%s", sec);
    } else if (!strcmp(cur, section)) {
      char *vs, *ve;
      if (line_key_value(p, len, section, key, &vs, &ve)) {
        char old[64];
        snprintf(old, sizeof old, "%.*s", (int)(ve - vs), vs);
        if (!strcmp(old, val))
          return 0; /* already what the file should say */
        const size_t vl = strlen(val);
        const size_t rest = strlen(ve); /* to the end of the buffer */
        memmove(vs + vl, ve, rest + 1);
        memcpy(vs, val, vl);
        return 1;
      }
    }
    if (!nl)
      break;
    p = nl + 1;
  }
  return 0;
}

/* --------------------------------------------------------------- the lever */

/* The config.ini value for one row, validated: "" when the row says "keep" (or
 * has a value this port does not know), which leaves the file alone. */
static const char *cfg_want(const CtlPatch *p, const char *const allowed[], int n_allowed) {
  const char *v = rt_config_get("controls", p->cfg_key);
  if (!v || !*v)
    return "";
  if (!strcmp(v, "keep"))
    return "";
  for (int i = 0; i < n_allowed; i++)
    if (!strcmp(v, allowed[i]))
      return v;
  debugPrintf("[ctlcfg] config.ini [controls] %s = \"%s\" is not a value this port knows: "
              "left alone (the row's own choices are the file's)\n",
              p->cfg_key, v);
  return "";
}

int fifa14_ctlcfg_patch(void) {
  static char buf[CTL_FILE_MAX];
  static const char *const k_01[] = {"0", "1"};
  static const char *const k_lay[] = {"0", "1", "2", "3"};
  int changed = 0, tried = 0;

  for (int i = 0; i < 2; i++) {
    const char *rel = i ? CTL_INI_COMMON : CTL_INI_CONTROLS;
    char dst[CTL_DST_MAX];
    if (fifa14_external_prepare(rel, dst, sizeof dst) != 0)
      continue; /* logged by the callee */
    if (read_file(dst, buf, sizeof buf) < 0)
      continue;
    int file_changed = 0;
    for (int k = 0; k < CTL_PATCH_COUNT; k++) {
      const CtlPatch *p = &k_patch[k];
      if (strcmp(p->rel, rel) != 0)
        continue;
      const int is_layout = !strcmp(p->key, "LAYOUT_ID");
      const char *want = cfg_want(p, is_layout ? k_lay : k_01, is_layout ? 4 : 2);
      tried++;
      if (!*want)
        continue; /* "keep": the game's own value stays */
      const int n = patch_in_section(buf, p->section, p->key, want);
      if (n) {
        file_changed += n;
        debugPrintf("[ctlcfg] %s: [%s] %s -> %s (config.ini [controls] %s)\n", rel, p->section,
                    p->key, want, p->cfg_key);
      }
    }
    if (!file_changed) {
      debugPrintf("[ctlcfg] %s: nothing to change (the rows that name it are \"keep\" or already "
                  "say it): left byte for byte alone\n",
                  rel);
      continue;
    }
    FILE *f = fopen(dst, "wb");
    if (!f || fwrite(buf, 1, strlen(buf), f) != strlen(buf) || fclose(f) != 0) {
      if (f)
        fclose(f);
      debugPrintf("[ctlcfg] %s: the patch could not be written back -- the file is unchanged "
                  "on the card, or half written\n",
                  rel);
      continue;
    }
    changed += file_changed;
    debugPrintf("[ctlcfg] %s PATCHED on the card (%d line(s))\n", rel, file_changed);
  }
  debugPrintf("[ctlcfg] the game's control-scheme files: %s (default \"keep\" leaves both for "
              "the game)\n",
              changed ? "written" : "nothing to write");
  return tried ? changed : -1;
}

/* ------------------------------------------------------------- the report */

int fifa14_ctlcfg_report(const char *why) {
  static char buf[CTL_FILE_MAX];
  int found = 0;
  debugPrintf("[ctlcfg] the game's control scheme, as served (%s):\n", why ? why : "?");
  for (int i = 0; i < 2; i++) {
    const int common = i == 1;
    const char *rel = common ? CTL_INI_COMMON : CTL_INI_CONTROLS;
    const char *section = common ? "SETTINGS" : "CONTROL_CONFIG";
    const char *const *keys = common ? k_keys_common_ini : k_keys_controls_ini;
    const int nkeys = (int)(common ? sizeof k_keys_common_ini / sizeof k_keys_common_ini[0]
                                   : sizeof k_keys_controls_ini / sizeof k_keys_controls_ini[0]);
    char dst[CTL_DST_MAX];
    if (fifa14_external_prepare(rel, dst, sizeof dst) != 0) {
      debugPrintf("[ctlcfg]   %s: not on the card and not in the OBB: the engine has whatever it "
                  "already had\n",
                  rel);
      continue;
    }
    if (read_file(dst, buf, sizeof buf) < 0)
      continue;
    for (int k = 0; k < nkeys; k++) {
      char v[64];
      if (value_in_section(buf, section, keys[k], v, sizeof v)) {
        debugPrintf("[ctlcfg]   %s [%s] %s = %s\n", rel, section, keys[k], v);
        found++;
      } else {
        debugPrintf("[ctlcfg]   %s [%s] %s: not in the file\n", rel, section, keys[k]);
      }
    }
    /* the file's own stamp, so a change between two reports is visible */
    struct stat st;
    if (stat(dst, &st) == 0)
      debugPrintf("[ctlcfg]   %s: %ld B, mtime %ld\n", rel, (long)st.st_size, (long)st.st_mtime);
  }
  return found;
}

/* ----------------------------------------------------- the engine's answer */
/* libFIFA14.so's own readers, reached from an exported symbol by the offset
 * measured in the file:
 *
 *   0x33ec1c  Java_com_ea_game_fifa14_Fifa14Activity_getMusicOptions  (exported)
 *   0x340acc  bool <registry>::Has(const char* key)      offset +0x1eb0
 *   0x340648  int  <registry>::GetInt(const char* key)   offset +0x1a2c
 *
 * Both are called ONLY after their first two instruction words have been read
 * back from the mapped image and matched against the file's own (a wrong
 * address would otherwise be a call into the dark):
 *
 *   0x340acc: e92d4830 push {r4,r5,fp,lr} ; e1a04000 mov r4,r0
 *   0x340648: e92d4070 push {r4,r5,r6,lr} ; e1a04000 mov r4,r0
 *
 * GetInt takes the key and returns the value (0 when the key is not there or
 * is 0 -- which is why Has() is asked too), Has returns 1/0. */
#define CTL_SYM_ANCHOR "Java_com_ea_game_fifa14_Fifa14Activity_getMusicOptions"
#define CTL_OFF_HAS    0x1eb0u
#define CTL_OFF_GET    0x1a2cu
static const uint32_t k_pro_has[2] = {0xe92d4830u, 0xe1a04000u};
static const uint32_t k_pro_get[2] = {0xe92d4070u, 0xe1a04000u};

static int (*s_get_int)(const char *key);
static int (*s_has)(const char *key);

/* Every key the port asks about: the engine's own runtime names first (what
 * its control code reads), then the ini's names (what the files write), then
 * the two the speech question turned up (fifa14_ctlcfg.h / the round's notes:
 * the audio init's "PRACTICE_MODE" gate). */
static const char *const k_engine_keys[] = {
    "CONTROL_CONFIG/LAYOUT_ID",  "CONTROL_CONFIG/SCHEME_ID_P0",   "CONTROL_CONFIG/SCHEME_ID_P1",
    "CONTROL_CONFIG/SCHEME_ID_LOCAL", "CONTROL_CONFIG/BIG_BUTTONS", "CONTROL_CONFIG/BIG_DPAD",
    "CONTROL_CONFIG/FLOATING_DPAD", "CONTROL_CONFIG/ALTERNATE_CONTROLS",
    "CONTROL_CONFIG/BASIC_HAND", "SETTINGS/BIG_BUTTONS",          "SETTINGS/BIG_DPAD",
    "SETTINGS/FLOATING_DPAD",    "SETTINGS/ALTERNATE_CONTROLS",   "SETTINGS/BASIC_HAND",
    "PRACTICE_MODE",             "SPEECH",                        "CHANTS",
};
#define CTL_ENGINE_KEY_COUNT (int)(sizeof k_engine_keys / sizeof k_engine_keys[0])

static int readers_ready(void) {
  static int tried, ok;
  if (tried)
    return ok;
  tried = 1;
  const uintptr_t anchor = so_try_find_addr_rx(&game_mod, CTL_SYM_ANCHOR);
  if (!anchor) {
    debugPrintf("[ctlcfg] the engine's own readers: the anchor symbol %s did not resolve: the "
                "registry is NOT asked\n",
                CTL_SYM_ANCHOR);
    return 0;
  }
  struct {
    const char *what;
    uintptr_t fn;
    const uint32_t *pro;
  } r[2] = {
      {"Has", anchor + CTL_OFF_HAS, k_pro_has},
      {"GetInt", anchor + CTL_OFF_GET, k_pro_get},
  };
  uintptr_t got[2];
  for (int i = 0; i < 2; i++) {
    const Elf32_Phdr *seg = so_segment_of(&game_mod, r[i].fn);
    if (!seg || !(seg->p_flags & PF_X)) {
      debugPrintf("[ctlcfg] the engine's own readers: %s at 0x%lx is not in an executable "
                  "segment: the registry is NOT asked\n",
                  r[i].what, (unsigned long)r[i].fn);
      return 0;
    }
    const uint32_t *w = (const uint32_t *)r[i].fn;
    if (w[0] != r[i].pro[0] || w[1] != r[i].pro[1]) {
      debugPrintf("[ctlcfg] the engine's own readers: at 0x%lx the %s reader is 0x%08lx 0x%08lx, "
                  "not the measured 0x%08lx 0x%08lx: the registry is NOT asked (a different build "
                  "of the .so?)\n",
                  (unsigned long)r[i].fn, r[i].what, (unsigned long)w[0], (unsigned long)w[1],
                  (unsigned long)r[i].pro[0], (unsigned long)r[i].pro[1]);
      return 0;
    }
    got[i] = r[i].fn;
  }
  s_has = (int (*)(const char *))got[0];
  s_get_int = (int (*)(const char *))got[1];
  debugPrintf("[ctlcfg] the engine's own readers are in hand: Has 0x%lx, GetInt 0x%lx (both "
              "verified by their prologue words)\n",
              (unsigned long)got[0], (unsigned long)got[1]);
  ok = 1;
  return 1;
}

void fifa14_ctlcfg_engine_report(void) {
  if (!readers_ready())
    return;
  debugPrintf("[ctlcfg] the engine's OWN registry (libFIFA14.so's reader, this run):\n");
  for (int i = 0; i < CTL_ENGINE_KEY_COUNT; i++)
    debugPrintf("[ctlcfg]   %-32s has %d  value %d\n", k_engine_keys[i],
                s_has(k_engine_keys[i]), s_get_int(k_engine_keys[i]));
}

/* ------------------------------------------------------------- the frame */

/* The engine's config is loaded by its own init, so the first ask waits for it
 * (the boot's step 8 is frame 1: 600 is a few seconds in). The cadence is slow
 * on purpose: the values only change when something changes them. */
#define CTL_ENGINE_FIRST 600ull
#define CTL_ENGINE_EVERY 1800ull
#define CTL_FILE_EVERY   300ull

static struct {
  const char *rel;
  long size, mtime;
  int known;
} s_watch[2] = {{CTL_INI_CONTROLS, -2, -2, 0}, {CTL_INI_COMMON, -2, -2, 0}};

void fifa14_ctlcfg_frame(unsigned long long frames) {
  static unsigned long long last_engine;
  if (frames >= CTL_ENGINE_FIRST && frames - last_engine >= CTL_ENGINE_EVERY) {
    last_engine = frames;
    /* [controls] log_engine_config (fifa14_config.c): the default is true --
     * the engine's own answer is the point of the instrument -- and false is
     * the one lever that takes the only call into the engine's registry away. */
    if (rt_config_bool("controls", "log_engine_config"))
      fifa14_ctlcfg_engine_report();
    else
      debugPrintf("[ctlcfg] the engine's registry is NOT asked (config.ini [controls] "
                  "log_engine_config = false)\n");
  }
  static unsigned long long last_file;
  if (frames - last_file < CTL_FILE_EVERY)
    return;
  last_file = frames;
  for (int i = 0; i < 2; i++) {
    char dst[CTL_DST_MAX];
    struct stat st;
    if (!fifa14_external_path(s_watch[i].rel, dst, sizeof dst) || stat(dst, &st) != 0)
      continue; /* not on the card (yet): nothing to watch */
    const long size = (long)st.st_size, mtime = (long)st.st_mtime;
    if (!s_watch[i].known) {
      s_watch[i].known = 1;
      s_watch[i].size = size;
      s_watch[i].mtime = mtime;
      continue;
    }
    if (size == s_watch[i].size && mtime == s_watch[i].mtime)
      continue;
    debugPrintf("[ctlcfg] %s CHANGED while the game ran (%ld -> %ld B, mtime %ld -> %ld): "
                "something rewrote it (the game's own settings save?)\n",
                s_watch[i].rel, s_watch[i].size, size, s_watch[i].mtime, mtime);
    s_watch[i].size = size;
    s_watch[i].mtime = mtime;
    fifa14_ctlcfg_report("the file changed while the game ran");
  }
}
