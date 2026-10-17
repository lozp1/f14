/* fifa14_padscope.c -- IS THERE A GAME TO GIVE A PAD KEY TO?
 *
 * The console's pad keys reach the engine's PhysicalKeyboard module whether the
 * game is in a match or in its menus, and the screen asked for a lever:
 *
 *     [controls] pad_keys_scope = match | always     (default match)
 *
 * "match" needs a signal that says a match is up. This is where that signal
 * lives, with the evidence it was measured from.
 *
 * ==========================================================================
 * WHAT WAS MEASURED, AND WHAT WAS NOT (libFIFA14.so, 18,187,340 B, the same
 * file as always; every address here is a FILE/VA address of that file, and
 * every instruction word quoted was read out of it)
 *
 * LOOKED FOR FIRST, AND NOT FOUND (the negative result is what moved the
 * signal out of the "settings" surface):
 *
 *   - The engine's contextual touch-button vocabulary IS in the file --
 *     BUTTON_MODE_WITH_POSSESSION @0x1025e0b and its twelve siblings, next to
 *     the SoccerAICore control module's own keys -- but NOTHING REFERENCES IT
 *     IN THIS BUILD: a whole-file scan finds no 32-bit word that resolves to
 *     any of the thirteen strings through the engine's own GOT (the scheme the
 *     engine itself uses for string literals: address = 0x10e77d8 + word),
 *     no direct pointer, and no R_DYN relocation either. They are dead
 *     strings; game code cannot be asked what mode it is in through them.
 *   - No exported symbol of the engine says "match" or "in game" (dynsym
 *     scan: only the online Blaze GameManager has "match*" names), and the
 *     game's own screen/overlay vocabulary (OpenOverlay/ControllerOverlay/
 *     CloseAllOverlays @0x1001a8f..0x1001ad0) is a table of ActionScript
 *     function names called through AptCallFunction @0xa8b594 -- the overlay
 *     state lives inside Flash, not in a native variable.
 *   - The engine's control-scheme settings (CONTROL_CONFIG/SCHEME_ID_x,
 *     OPTIONS/x_LOCAL @0x101f553..) are written by the settings-sync code
 *     (0x7460cc / 0x7461b4, and 0x746a30 copies _LOCAL back to _P0/P1), which
 *     runs on settings changes too: a registry key is NOT a match flag.
 *
 * WHAT IS USED INSTEAD -- THE ENGINE'S OWN GAME OBJECT POINTER:
 *
 *   - The engine keeps ONE static pointer in its own .bss, at VA 0x1181938.
 *     The engine's own code reads it, again and again, as
 *
 *         46a564  ldr  r0, [pc, #36]   @ 0x46a590 = 0x00c7d264
 *         46a568  ldr  r1, [pc, #36]   @ 0x46a594 = 0xffffa248
 *         46a56c  add  r0, pc, r0      ; r0 = 0x10e77d8, the engine's GOT
 *         46a570  ldr  r0, [r1, r0]    ; r0 = &the game object (GOT slot
 *                                      ;      0x10e77d8 + (-24008) = 0x10e1a20)
 *         46a574  ldr  r0, [r0]        ; r0 = the game object itself
 *         46a578  cmp  r0, #0
 *         46a57c  bne/movne/strbne ... ; if (G) G->[0xc81c4] = 1
 *
 *     and the shape `if (G) { ...game... }` is the same in 343 measured sites
 *     spread over the match modules (0x497054 in the "family play applies the
 *     control scheme" path, 0x64784c, 0x77b95c, 0x781dc0 in the match/HUD code,
 *     the whole SoccerAICore band 0x88e18c..0x8a039c, the HUD controller band
 *     0x6e7dc0..0x7046bc). G is a pointer to a big object: the sites read its
 *     fields at +0xc789, +0xc81c4, +0xc974 (a >50 KB object), and the same
 *     field offsets show up in functions that take that object as an argument
 *     (0x703928's r6, whose +0x974/+0xa00/+0xc789 tests guard match-side work).
 *
 * WHERE THE ADDRESS ITSELF COMES FROM, AND THE MISTAKE THE FIRST ROUND MADE
 * (this is the whole reason the first console run's check 2 failed):
 *
 *   - .rel.dyn has one R_ARM_RELATIVE entry whose r_offset is 0x10e1a20, and
 *     the RELATIVE addend is the word the loader adds the load bias to -- but
 *     the addend is the word *at that VIRTUAL ADDRESS*, which is NOT the word
 *     at that FILE OFFSET: libFIFA14.so's second PT_LOAD maps file offset
 *     0x10ac390 to virtual address 0x10ad390 (a +0x1000 delta), so the word at
 *     file offset 0x10e1a20 is the one that belongs to VA 0x10e2a20. Read the
 *     wrong one and you get 0x11e0c84; read the right one and you get
 *     0x1181938 -- and 0x1181938 is the one the engine's `ldr r0,[r0]` needs.
 *
 *   - The first console run (2026-10-13) proved it to the byte: the slot held
 *     0x3a9c7938 while this file expected 0x3aa26c84, and
 *     0x3a9c7938 - 0x1181938 = 0x39846000 = exactly the run's load base
 *     (OnRawKeyDown @0xdf0ba8 was logged at 0x3a636ba8, and
 *     0x3a636ba8 - 0xdf0ba8 = 0x39846000). So the loader HAD relocated the slot
 *     correctly; it was this file that was reading the wrong word of its own
 *     .so. The addend of the run is now the file's own word at VA 0x10e1a20.
 *
 * WHAT IS *NOT* PROVEN, AND THE RUN THAT PROVES IT: nothing in this file
 * depends on the claim that G is NULL in the menus -- that claim is what the
 * console run's [pad] lines are for. The port measured WHAT the engine reads,
 * HOW (the words above) and WHERE (the addresses above); whether the menus
 * leave it NULL is answered by a run, not by a guess, which is exactly why the
 * gate FAILS OPEN and why the report line prints the pointer at every change.
 *
 * HOW IT IS READ: one aligned 32-bit load per query, from the mapped image --
 * no engine call, no side effect, nothing written.
 *
 * HOW IT IS VERIFIED BEFORE IT IS USED (all reads of the mapped image; if any
 * check fails the signal is declared UNKNOWN and the caller's gate stays open):
 *
 *   1. the five instruction words of the engine's own read (0x46a564..) match
 *      the file's, and the two literal-pool words that feed them are still
 *      0x00c7d264 (the pc-relative word that makes r0 the GOT 0x10e77d8) and
 *      0xffffa248 (the GOT slot offset, -24008);
 *   2. the GOT slot 0x10e1a20 (that offset added to 0x10e77d8) holds exactly
 *      the loader's fixup of the file's own addend -- i.e.
 *      load_base + 0x1181938 (type 23, R_ARM_RELATIVE, read out of .rel.dyn at
 *      the word's VIRTUAL address -- see the file-offset trap above);
 *   3. every address the module touches lies inside the loaded image
 *      (game_mod.load_virtbase .. + load_size), so a different build cannot
 *      send this module into unmapped memory.
 *
 * THE LOG LINE THAT TELLS IT APART (the first run's lesson): the check prints
 * the load base, the address it expected and the word it actually read, so a
 * failure is arithmetic in the log and not a mystery -- and with check 2
 * passing, `fifa14_padscope_match()` gates for real.
 *
 * WHAT IT CAN AND CANNOT SAY (honest, because the next console run is the
 * proof): the reading is the engine's own game pointer. Whether it is NULL in
 * the menus *on this game's boot flow* is what the console log decides -- the
 * report line below prints it at every change, so one run answers it. That is
 * also why "match" fails OPEN: an unverified chain, or a build where this
 * pointer is always non-NULL, delivers the keys exactly as before, and a run
 * that shows "no game" inside a match is fixed by one config.ini line
 * (pad_keys_scope = always).
 *
 * MIT.
 */
