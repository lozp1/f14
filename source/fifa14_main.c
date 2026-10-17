/* fifa14_main.c -- FIFA 14's part of the boot (the runtime's main.c runs the
 * rest: runtime/source/main.c).
 *
 * port_load() : from the APK to the game's first code -- the three modules
 *   unpacked and loaded IN ORDER (libgnustl_shared.so -> libnimble.so ->
 *   libFIFA14.so), then bound. The order is libFIFA14.so's DT_NEEDED order and
 *   is load-bearing: libFIFA14.so imports 122 EA::Nimble* symbols that only the
 *   REAL libnimble.so exports, and libnimble.so in turn needs
 *   libgnustl_shared.so. That is why the modules are all loaded BEFORE any of
 *   them is resolved (resolve_module() below).
 *
 * port_run() : the constructors in Android's order, then the game itself --
 *   the JNI environment, JNI_OnLoad, the natives bound by symbol, initJNI,
 *   NativeOnCreate, runEntryPoint, the engine's init frame, the surface and the
 *   NativeOnDrawFrame loop. All of that is fifa14_boot.c, whose header is the
 *   order and the reason for each step (read out of libFIFA14.so's
 *   disassembly); this file only brings the modules up and calls it.
 *
 * STILL TODO (see NOTES.md and the first run's log):
 *   - fifa14_offsets.h: the engine offsets/patches (none derived yet);
 *   - the Mesa/GL hooks (gl_layer.h / gl_mesa.c) the engine's libEGL/libGLESv2
 *     imports need;
 *   - the ModuleCatalog natives (ds_loader.c's pattern: the port tells the
 *     engine which input modules exist);
 *   - audio (RT_OPENSLES vs. the Java/AudioTrack fallback);
 *   - input (the Switch pad -> the engine's touch/pad input);
 *   - __gnu_Unwind_Find_exidx for libgnustl_shared.so's C++ exception unwinding;
 *   - the extra 32-bit bionic shims the engine turns out to need.
 * MIT.
 */
#include <stdio.h>
#include <string.h>
#include <switch.h>

#include "config.h"
#include "dcr_path.h"
#include "dcr_setup.h"
#include "error.h"
#include "fifa14_assets.h"
#include "fifa14_boot.h"
#include "fifa14_external.h"
#include "fifa14_jni_tables.h"
#include "fifa14_obb.h"
#include "imports.h"
#include "rt_applet.h"
#include "rt_boot.h"
#include "so_util.h"
#include "util.h"

/* The three game modules, in load order. */
so_module gnustl_mod, nimble_mod, game_mod;

/* ----------------------------------------------------------------- modules */
static int load_module(so_module *mod, const char *name) {
  char path[512];
  rt_root_path(path, sizeof path, name); /* <game folder>/<name> */
  int rc = so_load(mod, path, NULL, PORT_SO_REGION_BYTES);
  if (rc < 0) {
    const char *why = rc == -1 ? "cannot open it, or it is not a 32-bit ARM ELF"
                    : rc == -2 ? "out of memory"
                    : rc == -3 ? "larger than PORT_SO_REGION_BYTES"
                    : rc == -4 ? "too many program headers"
                               : "?";
    debugPrintf("[boot] so_load(%s) failed rc=%d: %s\n", path, rc, why);
    return -1;
  }
  so_relocate(mod);
  debugPrintf("[boot] staged %-22s %6u KB at %p\n", name,
              (unsigned)(mod->load_size >> 10), mod->load_base);
  return 0;
}

/* Imports are bound only once ALL THREE modules are staged: libFIFA14.so binds
 * its 122 EA::Nimble* imports to libnimble.so's exports, which so_resolve finds
 * among the other already-loaded modules (through port_import_interpose).
 * Resolving libFIFA14.so before libnimble.so is loaded would leave all 122 of
 * them unresolved. */
static void resolve_module(so_module *mod, const char *name) {
  int missing = so_resolve(mod, dcr_imports, dcr_imports_count, /*taint=*/1);
  debugPrintf("[boot] resolved %-22s %6u KB  staged %p -> code %p  (%d unresolved)\n",
              name, (unsigned)(mod->load_size >> 10), mod->load_base,
              mod->load_virtbase, missing);
}

