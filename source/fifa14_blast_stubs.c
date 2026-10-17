/* fifa14_blast_stubs.c -- the handlers the tables name: the EA "blast"
 * framework's delegates, the EA service classes, and the Android pieces FIFA
 * reads its files and plays its sound through.
 *
 * The direction of the calls matters. In the real APK the blast Java classes
 * are thin: MainActivity and the delegates call NATIVE methods (declared
 * `native`, implemented as Java_com_ea_blast_... in libFIFA14.so). The
 * wrapper replaces that Java with this C, so the port drives those natives
 * itself, by symbol (the model is ds_loader.c's bind_natives + g_n.*; the
 * fifa14_main.c TODO block has the same plan). What is LEFT for these tables
 * is the other direction: the engine calling BACK into Java for a device
 * answer, a screen size, an asset, or a sound buffer. Those handlers are here.
 *
 * Nearly all of this is taken from hazavauks/deadspace_nx (Dead Space Mobile,
 * same blast framework): source/ds_java.c (the delegates), source/ds_assets.c
 * (AssetManager/InputStream/AssetFileDescriptor) and source/ds_audio.c
 * (AudioTrack). The class-level no-op entries ("the engine only calls, never
 * reads") and the offline services are FIFA 14's own; see
 * C:\Projects\ports\fifa14\notas\fifa14-jni-mapa.md.
 *
 * MIT.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>

#ifndef MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#endif
#include <miniz/miniz.h>

#include "dcr_apkcache.h"
#include "dcr_manifest.h"
#include "dcr_path.h"
#include "config.h"
#include "fifa14_assets.h"
#include "fifa14_boot.h"
#include "fifa14_jni_tables.h"
#include "jni.h"
#include "rt_apkfind.h"
#include "rt_applet.h"
#include "rt_audout.h"
#include "rt_cfg.h"
#include "rt_window.h"
#include "util.h"

/* ================================================== device / display =====
 * What the engine is told the phone is. Every value is a string of digits or
 * a name (the blast SystemAndroidDelegate's contract, ds_java.c:62-100).
 *
 * One of FIFA 14's own year: a Galaxy S3. Language: SPANISH, because that is
 * what the game's own file inside the OBB asks for and what its binary can
 * serve. MEASURED, not guessed:
 *   - the OBB ships the whole Spanish set -- data/gui/locale/SPA_ES.loc plus
 *     SPA_ES.loc in all 14 subdirectories that also have ENG_US.loc (beapro,
 *     ball_names, boot_names, cgexport, futconsole, futios, futplayerstyles,
 *     league_names, matchday, online, stadium_names, team_names, trivia,
 *     trophy_names), plus data/audio/speech/SPA_ES.mp3 and
 *     data/cmn/fe/about_ES.txt. (The container's 1217-entry index, read out of
 *     main.136.com.ea.game.fifa14_row.obb: 16 ENG_US.loc and 16 SPA_ES.loc.)
 *   - the OBB's ini/na/locale.ini has `LANG_OVERRIDE = 1`, and its own comment
 *     says 1 = "no override (use the device's language)"; so the engine asks
 *     THIS side for the language (SystemAndroidDelegate.GetLanguage/GetLocale,
 *     which these two strings answer) and picks the .loc from it.
 *   - the engine carries the OS-locale -> game-locale list itself, in
 *     libFIFA14.so at file offset 0x01019501:
 *       en_US, fr_FR, es_ES, de_DE, it_IT, ja_JP, ko_KR, zh_CN,
 *       nl_NL->DUT_NL, ru_RU, pt_BR->POR_BR, tr_TR->TUR_TR
 *     `es_ES` is in it, spelled exactly like this, and that list is what
 *     Localization::OverrideLanguageFromOS matches GetLocale() against
 *     (GetLocale returns Locale.toString(), i.e. "es_ES"/"en_US" -- read out of
 *     the APK's SystemAndroidDelegate.smali).
 * Everything else stays the same phone, so the capability answers still agree
 * with the device story. */
#define DEV_MODEL        "GT-I9300"
#define DEV_MANUFACTURER "samsung"
#define DEV_PLATFORM     "Android"
#define DEV_VERSION      "4.1.2"
#define DEV_API          "16"
#define DEV_ABI          "armeabi-v7a armeabi"
#define DEV_LANGUAGE     "es"
#define DEV_LOCALE       "es_ES"

#define STR(fn, text)                                                                        \
  JNI_H_DECL(fn) { (void)self; (void)a; (void)m; return jv_l(jni_str(text)); }

STR(fifa14_h_dev_str_one, "1")
STR(fifa14_h_dev_str_zero, "0")
STR(fifa14_h_dev_unknown, "-1")
STR(fifa14_h_dev_model, DEV_MODEL)
STR(fifa14_h_dev_manufacturer, DEV_MANUFACTURER)
STR(fifa14_h_dev_platform, DEV_PLATFORM)
STR(fifa14_h_dev_version, DEV_VERSION)
STR(fifa14_h_dev_api, DEV_API)
STR(fifa14_h_dev_abi, DEV_ABI)
STR(fifa14_h_dev_language, DEV_LANGUAGE)
STR(fifa14_h_dev_locale, DEV_LOCALE)
STR(fifa14_h_dev_false, "false")
/* Android's own paths; the runtime's path map (dcr_path.c) turns them into the
 * game folder's (ds_java.c:96-100). */
STR(fifa14_h_app_data_dir, DCR_ANDROID_FILES)
STR(fifa14_h_app_ext_dir, "/sdcard")

/* ---------------------------------- the device's capability counters, and
 * the app's own identity -- the engine's ()I questions.
 *
 * The engine asks all of these as ()I, NOT as the String this port's table
 * first declared ("()Ljava/lang/String;"), so GetMethodID matched nothing and
 * every one of them came back with the runtime's unhandled default:
 *
 *   [jni] unhandled com/ea/blast/SystemAndroidDelegate.GetVibratorCount()I
 *   [jni] unhandled ... GetTouchScreenCount()I, GetCameraCount()I,
 *         GetAccelerometerCount()I, GetGyroscopeCount()I, GetCompassCount()I,
 *         GetMicrophoneCount()I, GetTouchPadCount()I, GetTrackBallCount()I,
 *         GetApplicationVersionCode()I, GetHardwareFloatingPointSupport()
 *         Ljava/lang/String;, GetFirmware()Ljava/lang/String;,
 *         GetApplicationVersion()Ljava/lang/String;
 *   [jni] unhandled static com/ea/game/fifa14/Fifa14Activity.GetThePackage()
 *         Ljava/lang/String;  (on java/lang/Class)
 *   [jni] unhandled static com/ea/game/fifa14/Fifa14Activity.
 *         GetTheVersionCode()I  (on java/lang/Class)
 *
 * (the run's debug.log, right after the module manager and the first GL
 * frame). A zero there is not neutral: the engine's startup asks these to
 * decide what the device can do. Every value below is the hardware of the
 * phone this port already reports -- DEV_MODEL above, a Galaxy S3 / Android
 * 4.1.2, the device of FIFA 14's own year -- so the capability answers agree
 * with the rest of the device story instead of contradicting it. Each logs the
 * FIRST time the engine really calls it (never per call): only a real call
 * says the engine uses the answer.
 */
static int dev_ask(const char *name) {
  static const char *seen[24];
  static int n;
  for (int i = 0; i < n; i++)
    if (!strcmp(seen[i], name))
      return 0;
  if (n < (int)(sizeof seen / sizeof seen[0]))
    seen[n++] = name;
  return 1;
}

#define DEV_INT(cname, java, val, why)                                                       \
  JNI_H_DECL(cname) {                                                                        \
    (void)self; (void)a; (void)m;                                                            \
    if (dev_ask(java))                                                                       \
      debugPrintf("[dev] SystemAndroidDelegate.%s()I -> %d: %s (asked at startup)\n", java,  \
                  (int)(val), why);                                                          \
    return jv_i(val);                                                                        \
  }

DEV_INT(fifa14_h_dev_accelerometer, "GetAccelerometerCount", 1,
        "a GT-I9300 has one accelerometer")
/* The touch screen count is the port's ONE configurable device answer:
 * config.ini [controls] touch_screen (fifa14_config.c has the whole note).
 * 1 (the default) is what the port has always answered; 0 is the switch
 * user's experiment -- whether the game drops its on-screen buttons when the
 * device says it has no touch screen. It is answered from the config file, so
 * both values are testable on the console without rebuilding, and the log
 * below names which one this run is serving. */
static int dev_touch_screens(void) {
  const char *v = rt_config_get("controls", "touch_screen");
  int n = rt_config_value("controls.touch_screen", 1);
  if (!strcmp(v ? v : "", "0"))
    n = 0;
  return n;
}
JNI_H_DECL(fifa14_h_dev_touchscreen) {
  (void)self;
  (void)a;
  (void)m;
  const int n = dev_touch_screens();
  if (dev_ask("GetTouchScreenCount"))
    debugPrintf("[dev] SystemAndroidDelegate.GetTouchScreenCount()I -> %d: %s (asked at "
                "startup; config.ini [controls] touch_screen, default 1)\n",
                n, n == 1 ? "the phone's screen is the touch device the menus use"
                          : "THE EXPERIMENT: the game is told there is no touch screen at all");
  return jv_i(n);
}
DEV_INT(fifa14_h_dev_touchpad, "GetTouchPadCount", 0, "the GT-I9300 has no touchpad")
DEV_INT(fifa14_h_dev_camera, "GetCameraCount", 2, "a GT-I9300 has a rear and a front camera")
DEV_INT(fifa14_h_dev_compass, "GetCompassCount", 1, "a GT-I9300 has a magnetometer")
DEV_INT(fifa14_h_dev_gyroscope, "GetGyroscopeCount", 1, "a GT-I9300 has a gyroscope")
DEV_INT(fifa14_h_dev_microphone, "GetMicrophoneCount", 1, "a GT-I9300 has one microphone")
DEV_INT(fifa14_h_dev_trackball, "GetTrackBallCount", 0, "the GT-I9300 has no trackball")
DEV_INT(fifa14_h_dev_vibrator, "GetVibratorCount", 1,
        "the phone vibrates; the port's VibratorAndroidDelegate is a no-op")
/* Not in the log above -- the engine may ask them with either signature, so
 * both are answered and the next run's log says which one it uses. */
DEV_INT(fifa14_h_dev_phys_keyboard, "GetPhysicalKeyboardCount", 1,
        "the pad IS the engine's physical keyboard: AllocPhysicalKeyboardModule "
        "(libFIFA14.so @0x00df13e4) refuses to build the module when the count is < 1, and "
        "with no module there is nowhere for the pad's keyboard keys to land. The reported "
        "phone is a GT-I9300, whose pad the game treats as this module (fifa14_input.c has "
        "the whole chain)")
DEV_INT(fifa14_h_dev_virt_keyboard, "GetVirtualKeyboardCount", 1,
        "an Android soft keyboard exists (the port's is a no-op)")
