/* fifa14_jni_tables.c -- FIFA 14's four JNI tables, in the runtime's exact
 * format. See fifa14_jni_tables.h for what a table entry is for, and
 * C:\Projects\ports\fifa14\notas\fifa14-jni-mapa.md for the source of every
 * entry (which reference port it was taken from, or "invented here").
 *
 * The handlers are in fifa14_blast_stubs.c. MIT.
 */
#include <string.h>
#include <switch.h>

#include "fifa14_jni_tables.h"
#include "jni.h"
#include "util.h"

JObj *fifa14_activity; /* MainActivity / Fifa14Activity: the one activity */

/* ============================================================== methods ===
 *
 * The Java methods the ENGINE calls (through GetMethodID/CallXxx). The
 * game's own NATIVE methods (NativeOnCreate, NativeOnDrawFrame, ...) are NOT
 * here: the port drives them directly, by symbol (as ds_loader.c does), and
 * the runtime never routes a CallXxxMethod to the module's Java_* export.
 * A class with a wildcard entry ({"cls", NULL, NULL, h}) answers every method
 * of that class with h -- used only where the engine's calls are the Java ->
 * native glue (void setters / receivers), never getters it reads.
 */
const JMethodDef jni_method_defs[] = {
    /* ----------------------------------------------- the activity + blast */
    {F_MAIN_ACTIVITY, "GetInstance", "()L" F_MAIN_ACTIVITY ";", fifa14_h_activity_instance},
    {F_MAIN_ACTIVITY, "getAssets", "()L" F_ASSET_MANAGER ";", fifa14_h_assets},
    {F_MAIN_ACTIVITY, "finish", "()V", fifa14_h_finish},
    {F_FIFA14_ACTIVITY, "<init>", NULL, fifa14_h_noop},
    {F_FIFA14_ACTIVITY, "GetGameSessionsCount", "()I", jni_h_zero},
    /* The engine calls these two STATICALLY on the activity's class (the log's
     * "[jni] unhandled static com/ea/game/fifa14/Fifa14Activity.GetThePackage()
     * Ljava/lang/String; (on java/lang/Class)" and the GetTheVersionCode()I
     * beside it). Answering them by class name works for a static call: the
     * runtime looks the receiver's class up (java/lang/Class -> Fifa14Activity)
     * and walks this same table. Both values are the APK's own manifest. */
    {F_FIFA14_ACTIVITY, "GetThePackage", "()Ljava/lang/String;", fifa14_h_app_package},
    {F_FIFA14_ACTIVITY, "GetTheVersionCode", "()I", fifa14_h_app_version_code},

    /* -------------------------------------------- SystemAndroidDelegate
     * The capability counters are ()I -- NOT the "()Ljava/lang/String;" this
     * block used to declare for them, which is why GetMethodID matched nothing
     * and the engine got the runtime's unhandled zero for every one:
     *
     *   "[jni] unhandled com/ea/blast/SystemAndroidDelegate.GetVibratorCount()I"
     *   ... and GetTrackBallCount/TouchScreen/TouchPad/Microphone/Camera/
     *       Compass/Gyroscope/AccelerometerCount()I, GetApplicationVersionCode()I,
     *       GetFirmware()/GetHardwareFloatingPointSupport()/GetApplicationVersion()
     *       Ljava/lang/String;  (13 lines in the run's debug.log)
     *
     * The values are the hardware of the phone the port reports (DEV_MODEL, a
     * Galaxy S3, fifa14_blast_stubs.c), and the versions come from the user's
     * own APK. EVERY String entry further down is a string the engine really
     * reads as a string (it parses the device name, the CPU ABI, ...). */
    {F_SYSTEM_DELEGATE, "<init>", NULL, fifa14_h_noop},
    {F_SYSTEM_DELEGATE, "GetAccelerometerCount", "()I", fifa14_h_dev_accelerometer},
    {F_SYSTEM_DELEGATE, "GetTouchScreenCount", "()I", fifa14_h_dev_touchscreen},
    {F_SYSTEM_DELEGATE, "GetTouchPadCount", "()I", fifa14_h_dev_touchpad},
    {F_SYSTEM_DELEGATE, "GetCameraCount", "()I", fifa14_h_dev_camera},
    {F_SYSTEM_DELEGATE, "GetCompassCount", "()I", fifa14_h_dev_compass},
    {F_SYSTEM_DELEGATE, "GetGyroscopeCount", "()I", fifa14_h_dev_gyroscope},
    {F_SYSTEM_DELEGATE, "GetMicrophoneCount", "()I", fifa14_h_dev_microphone},
    {F_SYSTEM_DELEGATE, "GetTrackBallCount", "()I", fifa14_h_dev_trackball},
    {F_SYSTEM_DELEGATE, "GetVibratorCount", "()I", fifa14_h_dev_vibrator},
    /* Not in the log: answered with both signatures, so whichever the engine
     * asks resolves -- and the []I one logs, so the next run says which. */
    {F_SYSTEM_DELEGATE, "GetPhysicalKeyboardCount", "()I", fifa14_h_dev_phys_keyboard},
    {F_SYSTEM_DELEGATE, "GetVirtualKeyboardCount", "()I", fifa14_h_dev_virt_keyboard},
    {F_SYSTEM_DELEGATE, "GetDisplayCount", "()I", fifa14_h_dev_display},
    {F_SYSTEM_DELEGATE, "GetPhysicalKeyboardCount", "()Ljava/lang/String;", fifa14_h_dev_str_zero},
    {F_SYSTEM_DELEGATE, "GetVirtualKeyboardCount", "()Ljava/lang/String;", fifa14_h_dev_str_one},
    {F_SYSTEM_DELEGATE, "GetDisplayCount", "()Ljava/lang/String;", fifa14_h_dev_str_one},
    /* The two the engine asks for the app's identity, and the two strings of
     * the device it cannot read without them (all four were unhandled). */
    {F_SYSTEM_DELEGATE, "GetApplicationVersionCode", "()I", fifa14_h_app_version_code},
    {F_SYSTEM_DELEGATE, "GetApplicationVersion", "()Ljava/lang/String;", fifa14_h_app_version_name},
    {F_SYSTEM_DELEGATE, "GetFirmware", "()Ljava/lang/String;", fifa14_h_dev_firmware},
    {F_SYSTEM_DELEGATE, "GetHardwareFloatingPointSupport", "()Ljava/lang/String;",
     fifa14_h_dev_hw_fp_support},
    {F_SYSTEM_DELEGATE, "IsBatteryStateAvailable", "()Ljava/lang/String;", fifa14_h_dev_false},
    /* The 9 capability counters' old "()Ljava/lang/String;" entries are gone
     * on purpose: the engine asks them as ()I (the log proves it), and a
     * duplicate String entry would only hide which of the two conventions its
     * Java side uses. A String caller now falls to the runtime's default, which
     * logs and answers "" -- i.e. it shows up in the next run instead of
     * silently getting a value nobody asked for. */
    {F_SYSTEM_DELEGATE, "GetLocationAvailable", "()Ljava/lang/String;", fifa14_h_dev_false},
    {F_SYSTEM_DELEGATE, "GetDeviceModel", "()Ljava/lang/String;", fifa14_h_dev_model},
    {F_SYSTEM_DELEGATE, "GetDeviceName", "()Ljava/lang/String;", fifa14_h_dev_model},
    {F_SYSTEM_DELEGATE, "GetManufacturer", "()Ljava/lang/String;", fifa14_h_dev_manufacturer},
    {F_SYSTEM_DELEGATE, "GetPlatformRawName", "()Ljava/lang/String;", fifa14_h_dev_platform},
    {F_SYSTEM_DELEGATE, "GetPlatformStdName", "()Ljava/lang/String;", fifa14_h_dev_platform},
    {F_SYSTEM_DELEGATE, "GetPlatformVersion", "()Ljava/lang/String;", fifa14_h_dev_version},
    {F_SYSTEM_DELEGATE, "GetApiLevel", "()Ljava/lang/String;", fifa14_h_dev_api},
    {F_SYSTEM_DELEGATE, "GetProcessorArchitecture", "()Ljava/lang/String;", fifa14_h_dev_abi},
    {F_SYSTEM_DELEGATE, "GetLanguage", "()Ljava/lang/String;", fifa14_h_dev_language},
    {F_SYSTEM_DELEGATE, "GetLocale", "()Ljava/lang/String;", fifa14_h_dev_locale},
    {F_SYSTEM_DELEGATE, "GetChipset", "()Ljava/lang/String;", jni_h_empty_string},
    {F_SYSTEM_DELEGATE, "GetPhoneNumber", "()Ljava/lang/String;", jni_h_empty_string},
    {F_SYSTEM_DELEGATE, "GetDeviceSubscriberID", "()Ljava/lang/String;", jni_h_empty_string},
    {F_SYSTEM_DELEGATE, "GetTotalRAM", "()Ljava/lang/String;", fifa14_h_dev_unknown},
    {F_SYSTEM_DELEGATE, "GetDeviceUniqueId", "()Ljava/lang/String;", fifa14_h_dev_unknown},
    {F_SYSTEM_DELEGATE, "IntentView", "(Ljava/lang/String;)Z", jni_h_false},
    /* NO class-level wildcard here: most of this delegate's methods return a
     * String the engine parses, and a wildcard would answer NULL where the
     * runtime's own default answers "". An unlisted getter therefore falls to
     * that default and shows in the log -- add it above as it appears. */

    /* ------------------------------------------- DisplayAndroidDelegate */
    {F_DISPLAY_DELEGATE, "<init>", NULL, fifa14_h_noop},
    {F_DISPLAY_DELEGATE, "GetDefaultWidth", "()I", fifa14_h_display_width},
    {F_DISPLAY_DELEGATE, "GetDefaultHeight", "()I", fifa14_h_display_height},
    {F_DISPLAY_DELEGATE, "GetStdOrientation", "()I", fifa14_h_display_orientation},
    {F_DISPLAY_DELEGATE, "SetStdOrientation", "(I)V", fifa14_h_noop},
    {F_DISPLAY_DELEGATE, "GetDpiX", "()F", fifa14_h_display_dpi},
    {F_DISPLAY_DELEGATE, "GetDpiY", "()F", fifa14_h_display_dpi},
    /* The engine's DisplayAndroid::CreateView @0xe0bc70 asks for this one and
     * stores the answer in a global ref it hands to ViewAndroid; unhandled it
     * was NULL ("[jni] unhandled ...GetGLView()Ljava/lang/Object;") and the
     * engine's view had no view behind it. On Android it is the GLSurfaceView
     * whose context the port now makes itself (fifa14_gl.c). */
    {F_DISPLAY_DELEGATE, "GetGLView", "()Ljava/lang/Object;", fifa14_h_gl_view},

    /* -------------------------------------------------- the phone's dirs */
    {F_GETAPPDATA_DELEGATE, "<init>", NULL, fifa14_h_noop},
    {F_GETAPPDATA_DELEGATE, "GetAppDataDirectory", "()Ljava/lang/String;", fifa14_h_app_data_dir},
    {F_GETAPPDATA_DELEGATE, "GetExternalStorageDirectory", "()Ljava/lang/String;", fifa14_h_app_ext_dir},
    {F_TOUCH_SURFACE, "IsTouchScreenMultiTouch", "()Z", jni_h_true},

    /* -------------------------------------------------- input delegates
     * The engine sets listeners / drives these; nothing the engine READS. */
    {F_ACCEL_DELEGATE, NULL, NULL, fifa14_h_noop_log},
    {F_DEVICE_ORIENTATION, NULL, NULL, fifa14_h_noop_log},
    {F_POWER_MANAGER, NULL, NULL, fifa14_h_noop_log},
    {F_VIEW_DELEGATE, NULL, NULL, fifa14_h_noop_log},
    {F_VIRTUAL_KBD_DELEGATE, NULL, NULL, fifa14_h_noop_log},
    {F_PHYSICAL_KBD_DELEGATE, NULL, NULL, fifa14_h_noop_log},
    {F_KEYBOARD_ANDROID, NULL, NULL, fifa14_h_noop_log},
    {F_MOUSE_ANDROID, NULL, NULL, fifa14_h_noop_log},
    {F_TOUCHPAD_XPERIA, NULL, NULL, fifa14_h_noop_log},
    {F_MODULE_CATALOG, NULL, NULL, fifa14_h_noop_log},

    /* ------------------------------------------------ offline services
     * The engine's Java glue for these calls native (port-driven) or talks to
     * a device service that is not there. Nothing the engine reads. */
    {F_BATTERY_DELEGATE, NULL, NULL, fifa14_h_noop_log},
    {F_VIBRATOR_DELEGATE, NULL, NULL, fifa14_h_noop_log},
    {F_NOTIFICATION, NULL, NULL, fifa14_h_noop_log},
    {F_NFC, NULL, NULL, fifa14_h_noop_log},
    {F_C2DM_RECEIVER, NULL, NULL, fifa14_h_noop_log},
    {F_LOCAL_NOTIF_RECEIVER, NULL, NULL, fifa14_h_noop_log},

    /* -------------------------------------------------- EA services
     * EAIO.Startup(AssetManager)/Shutdown are routed (TODO: to the engine's
     * own Java_com_ea_EAIO_EAIO_StartupNativeImpl, as ds_java.c does). The
     * rest are the Java -> native glue; the port drives the natives. */
    {F_EAIO, "Startup", "(L" F_ASSET_MANAGER ";)V", fifa14_h_ea_startup},
    {F_EAIO, "Shutdown", "()V", fifa14_h_ea_shutdown},
    {F_EAIO, NULL, NULL, fifa14_h_noop_log},
    /* Every one of these is a String the engine's own EA::IO::StorageDirectory
     * reads: the class's native StartupNativeImpl asks Java for them by name
     * right after it has stored the JavaVM, so NONE may fall to the wildcard
     * (which answers ""). The paths are the ones the APK's Java returns, in the
     * Android spelling the runtime's map (dcr_path.c) sends into the game
     * folder -- /sdcard -> <game folder>/sdcard, /data/data/<pkg>/... ->
     * <game folder>/data/... -- see fifa14_blast_stubs.c. */
    {F_EAMIO_STORAGE, F_EAMIO_STARTUP, NULL, fifa14_h_eamio_startup},
    {F_EAMIO_STORAGE, F_EAMIO_SHUTDOWN, NULL, fifa14_h_eamio_shutdown},
    {F_EAMIO_STORAGE, F_EAMIO_DEDICATED, "()Ljava/lang/String;", fifa14_h_storage_dedicated},
    {F_EAMIO_STORAGE, F_EAMIO_INTERNAL, "()Ljava/lang/String;", fifa14_h_storage_internal},
    {F_EAMIO_STORAGE, F_EAMIO_EXTERNAL, "()Ljava/lang/String;", fifa14_h_storage_external},
    {F_EAMIO_STORAGE, F_EAMIO_STATE, "()Ljava/lang/String;", fifa14_h_storage_state},
    {F_EAMIO_STORAGE, NULL, NULL, fifa14_h_noop_log},
    /* The engine's own Java: EAAudioCore's AndroidEAAudioCore builds the
     * AudioTrack and hands it to its native Init -- and THAT is what starts the
     * engine's mixer. These entries have to come BEFORE the class wildcard below
     * (find_method_def returns the first match, and a wildcard matches every
     * name), which is the same order F_EAMIO_STORAGE uses above. With them on the
     * wildcard no-op the native was never called and the port had no sound; the
     * whole measured chain is in fifa14_blast_stubs.c's AudioTrack section. */
    {F_AUDIOCORE, "<init>", "()V", fifa14_h_noop},
    {F_AUDIOCORE, "Startup", "()V", fifa14_h_audiocore_startup},
    {F_AUDIOCORE, "Startup", "(I)V", fifa14_h_audiocore_startup_ch},
    {F_AUDIOCORE, "Shutdown", "()V", fifa14_h_audiocore_shutdown},
    {F_AUDIOCORE, NULL, NULL, fifa14_h_noop_log},
    {F_EAMAUDIO_WRAPPER, NULL, NULL, fifa14_h_noop_log},
    {F_EATHREAD, NULL, NULL, fifa14_h_noop_log},
    /* The Java side of EA::InAppWebBrowser::BrowserAndroid and
     * EA::VideoPlayer::PlayerAndroid. Their Startup is NOT a no-op to swallow:
     * it is what calls the native StartupNativeImpl that gives that class's
     * static EA::Jni::Context its JavaVM -- the Context the class's
     * EA::Jni::Delegate constructor calls GetEnv() on (@0xe3a374), the crash
     * in crash.log. Routed like EAIO's and StorageDirectory's above; the boot
     * calls the same two natives (fifa14_boot.c step 5d), so either order
     * works. Everything else on the two classes falls to the wildcard. */
    {F_BROWSER, "Startup", NULL, fifa14_h_browser_startup},
    {F_VIDEO_PLAYER, "Startup", NULL, fifa14_h_video_startup},
    {F_BROWSER, NULL, NULL, fifa14_h_noop_log},
    {F_VIDEO_PLAYER, NULL, NULL, fifa14_h_noop_log},
    {F_RWFILESYSTEM, NULL, NULL, fifa14_h_noop_log},
    {F_GOOGLEPLUS_HANDLER, NULL, NULL, fifa14_h_social},
    {F_YOUTUBE_HANDLER, NULL, NULL, fifa14_h_social},

    /* -------------------------------------------- the class-resolution chain
     * Class.getClassLoader() + ClassLoader.loadClass(name) is how the engine's
     * Nimble layer resolves every class name (libnimble.so's JNI_OnLoad caches
     * the loader; config.h decodes that block out of the module). Unhandled it
     * was NULL, loadClass ran on a null receiver, and CppBridge printed "Can't
     * find class ..." for classes that ARE in classes.txt -- among them
     * java/util/ArrayList, java/util/List and
     * com/ea/nimble/bridge/FacebookNativeCallback, right before the run that
     * ended when the user tried to share to YouTube/Facebook. Both answer from
     * the port's own class table now (fifa14_blast_stubs.c). */
    {JAVA("lang/Class"), "getClassLoader", "()Ljava/lang/ClassLoader;", fifa14_h_class_getloader},
    {JAVA("lang/Class"), "forName", "(Ljava/lang/String;)Ljava/lang/Class;",
     fifa14_h_classloader_loadclass},
    {JAVA("lang/Class"), "forName", "(Ljava/lang/String;ZLjava/lang/ClassLoader;)Ljava/lang/Class;",
     fifa14_h_classloader_loadclass},
    {JAVA("lang/ClassLoader"), "getSystemClassLoader", "()Ljava/lang/ClassLoader;",
     fifa14_h_class_getloader},
    {JAVA("lang/ClassLoader"), "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;",
     fifa14_h_classloader_loadclass},
    {JAVA("lang/ClassLoader"), "loadClass", "(Ljava/lang/String;Z)Ljava/lang/Class;",
     fifa14_h_classloader_loadclass},

    /* --------------------------------------- the social/network SDKs (offline)
     * EA's servers are gone and this console has no network and no other app.
     * The engine probes these by name; every answer is the inert one and every
     * first touch gets a "[social] ..." line. They are listed explicitly so the
     * log NAMES the door instead of showing the runtime's generic "unhandled". */
    {NIMBLE("Facebook"), NULL, NULL, fifa14_h_social},
    {NIMBLE("IFacebook"), NULL, NULL, fifa14_h_social},
    {NIMBLE("origin/Origin"), NULL, NULL, fifa14_h_social},
    {NIMBLE("origin/NetworkIdentity"), NULL, NULL, fifa14_h_social},
    {NIMBLE("identity/NimbleIdentity"), NULL, NULL, fifa14_h_social},
    {NIMBLE("friends/NimbleFriends"), NULL, NULL, fifa14_h_social},
    {NIMBLE("mtx/NimbleMTX"), NULL, NULL, fifa14_h_social},
    {NIMBLE("bridge/FacebookNativeCallback"), NULL, NULL, fifa14_h_social},

    /* ------------------------------------------ android.content.res */
    {F_ASSET_MANAGER, "open", "(Ljava/lang/String;)Ljava/io/InputStream;", fifa14_h_asset_open},
    {F_ASSET_MANAGER, "openFd", "(Ljava/lang/String;)L" F_ASSET_FD ";", fifa14_h_asset_open_fd},
    {F_ASSET_MANAGER, "list", "(Ljava/lang/String;)[Ljava/lang/String;", fifa14_h_asset_list},
    {F_ASSET_FD, "getLength", "()J", fifa14_h_asset_fd_length},
    {F_ASSET_FD, "getStartOffset", "()J", fifa14_h_asset_fd_offset},
    {F_ASSET_FD, "getDeclaredLength", "()J", fifa14_h_asset_fd_length},
    {F_ASSET_FD, "close", "()V", fifa14_h_noop},
    {F_INPUT_STREAM, "read", "([BII)I", fifa14_h_stream_read},
    {F_INPUT_STREAM, "read", "()I", fifa14_h_stream_read},
    {F_INPUT_STREAM, "skip", "(J)J", fifa14_h_stream_skip},
    {F_INPUT_STREAM, "close", "()V", fifa14_h_stream_close},
    {F_FILE_DESCRIPTOR, "getInt$", "()I", fifa14_h_fd_int},
    {F_FILE_DESCRIPTOR, "valid", "()Z", jni_h_false},

    /* -------------------------------------------- android.media.AudioTrack
     * EAAudioCore mixes the sound itself and writes it to a Java AudioTrack
     * (AndroidEAAudioCore.Startup): these are the calls the mixer's thread
     * makes (adapted from ds_audio.c). */
    {F_AUDIO_TRACK, "getNativeOutputSampleRate", "(I)I", fifa14_h_track_rate},
    {F_AUDIO_TRACK, "getMinBufferSize", "(III)I", fifa14_h_track_minbuf},
    {F_AUDIO_TRACK, "write", "([SII)I", fifa14_h_track_write},
    {F_AUDIO_TRACK, "write", "([BII)I", fifa14_h_track_write},
    {F_AUDIO_TRACK, "play", "()V", fifa14_h_track_play},
    {F_AUDIO_TRACK, "pause", "()V", fifa14_h_noop},
    {F_AUDIO_TRACK, "stop", "()V", fifa14_h_track_stop},
    {F_AUDIO_TRACK, "flush", "()V", fifa14_h_track_flush},
    {F_AUDIO_TRACK, "release", "()V", fifa14_h_track_stop},

    {NULL, NULL, NULL, NULL},
};