#include <stdio.h>
#include <string.h>
#include <switch.h>

#include "config.h"
#include "fifa14_padscope.h"
#include "so_util.h"
#include "util.h"

/* The engine's module (fifa14_boot.c loads it). */
extern so_module game_mod;

/* ------------------------------------------------------------- the addresses
 * All MEASURED in libFIFA14.so (18,187,340 B): the GOT itself (DT_PLTGOT,
 * 0x10e77d8), the slot the game object pointer is fetched through
 * (0x10e77d8 + (-24008) = 0x10e1a20), the object pointer's own VA
 * (0x1181938, in .bss: the second PT_LOAD's memory image beyond its file
 * bytes), and the engine's own read of it at 0x46a564 (five instruction words,
 * two pool words). THE VA, NOT THE FILE OFFSET: the .so's second PT_LOAD maps
 * offset 0x10ac390 to VA 0x10ad390, so the WORD AT VA 0x10e1a20 lives at file
 * offset 0x10e0a20 in that segment -- reading file offset 0x10e1a20 instead
 * answers 0x11e0c84, which is what made the first console run's check 2 fail
 * (the console's slot held 0x3a9c7938 = the run's load base 0x39846000 +
 * 0x1181938, exactly this constant). */
#define PADS_GOT            0x10e77d8u
#define PADS_GOT_SLOT       0x10e1a20u /* GOT + 0xffffa248 */
#define PADS_GAME           0x1181938u /* the game object pointer (in .bss)  */
#define PADS_SITE           0x46a564u  /* the engine's own read of it        */
#define PADS_POOL_GOT       0x46a590u  /* 0x00c7d264: ldr pc-relative -> GOT */
#define PADS_POOL_OFF       0x46a594u  /* 0xffffa248: the GOT slot offset    */
#define PADS_POOL_GOT_W     0x00c7d264u
#define PADS_POOL_OFF_W     0xffffa248u

