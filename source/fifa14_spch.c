/* fifa14_spch.c -- THE COMMENTATORS: the engine's own speech system, and the
 * one lever this port puts on it.
 *
 * ==========================================================================
 * WHAT WAS MEASURED (libFIFA14.so, 18,187,340 B, arm-none-eabi disassembly;
 * every word quoted here was read out of the file, every console number out
 * of the run of 2026-10-14)
 *
 *   - The speech system starts in TWO calls, made by the engine's own audio
 *     init (0x7ea5ac), one after the other:
 *
 *       7ea6bc  bl 0x7e69f0     ; r0 = the speech object
 *       7ea6c0  bl 0x7e6f1c     ; speech start(object)
 *
 *     0x7e69f0 is five instructions and returns a global pointer:
 *       e59f000c e59f100c e08f0000 e7910000 e12fff1e
 *       (r0 = GOT [0x10e77d8] ; r0 = *(0xfde28 + GOT) = *(base + 0x11e5600))
 *     0x7e6f1c is the whole start (0xe78 bytes: it reads AUDIO_LANGUAGE
 *     @0x10196a4, loads the event DB, SPCH_AddEventDB, SPCH_InitBankMem, the
 *     bank pointers, and opens the pack's files), and it takes that object as
 *     its argument (`mov sl, r0` at 0x7e6f28).
 *
 *   - THE GATE IN FRONT OF IT IS PRACTICE_MODE, and the console of
 *     2026-10-14 measured it OPEN: `[ctlcfg] PRACTICE_MODE has 1 value 0`, so
 *     the branch at 0x7ea6b4-0x7ea6b8 (bne 0x7ea6f4 = skip the speech) is NOT
 *     taken and the engine falls into 0x7ea6bc/0x7ea6c0. The two keys in that
 *     gate were re-resolved byte for byte for this round: the string both the
 *     Has (0x340acc) and the GetInt (0x340648) are given is at 0x10005a2 and
 *     it IS "PRACTICE_MODE" (built as GOT 0x10e77d8 + 0xfff18dca). So
 *     PRACTICE_MODE is not what keeps the commentators quiet.
 *
 *   - WHAT THE SPEECH START REQUIRES BEFORE IT DOES ANYTHING (its own early
 *     exits, measured):
 *       7e6f44  r0 = *(base + 0x11e5604)   ; the audio system; NULL -> return
 *       7e6f54  bl 0x63f544 ... [obj+0x35a8] < 0 -> return
 *       7e6f88  reads AUDIO_LANGUAGE (so [audio] language is the value this
 *               system is served: the pack's own file names are hardcoded
 *               ENG_US -- Speech/hdr_ENG_US.big, sth_ENG_US.big,
 *               dat_ENG_US.bh, pbp_ENG_US.evt @0x10042e4..0x1004325 -- and the
 *               Latino pack the player installed lives in exactly those slots)
 *       7e71f0  bl 0x49ddd4 ; != 1 -> return
 *     None of these can be watched from here without a hook in the engine's
 *     code (this runtime's 32-bit guest code has no hook facility in this
 *     port), which is exactly why the lever below exists.
 *
 * ==========================================================================
 * THE LEVER: [audio] force_speech = 1
 *
 * With it at 1, the port calls the engine's own pair itself -- 0x7e69f0 for
 * the object, then 0x7e6f1c(object) -- ONE time per run, at the first frame
 * the engine's readiness is there (the same global the start itself demands,
 * *(base + 0x11e5600), must be non-NULL) and never before
 * FIFA14_SPCH_MIN_FRAMES (default 120: the engine's own boot has been there
 * long before). Before the call EVERY instruction word it is about to run is
 * compared with the file's (five for the getter, three for the start): a build
 * that does not match is left alone and the log says so -- fail-open, nothing
 * called.
 *
 * WHAT IT CANNOT SAY, HONESTLY: whether the engine had ALREADY called the
 * start earlier in the run. Without a hook there is no way to see a guest
 * function being entered; the tell is the log AFTER the call -- the pack's
 * files are opened through the port's own file layer, so a run with
 * force_speech = 1 shows `[fs] obb asked="Speech/..."` lines the moment the
 * system comes up (and their absence says the call returned at one of the
 * early exits above, which the same run's [locale] AUDIO_LANGUAGE line then
 * explains: ENG_US is what the pack's slots are called).
 *
 * MIT.
 */
#include <stdio.h>
#include <string.h>
#include <switch.h>

#include "config.h"
#include "fifa14_spch.h"
#include "rt_cfg.h"
#include "so_util.h"
#include "util.h"

extern so_module game_mod;

/* The engine's own call pair (see the header). */
#define SPCH_GET        0x7e69f0u
#define SPCH_START      0x7e6f1cu
/* The global the getter returns and the start demands; the start also checks
 * the word right after it (0x11e5604) -- both are read, never written. */
#define SPCH_OBJ        0x11e5600u
#define SPCH_OBJ2       0x11e5604u

/* The words as they are in the file (verified before anything is called). */
static const uint32_t k_get[5] = {0xe59f000c, 0xe59f100c, 0xe08f0000, 0xe7910000, 0xe12fff1e};
static const uint32_t k_start[3] = {0xe92d4ff0, 0xe24dd0cc, 0xe24ddb01};