/* ================================================================ fields ===
 * Platform constants the engine may read (GetStaticIntField on
 * android/os/Build$VERSION). A field with no def returns 0; MODEL etc. are
 * strings and have to be declared to answer a device name. */
const JFieldDef jni_field_defs[] = {
    {"android/os/Build$VERSION", "SDK_INT", NULL, 19, NULL}, /* Android 4.4 */
    {"android/os/Build", "MODEL", NULL, 0, "R800i"},
    {"android/os/Build", "DEVICE", NULL, 0, "R800i"},
    {"android/os/Build", "MANUFACTURER", NULL, 0, "Sony Ericsson"},
    {"android/os/Build", "BRAND", NULL, 0, "Sony Ericsson"},
    {NULL, NULL, NULL, 0, NULL},
};

/* =============================================================== supers ===
 * The class hierarchy. Only needed so a method declared on a superclass is
 * found when looked up on the subclass (jni_core walks c->super). Anything
 * not listed falls back to java/lang/Object. */
const char *const jni_class_supers[][2] = {
    {F_FIFA14_ACTIVITY, F_ACTIVITY_BASE},
    {F_ACTIVITY_BASE, "android/app/Activity"},
    {"android/app/Activity", "android/content/Context"},
    {F_VIEW_DELEGATE, "android/view/View"},
    {"android/view/View", "java/lang/Object"},
    /* The object GetGLView() answers: a view, as GLSurfaceView is. */
    {F_GL_VIEW, "android/view/View"},
    {F_ASSET_FD, "java/lang/Object"},
    {F_AUDIO_TRACK, "java/lang/Object"},
    {F_INPUT_STREAM, "java/lang/Object"},
    {NULL, NULL},
};

