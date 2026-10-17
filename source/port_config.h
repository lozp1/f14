/* port_config.h -- FIFA 14's settings for the android32 runtime.
 *
 * Macros only: the runtime's C files, its assembly (exc32.S) and the 64-bit
 * launcher all read this (runtime/source/rt_settings.h). What each setting does
 * is next to its default in the runtime; the layout follows the two pilot ports
 * (dcr_sea_nx, tasm2_nx) and runtime/docs/. MIT.
 *
 * FIFA 14 (EA Mobile) -- engine: EA NIMBLE. Package com.ea.game.fifa14_row,
 * version 1.3.6 (versionCode 136), armeabi-v7a (ELF32 ARM, softfp).
 *
 * The SD folder is deliberately "/switch/fifa14" (PORT_ROOT_PATH, below),
 * WITHOUT the "_nx" that PORT_NAME/PORT_BANNER keep: PORT_NAME is the
 * program's INTERNAL name (the NRO, the .nsp/.build files and the payload's
 * romfs contract with the launcher), while the folder is only where the
 * game's data lives on the card. PORT_ROOT_PATH is the supported way to
 * split the two (its default is "/switch/" PORT_NAME, rt_settings.h); the
 * payload/program name is unchanged.
 */
#ifndef PORT_CONFIG_H
#define PORT_CONFIG_H

/* ------------------------------------------------------------------ the game */
#define PORT_TITLE   "FIFA 14"
#define PORT_NAME    "fifa14_nx"
#define PORT_PACKAGE "com.ea.game.fifa14_row"
#define PORT_BANNER  "fifa14_nx: FIFA 14 (EA Nimble engine, armeabi-v7a)"

/* The game folder on the SD card (sdmc:/switch/fifa14). Overrides the runtime
 * default "/switch/" PORT_NAME (runtime/source/rt_settings.h). Chosen WITHOUT
 * the "_nx" suffix on purpose: the folder name is what the player sees on the
 * card, and PORT_NAME/PORT_PAYLOAD_NAME keep "_nx" because they are the
 * program's internal name (NRO, .nsp, .build and the payload's romfs contract
 * the launcher matches on -- see launcher/Makefile). Only the folder changes. */
#define PORT_ROOT_PATH "/switch/fifa14"

/* Where the game's native libraries are inside the APK. The APK in hand
 * ("FIFA 14 - 1.3.6 - Mod Apk by apkdog.apk") ships them in lib/armeabi/
 * (a legacy folder name; the binaries are the same ELF32 ARM/ARMv7 ones that
 * newer builds put in lib/armeabi-v7a/ -- verified: identical uncompressed
 * sizes and ctype 8):
 *   libFIFA14.so        18,187,340 B  (the game; imports 122 EA::Nimble*)
 *   libnimble.so           352,780 B  (the engine; exports them)
 *   libgnustl_shared.so  1,142,099 B  (GNU libstdc++, C++ runtime)
 * Load order (libFIFA14.so NEEDED libnimble.so + libgnustl_shared.so; the
 * engine's libnimble.so NEEDED libgnustl_shared.so): gnustl -> nimble -> FIFA14. */
#define PORT_ABI_DIR "lib/armeabi/"

/* The name the game's APK is given (or looked for first) in the folder. */
#define PORT_APK_DEFAULT_NAME "FIFA14.apk"

/* The APK, by what is in it: its package, its versionCode (136, which also names
 * the OBB) and the three .so under PORT_ABI_DIR. A file is FIFA 14's when all
 * three libraries are present; the package/versionCode decide between versions
 * (the newest supported one is adopted to FIFA14.apk). */
#define PORT_APK_DESC "FIFA 14 (com.ea.game.fifa14_row 1.3.6, versionCode 136, armeabi-v7a)"
#define PORT_APK_ROLES \
  {.what = "FIFA 14 (the game)", .name = PORT_APK_DEFAULT_NAME, \
   .package = PORT_PACKAGE, .version_code = 136, .flags = RT_APK_ADOPT, \
   .need = (const char *const[]){PORT_ABI_DIR "libFIFA14.so", \
                                 PORT_ABI_DIR "libnimble.so", \
                                 PORT_ABI_DIR "libgnustl_shared.so", NULL}}

/* The reserved region the three modules are mapped into, end to end. Measured
 * from the game's own libraries (highest p_vaddr + p_memsz of each, summed):
 *   libFIFA14.so        0x013d8098  (20,807,832 B)
 *   libgnustl_shared.so 0x000c48c9  (   805,065 B)
 *   libnimble.so        0x00056e5c  (   355,932 B)
 *   sum = 0x014f37bd = 21,968,829 B = 20.95 MiB -> 32 MiB minimum.
 * 64 MiB reserved for headroom (the pilot ports over-reserve too: dcr, ~22 MB
 * mapped, reserves 48 MiB). An under-reserved region shows up as so_load() -3. */
#define PORT_SO_REGION_BYTES (64u * 1024 * 1024)

/* Earlier names of the game folder, moved into PORT_ROOT_PATH on the first
 * start. DELIBERATELY LEFT COMMENTED: the folder was renamed on the card by
 * hand (an instant rename in the file manager), so no automatic migration is
 * wanted -- enabling this would make the first launch COPY the whole 1.39 GB
 * folder instead of using it in place. The earlier name was "/switch/fifa14_nx". */
/* #define PORT_OLD_ROOT_PATHS "/switch/fifa14_nx" */

