/* fifa14_spch.h -- THE COMMENTATORS: the engine's own speech system, started by
 * the port when the lever asks (fifa14_spch.c has the measured chain: what the
 * engine's speech start reads, its own gates, and why PRACTICE_MODE was NOT
 * the one that was in the way).
 *
 * config.ini [audio] force_speech = 0 | 1   (default 0)
 *
 * MIT.
 */
#ifndef FIFA14_SPCH_H
#define FIFA14_SPCH_H

/* One frame: with [audio] force_speech = 1, call the engine's speech start
 * ONCE, as soon as the engine's own readiness is there (the frame the speech
 * global exists), with every instruction word verified before the call and
 * every step logged. Does nothing at all with the lever at 0. Called from the
 * frame loop (fifa14_boot.c), next to the other frame hooks. */
void fifa14_spch_frame(unsigned long long frames);

#endif /* FIFA14_SPCH_H */