/* ======================================================= missing classes ==
 * classes.txt (written from the APK) decides this when it is present; this
 * array is the fallback for when it is not. EMPTY by default: with it empty,
 * jni_class_exists() says every class but the framework's exists, so FindClass
 * never fails -- the safe first run. FIFA14_SOCIAL_MISSING=1 turns on the
 * offline candidates (the network/social SDK), which the engine should find
 * absent and skip (EA's servers are gone: Origin, Synergy, MTX, Facebook). */
const char *const jni_missing_classes[] = {
#if FIFA14_SOCIAL_MISSING
    NIMBLE("origin/*"),
    NIMBLE("mtx/*"),
    NIMBLE("friends/*"),
    NIMBLE("identity/*"),
    NIMBLE("inappmessage/*"),
    NIMBLE("pushnotificationgoogle/*"),
    NIMBLE("tracking/*"),
    NIMBLE("Facebook*"),
    NIMBLE("Synergy*"),
    NIMBLE("Network*"),
    NIMBLE("HttpRequest*"),
    NIMBLE("IHttp*"),
    NIMBLE("bridge/FacebookNativeCallback"),
    NIMBLE("bridge/FriendsNativeCallback"),
    NIMBLE("bridge/IdentityNativeCallback"),
    NIMBLE("bridge/MTXNativeCallback"),
    EAP("game/fifa14/GooglePlusHandler"),
    EAP("game/fifa14/YouTubeHandler"),
    BLAST("C2DMReceiver"),
    BLAST("NearFieldCommunicationAndroid"),
#endif
    NULL,
};

