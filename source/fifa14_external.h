/* fifa14_external.h -- the game's external storage (/sdcard/Android/data/
 * com.ea.game.fifa14_row/...), which dcr_path.c turns into
 * <game folder>/external/..., served from the OBB when it is not on the card.
 *
 * The engine asks for its data there and its own file layer
 * (rw::core::filesys::PosixFileDeviceDriver::Open) opens it by path; the port
 * translates the path to a file that does not exist on the SD card, open()
 * gives ENOENT, the engine stores a NULL file handle and aborts --
 *     pc  libFIFA14.so+0x772988   (str r0,[r5,#8], r5 = 0)
 *     lr  libFIFA14.so+0xec0984   FileSystem::LoadWithOffset+0x110
 *     [io] open(/sdcard/Android/data/com.ea.game.fifa14_row/data/sprites/
 *               fifa14_splash_wide.sm2) -> ...: errno 2
 * -- while that very file IS inside the OBB. This module reads the OBB's file
 * table at boot (fifa14_external_init(), called by fifa14_obb_init()) and, in
 * port_path_fixup()'s last step, puts the entry on the SD card the first time
 * the engine opens it, so the open finds an ordinary file.
 *
 * fifa14_external.c has the container's layout, read out of the real file
 * (magic 45 42 00 03, 1217 entries, djb2 hash of the path) and the evidence.
 * MIT. */
#ifndef FIFA14_EXTERNAL_H
#define FIFA14_EXTERNAL_H

#include <stddef.h>
#include <stdint.h>

/* Read the OBB's file table (the container's index) into RAM: the entry count,
 * the paths' hashes, the data offsets and the sizes. Needs the OBB open
 * (fifa14_obb_init()); log one line per step and a sample of what was read.
 * Returns the number of entries, or -1 (logged: not open, not an EA "EB"
 * container, or a table that does not fit). */
int fifa14_external_init(void);

/* The number of entries in the table (0 before fifa14_external_init()). */
int fifa14_external_count(void);

/* The container's own hash of a path inside it: djb2 (H = H*33 + c, H0 =
 * 5381) over "data/sprites/x.sm2", without a leading slash. Exported so the
 * init's self-check and any tool can ask the same question the game's engine
 * does. */
uint32_t fifa14_external_hash(const char *path);

/* port_path_fixup()'s half of this: `real` is a translated path (or the
 * engine's own Android path when no rule matched). When it is in the game's
 * external storage and the file is not on the card but IS in the OBB, the
 * entry is written out and the path that holds it is returned (in `out`, when
 * it does not fit `cap` the open is left as it was). Returns `real` when it
 * has nothing to say: not an external path, not in the OBB, or already on the
 * card. Every request is logged once (budgeted) and counted. */
const char *fifa14_external_fixup(const char *real, char *out, size_t cap);

/* What every external open did, and the distinct paths asked for, one line
 * each (port_run() calls it, next to fifa14_assets_report()). */
void fifa14_external_report(void);

/* Called at the end of fifa14_external_init(): asks the table the paths the
 * engine asks for (the crash log's own file among them), hashes them and
 * reads the first bytes of each entry back from the OBB, so one run's log says
 * whether the external data can be found BEFORE the engine asks. `why` is only
 * for the log. Nothing is written to the card. */
void fifa14_external_selfcheck(const char *why);

/* THE AUDIO LANGUAGE. The OBB's ini/na/locale.ini points the engine's
 * commentators at ENG_US (and at an [AUDIO_LANGUAGES] tag, 'mex', that has no
 * file in the container), while data/audio/speech/SPA_ES.mp3 IS there. This
 * patches AUDIO_LANGUAGE and the SPANISH audio tag to SPA_ES on the card --
 * idempotent, before the engine reads it -- and leaves the TEXT language alone.
 * The path is in is_user_override(), so the container's copy is never
 * re-extracted over the patch. Returns the number of lines changed, or -1
 * (logged). Call after fifa14_external_init(). */
int fifa14_locale_patch(void);

/* THE COMMENTARY PACK, ONCE, IN THE LOG (fifa14_external.c's is_user_override
 * covers the whole "Speech/" directory: a pack the player installs there is
 * served as it is and is never written over). FIFA 14 Android ships no
 * commentary -- its speech is EA::ContentManager content downloaded into
 * <external>/Speech/ -- so this lists the names the engine asks for and the
 * sizes the pack's own .cmmetadata declares, which is the one line a log needs
 * to say whether the commentators are installed. Reads the card, writes
 * nothing, and returns how many of the pack's files are there. Call after
 * fifa14_external_init(). */
int fifa14_speech_pack_report(void);

/* ANY external file, ready for the port to read and patch (2026-10-13, for
 * fifa14_ctlcfg.c's two ini files): <game>/external/<rel> into `dst`, taking
 * the entry out of the OBB first when the file is not on the card yet. 0 on
 * success, -1 (logged) when the table is not up, the path does not fit, or the
 * file is neither on the card nor in the container. `cap` is EXT_DST_MAX-sized
 * by the caller's convention (DCR_PATH_MAX + 320). Call after
 * fifa14_external_init(). */
int fifa14_external_prepare(const char *rel, char *dst, size_t cap);

/* The same path, WITHOUT touching the container: for a watcher that only wants
 * to stat() a file which may not be on the card yet. 1 when it fits. */
int fifa14_external_path(const char *rel, char *dst, size_t cap);

#endif /* FIFA14_EXTERNAL_H */