DEV_INT(fifa14_h_dev_display, "GetDisplayCount", 1, "one display")

#undef DEV_INT

/* GetFirmware()Ljava/lang/String; -- the firmware of the device the engine is
 * on. The port reports Android 4.1.2 (DEV_VERSION, a GT-I9300 of 2012), so its
 * firmware string is that release: a version the engine's own parsing accepts
 * (the same string GetPlatformVersion already answers). */
JNI_H_DECL(fifa14_h_dev_firmware) {
  (void)a; (void)m;
  if (dev_ask("GetFirmware"))
    debugPrintf("[dev] SystemAndroidDelegate.GetFirmware() -> \"%s\" (the reported "
                "device's OS release)\n", DEV_VERSION);
  return jv_l(jni_str(DEV_VERSION));
}

/* GetHardwareFloatingPointSupport()Ljava/lang/String; -- whether the CPU has
 * hardware floating point. The reported phone is a Cortex-A9 with VFPv3/NEON,
 * and the port's runtime probe says the same (b_getauxval: HWCAP/NEON), so the
 * answer is "true" (a "false" would send an engine down a software-float path
 * it does not need). */
JNI_H_DECL(fifa14_h_dev_hw_fp_support) {
  (void)a; (void)m;
  if (dev_ask("GetHardwareFloatingPointSupport"))
    debugPrintf("[dev] SystemAndroidDelegate.GetHardwareFloatingPointSupport() -> "
                "\"true\" (VFPv3/NEON, as the device reports)\n");
  return jv_l(jni_str("true"));
}

/* GetApplicationVersionCode()I / GetApplicationVersion()Ljava/lang/String;
 * (SystemAndroidDelegate) and the two the engine calls STATICALLY on the
 * activity's class, GetTheVersionCode()I / GetThePackage()Ljava/lang/String;
 * (Fifa14Activity, "unhandled static ... (on java/lang/Class)"). These are the
 * APK's own manifest values, not constants: the runtime reads them from the
 * user's APK (dcr_manifest.c) and a different build of the game would answer
 * with its own numbers -- the log's own "[manifest] com.ea.game.fifa14_row
 * 1.3.6 (versionCode 136)". */
JNI_H_DECL(fifa14_h_app_version_code) {
  (void)a; (void)m;
  if (dev_ask("GetApplicationVersionCode"))
    debugPrintf("[dev] GetApplicationVersionCode()I / GetTheVersionCode()I -> %d (the "
                "APK's own manifest)\n", dcr_manifest_version_code());
  return jv_i(dcr_manifest_version_code());
}

JNI_H_DECL(fifa14_h_app_version_name) {
  (void)a; (void)m;
  if (dev_ask("GetApplicationVersion"))
    debugPrintf("[dev] GetApplicationVersion()Ljava/lang/String; -> \"%s\" (the APK's "
                "own manifest)\n", dcr_manifest_version_name());
  return jv_l(jni_str(dcr_manifest_version_name()));
}

JNI_H_DECL(fifa14_h_app_package) {
  (void)a; (void)m;
  if (dev_ask("GetThePackage"))
    debugPrintf("[dev] Fifa14Activity.GetThePackage() (static) -> \"%s\" (the APK's own "
                "manifest)\n", dcr_manifest_package());
  return jv_l(jni_str(dcr_manifest_package()));
}

/* The screen the engine renders into: the runtime's own window. */
JNI_H_DECL(fifa14_h_display_width) {
  (void)a; (void)m;
  int w, h;
  dcr_window_size(&w, &h);
  return jv_i(w);
}
JNI_H_DECL(fifa14_h_display_height) {
  (void)a; (void)m;
  int w, h;
  dcr_window_size(&w, &h);
  return jv_i(h);
}
/* Orientation: 0 = normal / landscape-as-it-stands (deadspace's h_orientation). */
JNI_H_DECL(fifa14_h_display_orientation) { (void)self; (void)a; (void)m; return jv_i(0); }
/* DisplayMetrics.xdpi / ydpi for a 4.8" 720p phone. */
JNI_H_DECL(fifa14_h_display_dpi) { (void)self; (void)a; (void)m; return jv_f(306.0f); }
/* DisplayAndroidDelegate.GetGLView() -> the GL view. On Android this is the
 * activity's GLSurfaceView: the object whose EGL context the native side finds
 * through eglGetCurrentDisplay()/eglGetCurrentSurface() (no Java here: the port
 * makes that context itself, fifa14_gl.c / fifa14_boot.c step 7b). The engine
 * (DisplayAndroid::CreateView @0xe0bc70) puts the answer in a global ref and
 * hands it to EA::Blast::ViewAndroid, so NULL means "the view has no view
 * behind it" -- which is what "[jni] unhandled ...GetGLView..." was doing.
 * One immortal object, as Java's delegate holds one view. */
JNI_H_DECL(fifa14_h_gl_view) {
  (void)self;
  (void)a;
  (void)m;
  static JObj *view;
  if (!view) {
    view = jni_singleton(F_GL_VIEW);
    debugPrintf("[gl] DisplayAndroidDelegate.GetGLView() -> the port's %s object (the GL "
                "view the Java activity's GLSurfaceView would be; the context is the "
                "port's own, fifa14_gl.c)\n",
                F_GL_VIEW);
  }
  return jv_l(jni_retain(view));
}

/* ================================================== activity ============ */
JNI_H_DECL(fifa14_h_activity_instance) { (void)a; (void)m; return jv_l(jni_retain(fifa14_activity)); }
JNI_H_DECL(fifa14_h_assets) { (void)self; (void)a; (void)m; return jv_l(jni_retain(jni_singleton(F_ASSET_MANAGER))); }
JNI_H_DECL(fifa14_h_finish) {
  (void)self; (void)a; (void)m;
  debugPrintf("[java] Activity.finish(): the port's frame loop is asked to end\n");
  rt_request_exit();
  return jv_none();
}

/* ================================================== EA services =========
 * EAIO IS EA::IO, the engine's file system: this is the call that gives it the
 * app's folders, and therefore the layer the game reads its data (the APK's
 * assets and the OBB) through. In the APK, Java's EAIO.Startup(AssetManager)
 * computes the app's directories and calls the native
 * Java_com_ea_EAIO_EAIO_StartupNativeImpl(AssetManager, String, String, String)
 * (its disassembly @0x00d8575c forwards (am, s1, s2, s3) straight to
 * EA::IO::AssetManagerJNI::Startup(env, am, s1, s2, s3)); the port does the
 * same with the folders the runtime maps Android's onto (dcr_path.c):
 *   /data/data/<pkg>/files -> <game folder>/data/files
 *   /sdcard                -> <game folder>/sdcard
 *   /data/data/<pkg>/cache -> <game folder>/data/cache
 * WHAT IS AN INFERENCE: the native's three strings are the Java side's, which
 * the port cannot read (they live in classes.dex) -- the values above are the
 * Android directories those three calls name, in the order the Java tends to
 * use (files, external, cache). The log prints exactly what was passed; if the
 * engine's file errors name another directory, this is the line to fix.
 * FIFA14_EAIO_STARTUP=0 (config.h) leaves the call out. */
#if FIFA14_EAIO_STARTUP
typedef void (*Fifa14EaioStartupFn)(void *env, void *thiz, void *am, void *d1, void *d2,
                                   void *d3);
#endif

/* ================================== EAIO / the AssetManagerJNI ===========
 * com/ea/EAIO/EAIO.Startup(AssetManager) is the OTHER half of EA::IO's Java
 * side, and the one that fixes the 'appbundle:/' crash. In the APK its java
 * body computes the app's directories and calls the native
 * Java_com_ea_EAIO_EAIO_StartupNativeImpl(am, s1, s2, s3) @0xd8575c, whose
 * three instructions are `mov r1, r2; mov r2, r3; mov r3, ip` around a tail
 * call to EA::IO::AssetManagerJNI::Startup(env, am, s1, s2, s3) @0xd832d0.
 * That Startup (read out of the .so) is:
 *
 *   env->GetJavaVM(&sVM)                 -> sVM @0x12432c0, the field
 *                                           FileStream::Open's 'appbundle:/'
 *                                           branch dereferences (the crash)
 *   sOriginalEnv @0x12432bc = env
 *   GetStringUTFChars of the three strings -> sAndroidDataRoot @0x12432c4,
 *                                           sAndroidFileRoot @0x124434c4,
 *                                           sAndroidTempRoot @0x12436c4
 *                                           (= sAndroidFileRoot + "/tmp")
 *                                           and sAndroidExternalRoot
 *                                           @0x12438c4
 *   FindClass("java/io/InputStream")      and GetMethodID read/close/skip
 *   GetObjectClass(am), NewGlobalRef(am)  -> sObject_AssetManager @0x1243ad0
 *   GetMethodID open/list/openFd on it    -> the AssetManagerJNI::sMethodId_*
 *   FindClass("android/content/res/AssetFileDescriptor") + getLength
 *   NewByteArray(0x10000)                 -> sByteArray
 *
 * which is exactly the port's AssetManager table (fifa14_jni_tables.c:
 * F_INPUT_STREAM read/skip/close, F_ASSET_MANAGER open/list/openFd, F_ASSET_FD
 * getLength). The APK's Java calls nothing of this in the port, so the port
 * calls the engine's own native: the same reason (and the same shape) as
 * fifa14_storage_start() above.
 *
 * THE THREE STRINGS: the native's argument order maps to the roots in the
 * order (data, file+temp, external) -- see the list above. The values are the
 * Android directories the runtime's path map (dcr_path.c) sends into the game
 * folder; the log prints what was passed, so the engine's own file errors name
 * the one to change. FIFA14_EAIO_STARTUP=0 (config.h) leaves the call out.
 *
 * NOTE (recorded, not changed here): s1/s2/s3 keep the order the previous round
 * chose (files, /sdcard, cache), so the DataRoot gets the internal files
 * directory and the ExternalRoot gets the cache one. Nothing the 'appbundle:/'
 * path needs depends on them (GetAppBundledResourceDirectory returns the
 * literal "appbundle:/"), so this crash is independent of that choice; if the
 * engine's later asset paths come out wrong, this is the line to revisit. */
#if FIFA14_EAIO_STARTUP
typedef void (*Fifa14EaioStartupFn)(void *env, void *thiz, void *am, void *d1, void *d2,
                                   void *d3);
#endif

/* Once per process, like the storage one: Android's Java calls Startup once. */
static int g_assetmanager_started;

