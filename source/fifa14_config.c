/* fifa14_config.c -- FIFA 14's config.ini, on the runtime's INI engine
 * (runtime/source/rt_cfg.c, group G).
 *
 * The runtime's main.c calls dcr_config_load() early, before anything reads
 * rt_config(): it hands rt_config_load() the port's option table. The table
 * here is the runtime's own rows (rt_cfg.h) plus FIFA 14's; a minimal one for
 * now -- the port's derived fields and its own screen come later (NOTES.md).
 * The row order is the file's order; keep it stable.
 *
 * MIT.
 */
#include "rt_cfg.h"
#include "util.h"

/* The port's own two test levers (2026-10-13). Both are read with
 * rt_config_get()/rt_config_value(), never through a dst field, because the
 * code that uses them is not this file:
 *
 *   [controls] touch_screen -- the value this port answers for
 *   com/ea/blast/SystemAndroidDelegate.GetTouchScreenCount()I
 *   (fifa14_blast_stubs.c). 1 (the default) is what the port has always
 *   answered and what the menus were measured working with (the touch
 *   pointers drive them: "[input] frame N: ... panel up (2 fingers), last
 *   pointer MOVE ..."). 0 is the experiment the switch asked for: whether the
 *   engine's own UI drops its on-screen buttons when the device says it has
 *   no touch screen. MEASURED in libFIFA14.so: the engine registers its touch
 *   modules UNCONDITIONALLY (RegisterTouchscreenModule/RegisterTouchPadModule
 *   are called one after the other, with no test, at 0x33a34c/0x33a350), so
 *   this lever probably does NOT hide anything and may take the menus' own
 *   pointer path away. Kept 1 on purpose: a lever to test on the console, not
 *   a new default.
 *
 *   [audio] language -- what the port writes into AUDIO_LANGUAGE in
 *   <game>/external/ini/na/locale.ini. "keep" (the default) leaves the
 *   port's own choice (SPA_ES: data/audio/speech/SPA_ES.mp3 is the file the
 *   OBB really has). MEASURED in libFIFA14.so: the engine's speech system
 *   READS THIS KEY when it initialises its banks (the init at
 *   0x7e6f1c loads "AUDIO_LANGUAGE" -- 0x10196a4 -- twice and stores the
 *   string in the speech object), and the four speech file names the engine
 *   asks for are hardcoded ENG_US. So ENG_US is the one other value worth
 *   trying: change ONE line in config.ini and the next run says what the
 *   speech system was served.
 *
 * Neither row has a dst: their values are read where they are used, and the
 * engine appends both to an older config.ini on the next start. */

/* The two levers' help text (written above each key in config.ini). Plain
 * ASCII, like the rest of the file: the ini writer copies these bytes as they
 * are. */
#define CFG_HELP_TOUCH_SCREEN                                                                      \
  "What the port tells the game about the device's touch screen\n"                                  \
  "# (SystemAndroidDelegate.GetTouchScreenCount). 1 = the phone's screen\n"                          \
  "# (the menus are worked with touch pointers today: known good). 0 = the\n"                        \
  "# game is told there is no touch screen at all -- try it to see whether\n"                        \
  "# the on-screen buttons go away; measured in the engine, its touch modules\n"                     \
  "# are registered with no test at all (0x33a34c), so this may cost the\n"                          \
  "# menus their pointer path and change nothing else. Revert with 1."
#define CFG_HELP_AUDIO_LANGUAGE                                                                     \
  "The AUDIO language the port serves in the game's ini/na/locale.ini\n"                             \
  "# (AUDIO_LANGUAGE). keep = the port's own choice, SPA_ES\n"                                       \
  "# (data/audio/speech/SPA_ES.mp3 is the file the OBB really has). SPA_ES or\n"                      \
  "# ENG_US = force that one. Measured: the engine's speech system reads this\n"                      \
  "# key when it builds its banks (libFIFA14.so @0x7e6f84/0x7e70f8 load\n"                            \
  "# \"AUDIO_LANGUAGE\"), and the four voice file names it asks for are\n"                            \
  "# hardcoded ENG_US -- so ENG_US is the one worth trying if the voices stay\n"                      \
  "# silent with the pack installed."

