/* fifa14_padact.c -- THE PAD'S ACTIONS IN A MATCH: the engine's own on-screen
 * touch buttons, pressed for the player.
 *
 * ==========================================================================
 * WHY THIS FILE EXISTS (the round's finding, in one paragraph)
 *
 * The console run of 2026-10-13 settled it: the pad's KEYS reach the engine
 * (PhysicalKeyboard enabled, "direct calls 136 = 68 down + 68 up") and the
 * player still does not move or shoot, because the MATCH is not fed by the
 * keyboard at all: its input is the TOUCH surface (the phone's on-screen
 * buttons), and the keyboard module's notifications have no listener in this
 * build (measured: EA::Blast::Keyboard::AddKeyListener @0xe0699c and
 * PhysicalKeyboard::AddKeyListener @0xdf1054 have ZERO callers in
 * libFIFA14.so -- the game registers nothing, so NotifyKey reaches nobody).
 * The match listens to touches: the port delivers them already (the same
 * native Android's Java view calls), so the pad's actions have to be TOUCHES.
 * This file presses the engine's own touch buttons.
 *
 * ==========================================================================
 * WHERE THE BUTTONS ARE (MEASURED: libFIFA14.so, 18,187,340 B, arm-none-eabi
 * disassembly of the hud/input band 0x6e0000..0x704fff; every instruction word
 * quoted here was read out of the file)
 *
 *   - The engine's touch handler is the function at 0x6e75bc, called with
 *     (game object, action, ?, ?, x, y) -- it first proves the point is on the
 *     surface:
 *
 *       6e75e4  mov r0, #276 ; orr r0, r0, #0xc800    ; 0xc914
 *       6e75ec  ldr r0, [r4, r0]     ; Game[0xc914] = the surface WIDTH
 *       6e75f0  cmp r6, r0 ; bgt    ...              ; x > width  -> out
 *       6e7600  mov r0, #280 ; orr r0, r0, #0xc800    ; 0xc918
 *       6e7608  ldr r0, [r4, r0]     ; Game[0xc918] = the surface HEIGHT
 *       6e760c  cmp r8, r0 ; bgt    ...              ; y > height -> out
 *
 *     and then, for action 0 (touch DOWN) calls 0x6e846c, for action 1 calls
 *     0x6e98f8 (a touch UP/MOVE path -- same rectangles, no hits at
 *     0x6e772c/0x6e7740/0x6e890c..). Those two are the button hit-tests, and
 *     they test the SAME eight rectangles, each a 16-byte {x, y, w, h} with a
 *     0x18 stride, inside the game object:
 *
 *       rect i = Game[0xAF58 + 0x18*i ..], fields (x, y, w, h) at +0/+4/+8/+0xC
 *
 *     e.g. the first one, instruction for instruction (0x6e84e8):
 *
 *       6e84e8  mov r0, #856 ; orr r0, r0, #0xac00   ; 0xaf58
 *       6e84f0  ldr r0, [r4, r0]     ; x0
 *       6e84f4  cmp r0, r6 ; bgt    miss              ; x0 > touch.x
 *       6e84fc  mov r1, #3936 ; orr r1, r1, #0xa000  ; 0xaf60
 *       6e8504  ldr r1, [r4, r1]     ; w0
 *       6e8508  add r0, r1, r0       ; x0 + w0
 *       6e850c  cmp r0, r6 ; ble    miss              ; x0+w0 <= touch.x
 *             (the same pair for y at 0xaf5c / 0xaf64, then the hit branch)
 *
 *     The eight rectangles are exactly the eight hit-tests of that handler,
 *     in this order:
 *       0: 0xaf58/0xaf5c/0xaf60/0xaf64     4: 0xafb8/0xafbc/0xafc0/0xafc4
 *       1: 0xaf70/0xaf74/0xaf78/0xaf7c     5: 0xafd0/0xafd4/0xafd8/0xafdc
 *       2: 0xaf88/0xaf8c/0xaf90/0xaf94     6: 0xafe8/0xafec/0xaff0/0xaff4
 *       3: 0xafa0/0xafa4/0xafa8/0xafac     7: 0xb000/0xb004/0xb008/0xb00c
 *     (the widths/heights come from the second half of the table, read as the
 *     immediate + the pair's own add; the file's own words are quoted above.)
 *
 *   - WHICH RECTANGLE IS WHICH ON-SCREEN BUTTON IS NOT MEASURABLE FROM THE
 *     FILE (the labels live in the game's own layout/Flash): the port therefore
 *     LOGS ALL EIGHT with their geometry on the first match, and which one each
 *     pad button presses -- and the map is config.ini's
 *     [controls] pad_touch_map, so the mapping is one line away from being
 *     fixed on the console, with the log's numbers in hand.
 *
 * ==========================================================================
 * HOW IT PRESSES THEM
 *
 * Exactly the way a finger does: the port's own pointer injection
 * (fifa14_input.c's deliver() -- the engine native Android's Java view calls)
 * with a raw pointer id of its own (120+pad button, clear of the panel's
 * finger ids and of the cursor's 100/101):
 *
 *   DOWN at the rectangle's centre while the pad button is held,
 *   UP   when it is let go.
 *
 * A tap, a hold and a double tap are therefore all available (the engine's own
 * timeouts decide), and nothing is written into the engine: the port only
 * feeds it the same events the panel feeds.
 *
 * WHAT IS VERIFIED BEFORE ANYTHING IS TOUCHED (fail-open: an unverified
 * signal means NO injection at all):
 *   - the game pointer itself comes from fifa14_padscope.c (verified chain);
 *   - the surface size (Game[0xc914]/[0xc918]) must be a plausible screen;
 *   - each mapped rectangle must be inside that surface with a positive size;
 *   - every address read must be inside the loaded image.
 * And the whole bridge is behind [controls] pad_touch_buttons = on|off, with
 * `off` leaving the pad exactly as it was before this file existed.
 *
 * MIT.
 */