/* Let the engine's own boot get there first (30 fps: 120 frames = 4 s). */
#ifndef FIFA14_SPCH_MIN_FRAMES
#define FIFA14_SPCH_MIN_FRAMES 120ull
#endif

static struct {
  int armed;   /* config read */
  int force;   /* [audio] force_speech */
  int done;    /* the call was made (or refused) */
  int refused; /* the words did not match: nothing was called */
  unsigned long long at_frame;
} s_sp;

static int in_image(uintptr_t p, size_t len) {
  const uintptr_t lo = (uintptr_t)game_mod.load_virtbase;
  const uintptr_t hi = lo + game_mod.load_size;
  return lo != 0 && p >= lo && p + len <= hi;
}

void fifa14_spch_frame(unsigned long long frames) {
  if (!s_sp.armed) {
    s_sp.armed = 1;
    const char *v = rt_config_get("audio", "force_speech");
    s_sp.force = v && (!strcmp(v, "1") || !strcmp(v, "true") || !strcmp(v, "on"));
    debugPrintf("[spch] ==== the commentators: the engine's own speech system ====\n");
    debugPrintf("[spch] [audio] force_speech = %s (%s). The engine's speech start is the pair "
                "0x%05x (the object) + 0x%05x (the start) in libFIFA14.so, called by the engine's "
                "own audio init at 0x7ea6bc/0x7ea6c0; its PRACTICE_MODE gate was measured OPEN on "
                "the console (\"has 1 value 0\")\n",
                v ? v : "0", s_sp.force ? "the port will call that pair itself, once" : "the engine's own path only (unchanged)",
                SPCH_GET, SPCH_START);
    if (!s_sp.force)
      return;
    debugPrintf("[spch] the pack's own file names are hardcoded ENG_US (Speech/hdr_ENG_US.big "
                "sth_ENG_US.big dat_ENG_US.bh pbp_ENG_US.evt) and the LATINO pack lives in those "
                "slots: serve it with config.ini [audio] language = ENG_US so the engine's "
                "AUDIO_LANGUAGE agrees with the files it opens\n");
  }
  if (!s_sp.force || s_sp.done || s_sp.refused)
    return;
  if (frames < FIFA14_SPCH_MIN_FRAMES)
    return;
  const uintptr_t base = (uintptr_t)game_mod.load_virtbase;
  if (!base || !in_image(base + SPCH_GET, sizeof k_get) || !in_image(base + SPCH_START, sizeof k_start) ||
      !in_image(base + SPCH_OBJ, 8)) {
    s_sp.refused = 1;
    debugPrintf("[spch] the engine's image does not cover the speech start's addresses: NOTHING is "
                "called (fail-open)\n");
    return;
  }
  /* The engine's own readiness: the global the getter returns. Read, never
   * assumed -- and never written. */
  const uint32_t obj = *(const volatile uint32_t *)(base + SPCH_OBJ);
  if (!obj) {
    if (frames % 1800 == 0)
      debugPrintf("[spch] force_speech = 1: the speech global 0x%08x is still 0 (the engine's "
                  "audio system is not up): waiting\n",
                  SPCH_OBJ);
    return;
  }
  /* Everything it is about to run, verified against the file first. */
  {
    const uint32_t *g = (const uint32_t *)(base + SPCH_GET);
    const uint32_t *st = (const uint32_t *)(base + SPCH_START);
    int bad = -1;
    for (int i = 0; i < 5; i++)
      if (g[i] != k_get[i])
        bad = i;
    for (int i = 0; i < 3; i++)
      if (st[i] != k_start[i])
        bad = 100 + i;
    if (bad >= 0) {
      s_sp.refused = 1;
      debugPrintf("[spch] word %d at the speech start is NOT the file's (this is not that build): "
                  "NOTHING is called (fail-open)\n",
                  bad);
      return;
    }
    debugPrintf("[spch] force_speech = 1: calling the engine's own speech start now (frame %llu; "
                "the getter's five words and the start's three are the file's; the speech global "
                "0x%08x = 0x%08x, the word after it 0x%08x)\n",
                frames, SPCH_OBJ, (unsigned)obj,
                (unsigned)*(const volatile uint32_t *)(base + SPCH_OBJ2));
  }
  s_sp.done = 1;
  s_sp.at_frame = frames;
  {
    void *(*get)(void) = (void *(*)(void))(base + SPCH_GET);
    void (*start)(void *) = (void (*)(void *))(base + SPCH_START);
    void *o = get();
    debugPrintf("[spch] the engine's getter 0x%05x returned 0x%08x (%s)\n", SPCH_GET,
                (unsigned)(uintptr_t)o, o ? "calling the start with it" : "NULL: calling it anyway, "
                "the start checks it itself");
    start(o);
    debugPrintf("[spch] the engine's speech start returned (called by the PORT at frame %llu, ONCE). "
                "The tell is in this log below: the pack's files are opened through the port's own "
                "file layer, so `[fs] obb asked=\"Speech/...\"` lines now mean the system came up. "
                "If nothing of the sort appears, the start returned at one of its own early exits "
                "(the audio system, the [obj+0x35a8] field, or 0x49ddd4 != 1) -- and the "
                "[locale] AUDIO_LANGUAGE line above says what language it was served "
                "(ENG_US is what the pack's hardcoded file names want)\n",
                frames);
  }
}