int fifa14_assetmanager_start(const char *why) {
  if (g_assetmanager_started)
    return 0;
#if FIFA14_EAIO_STARTUP
  Fifa14EaioStartupFn fn = (Fifa14EaioStartupFn)fifa14_native_addr(FIFA14_ENTRY_EAIO_STARTUP);
  if (!fn) {
    debugPrintf("[assets] EAIO.Startup (%s): the native %s is NOT bound -- AssetManagerJNI::sVM "
                "stays NULL and every 'appbundle:/' open faults (see the [bind] lines)\n",
                why, FIFA14_ENTRY_EAIO_STARTUP);
    return -1;
  }
  g_assetmanager_started = 1;
  JObj *am = jni_singleton(F_ASSET_MANAGER);
  /* Not released: the native keeps them (strncpy of the UTF chars, and the
   * global ref of `am`); the port must not drop the only reference first. */
  JObj *data = jni_str(DCR_ANDROID_FILES);
  JObj *ext = jni_str("/sdcard");
  JObj *cache = jni_str(DCR_ANDROID_CACHE);
  debugPrintf("[assets] EAIO.Startup(am=%p) [%s] -> %s: the AssetManagerJNI's JavaVM (sVM), the "
              "four roots (\"%s\", \"%s\", \"%s\") and its method ids\n",
              (void *)am, why, FIFA14_ENTRY_EAIO_STARTUP, DCR_ANDROID_FILES, "/sdcard",
              DCR_ANDROID_CACHE);
  fn(g_jni_env, jni_class(F_EAIO)->obj, am, data, ext, cache);
  return 1;
#else
  debugPrintf("[assets] EAIO.Startup (%s): logged only (FIFA14_EAIO_STARTUP=0, config.h)\n", why);
  return -1;
#endif
}

JNI_H_DECL(fifa14_h_ea_startup) {
  (void)self;
  (void)m;
  const void *am = a[0].l;
  /* The APK's Java hands its own getAssets() here, which is the same singleton
   * fifa14_h_assets() gives the engine back -- so the port's AssetManager is
   * what the engine's AssetManagerJNI ends up holding, whichever side calls. */
  debugPrintf("[java] EAIO.Startup(am=%p)%s\n", am,
              am == (void *)jni_singleton(F_ASSET_MANAGER) ? "" : "  <- not the port's singleton");
  fifa14_assetmanager_start("the engine's Java call");
  return jv_none();
}
JNI_H_DECL(fifa14_h_ea_shutdown) {
  (void)a; (void)m;
  typedef void (*Fn)(void *env, void *thiz);
  Fn fn = (Fn)fifa14_native_addr(FIFA14_ENTRY_EAIO_SHUTDOWN);
  if (fn) {
    debugPrintf("[java] EAIO.Shutdown() -> the native %s\n", FIFA14_ENTRY_EAIO_SHUTDOWN);
    fn(g_jni_env, self);
    return jv_none();
  }
  debugPrintf("[java] EAIO.Shutdown(): the native is NOT bound -- nothing shut down\n");
  return jv_none();
}

/* ================================== EAMIO / the storage directories ======
 * com/ea/EAMIO/StorageDirectory -- EA::IO::StorageDirectory's Java side
 * (classes.dex: StorageDirectory.java; the bodies are quoted in config.h).
 * This is the class behind the NULL-env crash, and it is not a no-op to
 * swallow: its static Startup(Activity) is what calls the native
 * StartupNativeImpl, which is the ONLY code that gives EA::Jni::Context its
 * JavaVM. Without the JavaVM, EA::Jni::Context::GetEnv() (+0xe3a374) does
 * `ldr r0,[r0]` -- `this->m_javaVM` -- with r0 = 0 and faults on the next
 * instruction (+0xe3a390, `ldr r1,[r0]`, far = 0) in whatever thread got
 * there first: the crash was EA::Blast::GetExternalStorageDirectory ->
 * StorageDirectory::GetAllStorageList -> AppendPrimaryExternalStorageList on
 * three different threads.
 *
 * The four getters are the strings the native asks Java for BY NAME, right
 * after it has stored the JavaVM; each is the exact value the APK's Java
 * returns, in the Android spelling the runtime's path map (dcr_path.c) sends
 * into the game folder:
 *
 *   GetPrimaryExternalStorageDirectory  Environment.getExternalStorageDirectory()
 *       -> /sdcard            -> <game folder>/sdcard
 *   GetPrimaryExternalStorageState      Environment.getExternalStorageState()
 *       -> "mounted"          (see below)
 *   GetInternalStorageDirectory         activity.getFilesDir()
 *       -> /data/data/<pkg>/files -> <game folder>/data/files
 *   GetDedicatedDirectory               "Android/data/<pkg>/files/" -- RELATIVE
 *       on purpose: the engine joins it to the primary external directory
 *       (StorageDirectory::GetDedicatedDirectory @0x00e397ec), and the map sends
 *       <sd>/Android/data/<pkg>/... to <game folder>/external/... (dcr_path.c
 *       k_sd_ext), a folder the runtime already creates.
 *
 * The state must be "mounted": AppendPrimaryExternalStorageList compares it
 * against unmounted, unmountable, shared, removed, nofs, checking, bad_removal
 * and mounted_ro, and drops the directory for every one of them. The wildcard's
 * "" would match none of the nine and put the game on its generic path. */
#define FIFA14_STORAGE_EXTERNAL  "/sdcard"
#define FIFA14_STORAGE_STATE     "mounted"
#define FIFA14_STORAGE_DEDICATED "Android/data/" DCR_PKG_NAME "/files/"

/* Once per process: Android's Java calls Startup once, the port's boot calls it
 * too, and whichever comes first wins (calling the native twice would re-Init
 * its Delegate: a second FindClass + NewGlobalRef on the same class). */
static int g_eamio_started;

int fifa14_storage_start(const char *why) {
  if (g_eamio_started)
    return 0;
  typedef void (*Fn)(void *env, void *thiz);
  Fn fn = (Fn)fifa14_native_addr(FIFA14_ENTRY_EAMIO_STARTUP);
  if (!fn) {
    debugPrintf("[java] StorageDirectory.Startup (%s): the native %s is NOT bound -- the "
                "EA::Jni context keeps a NULL JavaVM and every storage query that asks for "
                "the JNIEnv will fault (see the [bind] lines)\n",
                why, FIFA14_ENTRY_EAMIO_STARTUP);
    return -1;
  }
  g_eamio_started = 1;
  debugPrintf("[java] StorageDirectory.Startup (%s) -> %s: env->GetJavaVM() into the "
              "EA::Jni context, then the class delegate and its four method ids\n",
              why, FIFA14_ENTRY_EAMIO_STARTUP);
  fn(g_jni_env, jni_class(F_EAMIO_STORAGE)->obj);
  return 1;
}

JNI_H_DECL(fifa14_h_eamio_startup) {
  (void)self; (void)a; (void)m;
  fifa14_storage_start("the engine's Java call");
  return jv_none();
}

JNI_H_DECL(fifa14_h_eamio_shutdown) {
  (void)self; (void)a; (void)m;
  typedef void (*Fn)(void *env, void *thiz);
  Fn fn = (Fn)fifa14_native_addr(FIFA14_ENTRY_EAMIO_SHUTDOWN);
  debugPrintf("[java] StorageDirectory.Shutdown()%s\n",
              fn ? " -> the native" : ": the native is NOT bound");
  if (fn)
    fn(g_jni_env, jni_class(F_EAMIO_STORAGE)->obj);
  g_eamio_started = 0; /* a later Startup is legitimate: Android's onDestroy/onCreate */
  return jv_none();
}

JNI_H_DECL(fifa14_h_storage_external) {
  (void)self; (void)a; (void)m;
  return jv_l(jni_str(FIFA14_STORAGE_EXTERNAL));
}
JNI_H_DECL(fifa14_h_storage_state) {
  (void)self; (void)a; (void)m;
  return jv_l(jni_str(FIFA14_STORAGE_STATE));
}
JNI_H_DECL(fifa14_h_storage_internal) {
  (void)self; (void)a; (void)m;
  return jv_l(jni_str(DCR_ANDROID_FILES)); /* /data/data/<pkg>/files */
}
JNI_H_DECL(fifa14_h_storage_dedicated) {
  (void)self; (void)a; (void)m;
  return jv_l(jni_str(FIFA14_STORAGE_DEDICATED));
}

/* ===================== the LAST TWO EA::Jni::Contexts (delegates) =======
 * The crash this fixes (crash.log, two threads):
 *
 *   [crash] pc  1a8bb390  libFIFA14.so+0xe3a390  (_ZN2EA3Jni7Context6GetEnvEv+0x1c)
 *   [crash] lr  1a8bb490  libFIFA14.so+0xe3a490  (_ZN2EA3Jni8DelegateC2EPKcPNS0_7ContextE+0x38)
 *   [crash] far 00000000  esr 92000006  insn e5901000  <- ldr r1,[r0] with r0 = 0
 *
 * It is the SAME failure as the two already fixed here (and in fifa14_boot.c),
 * in two more Contexts: EA::InAppWebBrowser::BrowserAndroid::mJniContext and
 * EA::VideoPlayer::PlayerAndroid::mJniContext. Their m_javaVM is filled by
 * their own Java_com_ea_*_StartupNativeImpl (72 bytes each: env->GetJavaVM()
 * then `str` into the Context), which the APK's Java calls and the port's Java
 * never would -- so, exactly like EAIO.Startup (fifa14_assetmanager_start) and
 * EAMIO/StorageDirectory.Startup (fifa14_storage_start), the port calls them
 * itself. config.h has the addresses, the two call sites of
 * Delegate::Delegate(char const*, Context*) (0xe357ec in BrowserAndroid's
 * constructor, 0xe3bdac in PlayerAndroid's) and how the two Contexts were
 * proved to be the objects those call sites read.
 *
 * WHY THE NATIVE AND NOT ONLY A SEED: the native is the engine's OWN code for
 * exactly this Context (it computes the same GOT slot the constructor does) and
 * does nothing else -- no allocation, no Java call, no state of its own -- so
 * calling it once at boot is the same act Android's Java performs in the APK
 * and it cannot disturb anything else. The seed beside it (fifa14_boot.c
 * jni_class_context_seed) is the belt to these braces, as for the other two.
 *
 * DOES IT BREAK WHAT WORKS: no. Each is called once per process (the flag
 * below), the Java-side Startup is once-by-contract in the APK too, and the
 * field it writes is the Context's own m_javaVM -- the same value (the port's
 * JavaVM) the other two seeds write, so GetEnv() answers g_jni_env for these
 * threads exactly as it already does for the storage and AssetManager ones. */
static int g_browser_jni_started;
static int g_video_jni_started;

/* One class's Startup: call its native once, with the class object the port's
 * tables intern. 1 = called now, 0 = already called, -1 = not bound. */
static int jni_class_start_one(const char *sym, const char *cls, const char *ctx_sym, const char *why, int *done) {
  if (*done)
    return 0;
  typedef void (*Fn)(void *env, void *thiz);
  Fn fn = (Fn)fifa14_native_addr(sym);
  if (!fn) {
    debugPrintf("[jni] %s.Startup (%s): the native %s is NOT bound -- its EA::Jni::Context keeps "
                "a NULL JavaVM and the first Delegate built from it faults in GetEnv (see the "
                "[bind] lines; fifa14_boot.c's seed tries the same field)\n",
                cls, why, sym);
    return -1;
  }
  *done = 1;
  debugPrintf("[jni] %s.Startup (%s) -> %s: env->GetJavaVM() into the class's static "
              "EA::Jni::Context (%s, its m_javaVM at +0), the Context the class's Delegate "
              "constructor is handed\n",
              cls, why, sym, ctx_sym);
  fn(g_jni_env, jni_class(cls)->obj);
  return 1;
}