/* THE GAME'S CONTROL SCHEME (2026-10-13, source/fifa14_ctlcfg.c has the whole
 * measured chain). The on-screen buttons are NOT a device capability the port
 * can answer away: they are part of the ESQUEMA DE CONTROL the engine reads
 * from its own settings, whose keys are IN the .so
 * ("CONTROL_CONFIG/BIG_BUTTONS" @0x1003098, "CONTROL_CONFIG/BIG_DPAD"
 * @0x10030c8, "CONTROL_CONFIG/FLOATING_DPAD" @0x1003036,
 * "CONTROL_CONFIG/ALTERNATE_CONTROLS" @0x10030f2, "CONTROL_CONFIG/BASIC_HAND"
 * @0x100306a, "CONTROL_CONFIG/SCHEME_ID_P0" @0x1003c97/_P1 @0x1003cb3,
 * "CONTROL_CONFIG/LAYOUT_ID" @0x100419d) and whose values the engine loads out
 * of two of its own ini files (the .so's own 12-name table at 0x10afc50; the
 * loader @0x55c6c0 builds "ini/%s/%s"):
 *
 *   ini/na/controls.ini  [CONTROL_CONFIG]  SCHEME_ID_P0 = 0 (0 = two buttons +
 *                        sprint, 1 = three buttons), SCHEME_ID_P1, LAYOUT_ID = 0
 *                        (0..3), RENDER_DEBUG, RENDER_DEBUG_SETPIECE,
 *                        USE_SIDE_BUTTONS
 *   ini/na/common.ini    [SETTINGS]        FLOATING_DPAD = 0, BASIC_HAND = 0,
 *                        BIG_BUTTONS = 1, BIG_DPAD = 1, ALTERNATE_CONTROLS = 0,
 *                        ...
 *
 * The port serves both out of the OBB, so each row below WRITES the value into
 * the file the engine loads (same mechanism as the audio language): the file
 * is patched on the card, line by line, and nothing is written when the line
 * already says it. "keep" (every default) changes nothing at all. Every
 * effective value is logged at boot -- [ctlcfg] -- and the engine's own answer
 * (its live registry, through its own readers) is logged a few seconds in, so
 * one run says which scheme/layout is active and whether the game kept or
 * overrode what it was served. */
#define CFG_HELP_CTL_LAYOUT                                                                         \
  "The game's CONTROL LAYOUT (ini/na/controls.ini, [CONTROL_CONFIG]\n"                                \
  "# LAYOUT_ID: the game's file says 0 = layout 0 ... 3 = layout 3). keep =\n"                        \
  "# leave the file as the game has it. The port only moves the line."
#define CFG_HELP_CTL_SCHEME                                                                         \
  "The game's CONTROL SCHEME (ini/na/controls.ini, [CONTROL_CONFIG]\n"                                \
  "# SCHEME_ID_P0 and SCHEME_ID_P1, one per player; the game's own file says\n"                       \
  "# 0 = two buttons + sprint (basic), 1 = three buttons (advanced)). keep =\n"                       \
  "# leave both lines as the game has them. This is the on-screen scheme, not\n"                      \
  "# the pad: the pad already works."
#define CFG_HELP_CTL_BIG_BUTTONS                                                                    \
  "Large Buttons, the game's own option (ini/na/common.ini, [SETTINGS]\n"                             \
  "# BIG_BUTTONS; the file ships 1). The engine reads it as\n"                                        \
  "# CONTROL_CONFIG/BIG_BUTTONS. keep = leave the file as the game has it."
#define CFG_HELP_CTL_BIG_DPAD                                                                       \
  "Large Joystick, the game's own option (ini/na/common.ini, [SETTINGS]\n"                             \
  "# BIG_DPAD; the file ships 1). keep = leave the file as the game has it."
#define CFG_HELP_CTL_FLOATING_DPAD                                                                  \
  "Floating Joystick, the game's own option (ini/na/common.ini, [SETTINGS]\n"                          \
  "# FLOATING_DPAD; the file ships 0 = fixed joystick). keep = leave the file\n"                       \
  "# as the game has it."
#define CFG_HELP_CTL_ALTERNATE                                                                      \
  "Alternate Controls, the game's own option (ini/na/common.ini, [SETTINGS]\n"                         \
  "# ALTERNATE_CONTROLS; the file ships 0). keep = leave the file as the game\n"                       \
  "# has it."
#define CFG_HELP_CTL_BASIC_HAND                                                                     \
  "Left Handed, the game's own option (ini/na/common.ini, [SETTINGS]\n"                                \
  "# BASIC_HAND; the game's file says 0 = left hand, 1 = right hand). keep =\n"                        \
  "# leave the file as the game has it."
#define CFG_HELP_CTL_RENDER_DEBUG                                                                   \
  "The game's own RENDER_DEBUG (ini/na/controls.ini, [CONTROL_CONFIG]; the\n"                          \
  "# file's own comment: 0 = don't render debug, 1 = render debug). What the\n"                       \
  "# engine draws with it is the engine's; try 1 in one run and read the log\n"                        \
  "# and the screen. keep = leave the file as the game has it."
#define CFG_HELP_CTL_LOG_ENGINE                                                                     \
  "Ask the engine's own settings readers (libFIFA14.so's registry, reached\n"                          \
  "# from an exported symbol by a measured offset and verified before it is\n"                         \
  "# called) what the live CONTROL_CONFIG values are, every 1800 frames, and\n"                        \
  "# log them as [ctlcfg] engine lines. true (the default) is the whole point\n"                      \
  "# of the instrumentation round; false silences that one call path."