#include <stdio.h>
#include <string.h>
#include <switch.h>

#include "config.h"
#include "fifa14_input.h"
#include "fifa14_padact.h"
#include "fifa14_padscope.h"
#include "rt_cfg.h"
#include "so_util.h"
#include "util.h"

extern so_module game_mod;

/* ------------------------------------------------------------- the addresses
 * All MEASURED in libFIFA14.so (see the header): the eight button rectangles,
 * their stride, and the surface the engine hit-tests the touches against. */
#define PADACT_RECT_BASE   0xaf58u
#define PADACT_RECT_STRIDE 0x18u
#define PADACT_RECTS       8
#define PADACT_SURF_W      0xc914u
#define PADACT_SURF_H      0xc918u

/* The raw pointer ids this bridge's touches carry: clear of the panel's own
 * finger ids and of the cursor's 100/101 (fifa14_input.c). */
#define PADACT_RAW_BASE 120
#define PADACT_RAW_MAX  128

/* How often the [padact] line is written while nothing changes, in frames. */
#ifndef FIFA14_PADACT_REPORT_FRAMES
#define FIFA14_PADACT_REPORT_FRAMES 1800u
#endif

/* The pad buttons this bridge can own, in its own order; the map's indices are
 * this table's. The bits are the same ones fifa14_input.c's k_keys uses. */
typedef struct {
  uint64_t bit;
  const char *name; /* the config.ini name in pad_touch_map */
} Fifa14PadBtn;

static const Fifa14PadBtn k_padbtns[] = {
    {HidNpadButton_A, "a"},  {HidNpadButton_B, "b"},   {HidNpadButton_X, "x"},
    {HidNpadButton_Y, "y"},  {HidNpadButton_L, "l"},   {HidNpadButton_R, "r"},
    {HidNpadButton_ZL, "zl"}, {HidNpadButton_ZR, "zr"},
};
#define PADACT_NBTN (int)(sizeof k_padbtns / sizeof k_padbtns[0])

/* The default map: the four face buttons on the first four rectangles. IT IS A
 * GUESS only in the sense that WHICH rectangle each one is was not measurable
 * from the file (see the header); the log line names every rectangle's
 * geometry, so the first console run says the true map and this line is the
 * only thing to edit. */
#define PADACT_MAP_DEFAULT "a:0,b:1,x:2,y:3"