/* The five instruction words at PADS_SITE, quoted in this file's header. */
static const uint32_t k_site[5] = {
    0xe59f0024u, /* ldr r0, [pc, #36]  @ PADS_POOL_GOT */
    0xe59f1024u, /* ldr r1, [pc, #36]  @ PADS_POOL_OFF */
    0xe08f0000u, /* add r0, pc, r0     ; r0 = the GOT   */
    0xe7910000u, /* ldr r0, [r1, r0]   ; r0 = &G        */
    0xe5900000u, /* ldr r0, [r0]       ; r0 = G         */
};

/* How often the report line is written while the state does not change, in
 * frames. The line is written on every CHANGE immediately. */
#ifndef FIFA14_PADSCOPE_REPORT_FRAMES
#define FIFA14_PADSCOPE_REPORT_FRAMES 1800u
#endif

static struct {
  int tried;  /* the verification has been attempted (once) */
  int ok;     /* ...and passed: s_ps.game may be read */
  int missing; /* which check failed (for the log), 0 when none */
  uintptr_t base;
  volatile uint32_t *game; /* &G, inside the loaded image */
  int last;                /* the last state this module reported: 1/0/-1 */
  unsigned n_reads;        /* queries this run */
  unsigned n_live, n_null; /* reads that said 1 / said 0 */
  unsigned long long frames;
  int logged;              /* the first line has been written */
} s_ps;

/* Inside the engine's mapped image? Every address above is checked with this
 * before it is dereferenced (the same guard fifa14_physkbd.c uses). */
static int in_image(uintptr_t p, size_t len) {
  const uintptr_t lo = (uintptr_t)game_mod.load_virtbase;
  const uintptr_t hi = lo + game_mod.load_size;
  return lo != 0 && p >= lo && p + len <= hi;
}