/* WHEN the pad's keys go in (2026-10-13, source/fifa14_padscope.c has the whole
 * measured chain). The console's pad keys reach the engine's PhysicalKeyboard
 * module in the menus as well, and this row is the lever for it:
 *
 *   match  (default) -- the keys are delivered only while the engine has a
 *                       live game object. The signal is the engine's OWN: a
 *                       pointer in libFIFA14.so's .bss at VA 0x11e0c84, the
 *                       address its own code reads at 0x46a564 (five
 *                       instruction words + two pool words + the GOT slot,
 *                       all checked against the mapped image before use).
 *                       The gate FAILS OPEN: a chain that does not verify
 *                       delivers every key, exactly as before.
 *   always           -- every key, always: what this port has done so far.
 *
 * The touch layer (the right stick's cursor, StickR/StickL) and +/- (Android's
 * BACK) are NOT scoped: they are how the menus are worked. */
#define CFG_HELP_PAD_SCOPE                                                                          \
  "WHEN the pad's keys are delivered to the game: match = only while the\n"                           \
  "# engine has a live game object (a match up); always = every key, always.\n"                        \
  "# The signal is the engine's OWN game object pointer (libFIFA14.so, .bss\n"                         \
  "# VA 0x1181938, read by the engine at 0x46a564; the port verifies those\n"                          \
  "# instruction words, the GOT slot and its own pointer before reading it --\n"                       \
  "# source/fifa14_padscope.c has the chain). NULL = no match: the pad's KEYS\n"                       \
  "# are held back and counted ([pad] lines in debug.log), the touch layer\n"                          \
  "# (right stick cursor, StickR/StickL) and +/- (Android BACK) are NOT scoped,\n"                     \
  "# and an unverifiable signal leaves the gate OPEN. If a run shows \"no game\"\n"                    \
  "# inside a match, set this to always."
/* THE PAD'S ACTIONS IN A MATCH (2026-10-14, source/fifa14_padact.c has the
 * whole measurement). The console run settled the round's question: the pad's
 * KEYS reach the engine and the match does not react, because the match is fed
 * by the TOUCH surface (the on-screen buttons) and this build's keyboard
 * notifications have no listener at all (Keyboard::AddKeyListener @0xe0699c and
 * PhysicalKeyboard::AddKeyListener @0xdf1054 have ZERO callers in
 * libFIFA14.so). So the bridge presses the engine's own buttons:
 *
 *   pad_touch_buttons (on by default) -- the four face buttons become touches
 *     on the engine's eight on-screen button rectangles, which live in the
 *     game object at +0xaf58 + 0x18*i (the engine's own hit test 0x6e846c
 *     reads {x, y, w, h} there; the surface it checks against is +0xc914/
 *     +0xc918). DOWN at the rectangle's centre while the button is held, UP on
 *     release -- the same pointer native the panel uses, ids 120+. Nothing is
 *     written into the engine. off = the old behaviour, exactly.
 *   pad_touch_map -- "pad-button:rectangle" pairs. The rectangles' geometry is
 *     NOT in this .so (the labels are the game's own layout), so the FIRST
 *     match's log prints all eight with their positions and centres (the
 *     [padact] lines); set the map from those numbers and the next run is the
 *     one that has the right buttons. */
#define CFG_HELP_PAD_TOUCH                                                                          \
  "The pad's actions in a match: on = the face buttons press the engine's OWN\n"                      \
  "# on-screen touch buttons (the match listens to touches, not to keys --\n"                         \
  "# source/fifa14_padact.c has the measurement), off = the old behaviour.\n"                         \
  "# The first match's [padact] log lines name every engine rectangle with its\n"                     \
  "# geometry; [controls] pad_touch_map decides which pad button presses which."
#define CFG_HELP_PAD_TOUCH_MAP                                                                      \
  "Which engine touch rectangle each pad button presses, as button:rect pairs\n"                      \
  "# (rect 0..7). The default a:0,b:1,x:2,y:3 is a guess only in WHICH engine\n"                      \
  "# rectangle each one is: the first match's [padact] lines print all eight\n"                       \
  "# with their x,y,w,h and centre, so the map can be set from the log alone.\n"                      \
  "# Faces: a,b,x,y; shoulders/triggers: l,r,zl,zr (e.g. \"a:2,b:0,x:1,y:3,r:0\")."
/* THE COMMENTATORS (2026-10-14, source/fifa14_spch.c): the speech system's gate
 * was measured OPEN on the console (PRACTICE_MODE has 1 value 0), so with the
 * lever at 1 the port calls the engine's own speech start itself, once, with
 * every instruction word verified first. 0 (the default) leaves the engine's
 * own path alone. */