static struct {
  int armed;
  int on;              /* [controls] pad_touch_buttons */
  int map[PADACT_NBTN];/* pad button -> rectangle index, -1 = not this bridge's */
  int n_mapped;
  uintptr_t g;         /* the game object the rectangles were read from */
  int w, h;            /* the surface, from the game object */
  int have_rects;      /* the rectangles were read and passed the checks */
  int r_ok[PADACT_RECTS];
  int r_x[PADACT_RECTS], r_y[PADACT_RECTS], r_w[PADACT_RECTS], r_h[PADACT_RECTS];
  int held[PADACT_NBTN]; /* this button's touch is down */
  int last_cx[PADACT_NBTN], last_cy[PADACT_NBTN]; /* where its DOWN/UP went */
  int logged_table;      /* the rectangle table has been logged */
  int logged_map;        /* the mapping has been logged */
  int bad_logged[PADACT_RECTS];
  unsigned n_down, n_up;
  unsigned long long frames;
  int last_state;        /* for the report line */
} s_pa;

static int in_image(uintptr_t p, size_t len) {
  const uintptr_t lo = (uintptr_t)game_mod.load_virtbase;
  const uintptr_t hi = lo + game_mod.load_size;
  return lo != 0 && p >= lo && p + len <= hi;
}

static int32_t rd32(uintptr_t p) { return *(const volatile int32_t *)p; }

/* --------------------------------------------------------------- the config */

static void parse_map(const char *spec) {
  for (int i = 0; i < PADACT_NBTN; i++)
    s_pa.map[i] = -1;
  s_pa.n_mapped = 0;
  if (!spec || !*spec)
    spec = PADACT_MAP_DEFAULT;
  const char *p = spec;
  while (*p) {
    while (*p == ' ' || *p == ',' || *p == ';')
      p++;
    if (!*p)
      break;
    const char *colon = strchr(p, ':');
    if (!colon)
      break;
    const size_t nlen = (size_t)(colon - p);
    int btn = -1;
    for (int i = 0; i < PADACT_NBTN; i++)
      if (strlen(k_padbtns[i].name) == nlen && !strncmp(p, k_padbtns[i].name, nlen))
        btn = i;
    const char *v = colon + 1;
    int rect = -1;
    if (*v >= '0' && *v <= '9' && (unsigned)(*v - '0') < PADACT_RECTS)
      rect = *v - '0';
    if (btn >= 0 && rect >= 0) {
      s_pa.map[btn] = rect;
      s_pa.n_mapped++;
    } else if (btn >= 0) {
      debugPrintf("[padact] pad_touch_map: \"%.*s\" is not a rectangle of the engine's "
                  "(0..%d): the button stays on its key only\n",
                  (int)(strcspn(p, ",;")), p, PADACT_RECTS - 1);
    }
    while (*p && *p != ',' && *p != ';')
      p++;
  }
}

void fifa14_padact_arm(void) {
  if (s_pa.armed)
    return;
  s_pa.armed = 1;
  const char *on = rt_config_get("controls", "pad_touch_buttons");
  s_pa.on = !(on && (!strcmp(on, "off") || !strcmp(on, "0"))); /* default on */
  parse_map(rt_config_get("controls", "pad_touch_map"));
  debugPrintf("[padact] ==== the pad's actions in a match: the engine's own touch buttons ====\n");
  debugPrintf("[padact] %s ([controls] pad_touch_buttons = %s); the map \"%s\": %d pad button(s) "
              "pressing an engine rectangle (0..%d at Game+0x%x, stride 0x%x)\n",
              s_pa.on ? "ARMED" : "OFF", on ? on : "on",
              rt_config_get("controls", "pad_touch_map") ? rt_config_get("controls", "pad_touch_map")
                                                         : PADACT_MAP_DEFAULT,
              s_pa.n_mapped, PADACT_RECTS - 1, PADACT_RECT_BASE, PADACT_RECT_STRIDE);
  debugPrintf("[padact] how it presses them: the port's pointer native (the same one the panel "
              "uses), DOWN at the rectangle's centre while the pad button is held and UP when it is "
              "let go, raw pointer ids %d..%d. Nothing is written into the engine.\n",
              PADACT_RAW_BASE, PADACT_RAW_BASE + PADACT_NBTN - 1);
  if (!s_pa.on)
    debugPrintf("[padact] the bridge is OFF: the pad buttons keep delivering their KEYS only, "
                "exactly as before this file existed\n");
}

/* ---------------------------------------------------------- the rectangles */