int fifa14_jni_class_start(const char *why) {
  int called = 0;
  const int b = jni_class_start_one(FIFA14_ENTRY_BROWSER_JNI_STARTUP, F_BROWSER,
                                    FIFA14_SYM_BROWSER_JNI_CONTEXT, why, &g_browser_jni_started);
  const int v = jni_class_start_one(FIFA14_ENTRY_VIDEO_JNI_STARTUP, F_VIDEO_PLAYER,
                                    FIFA14_SYM_VIDEO_JNI_CONTEXT, why, &g_video_jni_started);
  called += b == 1;
  called += v == 1;
  if (!called)
    debugPrintf("[jni] the InAppWebBrowser and VideoPlayer contexts (%s): %s\n", why,
                (b == 0 && v == 0) ? "both Startups had already run: nothing to do"
                                   : "NEITHER Startup is bound: the two Contexts stay NULL");
  return called ? called : ((b < 0 && v < 0) ? -1 : 0);
}

JNI_H_DECL(fifa14_h_browser_startup) {
  (void)self; (void)a; (void)m;
  jni_class_start_one(FIFA14_ENTRY_BROWSER_JNI_STARTUP, F_BROWSER, FIFA14_SYM_BROWSER_JNI_CONTEXT,
                      "the engine's Java call", &g_browser_jni_started);
  return jv_none();
}
JNI_H_DECL(fifa14_h_video_startup) {
  (void)self; (void)a; (void)m;
  jni_class_start_one(FIFA14_ENTRY_VIDEO_JNI_STARTUP, F_VIDEO_PLAYER, FIFA14_SYM_VIDEO_JNI_CONTEXT,
                      "the engine's Java call", &g_video_jni_started);
  return jv_none();
}

/* The class-level no-op: a delegate's setter or a service call the engine
 * makes with nothing to read back. The engine's own NATIVE methods (the ones
 * the real Java would call) are driven by the port, not through here. */
JNI_H_DECL(fifa14_h_noop) { (void)self; (void)a; (void)m; return jv_none(); }

/* The WILDCARD no-op: the same answer (the zero of the return type -- all
 * eight bytes of the jvalue are cleared), plus ONE log line the first time
 * each (class, method) is called. A wildcard entry would otherwise hide the
 * runtime's own "[jni] unhandled <class>.<method><sig>" line, and that log is
 * the list of what the engine asked for and nothing answered -- exactly what
 * the next round of this port needs to see. */
#define FIFA14_SEEN_MAX 128
static struct {
  char cls[112]; /* a JClass name */
  char name[64]; /* a JMethod name */
} g_seen[FIFA14_SEEN_MAX];
static int g_nseen;
static Mutex g_seen_lock;

/* From fifa14_java_init(): before the engine can call anything. */
void fifa14_stubs_init(void) { mutexInit(&g_seen_lock); }

/* The first call of (cls, method): 1 (log it), 0 (seen already), 2 (the list is
 * full: the port says so once and stops). Every wildcard handler goes through
 * this, so the log names each (class, method) exactly once. */
static int seen_first(const char *cls, const char *name) {
  static int full_warned;
  int log_it = 0;
  mutexLock(&g_seen_lock);
  int seen = 0;
  for (int i = 0; i < g_nseen; i++)
    if (!strcmp(g_seen[i].cls, cls) && !strcmp(g_seen[i].name, name)) {
      seen = 1;
      break;
    }
  if (!seen) {
    if (g_nseen < FIFA14_SEEN_MAX) {
      snprintf(g_seen[g_nseen].cls, sizeof g_seen[g_nseen].cls, "%s", cls);
      snprintf(g_seen[g_nseen].name, sizeof g_seen[g_nseen].name, "%s", name);
      g_nseen++;
      log_it = 1;
    } else if (!full_warned) {
      full_warned = 1;
      log_it = 2;
    }
  }
  mutexUnlock(&g_seen_lock);
  return log_it;
}

/* Does this method answer a java.lang.String? THE ENGINE READS THE FIRST BYTE
 * OF EVERY STRING IT GETS (crash.log's last block: a data abort at
 * libFIFA14.so+0x446828, `ldrb r0,[r0]` with r0 = 0, in the frame that ran
 * ...GetStaticMethodID + CallObjectMethod and then printed "[YOUTUBE] Get:
 * (null)"). A wildcard that answered the zero of the return type therefore
 * answered NULL, and a NULL String is a crash, never a benign "empty". */
static int returns_string(const JMethod *m) {
  const char *r = m ? strchr(m->sig, ')') : NULL; /* m->sig is an array: never NULL */
  return r && !strcmp(r + 1, "Ljava/lang/String;");
}

JNI_H_DECL(fifa14_h_noop_log) {
  (void)self; (void)a;
  const char *cls = m && m->cls ? m->cls->name : "?";
  const char *name = m && m->name[0] ? m->name : "?";
  const int log_it = seen_first(cls, name);
  if (log_it)
    debugPrintf("[java] %s.%s%s -> no-op%s (%s)\n", cls, name, m ? m->sig : "",
                returns_string(m) ? " returning \"\"" : "",
                log_it == 2 ? "the seen list is full: no more of these are logged"
                            : "one of the port's wildcard classes");
  /* A String is never NULL (see returns_string); every other type keeps the
   * zero of its own kind. */
  if (returns_string(m))
    return jv_l(jni_str(""));
  return jv_none();
}

/* =============================================== THE DOORS THAT LEAD OUT ====
 * Share to YouTube / Facebook, the social login, the ad and store SDKs: every
 * one of them leaves the app on a phone and is answered on a phone by ANOTHER
 * APP (or by EA's servers, which are gone). There is no other app on this
 * console and no network, so the honest answer is the one the app's own code
 * takes when the plugin is not there: an inert but VALID object, and the
 * "cancelled / nothing happened" value for anything the engine reads back, so
 * the game stays where it is instead of dying.
 *
 * The user's report: "cuando intente compartir algo por yt o fb, me saco" --
 * the log's last lines of that run are this exact path:
 *
 *   [jni] call through a bad method ID 0x0 from libnimble.so+0x359xx   (x5)
 *   [F/CppBridge] :Can't find class com/ea/nimble/bridge/FacebookNativeCallback
 *   [F/CppBridge] :Can't find class java/util/ArrayList
 *   [F/CppBridge] :Can't find class java/util/List
 *   [jni] unhandled field static com/ea/game/fifa14/YouTubeHandler.youtube ...
 *   [java] com/ea/game/fifa14/YouTubeHandler.getAccountName()Ljava/lang/String;
 *          -> no-op (one of the port's wildcard classes)
 *   [printf] a NULL string printed as (null) (format "[YOUTUBE] Get: %s")
 *   [I/wrap_printf] [YOUTUBE] Get: (null)
 *   <-- the log ends here
 *
 * TWO measured causes, both fixed by this file:
 *   1) the wildcard answered NULL for that String (the data abort above);
 *   2) Java's class lookup was answered NULL (Class.getClassLoader /
 *      ClassLoader.loadClass were unhandled), which is why Nimble's CppBridge
 *      could not find classes that ARE in classes.txt -- java/util/ArrayList
 *      among them. Both are answered here and in fifa14_jni_tables.c.
 *
 * This handler is the same inert answer as fifa14_h_noop_log plus ONE line
 * naming the door, so a next run's log says exactly which one was touched. */
JNI_H_DECL(fifa14_h_social) {
  (void)self; (void)a;
  const char *cls = m && m->cls ? m->cls->name : "?";
  const char *name = m && m->name[0] ? m->name : "?";
  const int log_it = seen_first(cls, name);
  if (log_it)
    debugPrintf("[social] the engine asked for %s.%s%s -> mitigated (there is no other app on "
                "this console: an inert answer, the value the game reads when nothing happened, "
                "and the game stays where it was)%s\n",
                cls, name, m ? m->sig : "", returns_string(m) ? "; a String, answered \"\"" : "");
  if (returns_string(m))
    return jv_l(jni_str(""));
  return jv_none();
}

/* ----------------------------------------- java.lang.Class / ClassLoader ----
 * The engine's Nimble layer resolves EVERY class name through
 * Class.getClassLoader() + ClassLoader.loadClass(): libnimble.so's JNI_OnLoad
 * caches the loader (config.h decodes that very block out of the module:
 * {JavaVM *sJavaVM; pthread_key_t sKey; jobject sBaseNativeCallback; jmethodID
 * sClassLoader_loadClass;}) and CppBridge then loads by name. The port answered
 * NULL for the loader, so loadClass() ran on a null receiver and Nimble printed
 * "Can't find class java/util/ArrayList" for classes that exist in
 * classes.txt -- and the engine took its "the plugin is not installed" path,
 * which is where the share attempt ended.
 *
 * The answer is the port's OWN class table: the same names FindClass answers
 * for (the APK's classes.txt plus the Android framework), so loadClass and
 * FindClass can never disagree. A name that is really not there answers NULL --
 * that is the honest "no such app", and it is what the engine handles. */
#define FIFA14_CLASSLOADER "java/lang/ClassLoader"

JNI_H_DECL(fifa14_h_class_getloader) {
  (void)self; (void)a; (void)m;
  /* one immortal singleton per class: the engine keeps it in a global ref and
   * asks loadClass on it many times */
  return jv_l(jni_singleton(FIFA14_CLASSLOADER));
}

JNI_H_DECL(fifa14_h_classloader_loadclass) {
  (void)self; (void)m;
  const char *name = (a && a[0].l) ? jni_utf(a[0].l) : NULL;
  char buf[112];
  if (!name || !name[0])
    return jv_l(NULL);
  /* Java spells it with dots, the port's tables with slashes */
  snprintf(buf, sizeof buf, "%s", name);
  for (char *p = buf; *p; p++)
    if (*p == '.')
      *p = '/';
  if (!jni_class_exists(buf)) {
    if (seen_first("ClassLoader", "loadClass:missing"))
      debugPrintf("[social] ClassLoader.loadClass(\"%s\"): not a class of this app and not the "
                  "framework -> NULL, which is what a phone answers when the app is not installed "
                  "(mitigated: no such app on this console)\n",
                  buf);
    return jv_l(NULL);
  }
  return jv_l(jni_class(buf)->obj);
}

/* ================================================== AssetManager ========
 * The APK's assets/, indexed once, read through the runtime's block cache.
 * Adapted from ds_assets.c:1701 entries there, the same shape here. FIFA's
 * engine reads its data with EA::IO's AssetManagerJNI for "appbundle:/" and
 * "assets:/" paths. */
#define PREFIX "assets/"
#define PREFIX_LEN 7

typedef struct {
  char *name; /* after "assets/" */
  uint32_t lho, size, comp_size;
  uint32_t data;
  uint8_t stored;
} Asset;

