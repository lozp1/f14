/* fifa14_ctlcfg.h -- the game's own CONTROL SCHEME: the two ini files the
 * engine reads it from, the levers in the port's config.ini that move them,
 * and the log line that says which scheme is active.
 *
 * MEASURED, in libFIFA14.so (18,187,340 B, the same file as always):
 *
 *   - the on-screen buttons are NOT a device capability: the keys the engine
 *     reads for them are names of its own settings ("CONTROL_CONFIG/BIG_BUTTONS"
 *     @0x1003098, "CONTROL_CONFIG/BIG_DPAD" @0x10030c8,
 *     "CONTROL_CONFIG/FLOATING_DPAD" @0x1003036,
 *     "CONTROL_CONFIG/ALTERNATE_CONTROLS" @0x10030f2,
 *     "CONTROL_CONFIG/BASIC_HAND" @0x100306a,
 *     "CONTROL_CONFIG/SCHEME_ID_P0" @0x1003c97 / "_P1" @0x1003cb3,
 *     "CONTROL_CONFIG/LAYOUT_ID" @0x100419d,
 *     "CONTROL_CONFIG/SCHEME_ID_LOCAL" @0x101f553), and they are read with the
 *     engine's own settings readers -- "EA::AIP::CmdDecomposer::GetIntByName"
 *     @0xa26b44 (the profile/command string) and the static-registry reader
 *     @0x340648 ("does it exist" @0x340acc + "value" @0x340648).
 *
 *   - WHERE THEY COME FROM, on the card: the engine loads a fixed list of 12
 *     ini files at boot -- the name table is IN the .so at 0x10afc50
 *     (common.ini, ai.ini, sku.ini, locale.ini, emotion.ini, career.ini,
 *     training.ini, player.ini, controls.ini, fame.ini, easfc.ini, fut.ini,
 *     the "controls.ini" pointer at 0x10afc70) and builds each path with
 *     "ini/%s/%s" @0x10085bf (the loader @0x55c6c0, which then reads
 *     CONTROL_CONFIG/... back through 0x340648). The port serves those files
 *     out of the OBB into <game>/external/ini/na/..., so patching them on the
 *     card is patching what the engine loads. The blocks, as the OBB ships
 *     them:
 *
 *       ini/na/controls.ini  [CONTROL_CONFIG]  SCHEME_ID_P0 = 0   (0 = two
 *                            buttons + sprint, 1 = three buttons), SCHEME_ID_P1,
 *                            LAYOUT_ID = 0 (0..3), RENDER_DEBUG = 0,
 *                            RENDER_DEBUG_SETPIECE = 0, USE_SIDE_BUTTONS = 0
 *       ini/na/common.ini    [SETTINGS]        FLOATING_DPAD = 0,
 *                            BASIC_HAND = 0, BIG_BUTTONS = 1, BIG_DPAD = 1,
 *                            ALTERNATE_CONTROLS = 0, ACTION_TAKEN_TEXT_ENABLED,
 *                            ASSISTED_TACKLE, FIRST_TOUCH, TUTORIAL_DPAD ...
 *
 *   MIT.
 */
#ifndef FIFA14_CTLCFG_H
#define FIFA14_CTLCFG_H

/* PATCH what the engine will be served, from config.ini's [controls] rows
 * (fifa14_config.c): layout_id, scheme_id, big_buttons, big_dpad,
 * floating_dpad, alternate_controls, basic_hand, render_debug. Each row is
 * "keep" (the default: the file is left byte for byte alone) or a number.
 * The two files are patched on the card, in place, line by line; a file that
 * already has the wanted values is not written at all. Idempotent, and every
 * effective value is logged. Call after fifa14_external_init() -- it needs the
 * OBB table to take a file out of the container when it is not on the card
 * yet. Returns the number of lines changed, or -1 (logged). */
int fifa14_ctlcfg_patch(void);

/* LOG the effective values of the control-scheme keys in both files, as they
 * are ON THE CARD right now (this is what the engine is served). `why` labels
 * the line. Reads, writes nothing. Returns the number of keys found. */
int fifa14_ctlcfg_report(const char *why);

/* THE ENGINE'S OWN ANSWER, once in a while: asks libFIFA14.so's own readers
 * (the "does it exist" one and the value one, both reached from an exported
 * symbol by a measured offset, and both VERIFIED by their first two
 * instruction words before they are called) what the live registry says for
 * each CONTROL_CONFIG key. This is the value the engine's own control code
 * reads -- the .ini is what it is served, this is what it resolved. Logs the
 * reason and does nothing when the readers cannot be verified. */
void fifa14_ctlcfg_engine_report(void);

/* The frame hook (fifa14_boot.c's loop): the engine report on a slow cadence,
 * and the file report again whenever either file's size or mtime changes (the
 * game writes its settings back when the player changes them in-game, so a
 * change in the log is the game's own doing). `frames` is the loop's counter. */
void fifa14_ctlcfg_frame(unsigned long long frames);

#endif /* FIFA14_CTLCFG_H */