/* Read the eight rectangles out of the game object and check each one against
 * the surface the engine itself checks touches against. Logs the whole table
 * whenever it changes (that line is what a console run needs to name the
 * buttons -- the first one, and any layout change the game makes). */
static void read_rects(uintptr_t g) {
  s_pa.g = g;
  const int have_surf =
      in_image(g + PADACT_SURF_W, 4) && in_image(g + PADACT_SURF_H, 4);
  s_pa.w = have_surf ? rd32(g + PADACT_SURF_W) : 0;
  s_pa.h = have_surf ? rd32(g + PADACT_SURF_H) : 0;
  const int surf_ok = have_surf && s_pa.w >= 320 && s_pa.w <= 8192 && s_pa.h >= 240 &&
                      s_pa.h <= 8192;
  if (!surf_ok) {
    s_pa.have_rects = 0;
    if (!s_pa.logged_table) {
      s_pa.logged_table = 1;
      debugPrintf("[padact] the engine's surface is not where this file measured it "
                  "(Game+0x%x/%x = %dx%d, in the image: %s): the bridge stays out and no touch "
                  "is ever injected\n",
                  PADACT_SURF_W, PADACT_SURF_H, s_pa.w, s_pa.h, have_surf ? "yes" : "no");
    }
    return;
  }
  int ok = 0, changed = !s_pa.logged_table;
  int nx[PADACT_RECTS], ny[PADACT_RECTS], nw[PADACT_RECTS], nh[PADACT_RECTS];
  for (int i = 0; i < PADACT_RECTS; i++) {
    const uintptr_t p = g + PADACT_RECT_BASE + (uintptr_t)i * PADACT_RECT_STRIDE;
    nx[i] = ny[i] = nw[i] = nh[i] = 0;
    if (in_image(p, 16)) {
      nx[i] = rd32(p + 0);
      ny[i] = rd32(p + 4);
      nw[i] = rd32(p + 8);
      nh[i] = rd32(p + 12);
    }
    if (nx[i] != s_pa.r_x[i] || ny[i] != s_pa.r_y[i] || nw[i] != s_pa.r_w[i] ||
        nh[i] != s_pa.r_h[i])
      changed = 1;
  }
  for (int i = 0; i < PADACT_RECTS; i++) {
    s_pa.r_x[i] = nx[i];
    s_pa.r_y[i] = ny[i];
    s_pa.r_w[i] = nw[i];
    s_pa.r_h[i] = nh[i];
    const int good = s_pa.r_w[i] > 0 && s_pa.r_h[i] > 0 && s_pa.r_x[i] >= 0 &&
                     s_pa.r_y[i] >= 0 && s_pa.r_x[i] + s_pa.r_w[i] <= s_pa.w &&
                     s_pa.r_y[i] + s_pa.r_h[i] <= s_pa.h && s_pa.r_w[i] <= 2048 &&
                     s_pa.r_h[i] <= 2048;
    if (s_pa.r_ok[i] != good)
      changed = 1;
    s_pa.r_ok[i] = good;
    ok += good;
  }
  s_pa.have_rects = ok > 0;
  if (changed) {
    s_pa.logged_table = 1;
    debugPrintf("[padact] the engine's touch buttons, out of the game object %p (surface %dx%d):\n",
                (void *)g, s_pa.w, s_pa.h);
    for (int i = 0; i < PADACT_RECTS; i++)
      debugPrintf("[padact]   rect %d @Game+0x%x: x %d, y %d, w %d, h %d, centre %d,%d -- %s\n", i,
                  PADACT_RECT_BASE + i * PADACT_RECT_STRIDE, s_pa.r_x[i], s_pa.r_y[i], s_pa.r_w[i],
                  s_pa.r_h[i], s_pa.r_x[i] + s_pa.r_w[i] / 2, s_pa.r_y[i] + s_pa.r_h[i] / 2,
                  s_pa.r_ok[i] ? "usable" : "NOT usable (outside the surface or empty)");
    debugPrintf("[padact] %d of %d rectangle(s) are usable; a rectangle the map names but the "
                "engine has emptied is skipped (and its button stays a key)\n",
                ok, PADACT_RECTS);
  }
}

/* The mapping, once: which pad button presses which rectangle (and its centre,
 * so the log can be held against the screen). */
