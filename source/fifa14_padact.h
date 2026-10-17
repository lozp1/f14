/* fifa14_padact.h -- THE PAD'S ACTIONS IN A MATCH: the engine's own on-screen
 * touch buttons, pressed for the player (fifa14_padact.c has the whole
 * measured story: WHERE the button rectangles live in the engine's game
 * object, the code that hit-tests them, and why this is the only path the
 * match listens to).
 *
 * config.ini:
 *   [controls] pad_touch_buttons = on | off   (default on)
 *   [controls] pad_touch_map = "a:0,b:1,x:2,y:3"  (pad button : engine rect)
 *
 * MIT.
 */
#ifndef FIFA14_PADACT_H
#define FIFA14_PADACT_H

#include <stdint.h>

/* Arm the bridge: read config.ini once and log what it will do. Called by
 * fifa14_input_init() (nothing happens until a match is up and the rects have
 * been verified). Safe to call twice. */
void fifa14_padact_arm(void);

/* One pad sample (the raw button word, the same one fifa14_input.c reads).
 * Buttons that are not the pad's KEYS (the face buttons, when they are mapped)
 * become touches on the engine's own touch buttons: DOWN while held, UP on
 * release. Everything else is ignored. Called once per frame from the input
 * poll; does nothing outside a match. */
void fifa14_padact_poll(uint64_t buttons);

/* Let go of every touch this bridge is holding: the game object went away
 * (back to the menus) or the applet lost focus. Called by
 * fifa14_input_release_all() and whenever the match ends. */
void fifa14_padact_release_all(void);

/* One [padact] line, on a slow cadence and on every change: which engine
 * rectangle each mapped pad button is pressing, and the surface it is pressed
 * in. `frames` is the frame loop's counter (via fifa14_input_poll). */
void fifa14_padact_report(unsigned long long frames);

#endif /* FIFA14_PADACT_H */
