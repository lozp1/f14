/* fifa14_storage.c -- FIFA 14's first launch: the setup plan for the runtime's
 * dcr_setup.c, and the APK help text.
 *
 * The runtime's dcr_setup.c unpacks port_setup_plan.libs from PORT_ABI_DIR in
 * the APK to the game folder (stamped in .setup: an APK change re-unpacks),
 * lists the Java classes (classes.txt), and shows the progress bar. The bar, in
 * permille:
 *     0- 980  libgnustl_shared.so / libnimble.so / libFIFA14.so (by bytes)
 *   980-1000  the Java class list
 *       1000  the game starts
 *
 * .setup keys: the module file names -- never rename them, or every player
 * unpacks again once. MIT.
 */
#include "config.h"
#include "dcr_setup.h"

static const char *const k_libs[] = {
    FIFA14_LIB_GNUSTL, FIFA14_LIB_NIMBLE, FIFA14_LIB_GAME};

const RtSetupPlan port_setup_plan = {
    .libs = k_libs,
    .nlibs = sizeof k_libs / sizeof k_libs[0],
    .libs_what = "Unpacking the game's libraries",
    .apk_requirement = "This port needs the 32-bit ARM (armeabi-v7a) build of FIFA 14\n"
                       "(com.ea.game.fifa14_row, version 1.3.6 / versionCode 136).",
    .libs_p0 = 0,
    .libs_p1 = 980,
    .classes_p0 = 980,
    .classes_p1 = 1000,
    /* The APK is used as it is: FIFA's data lives mostly in the OBB (1.39 GB),
     * which is NOT unpacked here -- it stays on the SD card and is read directly
     * once OBB access is implemented (TODO, see NOTES.md). */
    .store_apk_prefix = NULL,
    .store_p0 = 0,
    .store_p1 = 0,
    .steps = NULL,
    .nsteps = 0,
    .finish_in_port = 0,
};

const char *port_apk_help(void) {
  return "Copy your own ARMv7 APK of FIFA 14 (com.ea.game.fifa14_row, version\n"
         "1.3.6 / versionCode 136) into this folder, under any name: the game\n"
         "reads its data from it, and the three libraries are unpacked from it\n"
         "on the first launch. Put main.136.com.ea.game.fifa14_row.obb next to\n"
         "it as well.";
}