/* ================================================= the class inventory ====
 * Not a table the runtime reads: the explicit list of the engine's own Java
 * classes (from fifa14-superficie-jni.md sections 6). libFIFA14.so's 26 and
 * libnimble.so's 101 -- what a classes.txt writer would emit. Kept here so the
 * surface is documented next to the tables. */
const char *const fifa14_game_classes[] = {
    ANDROID("content/res/AssetFileDescriptor"),
    ANDROID("view/ViewRoot"),
    F_GL_VIEW, /* the object DisplayAndroidDelegate.GetGLView() answers */
    F_AUDIOCORE, F_EAMIO_STORAGE, F_BROWSER, F_VIDEO_PLAYER,
    F_ACCEL_DELEGATE, F_BATTERY_DELEGATE, F_DEVICE_ORIENTATION, F_DISPLAY_DELEGATE,
    F_MAIN_ACTIVITY, F_NFC, F_NOTIFICATION, F_PHYSICAL_KBD_DELEGATE, F_POWER_MANAGER,
    F_SYSTEM_DELEGATE, F_TOUCHPAD_XPERIA, F_TOUCH_SURFACE, F_VIBRATOR_DELEGATE,
    F_VIEW_DELEGATE, F_VIRTUAL_KBD_DELEGATE, F_FIFA14_ACTIVITY,
    F_GOOGLEPLUS_HANDLER, F_YOUTUBE_HANDLER,
    JAVA("io/FileDescriptor"), JAVA("io/InputStream"),
    NULL,
};
const int fifa14_game_class_count = 26;