static Asset *g_assets;
static int g_nassets, g_cap;

typedef struct {
  uint32_t base, size, pos;
  uint8_t *mem;
} Stream;

static Mutex g_file_lock;
static FILE *g_file;

static int apk_read(uint64_t off, void *buf, size_t n) {
  if (dcr_apkcache_read(off, buf, n) == (ssize_t)n)
    return 0;
  mutexLock(&g_file_lock);
  if (!g_file)
    g_file = fopen(dcr_apk_path(), "rb");
  const int ok = g_file && fseek(g_file, (long)off, SEEK_SET) == 0 && fread(buf, 1, n, g_file) == n;
  mutexUnlock(&g_file_lock);
  return ok ? 0 : -1;
}

static uint32_t data_offset(Asset *a) {
  if (a->data)
    return a->data;
  uint8_t h[30];
  if (apk_read(a->lho, h, sizeof h) != 0 || memcmp(h, "PK\3\4", 4) != 0)
    return 0;
  a->data = a->lho + 30 + (uint32_t)(h[26] | h[27] << 8) + (uint32_t)(h[28] | h[29] << 8);
  return a->data;
}

static int add_entry(const RtZipEntry *e, void *ctx) {
  int *deflated = ctx;
  if (e->name_len <= PREFIX_LEN || e->name_len >= sizeof e->name ||
      strncmp(e->name, PREFIX, PREFIX_LEN) != 0 || e->name[e->name_len - 1] == '/')
    return 0;
  if (g_nassets == g_cap) {
    const int cap = g_cap ? g_cap * 2 : 2048;
    Asset *grown = realloc(g_assets, sizeof *grown * (size_t)cap);
    if (!grown)
      return 1;
    g_assets = grown;
    g_cap = cap;
  }
  Asset *a = &g_assets[g_nassets];
  memset(a, 0, sizeof *a);
  if (!(a->name = strdup(e->name + PREFIX_LEN)))
    return 1;
  a->lho = e->lho;
  a->size = e->size;
  a->comp_size = e->comp_size;
  a->stored = e->method == 0;
  *deflated += !a->stored;
  g_nassets++;
  return 0;
}

static int by_name(const void *a, const void *b) {
  return strcmp(((const Asset *)a)->name, ((const Asset *)b)->name);
}

int fifa14_assets_init(void) {
  int deflated = 0;
  mutexInit(&g_file_lock);
  fifa14_asset_log_init(); /* fifa14_assets.c: its lock, before any request */
  if (rt_zip_walk_path(dcr_apk_path(), add_entry, &deflated) != 0 || !g_nassets) {
    debugPrintf("[assets] %s: no assets/ could be read\n", dcr_apk_path());
    return -1;
  }
  qsort(g_assets, (size_t)g_nassets, sizeof *g_assets, by_name);
  debugPrintf("[assets] %d files in the APK's assets/ (%d still deflated)\n", g_nassets, deflated);
  /* The index is kept: the engine's own file layer asks for these paths on the
   * SD card (its EA::IO FileStream/AssetStream), where a deflated entry has no
   * bytes at all, so fifa14_assets.c serves each one on its first open --
   * inflated, written to <game folder>/assets/<name>, and logged. */
  fifa14_asset_index_ready();
  debugPrintf("[assets] index ready: a deflated entry is served on its first open (inflated to "
              "<game folder>/assets/<name>); every open is logged\n");
  /* The index's own check, before the engine can ask: see
   * fifa14_assets_selfcheck() (further down this file). It is what makes one
   * run's log say whether the engine's EAMCore.ini openFd/open will find the
   * entry -- the bug this round fixed. */
  fifa14_assets_selfcheck("fifa14_assets_init, the boot's port_load");
  return g_nassets;
}

/* ---------------------------------------------- what the port serves from
 * fifa14_assets.c (port_path_fixup) and the JNI side read through. The index
 * above is the one source: name (after "assets/") -> local header, sizes,
 * compression. Read-only, so the path layer's threads may all call it. */
static Asset *find_asset(const char *name);          /* below */
static uint8_t *inflate_entry(Asset *a, uint32_t data); /* below */

int fifa14_assets_count(void) { return g_nassets; }

int fifa14_asset_info(const char *name, uint32_t *raw_size, uint32_t *comp_size, int *stored) {
  Asset *a = find_asset(name);
  if (!a)
    return 0;
  if (raw_size)
    *raw_size = a->size;
  if (comp_size)
    *comp_size = a->comp_size;
  if (stored)
    *stored = a->stored;
  return 1;
}

uint8_t *fifa14_asset_bytes(const char *name, uint32_t *size) {
  Asset *a = find_asset(name);
  if (!a)
    return NULL;
  if (a->stored) {
    uint8_t *out = malloc(a->size ? a->size : 1);
    if (out && apk_read(data_offset(a), out, a->size) == 0) {
      if (size)
        *size = a->size;
      return out;
    }
    free(out);
    return NULL;
  }
  uint8_t *out = inflate_entry(a, data_offset(a));
  if (out && size)
    *size = a->size;
  return out;
}

/* The ONE normalization of a caller's asset name, used by open(), openFd() and
 * list() alike (so the three can never disagree) and made to accept the same
 * spellings fifa14_assets.c's asset_rel() accepts for the engine's own file
 * layer, plus the one FileStream::Open passes after stripping 'appbundle:/'
 * itself:
 *
 *   "EAMCore.ini"                    -> "EAMCore.ini"
 *   "assets/EAMCore.ini"             -> "EAMCore.ini"
 *   "/assets/EAMCore.ini"            -> "EAMCore.ini"
 *   "assets:/EAMCore.ini"            -> "EAMCore.ini"
 *   "appbundle:/EAMCore.ini"         -> "EAMCore.ini"
 *   "appbundle:/assets/EAMCore.ini"  -> "EAMCore.ini"
 *   "./assets/EAMCore.ini"           -> "EAMCore.ini"
 *   "file://.../assets/EAMCore.ini"  -> "EAMCore.ini"
 *
 * Every branch removes a prefix NO index entry can start with (add_entry()
 * strips "assets/" at build time), so this can only ever turn a caller's
 * spelling into the entry it means -- never into a different one. The loop
 * ends because every branch advances the pointer. */
static const char *normalize(const char *name) {
  for (;;) {
    if (!strncmp(name, "file://", 7)) {
      name += 7;
    } else if (!strncmp(name, "appbundle:/", 11)) {
      name += 11;
    } else if (!strncmp(name, "assets:/", 8)) {
      name += 8;
    } else if (!strncmp(name, PREFIX, PREFIX_LEN)) {
      name += PREFIX_LEN;
    } else if (*name == '/') {
      name++;
    } else if (!strncmp(name, "./", 2)) {
      name += 2;
    } else {
      break;
    }
  }
  return name;
}

static Asset *find_asset(const char *name) {
  if (!g_nassets) /* before the index (or no APK): nothing to find, and bsearch
                     over no array is not a question to ask */
    return NULL;
  Asset key = {.name = (char *)normalize(name)};
  return bsearch(&key, g_assets, (size_t)g_nassets, sizeof *g_assets, by_name);
}

/* Equal modulo ASCII case (strcasecmp is not portable to every bionic here). */
static int ci_eq(const char *a, const char *b) {
  for (; *a && *b; a++, b++) {
    int ca = (*a >= 'A' && *a <= 'Z') ? *a + 32 : (unsigned char)*a;
    int cb = (*b >= 'A' && *b <= 'Z') ? *b + 32 : (unsigned char)*b;
    if (ca != cb)
      return 0;
  }
  return *a == *b;
}

/* `name` looked like `cand`: the same string modulo case, or the same last
 * path component. Used by not_found() below, so the log tells "the engine asks
 * for something the APK does not have" from "the port's name handling is
 * wrong" -- the two were indistinguishable before (both printed "not in the
 * APK"). */
static int looks_like(const char *name, const char *cand) {
  if (ci_eq(name, cand))
    return 1;
  const char *a = strrchr(name, '/'), *b = strrchr(cand, '/');
  return ci_eq(a ? a + 1 : name, b ? b + 1 : cand);
}

/* The entry is really not there (or the index is not built). The message names
 * the NORMALIZED name, says how many entries the index holds and, when a name
 * in the index looks like it, prints it: an entry that IS in the index must
 * never be reported through here (that is what the openFd fix removed). */
static void *not_found(const char *what, const char *name) {
  const char *rel = normalize(name);
  debugPrintf("[assets] %s(\"%s\") -> NOT in the APK's assets/ (the index holds %d entries, and "
              "\"%s\" is not one of them): the engine gets FileNotFoundException\n",
              what, name, g_nassets, rel);
  int hints = 0;
  for (int i = 0; i < g_nassets && hints < 3; i++)
    if (looks_like(rel, g_assets[i].name)) {
      hints++;
      debugPrintf("[assets]   ...but the index has \"%s\" (%u B, %s): if the engine asked for that "
                  "name, this is the port's name handling, not the APK\n",
                  g_assets[i].name, (unsigned)g_assets[i].size,
                  g_assets[i].stored ? "stored" : "deflated");
    }
  jni_throw("java/io/FileNotFoundException", name);
  return NULL;
}

/* One line per AssetManager call the port answered, capped: the engine may ask
 * for the same asset again and again, and the log still has to end. */
#define JNI_ASSET_LOG_MAX 400
static int g_asset_calls_logged;
static int g_asset_list_logged;

static void asset_call_log(const char *what, const char *name, const Asset *as) {
  if (g_asset_calls_logged > JNI_ASSET_LOG_MAX)
    return;
  if (g_asset_calls_logged == JNI_ASSET_LOG_MAX) {
    g_asset_calls_logged++;
    debugPrintf("[assets] %d AssetManager calls logged (open/openFd/list); later ones are "
                "counted, not printed\n",
                JNI_ASSET_LOG_MAX);
    return;
  }
  g_asset_calls_logged++;
  debugPrintf("[assets] %s(\"%s\"): found in the index -- %u B %s\n", what, name, (unsigned)as->size,
              as->stored ? "(stored in the APK)" : "(deflated in the APK; open() inflates it)");
}

static void stream_free(JObj *self) {
  Stream *s = self->p;
  if (s) {
    free(s->mem);
    free(s);
    self->p = NULL;
  }
}

static uint8_t *inflate_entry(Asset *a, uint32_t data) {
  uint8_t *comp = malloc(a->comp_size ? a->comp_size : 1), *out = malloc(a->size ? a->size : 1);
  if (comp && out && apk_read(data, comp, a->comp_size) == 0 &&
      tinfl_decompress_mem_to_mem(out, a->size, comp, a->comp_size, 0) == a->size) {
    free(comp);
    return out;
  }
  free(comp);
  free(out);
  return NULL;
}

