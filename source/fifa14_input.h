/* fifa14_input.h -- the input: the console's touchscreen and the pad, delivered
 * to the engine the way Android's Java delivers them (fifa14_input.c).
 *
 * WHAT THE ENGINE LISTENS TO (measured, see the .c's header for the whole
 * chain): the game's own exported native
 *   Java_com_ea_blast_TouchSurfaceAndroid_NativeOnPointerEvent @0x00e0620c
 * which is the single entry Android's Java view calls for every touch, and the
 * platform's raw-pointer action codes come from the engine's own
 * NativeGetIdRawPointer* @0x00e062f0..0x00e06318:
 *   down 0x0006000E, up 0x0008000E, move 0x0004000E, cancel 0x0000000E
 * and the module id of the touch device from
 * NativeGetModuleTypeIdTouchScreen @0x00e01bb8 -> 1000.
 * The port calls that native; it does NOT re-implement the engine's pointer
 * bookkeeping (TouchSurface::OnRawTouch*), which is where the std pointer ids
 * and the coordinate transformation live.
 *
 * So: the console's touchscreen is the phone's screen (handheld), and the pad
 * stands in for it in the dock (left stick moves the port's cursor, A clicks,
 * B is the second finger, +/- is Android's BACK).
 *
 * THE PAD'S KEY SCOPE (2026-10-13): config.ini [controls] pad_keys_scope =
 * match | always decides WHEN the pad's KEYS (the engine's keycodes, the k_keys
 * table in the .c) are delivered. "match" -- the default -- delivers them only
 * while the engine has a live game object, so the buttons cannot act in the
 * menus; "always" is the port's behaviour before this lever. The signal and its
 * verification live in fifa14_padscope.c; the gate is fail-open (an
 * unverifiable signal delivers every key). The touch layer the .c describes
 * (right stick cursor, StickR, StickL) and +/- (BACK) are never scoped: the
 * menus are worked with those.
 *
 * MIT.
 */
#ifndef FIFA14_INPUT_H
#define FIFA14_INPUT_H

/* 1: the console's touchscreen (handheld) becomes the engine's pointer events.
 * The panel reports 0..1280 x 0..720 (the same pixels the framebuffer uses),
 * which is the space the engine's TouchSurface is sized in. */
#ifndef FIFA14_INPUT_TOUCH
#define FIFA14_INPUT_TOUCH 1
#endif

/* 1: the pad: every button is the engine's OWN gamepad key -- A/B/X/Y, L/R,
 * ZL/ZR, the cross and the left stick (as the D-pad: the engine has no axis
 * input at all), + = START, - = SELECT -- delivered through the engine's own
 * KeyboardAndroid key natives, which is the same call Android's
 * MainActivity.onKeyDown makes. The parts of the pad the engine has no use for
 * keep the touch layer reachable: the RIGHT stick is the cursor, StickR a
 * click, StickL the second finger. See the .c's header for the whole measured
 * chain and the engine's keycodes. */
#ifndef FIFA14_INPUT_PAD
#define FIFA14_INPUT_PAD 1
#endif

/* The pad's cursor, in the engine's pixels per frame at full deflection (the
 * engine runs at 30 fps: 12 px/frame crosses 1280 in ~3.5 s), and the stick's
 * dead zone. The response is squared, so small deflections are precise. */
#ifndef FIFA14_INPUT_CURSOR_SPEED
#define FIFA14_INPUT_CURSOR_SPEED 12.0f
#endif
#ifndef FIFA14_INPUT_CURSOR_DEADZONE
#define FIFA14_INPUT_CURSOR_DEADZONE 0.22f
#endif

/* How often the [input] summary line is written, in frames (the engine's). */
#ifndef FIFA14_INPUT_SUMMARY_FRAMES
#define FIFA14_INPUT_SUMMARY_FRAMES 300u
#endif

/* 1: log EVERY delivered event, not only the first of each kind (very noisy:
 * one line per touch sample). For a run that has to show what the panel does. */
#ifndef FIFA14_INPUT_TRACE
#define FIFA14_INPUT_TRACE 0
#endif

/* Resolve the engine's pointer entry points, bring the touchscreen up, set the
 * controllers up and log what was found. Call once, after the boot's step 10
 * (the engine's state is 2 by then: the pointer native checks it and drops
 * every event that arrives earlier). Safe to call twice. */
void fifa14_input_init(void);

/* One scan of the touchscreen and the pad, called once per frame from the frame
 * loop BEFORE the frame is drawn, so the engine consumes the message the
 * pointer native posts during the frame it belongs to. */
void fifa14_input_poll(void);

/* Forget every finger (on focus loss: Android's Activity gets ACTION_CANCEL). */
void fifa14_input_release_all(void);

/* A TOUCH THE PORT PRESSES FOR THE PLAYER (fifa14_padact.c): one pointer event
 * on the engine's own on-screen buttons, through the very native the panel
 * uses (see the .c's header). `raw_id` must be one of the bridge's own ids
 * (fifa14_padact.c's 120+button), `down` 1/0. Does nothing when the engine's
 * pointer entry is not in hand: fail-open, never a crash. */
void fifa14_input_inject_touch(int raw_id, int down, float x, float y);

#endif /* FIFA14_INPUT_H */