const char *const fifa14_nimble_classes[] = {
    NIMBLE("ApplicationEnvironment"), NIMBLE("Base"), NIMBLE("Error"), NIMBLE("Facebook"), NIMBLE("Global"),
    NIMBLE("HttpRequest"), NIMBLE("IApplicationEnvironment"), NIMBLE("IFacebook"),
    NIMBLE("IHttpRequest$Method"), NIMBLE("IHttpRequest$OverwritePolicy"), NIMBLE("IHttpResponse"),
    NIMBLE("ILog"), NIMBLE("INetwork"), NIMBLE("IPersistenceService"), NIMBLE("ISynergyEnvironment"),
    NIMBLE("ISynergyIdManager"), NIMBLE("ISynergyNetwork"), NIMBLE("ISynergyResponse"), NIMBLE("Log"),
    NIMBLE("Network"), NIMBLE("NetworkConnectionHandle"), NIMBLE("NimbleConfiguration"), NIMBLE("Persistence"),
    NIMBLE("Persistence$Storage"), NIMBLE("PersistenceService"), NIMBLE("PersistenceService$PersistenceMergePolicy"),
    NIMBLE("SynergyEnvironment"), NIMBLE("SynergyIdManager"), NIMBLE("SynergyNetwork"),
    NIMBLE("SynergyNetworkConnectionHandle"), NIMBLE("SynergyRequest"), NIMBLE("Utility"),
    NIMBLE("bridge/BaseNativeCallback"), NIMBLE("bridge/FacebookNativeCallback"),
    NIMBLE("bridge/FriendsNativeCallback"), NIMBLE("bridge/IdentityNativeCallback"),
    NIMBLE("bridge/MTXNativeCallback"), NIMBLE("bridge/OriginHelper"),
    NIMBLE("friends/INimbleFriends"), NIMBLE("friends/NimbleFriends"), NIMBLE("friends/NimbleFriendsList"),
    NIMBLE("friends/NimbleFriendsRefreshBasicInfo"), NIMBLE("friends/NimbleFriendsRefreshIdentityInfo"),
    NIMBLE("friends/NimbleUser"), NIMBLE("identity/INimbleIdentity"), NIMBLE("identity/NimbleIdentity"),
    NIMBLE("identity/authenticator/INimbleIdentityAuthenticator"),
    NIMBLE("identity/authenticator/NimbleIdentityLoginParams$FacebookAccessTokenLoginParams"),
    NIMBLE("identity/authenticator/NimbleIdentityLoginParams$FacebookClientLoginParams"),
    NIMBLE("identity/authenticator/NimbleIdentityLoginParams$OriginCredentialsLoginParams"),
    NIMBLE("identity/authenticator/NimbleIdentityLoginParams$OriginOAuthCodeLoginParams"),
    NIMBLE("identity/dataobject/NimbleIdentityPersona"), NIMBLE("identity/dataobject/NimbleIdentityPidInfo"),
    NIMBLE("identity/dataobject/NimbleIdentityUserInfo"), NIMBLE("inappmessage/IInAppMessage"),
    NIMBLE("inappmessage/InAppMessage"), NIMBLE("mtx/INimbleMTX"), NIMBLE("mtx/NimbleCatalogItem"),
    NIMBLE("mtx/NimbleCatalogItem$ItemType"), NIMBLE("mtx/NimbleMTX"), NIMBLE("mtx/NimbleMTXTransaction"),
    NIMBLE("origin/Friend"), NIMBLE("origin/LoginDetails"), NIMBLE("origin/NetworkIdentity"),
    NIMBLE("origin/Origin"), NIMBLE("origin/Origin$LoginStatus"), NIMBLE("origin/SynergyUid"),
    NIMBLE("pushnotificationgoogle/IPushNotification"), NIMBLE("pushnotificationgoogle/PushNotification"),
    NIMBLE("tracking/ITracking"), NIMBLE("tracking/SynergyConstants"), NIMBLE("tracking/Tracking"),
    JAVA("io/ByteArrayOutputStream"), JAVA("io/InputStream"), JAVA("lang/Boolean"), JAVA("lang/Byte"),
    JAVA("lang/Class"), JAVA("lang/ClassLoader"), JAVA("lang/Double"), JAVA("lang/Enum"), JAVA("lang/Float"),
    JAVA("lang/Integer"), JAVA("lang/Long"), JAVA("lang/Number"), JAVA("lang/Object"), JAVA("lang/Short"),
    JAVA("lang/String"), JAVA("lang/Throwable"), JAVA("net/URL"), JAVA("util/ArrayList"),
    JAVA("util/Collection"), JAVA("util/Date"), JAVA("util/EnumSet"), JAVA("util/HashMap"),
    JAVA("util/Iterator"), JAVA("util/List"), JAVA("util/Map"), JAVA("util/Map$Entry"), JAVA("util/Set"),
    "org/json/JSONArray", "org/json/JSONObject",
    NULL,
};
const int fifa14_nimble_class_count = 101;