JNI_H_DECL(fifa14_h_asset_open) {
  (void)m;
  const char *name = jni_utf(a[0].l);
  Asset *as = find_asset(name);
  if (!as)
    return jv_l(not_found("open", name));
  const uint32_t data = data_offset(as);
  if (!data) {
    /* IN the index (so not_found() must not be used) but the APK's local
     * header could not be read: an I/O failure, not a missing file. */
    debugPrintf("[assets] open(\"%s\"): the index HAS it (%u B, %s) but its local header could "
                "not be read from %s\n",
                name, (unsigned)as->size, as->stored ? "stored" : "deflated", dcr_apk_path());
    jni_throw("java/io/IOException", name);
    return jv_l(NULL);
  }
  Stream *s = calloc(1, sizeof *s);
  if (!s) {
    debugPrintf("[assets] open(\"%s\"): out of memory for the stream\n", name);
    return jv_l(NULL);
  }
  s->base = data;
  s->size = as->size;
  if (!as->stored && !(s->mem = inflate_entry(as, data))) {
    debugPrintf("[assets] open(%s): could not inflate %u bytes\n", name, (unsigned)as->size);
    free(s);
    jni_throw("java/io/IOException", name);
    return jv_l(NULL);
  }
  /* ================ THE CONTENT MANAGER'S SPEECH CONFIG ==================
   * The engine's AssetManagerJNI reads contentmanagerSpeech.cmconfig through
   * THIS open (the run's log: `[assets] open("contentmanagerSpeech.cmconfig"):
   * found in the index -- 901 B (deflated in the APK; open() inflates it)`),
   * so this is where the port can hand the ContentManager a configuration that
   * fits a build with no EA server. fifa14_cm_config_patch() (fifa14_assets.c)
   * has the whole measurement -- the APK's "updateCheckPeriodDays": 0 makes
   * FlowInitialization::GetRecommendedSyncAction @0x00dd3a94 answer "a check is
   * due" on every run, the check goes to serverUrl (http://127.0.0.1: nothing
   * there), the description download fails and the game's language screen goes
   * on offering the commentator pack as a DOWNLOAD although its 476 MB are on
   * the card and served. The patch rewrites that one value to -1 ("unlimited",
   * the game's own documented value) and never changes a byte's LENGTH: the
   * engine measured this file with openFd().getLength() before reading it. */
  if (s->mem) {
    if (!strcmp(normalize(name), "contentmanagerSpeech.cmconfig"))
      fifa14_cm_config_patch(s->mem, s->size);
  } else if (as->stored && !strcmp(normalize(name), "contentmanagerSpeech.cmconfig")) {
    debugPrintf("[speech] contentmanagerSpeech.cmconfig is a STORED entry: the port serves the "
                "APK's own bytes off the card and cannot rewrite the update-check period in "
                "the stream (if the language screen goes on offering the pack as a download, "
                "this is why)\n");
  }
  JObj *o = jni_new(F_INPUT_STREAM);
  o->p = s;
  o->finalize = stream_free;
  /* The SUCCESS line (there was none before: only the failures were logged,
   * which is why the log of the run that crashed showed openFd failing and
   * said nothing about open() having worked). */
  asset_call_log("open", name, as);
  return jv_l(o);
}

JNI_H_DECL(fifa14_h_stream_read) {
  Stream *s = self ? self->p : NULL;
  if (!s)
    return jv_i(-1);
  /* read(): one byte -- the 0-argument overload (its signature is "()I"; the
   * args array holds nothing then, so the signature, not a[0], decides). */
  if (strcmp(m->sig, "([BII)I") != 0) {
    if (s->pos >= s->size)
      return jv_i(-1);
    uint8_t b = 0;
    if (s->mem) {
      b = s->mem[s->pos];
    } else if (apk_read((uint64_t)s->base + s->pos, &b, 1) != 0) {
      return jv_i(-1);
    }
    s->pos++;
    return jv_i(b);
  }
  JObj *arr = a[0].l;
  jint off = a[1].i, len = a[2].i;
  if (arr->kind != JK_ARRAY || off < 0 || len < 0 || off + len > arr->a.len)
    return jv_i(-1);
  if (s->pos >= s->size)
    return jv_i(-1);
  if ((uint32_t)len > s->size - s->pos)
    len = (jint)(s->size - s->pos);
  uint8_t *dst = (uint8_t *)arr->a.data + off;
  if (s->mem)
    memcpy(dst, s->mem + s->pos, (size_t)len);
  else if (apk_read((uint64_t)s->base + s->pos, dst, (size_t)len) != 0)
    return jv_i(-1);
  s->pos += (uint32_t)len;
  return jv_i(len);
}

JNI_H_DECL(fifa14_h_stream_skip) {
  (void)a; (void)m;
  Stream *s = self ? self->p : NULL;
  jlong n = a[0].j;
  if (!s || n <= 0)
    return jv_j(0);
  if (n > (jlong)(s->size - s->pos))
    n = (jlong)(s->size - s->pos);
  s->pos += (uint32_t)n;
  return jv_j(n);
}

JNI_H_DECL(fifa14_h_stream_close) {
  (void)a; (void)m;
  if (self)
    stream_free(self);
  return jv_none();
}

/* ========================= THE openFd FIX ================================
 * What the log of the crash said, and what was really wrong:
 *
 *   [assets] openFd(EAMCore.ini): not in the APK
 *   [jni] throw java/io/FileNotFoundException: EAMCore.ini
 *
 * but EAMCore.ini IS in the APK (assets/EAMCore.ini, 3119 B, DEFLATE): the
 * handler below REJECTED it for being deflated --
 *
 *     if (!as || !as->stored)      <- `!as->stored` is the whole bug
 *       return not_found("openFd", name);
 *
 * -- and reported that rejection through the same not_found() a genuinely
 * missing name goes through, so the log said "not in the APK" for an entry the
 * index had just found. 16 of FIFA's 19 assets are DEFLATE (only the three
 * .mp4 are STORE), so openFd() failed for nearly every asset while open()
 * served them -- the port's two paths disagreed, and the engine uses both.
 *
 * WHY IT MATTERS (AssetManagerJNI::Open @0x00d83c58, the only openFd caller):
 * FileStream::Open's 'appbundle:/' branch calls
 * `AssetManagerJNI::Open(AutoJNIEnv const&, char const*, unsigned long long*)`,
 * whose body is
 *
 *     jstring   n  = env->NewStringUTF(name);
 *     jobject   is = env->CallObjectMethod(sObject_AssetManager,
 *                                          sMethodId_AssetManager_Open,    n);   <- 1st
 *     if (env->ExceptionOccurred()) { env->ExceptionClear(); return 0; }
 *     jobject   fd = env->CallObjectMethod(sObject_AssetManager,
 *                                          sMethodId_AssetManager_OpenFd,  n);   <- 2nd
 *     if (env->ExceptionOccurred()) { *size = -1, -1; env->ExceptionClear(); goto out; }
 *     *size = env->CallLongMethod(fd, sMethodId_AssetFileDescriptor_GetLength);
 *   out:  NewGlobalRef / DeleteLocalRef, return
 *
 * (the three method ids are read out of the loaded GOT: 0x010e67dc ->
 * sMethodId_AssetManager_Open @0x1243ad4, 0x010e67fc ->
 * sMethodId_AssetManager_OpenFd @0x1243ad8, 0x010e6800 ->
 * sMethodId_AssetFileDescriptor_GetLength @0x1243ae0 -- all three
 * AssetManagerJNI's own fields.) So the port's open() had ALREADY succeeded
 * (which is why the log shows no 'open' line at all: a successful open was not
 * logged) and the engine threw the whole file away because the SECOND call
 * raised FileNotFoundException -- one pending Java exception, and
 * FileStream::Open returns a failed stream: the NULL stream
 * AssetStream::CreateMemoryStreamFromFile dereferences.
 *
 * WHAT IT DOES NOW: every entry the index holds answers openFd(), deflated or
 * not, and getLength() = the entry's UNCOMPRESSED size -- the number of bytes
 * open() serves through the InputStream (a->size; for a STORE entry that is
 * also the number of bytes in the APK). That is what the engine is measuring:
 * it never touches the descriptor's fd (the port has none to give: no
 * FileDescriptor.getInt$ on this path), it only reads the length, so a
 * deflated entry can answer it exactly and the file is read through the
 * InputStream open() returns.
 *
 * THE ALTERNATIVE, AND WHY THIS ONE (checked, not assumed): on a real phone
 * AssetManager.openFd() REFUSES a compressed entry, and this engine can live
 * with that -- the exception path sets *size = -1 (0xFFFFFFFF), and
 * FileStream::GetSize() (@0xd84ed8) treats that sentinel as "unknown" and calls
 * AssetManagerJNI::DetermineFileLengthBySkipping (@0xd83f40), which opens the
 * asset AGAIN and sums InputStream.skip(0x40000) until it returns 0. So the
 * port could keep throwing and still be fed a length -- the SAME length (the
 * skipping measures the inflated stream). Answering it here instead is
 * preferred because: it is the honest answer for an entry the port CAN serve
 * (open() inflates it, so the bytes behind the length exist), it costs no
 * second open and no skip loop, and it keeps the three AssetManager calls
 * consistent (the bug of this round: open() found EAMCore.ini and openFd() said
 * "not in the APK"). The -1 path stays available for the entries the index does
 * not hold: those still fail here, with FileNotFoundException, as Android's
 * would.
 *
 * THE CONSISTENCY the three AssetManager calls now keep (all via the ONE
 * searcher, find_asset() -> normalize(), so "EAMCore.ini", "assets/EAMCore.ini"
 * and "/assets/EAMCore.ini" are one entry):
 *   open(name)   the bytes, deflated entries inflated into memory;
 *   openFd(name) the same entry's length (and start offset 0);
 *   list(dir)    the names of that same index, one folder deep.
 * fifa14_assets_selfcheck() (below, run at boot) asks the index all three for
 * the entries the engine reads first, in every spelling, and prints the
 * answers -- see the [assets] self-check lines of the next run's log. */
JNI_H_DECL(fifa14_h_asset_open_fd) {
  (void)m;
  const char *name = jni_utf(a[0].l);
  Asset *as = find_asset(name);
  if (!as)
    return jv_l(not_found("openFd", name)); /* really not in the index */
  JObj *o = jni_new(F_ASSET_FD);
  /* getLength(): the length of what open() serves (see above); getStartOffset:
   * 0, the whole entry. v[] is the AssetFileDescriptor's only state. */
  o->v[0] = (intptr_t)as->size;
  o->v[1] = 0; /* start offset */
  asset_call_log("openFd", name, as);
  return jv_l(o);
}
JNI_H_DECL(fifa14_h_asset_fd_length) {
  (void)a; (void)m;
  return jv_j(self ? (jlong)(uint32_t)self->v[0] : 0);
}
JNI_H_DECL(fifa14_h_asset_fd_offset) {
  (void)a; (void)m;
  return jv_j(self ? (jlong)self->v[1] : 0);
}

/* The part of an index entry's name that lies INSIDE the folder `dir` (already
 * normalized: after "assets/", no leading '/', and "" for the top of assets/):
 * the rest of the path, which may still hold '/'. NULL when the entry is not
 * inside that folder. THE one place "what is in this folder" is answered, so
 * the list() handler and the boot self-check can never disagree about it. */
