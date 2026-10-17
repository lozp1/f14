/* fifa14_padscope.h -- THE PAD'S KEY SCOPE: is there a game to give a pad key
 * to? (fifa14_padscope.c has the whole measured chain: which pointer, which
 * address the engine's own code reads it from, and how this port verifies that
 * chain before it reads anything.)
 *
 * config.ini [controls] pad_keys_scope = match | always.  Split out of
 * fifa14_input.c because the signal is a piece of the ENGINE (a pointer in
 * libFIFA14.so's own .bss), not a piece of the pad.
 *
 * MIT.
 */
#ifndef FIFA14_PADSCOPE_H
#define FIFA14_PADSCOPE_H

/* Resolve and VERIFY the engine's own read of its game object, once. Logs the
 * whole chain: the address, the instruction words it was verified by, and the
 * value read. Safe to call twice; nothing is gated when the verification
 * fails (fifa14_padscope_match() then answers -1). Call after the engine is
 * loaded and mapped (fifa14_input_init does). */
void fifa14_padscope_init(void);

/* 1 = the engine has a live game object (a match is up),
 * 0 = it does not (menus / nothing mounted),
 * -1 = the chain could not be verified: UNKNOWN, and every caller must treat
 *      it as "open" (fail-open: a different build of the game cannot cost the
 *      pad its keys). One word read per call; no engine call at all. */
int fifa14_padscope_match(void);

/* THE GAME OBJECT'S ADDRESS, for the code that needs the object itself (the
 * pad's own touch buttons: fifa14_padact.c reads its button rectangles). 1 and
 * *out filled when the chain is verified AND a game is up; 0 otherwise (the
 * caller must then do nothing at all). One word read; nothing is written. */
int fifa14_padscope_game(uintptr_t *out);

/* The frame hook: one line when the state changes, and a slow heartbeat, so a
 * run's log says what this signal did. `frames` is the frame loop's counter. */
void fifa14_padscope_report(unsigned long long frames);

#endif /* FIFA14_PADSCOPE_H */