/* ================================================================ monitors ==
 * The runtime's MonitorEnter/MonitorExit do not lock; Dead Space's engine
 * needs them real (its AssetManagerJNI::Read holds one across
 * InputStream.read -> GetByteArrayRegion, and without it two loading threads
 * read each other's bytes). FIFA's engine reads its files the same way, so the
 * same fix is installed here (ds_java.c:204-257). */
#define JNI_MONITOR_ENTER 217 /* JNINativeInterface slots */
#define JNI_MONITOR_EXIT 218
#define MAX_MONITORS 16
static struct { const void *obj; RMutex lock; } g_monitors[MAX_MONITORS];
static int g_nmonitors;
static Mutex g_monitors_lock;

static RMutex *monitor_of(const void *obj) {
  mutexLock(&g_monitors_lock);
  int i = 0;
  while (i < g_nmonitors && g_monitors[i].obj != obj)
    i++;
  if (i == MAX_MONITORS)
    i--;
  else if (i == g_nmonitors) {
    g_monitors[i].obj = obj;
    rmutexInit(&g_monitors[i].lock);
    g_nmonitors++;
  }
  mutexUnlock(&g_monitors_lock);
  return &g_monitors[i].lock;
}
static int monitor_enter(void *env, void *obj) { (void)env; rmutexLock(monitor_of(obj)); return 0; }
static int monitor_exit(void *env, void *obj) { (void)env; rmutexUnlock(monitor_of(obj)); return 0; }

/* =================================================================== setup */
void fifa14_java_init(void) {
  jni_init();
  fifa14_stubs_init(); /* fifa14_blast_stubs.c's wildcard log list */
  void **env = *(void ***)g_jni_env; /* the JNIEnv function table */
  env[JNI_MONITOR_ENTER] = (void *)monitor_enter;
  env[JNI_MONITOR_EXIT] = (void *)monitor_exit;
  fifa14_activity = jni_singleton(F_FIFA14_ACTIVITY);
  JObj *am = jni_singleton(F_ASSET_MANAGER);
  /* AssetManager.mObject: a native AAssetManager. Only handed to
   * AConfiguration_fromAssetManager, which does not look inside (pvz_java.c). */
  static uint32_t fake_aasset_manager[16];
  jni_set_field(am, "mObject", jv_j((jlong)(uintptr_t)fake_aasset_manager), 0);
  debugPrintf("[java] FIFA 14 surface ready: activity %p, %d game + %d nimble classes\n",
              (void *)fifa14_activity, fifa14_game_class_count, fifa14_nimble_class_count);
}