static const char *asset_in_dir(const Asset *a, const char *dir, size_t dl) {
  const char *p = a->name;
  if (dl) {
    if (strncmp(p, dir, dl) != 0 || p[dl] != '/')
      return NULL;
    p += dl + 1;
  }
  return p;
}

JNI_H_DECL(fifa14_h_asset_list) {
  (void)m;
  /* The names directly in a folder; the index is sorted, so children are
   * together (ds_assets.c:248). The folder name goes through the same
   * normalize()/find_asset() path open() and openFd() use, so the three
   * answer about the same entries; a name too long to hold is refused instead
   * of truncated (a truncated folder would list the WRONG children). */
  char dir[512];
  const char *raw = jni_utf(a[0].l);
  const int wrote = snprintf(dir, sizeof dir, "%s", normalize(raw));
  if (wrote < 0 || (size_t)wrote >= sizeof dir) {
    debugPrintf("[assets] list(\"%s\"): the folder name needs more than %u B: an EMPTY list is "
                "returned rather than a listing of another folder\n",
                raw, (unsigned)sizeof dir);
    return jv_l(jni_array('L', 0));
  }
  size_t dl = strlen(dir);
  while (dl && dir[dl - 1] == '/')
    dir[--dl] = 0;
  JObj *arr = NULL;
  for (int pass = 0; pass < 2; pass++) {
    int n = 0;
    const char *last = NULL;
    size_t last_len = 0;
    for (int i = 0; i < g_nassets; i++) {
      const char *p = asset_in_dir(&g_assets[i], dir, dl);
      if (!p)
        continue;
      const size_t len = strcspn(p, "/");
      if (last && len == last_len && !strncmp(p, last, len))
        continue;
      last = p;
      last_len = len;
      if (pass) {
        char child[256];
        snprintf(child, sizeof child, "%.*s", (int)len, p);
        ((JObj **)arr->a.data)[n] = jni_str(child);
      }
      n++;
    }
    if (!pass)
      arr = jni_array('L', n);
  }
  if (g_asset_list_logged < 64) {
    g_asset_list_logged++;
    debugPrintf("[assets] list(\"%s\") -> \"%s\": %d name(s)\n", raw, dir,
                (int)(arr->a.len));
  }
  return jv_l(arr);
}

/* FileDescriptor.getInt$(): the engine should read via the InputStream; a
 * descriptor is the only thing not backed here, so -1 (invalid) is returned
 * and a direct read() on it is not attempted. */
JNI_H_DECL(fifa14_h_fd_int) { (void)self; (void)a; (void)m; return jv_i(-1); }

/* ================== the boot self-check of the asset layer ==============
 * Run once at the end of fifa14_assets_init(), before the engine can ask for
 * anything: it asks the INDEX the same three questions the engine's
 * AssetManager asks (open's bytes, openFd's length, list's names) about the
 * entries the engine reads first, in every spelling the engine's paths arrive
 * in, and prints the answers. There is no console and no emulator: this is how
 * one run's debug.log proves by itself that the port can find what the engine
 * asks for -- and, when it cannot, which of the three disagreed. It is the
 * check for the bug this round fixed (openFd rejecting every DEFLATE entry:
 * EAMCore.ini is exactly that case) and for any future name mismatch. */
void fifa14_assets_selfcheck(const char *path) {
  /* The entries the engine's first reads name (Blast::System::LoadProperties
   * reads 'appbundle:/EAMCore.ini' first) plus one inside a subfolder. */
  static const char *const probe[] = {
      "EAMCore.ini",       "Fifa14.ini",        "EAMToolManager.ini", "config.xml",
      "version.properties", "adview.html",      "adview.js",          "bootstrap.html",
      "burstly.properties", "close-button.png", "contentmanager.cmconfig",
      "video/android/FUT14_GEN3_ICEBREAKER_V05.mp4",
  };
  /* The spellings those names arrive in: the bare name (FileStream::Open
   * strips 'appbundle:/' before it calls openFd/open), the APK's own
   * "assets/<name>", an absolute form, the two EA::IO schemes whose ':' the
   * runtime's path map leaves alone (fifa14_assets.c's asset_rel) and the
   * untrimmed 'appbundle:/assets/<name>'. */
  static const char *const form[] = {"%s",          "assets/%s",       "/assets/%s",
                                     "assets:/%s",  "appbundle:/%s",  "appbundle:/assets/%s"};
  const unsigned nform = (unsigned)(sizeof form / sizeof form[0]);
  int bad = 0;
  debugPrintf("[assets] ==== self-check of the APK's assets/ index (%s) ====\n", path);
  debugPrintf("[assets] the index holds %d entries, all read from %s\n", g_nassets, dcr_apk_path());
  for (unsigned p = 0; p < sizeof probe / sizeof probe[0]; p++) {
    uint32_t size = 0, comp = 0, first = 0;
    int stored = 0, found = 0, agree = 1;
    char name[256];
    for (unsigned f = 0; f < nform; f++) {
      snprintf(name, sizeof name, form[f], probe[p]);
      const int ok = fifa14_asset_info(name, &size, &comp, &stored);
      found += ok;
      if (f == 0)
        first = size;
      else if (!ok || size != first)
        agree = 0;
    }
    if (!found) {
      bad++;
      debugPrintf("[assets]   %-40s NOT in the index in ANY of the %u spellings: an engine open of "
                  "it fails\n",
                  probe[p], nform);
      continue;
    }
    if (!agree)
      bad++;
    debugPrintf("[assets]   %-40s %8u B (%s), %u/%u spellings found%s\n", probe[p],
                (unsigned)size, stored ? "stored" : "deflated", (unsigned)found, nform,
                agree ? "" : "  <-- the SPELLINGS DISAGREE: this is the port's bug, not the APK's");
  }
  /* The two things the engine does with the first of them: openFd() answers a
   * length (getLength -> fifa14_h_asset_fd_length), open() serves the bytes.
   * Reading EAMCore.ini back through the same helper open() uses proves the
   * APK read AND the inflate path work before the engine asks -- a length with
   * no readable bytes behind it would be the next failure. */
  {
    const char *first = "EAMCore.ini";
    uint32_t size = 0, comp = 0, got = 0;
    int stored = 0;
    if (fifa14_asset_info(first, &size, &comp, &stored)) {
      uint8_t *bytes = fifa14_asset_bytes(first, &got);
      char head[25];
      size_t hl = 0;
      for (uint32_t i = 0; bytes && i < got && hl < sizeof head - 1; i++)
        head[hl++] = (bytes[i] >= 32 && bytes[i] < 127) ? (char)bytes[i] : '.';
      head[hl] = 0;
      if (!bytes || got != size) {
        bad++;
        debugPrintf("[assets]   %s read back (the bytes open() serves): FAILED -- %u B of the "
                    "%u the index records%s\n",
                    first, (unsigned)got, (unsigned)size, bytes ? "" : " (no memory / bad entry)");
      } else {
        debugPrintf("[assets]   %s read back: %u B, starts \"%s\" -- openFd's getLength() and "
                    "open()'s stream are the same file\n",
                    first, (unsigned)got, head);
      }
      free(bytes);
    }
  }
  /* list(""): the names at the top of the APK's assets/, through the same
   * helper the list() handler uses -- the engine's list() must see the names
   * open()/openFd() answer for. */
  {
    char child[32][64];
    int n = 0, more = 0, dup = 0;
    for (int i = 0; i < g_nassets; i++) {
      const char *p = asset_in_dir(&g_assets[i], "", 0);
      const size_t len = strcspn(p, "/");
      dup = 0;
      for (int j = 0; j < n && !dup; j++)
        dup = strlen(child[j]) == len && !strncmp(child[j], p, len);
      if (dup)
        continue;
      if (n >= (int)(sizeof child / sizeof child[0]) || len >= sizeof child[0]) {
        more = 1;
        continue;
      }
      snprintf(child[n], sizeof child[0], "%.*s", (int)len, p);
      n++;
    }
    debugPrintf("[assets] list(\"\"): %d name(s)%s:", n, more ? " (capped)" : "");
    for (int i = 0; i < n; i++)
      debugPrintf(" %s", child[i]);
    debugPrintf("\n");
    /* The one subfolder the engine reads from: list("video") -> "android". */
    int subs = 0;
    for (int i = 0; i < g_nassets; i++) {
      if (asset_in_dir(&g_assets[i], "video", 5)) {
        subs++;
        break;
      }
    }
    debugPrintf("[assets] list(\"video\"): %s\n", subs ? "has children (video/android/...)" : "empty");
  }
  /* The index itself, entry by entry: what the port believes the APK holds.
   * With it, one run's log can be read against the APK's central directory
   * without a console. Capped, so a different APK cannot flood the log. */
  {
    const int cap = 48;
    debugPrintf("[assets] the index, entry by entry (name, bytes, method)%s:\n",
                g_nassets > cap ? " -- the first 48" : "");
    for (int i = 0; i < g_nassets && i < cap; i++)
      debugPrintf("[assets]   %-48s %8u B  %s\n", g_assets[i].name, (unsigned)g_assets[i].size,
                  g_assets[i].stored ? "STORE " : "DEFLATE");
    if (g_nassets > cap)
      debugPrintf("[assets]   ... and %d more\n", g_nassets - cap);
  }
  debugPrintf("[assets] ==== self-check %s: %d problem(s) ====\n", bad ? "FOUND PROBLEMS" : "clean",
              bad);
}

/* ================================================== AudioTrack ==========
 * THE CHAIN, AS MEASURED. On Android the game's own Java builds the track and
 * hands it to the engine's native, which then caches the track and drives its
 * mixer from it. The whole of it is in the APK's
 * com/ea/EAAudioCore/AndroidEAAudioCore.smali:
 *
 *   Startup(channels = 2)
 *     samplerate = AudioTrack.getNativeOutputSampleRate(3)     // 3 = STREAM_MUSIC
 *     channelConfig = 12 (CHANNEL_OUT_STEREO; 4 if channels == 1)
 *     format = 2 (ENCODING_PCM_16BIT)
 *     bufsize = AudioTrack.getMinBufferSize(samplerate, channelConfig, format)
 *     sAudioTrack = new AudioTrack(3, samplerate, channelConfig, format,
 *                                  bufsize, 1 - MODE_STREAM )
 *     frames = bufsize / (2 * channels)
 *     Init(sAudioTrack, frames, channels, samplerate, sIsOtherMusicPlaying)  // NATIVE
 *     sInit = true
 *
 * `Init` is `private static native Init(Landroid/media/AudioTrack;IIII)V`, i.e.
 * libFIFA14.so's Java_com_ea_EAAudioCore_AndroidEAAudioCore_Init
 * (@0x00ccef5c, 440 B). Its argument order is the Java call's -- the native
 * reads the track from r2, the first int from r3 and the rest from [sp+32],
 * [sp+36], [sp+40], and it stores them into AndroidEAAudioCoreJni's statics
 * (sAudioTrackObj, sNumChans, sSampleRate, sBufferSize,
 * sIsOtherMusicPlaying) after caching the three jmethodIDs it will use --
 * sMethodIdWrite, sMethodIdPlay, sMethodIdStop -- on the AudioTrack class, and
 * `NewShortArray(channels << 9)` for the mix it hands to write(). So the mixer
 * thread calls, on THIS port's objects:
 *
 *   play()                          -> audout is already running, a flag
 *   write([SII) / write([BII)       -> the samples, into audout
 *   stop() / flush() / release()    -> the end
 *
 * The port has to do the Java's half (the track object and the buffer size) and
 * call the native. It does, in fifa14_h_audiocore_startup below.
 *
 * WHY THERE WAS NO SOUND: `AndroidEAAudioCore.Startup()V` was landing on the
 * port's wildcard no-op (`[java] com/ea/EAAudioCore/AndroidEAAudioCore.Startup()V
 * -> no-op` in debug.log), so the native Init was never called, the mixer never
 * existed, and no write() ever arrived; and fifa14_audio_init() was never called
 * either, so the write path would have dropped the samples on the floor anyway.
 * Both are wired here.
 *
 * THE SUBMIT BLOCKS. rt_audout_submit waits while all three buffers are queued
 * (rt_audout.c) and that wait is what paces this mixer -- which is the ENGINE's
 * own audio thread, not the frame thread, so it cannot cost a frame. */