/* ------------------------------------------------------------------ port_load */
int port_load(const char *apk) {
  /* libgnustl_shared.so / libnimble.so / libFIFA14.so and classes.txt, from the
   * APK when they are missing or it has changed (source/fifa14_storage.c's
   * port_setup_plan; stamped in .setup). */
  dcr_setup_from_apk(apk);

  /* The game's data, before the game itself: the expansion file (1.39 GB, the
   * bulk of it) and the APK's assets/. Both are only reported and indexed here;
   * the engine opens them through its own EA::IO / AssetManager layer, which
   * bionic_io.c + dcr_path.c serve from the card (the OBB: fifa14_obb.c). A
   * failure is NOT fatal: the game starts anyway and its own file errors say
   * what it wanted -- that is the point of the first run's log. */
  fifa14_obb_init();
  fifa14_assets_init();

  /* IN ORDER -- the DT_NEEDED chain: gnustl -> nimble -> FIFA14. */
  if (load_module(&gnustl_mod, FIFA14_LIB_GNUSTL) < 0 ||
      load_module(&nimble_mod, FIFA14_LIB_NIMBLE) < 0 ||
      load_module(&game_mod, FIFA14_LIB_GAME) < 0)
    fatal_error("Could not load the game libraries from %s.\n\n"
                "They are unpacked from the APK (" PORT_ABI_DIR ") on launch:\n"
                "delete libgnustl_shared.so, libnimble.so, libFIFA14.so and .setup\n"
                "in that folder to unpack them again.",
                dcr_game_root());

  /* Bind every import, only now that all three are staged (see resolve_module). */
  resolve_module(&gnustl_mod, FIFA14_LIB_GNUSTL);
  resolve_module(&nimble_mod, FIFA14_LIB_NIMBLE);
  resolve_module(&game_mod, FIFA14_LIB_GAME);

  /* Seal each module as RX/RW code and flush the caches. */
  so_finalize(&gnustl_mod);
  so_finalize(&nimble_mod);
  so_finalize(&game_mod);
  so_flush_caches(&gnustl_mod);
  so_flush_caches(&nimble_mod);
  so_flush_caches(&game_mod);
  debugPrintf("[boot] modules mapped RX/RW\n");

  /* TODO(offsets): fifa14_offsets.h is not derived -- no engine patch is applied
   * yet (the offsets at which libFIFA14.so must be patched, symbol-table free). */
  /* TODO(gl/mesa): with DCR_GL_MESA=1 the runtime's layer serves the engine's
   * libEGL/libGLESv2 imports; the gl* entry points the engine reaches another
   * way need the port's own hooks (gl_layer.h). Verify against the imports. */
  /* TODO(audio): decide RT_OPENSLES (port_config.h) and the audout path; the
   * engine's AndroidEAAudioCore.Init is a logged wildcard for now
   * (fifa14_jni_tables.c). */
  /* TODO(input): map the Switch pad to the engine's touch/pad input. */
  /* TODO(exidx): install __gnu_Unwind_Find_exidx for libgnustl_shared.so's C++
   * exception unwinding (see NOTES.md). */
  return 0;
}

/* ------------------------------------------------------------------- port_run */
void port_run(void) {
  /* Android runs each library's constructors when it is loaded
   * (System.loadLibrary -> dlopen): gnustl, then nimble, then FIFA14's own.
   * libgnustl_shared.so and libnimble.so have no JNI dependency, so their
   * constructors run here; libFIFA14.so's constructor, its JNI_OnLoad and the
   * Java entry points need the runtime's JNI environment, so they are the first
   * steps of fifa14_boot_run() below. */
  so_execute_init_array(&gnustl_mod);
  so_free_temp(&gnustl_mod);
  so_execute_init_array(&nimble_mod);
  so_free_temp(&nimble_mod);
  debugPrintf("[boot] libgnustl_shared / libnimble constructors done\n");

  /* The JNI environment, the natives bound by symbol, JNI_OnLoad, initJNI,
   * NativeOnCreate, runEntryPoint, the engine's init frame, the surface, and
   * then the NativeOnDrawFrame loop until the game or the system ends it.
   * fifa14_boot.c's header is the order and the reason for each step. */
  fifa14_boot_run();

  fifa14_obb_stats();
  fifa14_external_report(); /* every external path the game opened, and what it got */
  fifa14_assets_report(); /* every distinct asset path the game opened, and what it got */
  debugPrintf("[boot] port_run is over\n");
}