void fifa14_padscope_init(void) {
  if (s_ps.tried)
    return;
  s_ps.tried = 1;
  s_ps.last = -1;
  s_ps.missing = 1; /* until every check passes */

  const uintptr_t base = (uintptr_t)game_mod.load_virtbase;
  s_ps.base = base;

  debugPrintf("[pad] ==== the pad's key scope: is there a game to give a key to? ====\n");
  debugPrintf("[pad] the signal: the engine's own game object pointer, VA 0x%08x in its .bss, "
              "read by the engine at 0x%08x through the GOT slot 0x%08x (the engine's GOT is "
              "0x%08x; the engine's own read is the five instruction words quoted in "
              "source/fifa14_padscope.c)\n",
              PADS_GAME, PADS_SITE, PADS_GOT_SLOT, PADS_GOT);

  if (!base) {
    debugPrintf("[pad] the engine is not mapped (load_virtbase is 0): the signal is UNKNOWN, and "
                "the gate stays OPEN (every key is delivered)\n");
    return;
  }
  if (!in_image(base + PADS_SITE, sizeof k_site) || !in_image(base + PADS_POOL_GOT, 8) ||
      !in_image(base + PADS_GOT_SLOT, 4) || !in_image(base + PADS_GAME, 4)) {
    s_ps.missing = 3;
    debugPrintf("[pad] the addresses are NOT all inside the engine's image (%p..%p): the signal "
                "is UNKNOWN and the gate stays OPEN\n",
                (void *)base, (void *)(base + game_mod.load_size));
    return;
  }

  /* 1. the engine's own read, word for word */
  {
    const uint32_t *site = (const uint32_t *)(base + PADS_SITE);
    const uint32_t pool_got = *(const uint32_t *)(base + PADS_POOL_GOT);
    const uint32_t pool_off = *(const uint32_t *)(base + PADS_POOL_OFF);
    int bad = -1;
    for (int i = 0; i < 5; i++) {
      if (site[i] != k_site[i]) {
        bad = i;
        break;
      }
    }
    debugPrintf("[pad] the engine's own read at 0x%08x: 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x "
                "(the file's own words), pool 0x%08x = 0x%08x, 0x%08x = 0x%08x\n",
                PADS_SITE, (unsigned)site[0], (unsigned)site[1], (unsigned)site[2],
                (unsigned)site[3], (unsigned)site[4], PADS_POOL_GOT, (unsigned)pool_got,
                PADS_POOL_OFF, (unsigned)pool_off);
    if (bad >= 0) {
      s_ps.missing = 1;
      debugPrintf("[pad] word %d IS NOT the file's 0x%08x: this is NOT that build of the game: "
                  "the signal is UNKNOWN and the gate stays OPEN\n",
                  bad, (unsigned)k_site[bad]);
      return;
    }
    if (pool_got != PADS_POOL_GOT_W || pool_off != PADS_POOL_OFF_W) {
      s_ps.missing = 1;
      debugPrintf("[pad] the pool words are not the file's 0x%08x / 0x%08x: the signal is UNKNOWN "
                  "and the gate stays OPEN\n",
                  PADS_POOL_GOT_W, PADS_POOL_OFF_W);
      return;
    }
  }

  /* 2. the GOT slot must be the address of the game pointer (what the loader's
   *    R_ARM_RELATIVE fixup of the file's own addend 0x1181938 puts there) */
  {
    const uint32_t slot = *(const uint32_t *)(base + PADS_GOT_SLOT);
    debugPrintf("[pad] the load base 0x%08x; the GOT slot 0x%08x = 0x%08x; the file's addend is "
                "0x%08x, so this run's arithmetic is 0x%08x + 0x%08x = 0x%08x (what the loader "
                "should have put there)\n",
                (unsigned)base, PADS_GOT_SLOT, (unsigned)slot, PADS_GAME, (unsigned)base, PADS_GAME,
                (unsigned)(base + PADS_GAME));
    if (slot != (uint32_t)(base + PADS_GAME)) {
      s_ps.missing = 2;
      debugPrintf("[pad] it does NOT point at the game pointer (off by 0x%08x): the signal is "
                  "UNKNOWN and the gate stays OPEN\n",
                  (unsigned)(slot - (uint32_t)(base + PADS_GAME)));
      return;
    }
  }

  s_ps.game = (volatile uint32_t *)(base + PADS_GAME);
  s_ps.ok = 1;
  s_ps.missing = 0;
  {
    const uint32_t v = *s_ps.game;
    debugPrintf("[pad] VERIFIED: the engine's game object 0x%08x -> 0x%08x (%s). The port reads "
                "that one word per query; nothing is called, nothing is written.\n",
                PADS_GAME, (unsigned)v,
                v ? "there IS a game: a match is up" : "there is NO game (menus / nothing mounted)");
    s_ps.last = v ? 1 : 0;
    s_ps.n_live += v ? 1 : 0;
    s_ps.n_null += v ? 0 : 1;
  }
  debugPrintf("[pad] what this can and cannot say: the pointer is the engine's own; whether it is "
              "NULL in the menus is what this run's log settles. An unverified chain, or a build "
              "where it never goes NULL, delivers every key as before (the gate fails OPEN); if a "
              "run shows \"no game\" inside a match, set [controls] pad_keys_scope = always.\n");
}

int fifa14_padscope_match(void) {
  if (!s_ps.ok)
    return -1; /* unverified: the caller MUST treat this as open */
  s_ps.n_reads++;
  const uint32_t v = *s_ps.game;
  if (v)
    s_ps.n_live++;
  else
    s_ps.n_null++;
  return v ? 1 : 0;
}

/* THE GAME OBJECT ITSELF, for the code that has business with it (the pad's
 * touch buttons, fifa14_padact.c: the engine's own button rectangles live in
 * this object). 1 = *out was filled (the chain is verified); 0 = the signal is
 * UNKNOWN or there is no game: *out is untouched. One word read. */
int fifa14_padscope_game(uintptr_t *out) {
  if (!s_ps.ok || !out)
    return 0;
  const uint32_t v = *s_ps.game;
  if (!v)
    return 0;
  *out = (uintptr_t)v;
  return 1;
}

void fifa14_padscope_report(unsigned long long frames) {
  s_ps.frames = frames;
  if (!s_ps.ok) {
    if (!s_ps.logged) {
      s_ps.logged = 1;
      debugPrintf("[pad] the scope's signal was not verified (check %d failed): a key scope of "
                  "\"match\" cannot gate anything this run -- every key is delivered\n",
                  s_ps.missing);
    }
    return;
  }
  const int now = *s_ps.game ? 1 : 0;
  const int changed = now != s_ps.last;
  if (!changed && (!s_ps.logged || frames % FIFA14_PADSCOPE_REPORT_FRAMES != 0))
    return;
  s_ps.logged = 1;
  s_ps.last = now;
  debugPrintf("[pad] the engine's game object 0x%08x = 0x%08x -> %s (%u reads this run: %u said "
              "a game, %u said none%s)\n",
              PADS_GAME, (unsigned)*s_ps.game,
              now ? "a game is live (pad keys go in)" : "no game (pad keys are held back)",
              s_ps.n_reads, s_ps.n_live, s_ps.n_null, changed ? ", CHANGED since the last line" : "");
}