static void log_map(void) {
  if (s_pa.logged_map || !s_pa.have_rects)
    return;
  s_pa.logged_map = 1;
  for (int i = 0; i < PADACT_NBTN; i++) {
    const int r = s_pa.map[i];
    if (r < 0)
      continue;
    debugPrintf("[padact] pad %s -> rect %d @%d,%d (%dx%d), centre %d,%d -- %s\n", k_padbtns[i].name,
                r, s_pa.r_x[r], s_pa.r_y[r], s_pa.r_w[r], s_pa.r_h[r],
                s_pa.r_x[r] + s_pa.r_w[r] / 2, s_pa.r_y[r] + s_pa.r_h[r] / 2,
                s_pa.r_ok[r] ? "live" : "the engine has this rectangle empty: NOT pressed");
  }
  debugPrintf("[padact] the map is [controls] pad_touch_map; the rectangles' own geometry is "
              "above, so the buttons can be named from this log alone\n");
}

/* ------------------------------------------------------------------ the poll */

void fifa14_padact_release_all(void) {
  for (int i = 0; i < PADACT_NBTN; i++) {
    if (!s_pa.held[i])
      continue;
    s_pa.held[i] = 0;
    /* The UP goes where the DOWN went (the engine knows the pointer by id, but
     * a lift at the last position is what the panel itself sends). */
    fifa14_input_inject_touch(PADACT_RAW_BASE + i, 0, (float)s_pa.last_cx[i],
                              (float)s_pa.last_cy[i]);
    s_pa.n_up++;
  }
}

void fifa14_padact_poll(uint64_t buttons) {
  if (!s_pa.armed || !s_pa.on || !s_pa.n_mapped)
    return;
  uintptr_t g = 0;
  if (!fifa14_padscope_game(&g)) {
    /* No match (or the chain is unverified): hold nothing. */
    if (s_pa.g) {
      fifa14_padact_release_all();
      s_pa.g = 0;
      s_pa.have_rects = 0;
    }
    return;
  }
  if (g != s_pa.g) {
    /* A new match: the old rectangles are gone with the old object. */
    if (s_pa.g)
      fifa14_padact_release_all();
    s_pa.g = g;
    s_pa.logged_table = 0; /* the new match's table gets its own line */
    s_pa.logged_map = 0;
  }
  read_rects(g); /* every frame: 8 rectangles is nothing, and a layout change is noticed */
  if (!s_pa.have_rects)
    return;
  log_map();
  for (int i = 0; i < PADACT_NBTN; i++) {
    const int r = s_pa.map[i];
    if (r < 0)
      continue;
    const int on = (buttons & k_padbtns[i].bit) != 0;
    const int cx = s_pa.r_x[r] + s_pa.r_w[r] / 2, cy = s_pa.r_y[r] + s_pa.r_h[r] / 2;
    if (on && !s_pa.held[i] && s_pa.r_ok[r]) {
      s_pa.held[i] = 1;
      s_pa.last_cx[i] = cx;
      s_pa.last_cy[i] = cy;
      fifa14_input_inject_touch(PADACT_RAW_BASE + i, 1, (float)cx, (float)cy);
      s_pa.n_down++;
    } else if (!on && s_pa.held[i]) {
      s_pa.held[i] = 0;
      fifa14_input_inject_touch(PADACT_RAW_BASE + i, 0, (float)s_pa.last_cx[i],
                                (float)s_pa.last_cy[i]);
      s_pa.n_up++;
    }
  }
}

void fifa14_padact_report(unsigned long long frames) {
  s_pa.frames = frames;
  if (!s_pa.armed || !s_pa.on || !s_pa.n_mapped || !s_pa.have_rects)
    return;
  int held = 0;
  for (int i = 0; i < PADACT_NBTN; i++)
    held += s_pa.held[i] != 0;
  const int state = held;
  if (state == s_pa.last_state && frames % FIFA14_PADACT_REPORT_FRAMES != 0)
    return;
  const int changed = state != s_pa.last_state;
  s_pa.last_state = state;
  debugPrintf("[padact] the pad's touches: %u down + %u up so far, %d held now%s (the engine's "
              "rectangles, surface %dx%d)\n",
              s_pa.n_down, s_pa.n_up, held, changed ? ", CHANGED" : "", s_pa.w, s_pa.h);
}