#define CFG_HELP_FORCE_SPEECH                                                                       \
  "1 = the port calls the engine's own speech start (libFIFA14.so 0x7e69f0 +\n"                       \
  "# 0x7e6f1c, the pair the engine's own audio init calls at 0x7ea6bc/0x7ea6c0)\n"                    \
  "# ONCE, as soon as the engine's speech global exists, with every word\n"                           \
  "# verified first -- for the commentators, whose PRACTICE_MODE gate was\n"                          \
  "# measured open and still does not load the pack. Serve the pack with\n"                           \
  "# [audio] language = ENG_US: its file names are hardcoded ENG_US and the\n"                        \
  "# Latino pack lives in exactly those slots. 0 = leave the engine alone."
static const CfgOpt k_opts[] = {
    CFG_ROW_RESOLUTION("auto", CFG_HELP_RESOLUTION),
    CFG_ROW_BOOST(CFG_HELP_BOOST, NULL),
    CFG_ROW_SWAP_AB("Swap the A and B buttons.", NULL),
    CFG_ROW_GL_SELFTEST(NULL),
    CFG_ROW_BOOT_LOG(CFG_HELP_BOOT_LOG, NULL),
    CFG_ROW_LOG_JNI(CFG_HELP_LOG_JNI, NULL),
    {"controls", "touch_screen", "1", CFG_HELP_TOUCH_SCREEN, CFG_CHOICE, "1,0", NULL, 0, 0, 0, 0},
    {"audio", "language", "keep", CFG_HELP_AUDIO_LANGUAGE, CFG_CHOICE, "keep,SPA_ES,ENG_US", NULL, 0,
     0, 0, 0},
    /* The game's control scheme (fifa14_ctlcfg.c). The file's order is this
     * order; keep it stable. */
    {"controls", "layout_id", "keep", CFG_HELP_CTL_LAYOUT, CFG_CHOICE, "keep,0,1,2,3", NULL, 0, 0, 0,
     0},
    {"controls", "scheme_id", "keep", CFG_HELP_CTL_SCHEME, CFG_CHOICE, "keep,0,1", NULL, 0, 0, 0, 0},
    {"controls", "big_buttons", "keep", CFG_HELP_CTL_BIG_BUTTONS, CFG_CHOICE, "keep,0,1", NULL, 0, 0,
     0, 0},
    {"controls", "big_dpad", "keep", CFG_HELP_CTL_BIG_DPAD, CFG_CHOICE, "keep,0,1", NULL, 0, 0, 0, 0},
    {"controls", "floating_dpad", "keep", CFG_HELP_CTL_FLOATING_DPAD, CFG_CHOICE, "keep,0,1", NULL, 0,
     0, 0, 0},
    {"controls", "alternate_controls", "keep", CFG_HELP_CTL_ALTERNATE, CFG_CHOICE, "keep,0,1", NULL,
     0, 0, 0, 0},
    {"controls", "basic_hand", "keep", CFG_HELP_CTL_BASIC_HAND, CFG_CHOICE, "keep,0,1", NULL, 0, 0, 0,
     0},
    {"controls", "render_debug", "keep", CFG_HELP_CTL_RENDER_DEBUG, CFG_CHOICE, "keep,0,1", NULL, 0,
     0, 0},
    {"controls", "log_engine_config", "true", CFG_HELP_CTL_LOG_ENGINE, CFG_BOOL, NULL, NULL, 0, 0, 0,
     0},
    /* THE PAD'S KEY SCOPE (fifa14_padscope.c): match = the pad's keys go in
     * only while the engine has a live game object; always = as before. */
    {"controls", "pad_keys_scope", "match", CFG_HELP_PAD_SCOPE, CFG_CHOICE, "match,always", NULL, 0,
     0, 0, 0},
    /* THE PAD'S ACTIONS IN A MATCH (fifa14_padact.c): the face buttons press
     * the engine's own on-screen touch buttons, which is the only input a
     * match listens to. */
    {"controls", "pad_touch_buttons", "on", CFG_HELP_PAD_TOUCH, CFG_CHOICE, "on,off", NULL, 0, 0, 0,
     0},
    {"controls", "pad_touch_map", "a:0,b:1,x:2,y:3", CFG_HELP_PAD_TOUCH_MAP, CFG_TEXT, NULL, NULL, 0,
     0, 64, 0},
    /* THE COMMENTATORS (fifa14_spch.c): the port calls the engine's own speech
     * start once when this is 1. */
    {"audio", "force_speech", "0", CFG_HELP_FORCE_SPEECH, CFG_CHOICE, "0,1", NULL, 0, 0, 0, 0},
};

static const CfgTable k_table = {
    .opts = k_opts,
    .nopts = CFG_COUNT(k_opts),
    .version = 1,
};
void dcr_config_load(void) { rt_config_load(&k_table); }
