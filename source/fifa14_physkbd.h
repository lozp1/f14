/* fifa14_physkbd.h -- the engine's PhysicalKeyboard module: the last link
 * between the port's keys and the game. (fifa14_physkbd.c has the whole
 * measured chain: which object it is, what turns it on, and the exact
 * instruction, in the engine, that drops a key when it is not.)
 *
 * MIT.
 */
#ifndef FIFA14_PHYSKBD_H
#define FIFA14_PHYSKBD_H

/* 1: the pad's keys are ALSO delivered by calling the module's own
 *    OnRawKeyDown/OnRawKeyUp (the decisive experiment: it is the call the
 *    engine's message dispatch would make, with the routing taken out).
 * 0: that path is off.
 * See FIFA14_INPUT_KEY_ROUTED below: the two are two PATHS, and the round of
 * 2026-10-13 (the module identified and enabled) turns both on so one run's
 * log and one look at the screen say which of them the game answers to. */
#ifndef FIFA14_INPUT_KEY_DIRECT
#define FIFA14_INPUT_KEY_DIRECT 1
#endif
/* 1: the pad's keys are delivered the Android way as well -- KeyboardAndroid's
 * natives, so the engine posts its own 0x00040008/0x00080008 messages and its
 * ModuleManager routes them to the module (which now exists and is enabled:
 * on a phone this is the ONLY path there is).
 * 0: off. Both paths carry their own counter in the log ("direct calls N = D
 * down + U up", "routed calls N = D down + U up"), so a run says what each one
 * delivered. BOTH BEING ON IS SAFE, and that is measured, not hoped: the
 * engine's own OnRawKeyDown @0x00df0ba8 inserts the key into the pressed set
 * (`bl <hashtable::DoInsertValue>` at 0xdf0bf8) and RETURNS EARLY when the key
 * was already there

 *   df0bfc  ldrb r0,[sp,#16]        ; DoInsertValue's "inserted" flag
 *   df0c00  cmp r0,#0; beq <return> ; already pressed -> no second notify
 *
 * -- so a key delivered twice (once per path) notifies the game ONCE. */
#ifndef FIFA14_INPUT_KEY_ROUTED
#define FIFA14_INPUT_KEY_ROUTED 1
#endif

/* How often the module's enable flag is written to the log, in frames (the
 * engine's own). The first FIFA14_PKBD_REPORT_EARLY frames are reported at
 * this rate, then it is FIFA14_PKBD_REPORT_FRAMES -- enough to watch the flag
 * change without filling the log. */
#ifndef FIFA14_PKBD_REPORT_FRAMES
#define FIFA14_PKBD_REPORT_FRAMES 30u
#endif
#ifndef FIFA14_PKBD_REPORT_EARLY
#define FIFA14_PKBD_REPORT_EARLY 1800u
#endif
#ifndef FIFA14_PKBD_REPORT_SLOW
#define FIFA14_PKBD_REPORT_SLOW 300u
#endif

/* Find the engine's PhysicalKeyboard module -- resolving gSystem, its
 * ModuleManager and asking it for module type 600, the engine's own constant
 * -- and identify the object BY ITS TABLE'S ENTRIES (the engine's own
 * OnRawKeyDown/OnRawKeyUp/IsEnabled/SetEnabled/HandleMessage/
 * OnLifeCycleFocusGained, resolved by name, at the offsets measured out of the
 * .so; see the .c for why the first word against one class's vtable symbol is
 * NOT a test: the engine's GetModule hands back the class's +4 interface
 * pointer, and the phone's class is the DERIVED PhysicalKeyboardAndroid).
 * If the engine has not built the module, this is what builds it: on a phone
 * the module is instantiated on demand, and nothing in this port ever asked
 * for it. Then it reads the module's IsEnabled flag (byte +0x90 of the object,
 * the byte SetEnabled writes and OnRawKeyDown demands) and, when it is 0,
 * delivers the life-cycle call that turns it on.
 * Logs every step. Returns 1 when the module is in hand. Safe to call twice. */
int fifa14_physkbd_init(void);

/* 1 once fifa14_physkbd_init() has the module. */
int fifa14_physkbd_ready(void);

/* The module's IsEnabled flag: 1, 0, or -1 when the module is not in hand. */
int fifa14_physkbd_enabled(void);

/* The module as the engine's own API hands it back (the +4 interface pointer)
 * and the object itself (what OnRawKeyDown's `this` has to be; the engine's
 * GetModule returns the former, MEASURED 2026-10-13). NULL until init() has it,
 * and fifa14_physkbd_shift() is -4 when the two differ. Only for the log. */
void *fifa14_physkbd_module(void);
void *fifa14_physkbd_object(void);
int fifa14_physkbd_shift(void);

/* One key, straight into the module (the engine's OnRawKeyDown/OnRawKeyUp
 * @0x00df0ba8 / @0x00df0d10), `android_key` being an Android KeyEvent code
 * exactly as KeyboardAndroid's natives take it. `down` picks the direction.
 * Does nothing when the module is not in hand. */
void fifa14_physkbd_raw_key(int down, int android_key, int alt);

/* THE DIRECT PATH'S COUNTERS (the "direct calls N = D down + U up" line):
 * the call count, and the two halves through the out-parameters. */
unsigned fifa14_physkbd_direct_calls(unsigned *down, unsigned *up);

/* The once-in-a-while line: the module, its mapper and its IsEnabled flag, and
 * the direct-call counters. Called from the frame loop. */
void fifa14_physkbd_report(void);

#endif /* FIFA14_PHYSKBD_H */