/* ------------------------------------------------------------------ launcher */
#define PORT_SETUP_NOTE          "Getting the game ready (after an install or an update)"
#define PORT_LAUNCHER_START_NOTE "(the first start unpacks the game's libraries from the APK)"
#define PORT_LAUNCHER_BYLINE     "an unofficial android32 port (runtime by aks796)"

/* ------------------------------------------------------------------ platform */
/* FIFA 14 is landscape; 1280x720 is the Switch's handheld and docked window. */
#define RT_SCREEN_W 1280
#define RT_SCREEN_H 720
#define RT_PAD_MAX_PLAYERS 1 /* single player (TODO: confirm whether the build has local co-op) */

/* OpenSL ES: 0 = slCreateEngine refuses (android_ndk.c), so the game falls back
 * to its Java/AudioTrack path; 1 = opensles.c implements it on audout.
 * PROVISIONAL 0 -- decide once the engine's audio path is traced (NOTES.md). */
#define RT_OPENSLES 0

/* ------------------------------------------------------------------ files */
/* EA mobile titles open their data relative to the app's working directory
 * (/data/data/<pkg>) and read the expansion from Android/obb/<pkg>/. The runtime
 * resolves relative paths against dcr_cwd() and maps shared storage under the
 * game folder (RT_PATH_*). */
#define RT_PATH_CWD_RELATIVE 0
#define RT_DIRCACHE 0
/* Where the folders Android would have go inside the game folder. The last one
 * is the primary external directory the engine's own EA::IO::StorageDirectory
 * asks Java for (Environment.getExternalStorageDirectory() = /sdcard, answered
 * by fifa14_blast_stubs.c's fifa14_h_storage_external): dcr_path.c maps any
 * /sdcard/... path to <game folder>/sdcard/..., so that folder must exist or
 * the engine's first write there fails. */
#define RT_PATH_EXTRA_DIRS "data", "files", "cache", "obb", "Android/obb/" PORT_PACKAGE, "sdcard"
#define RT_PATH_TRACE_EXTRA ".obb" /* an OBB open is worth a log line */

/* ---- the [fs] file-opening trace -------------------------------------------
 * Every path-based file operation the engine's libc calls reach logs one line
 * with BOTH paths (the one the game asked for and the one the SD card was
 * asked for), the flags, the result, and -- on a failure -- the engine function
 * that made the call:
 *
 *   [fs] WIDE    wide=".../assets/adview.html" -> char="assets/adview.html" (n=4096 rc=18)
 *   [fs] dcr     in="assets/adview.html" -> out="sdmc:/switch/fifa14/assets/adview.html" mapped=1
 *   [fs] assets  asked="adview.html" real="sdmc:/switch/fifa14/assets/adview.html" status=served from the APK now served=1
 *   [fs] open    asked="assets/adview.html" real="sdmc:/switch/fifa14/assets/adview.html" flags=0x0 rc=-1 errno=2 (FAILED) from=libFIFA14.so+0x... (fn+0x...)
 *
 * debug.log is flushed (fsync) per line, so the last line before a crash is on
 * the card. Each has its own budget (bionic_io.c: RT_IO_TRACE_*, dcr_path.c:
 * RT_PATH_TRACE) so a per-frame open cannot flood the log; set any of them to 0
 * to silence that family of lines. */
#define RT_IO_TRACE_OPEN 1
#define RT_PATH_TRACE    1

/* ------------------------------------------------------------------ frames */
/* CONFIRMED ON HARDWARE (2026-10-13): the engine does NOT swap. With the port's
 * context current (fifa14_gl_boot) and the engine's frame loop running ~4500
 * frames at 30 fps, dcr_gl_frames() stayed 0 and debug.log had no
 * "[egl] eglSwapBuffers" line at all: libFIFA14.so imports eglSwapBuffers but
 * never calls it. On the phone the swap is the Java GLSurfaceView's (its
 * GLThread's EGLHelper.swap(), after the Renderer's onDrawFrame); the native
 * engine only paints the back buffer. So the port loop owns the frame AND the
 * present: it brackets each frame (dcr_boost) and calls fifa14_gl_swap() after
 * every engine frame (fifa14_boot.c, fifa14_gl.c) -- the Java step. Keeping 0
 * means b_eglSwapBuffers does not re-bracket the boost (the loop already does),
 * so the engine's own pacing is untouched; if the engine ever swaps itself too,
 * this stays correct. */
#define RT_GL_SWAP_ENDS_FRAME 0
#define RT_MAIN_THREAD_NAME   "main" /* TODO: the engine's render thread's name */

/* ------------------------------------------------------------------ watchdog */
#define RT_WATCHDOG_PRIO 0x2B
#define RT_WATCHDOG_CORE -2
/* 1: the watchdog also reports a stop BEFORE the frame loop (rt_applet.c's
 * phase), i.e. a hang inside the boot itself -- the constructors, NativeOnCreate
 * or the engine's Init on the first frame (fifa14_boot.c). The bring-up value:
 * the first console run then ends its log with which step it stopped in,
 * instead of stopping at the last line the ring happened to reach the card.
 * The port starts the watchdog (and defines dcr_boot_frames) in port_run. */
#define RT_WATCHDOG_BOOT 1

/* ------------------------------------------------------------------ misc */
/* Emulator fixups (instructions Ryujinx's A32 decoder lacks). 1 = on; a no-op
 * on hardware. */
#define RT_EMU_FIXUPS 1

#endif /* PORT_CONFIG_H */