static int g_ao_ready;
static volatile int g_a_paused;
static volatile uint32_t g_a_blocks;
static int16_t g_a_block[RT_AUDOUT_FRAMES * 2];
static int g_a_fill;

/* What the summary line reports: the mixer's own counters (the peak is the
 * loudest sample seen, so a silent devkit says so instead of looking OK). */
static volatile unsigned g_a_samples;   /* int16 samples handed to write()      */
static volatile unsigned g_a_peak;      /* |sample|, the loudest since the last report */
static volatile unsigned g_a_peak_seen; /* the loudest of the whole run          */
static int g_a_playing;                 /* the engine called play()              */

/* The device's own line, once. */
int fifa14_audio_init(void) {
  if (g_ao_ready)
    return 0;
  if (rt_audout_open() != 0) {
    debugPrintf("[audio] audout did not open: no sound\n");
    return -1;
  }
  g_ao_ready = 1;
  debugPrintf("[audio] ==== the device: audout ====\n");
  debugPrintf("[audio] audout open at %u Hz, %d channel(s), s16 (PCM), %d-frame buffers, 3 "
              "queued\n",
              rt_audout_rate(), 2, RT_AUDOUT_FRAMES);
  debugPrintf("[audio] the engine's track: %u Hz, 2 channels, s16, %d frames a write "
              "(the same numbers the device takes, so a submit is a straight copy)\n",
              rt_audout_rate(), RT_AUDOUT_FRAMES);
  return 0;
}

/* The mixer's thread may be inside a submit at exit. */
void fifa14_audio_shutdown(void) {
  if (!g_ao_ready)
    return;
  rt_audout_cancel(1);
  rt_audout_close();
  g_ao_ready = 0;
  fifa14_audio_report();
  debugPrintf("[audio] closed after %u blocks\n", (unsigned)g_a_blocks);
}
void fifa14_audio_pause(int paused) { g_a_paused = paused; }

/* One line per window (the boot's frame loop, every FIFA14_FPS_LOG_FRAMES
 * frames), so the log says whether the engine is feeding the device and how
 * loud. `frames` is the engine's own count, the same series the [loop] line
 * uses. */
void fifa14_audio_report(void) {
  static unsigned last_blocks, last_samples, last_reported;
  static int warned;
  RtAudoutStats st;
  const unsigned blocks = g_a_blocks, samples = g_a_samples;
  memset(&st, 0, sizeof st);
  rt_audout_stats(&st);
  debugPrintf("[audio] %u blocks (+%u), %u samples (+%u) = %u frames, peak %u (whole run "
              "%u), underruns %lu, dropped %lu, %s\n",
              blocks, blocks - last_blocks, samples, samples - last_samples,
              samples / 2, g_a_peak, g_a_peak_seen, st.underruns, st.dropped,
              g_a_paused ? "paused" : (g_a_playing ? "playing" : "not started"));
  last_blocks = blocks;
  last_samples = samples;
  last_reported++;
  if (!blocks && last_reported > 2 && !warned) {
    warned = 1;
    debugPrintf("[audio] NOTHING has reached the device after %u windows: the engine's "
                "mixer is not calling AudioTrack.write (was AndroidEAAudioCore.Init "
                "reached? the [audio] ==== open line above says whether the device is up)\n",
                last_reported);
  }
  g_a_peak = 0;
}

/* ---- the engine's own Java, done by the port ------------------------------ */

/* The native the Java's Startup calls, and the numbers the Java computes for
 * it: the device's rate, two channels, s16, and the buffer size the Java asks
 * getMinBufferSize for. frames = bufsize / (2 * channels), so RT_AUDOUT_FRAMES
 * frames of a 1024-frame buffer with its two channels -- exactly what
 * write_samples submits. */
typedef void (*Fifa14AudioCoreInitFn)(void *env, void *cls, void *track, int frames,
                                     int channels, int rate, int otherMusic);

static void *g_a_track; /* the AudioTrack object the engine was handed */

/* The mixer may only be started once (the Java's own sInit flag). */
static int g_a_started;

static void audiocore_startup(int channels) {
  Fifa14AudioCoreInitFn init;
  const int rate = (int)rt_audout_rate();
  const int minbuf = RT_AUDOUT_FRAMES * 4 * 2; /* the same answer this port's
                                                * getMinBufferSize handler gives */
  int frames;
  if (g_a_started)
    return;
  if (channels < 1)
    channels = 2;
  if (channels > 2)
    channels = 2; /* the Java clamps to 2 as well, and audout is stereo */
  frames = minbuf / (2 * channels);
  if (fifa14_audio_init() != 0)
    return; /* no device: the mixer is not built, and the log said why */
  g_a_track = jni_new(F_AUDIO_TRACK);
  init = (Fifa14AudioCoreInitFn)fifa14_native_addr(FIFA14_ENTRY_EAAUDIOCORE_INIT);
  if (!init) {
    debugPrintf("[audio] %s MISSING: the engine's mixer cannot be started\n",
                FIFA14_ENTRY_EAAUDIOCORE_INIT);
    return;
  }
  debugPrintf("[audio] ==== the engine's Java, done by the port (AndroidEAAudioCore."
              "Startup) ====\n");
  debugPrintf("[audio] getMinBufferSize(%d, %d, 2) -> %d B, %d frames, %d channel(s); the "
              "track object is %s\n",
              rate, channels == 1 ? 4 : 12, minbuf, frames, channels, F_AUDIO_TRACK);
  debugPrintf("[audio] %s(track, %d frames, %d channels, %d Hz, other-music 0) -- the "
              "engine caches the track and its write/play/stop method ids\n",
              FIFA14_ENTRY_EAAUDIOCORE_INIT, frames, channels, rate);
  init(g_jni_env, NULL, g_a_track, frames, channels, rate, 0);
  g_a_started = 1;
  debugPrintf("[audio] the engine's audio core is up: its mixer thread will call "
              "AudioTrack.write from now on\n");
}

JNI_H_DECL(fifa14_h_audiocore_startup) {
  (void)self; (void)m;
  audiocore_startup(2); /* AndroidEAAudioCore.Startup()V passes channels = 2 */
  return jv_none();
}
JNI_H_DECL(fifa14_h_audiocore_startup_ch) {
  (void)self; (void)m;
  audiocore_startup(a[0].i);
  return jv_none();
}
JNI_H_DECL(fifa14_h_audiocore_shutdown) {
  void *rel;
  (void)self; (void)a; (void)m;
  rel = fifa14_native_addr(FIFA14_ENTRY_EAAUDIOCORE_RELEASE);
  if (rel)
    ((void (*)(void *))rel)(g_jni_env);
  g_a_started = 0;
  fifa14_audio_shutdown();
  return jv_none();
}
/* AudioTrack.play() -- audout is already playing (rt_audout_open starts it), so
 * this only records that the engine asked. */
JNI_H_DECL(fifa14_h_track_play) {
  static int logged;
  (void)self; (void)a; (void)m;
  g_a_playing = 1;
  if (!logged) {
    logged = 1;
    debugPrintf("[audio] the engine called AudioTrack.play() (audout has been playing "
                "since the boot)\n");
  }
  return jv_none();
}
JNI_H_DECL(fifa14_h_track_stop) {
  (void)self; (void)a; (void)m;
  g_a_playing = 0;
  return jv_none();
}
JNI_H_DECL(fifa14_h_track_flush) {
  (void)self; (void)a; (void)m;
  g_a_fill = 0; /* drop the half buffer, as flush() does */
  return jv_none();
}
JNI_H_DECL(fifa14_h_track_rate) { (void)self; (void)a; (void)m; return jv_i((jint)rt_audout_rate()); }
JNI_H_DECL(fifa14_h_track_minbuf) { (void)self; (void)a; (void)m; return jv_i(RT_AUDOUT_FRAMES * 4 * 2); }

static void write_samples(const int16_t *src, int count) {
  if (count > 0) {
    unsigned peak = g_a_peak;
    for (int i = 0; i < count; i++) {
      const int v = src[i] < 0 ? -src[i] : src[i];
      if ((unsigned)v > peak)
        peak = (unsigned)v;
    }
    g_a_peak = peak;
    if (peak > g_a_peak_seen)
      g_a_peak_seen = peak;
    g_a_samples += (unsigned)count;
  }
  while (count > 0) {
    int n = (int)(sizeof g_a_block / sizeof g_a_block[0]) - g_a_fill;
    if (n > count)
      n = count;
    memcpy(g_a_block + g_a_fill, src, sizeof(int16_t) * (size_t)n);
    g_a_fill += n;
    src += n;
    count -= n;
    if (g_a_fill < (int)(sizeof g_a_block / sizeof g_a_block[0]))
      break;
    g_a_fill = 0;
    if (g_a_paused || !g_ao_ready) {
      svcSleepThread((s64)RT_AUDOUT_FRAMES * 1000000000ll / (s64)rt_audout_rate());
      continue;
    }
    if (rt_audout_submit(g_a_block) == 0)
      g_a_blocks++;
  }
}

JNI_H_DECL(fifa14_h_track_write) {
  (void)m;
  const JObj *arr = a[0].l;
  const jint off = a[1].i, count = a[2].i;
  if (!arr || arr->kind != JK_ARRAY || !arr->a.data || off < 0 || count < 0 || off + count > arr->a.len)
    return jv_i(-2); /* AudioTrack.ERROR_BAD_VALUE */
  if (arr->a.elem == 'S')
    write_samples((const int16_t *)arr->a.data + off, (int)count);
  else
    write_samples((const int16_t *)((const uint8_t *)arr->a.data + off), (int)count / 2);
  return jv_i(count);
}
