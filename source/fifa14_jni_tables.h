/* fifa14_jni_tables.h -- the Java surface FIFA 14 is given, and where each
 * piece of it comes from.
 *
 * The runtime's fake VM (runtime/source/jni_core.c + jni.h) needs four tables,
 * defined by the port:
 *
 *     const JMethodDef jni_method_defs[];
 *     const JFieldDef  jni_field_defs[];
 *     const char *const jni_class_supers[][2];
 *     const char *const jni_missing_classes[];
 *
 * jni_core.c interns EVERY class name it is asked for (jni_class() never
 * returns NULL), so a class needs no table entry to "exist": FindClass
 * succeeds for anything not in jni_missing_classes. A method with no matching
 * JMethodDef is answered with its return type's zero (and "" for a String
 * return) and logged once -- that log is the to-do list. So a table entry is
 * only needed when the engine must get a NON-zero / non-empty answer (a device
 * model, the screen size, the APK's files, the sound track).
 *
 * The tables themselves are in fifa14_jni_tables.c; the handlers they name are
 * in fifa14_blast_stubs.c. Both are NEW files for this port. The main reference
 * is hazavauks/deadspace_nx (Dead Space Mobile, EA, the SAME "EA blast"
 * framework): its source/ds_java.c is the model for the blast delegates, and
 * source/ds_assets.c / source/ds_audio.c for AssetManager / AudioTrack. See
 * C:\Projects\ports\fifa14\notas\fifa14-jni-mapa.md for the full source map.
 *
 * MIT.
 */
#ifndef FIFA14_JNI_TABLES_H
#define FIFA14_JNI_TABLES_H

#include "jni.h"

/* ------------------------------------------------------------------ names
 * The class names exactly as the game passes them to FindClass (extracted from
 * the .so string tables; see fifa14-superficie-jni.md). */

#define BLAST(x)  "com/ea/blast/" x
#define EAP(x)    "com/ea/" x
#define NIMBLE(x) "com/ea/nimble/" x
#define ANDROID(x) "android/" x
#define JAVA(x)   "java/" x

/* libFIFA14.so -- 22 of its own + 2 Android + 2 java (26 total). */
#define F_MAIN_ACTIVITY        BLAST("MainActivity")
#define F_ANDROID_RENDERER     BLAST("AndroidRenderer")
#define F_TOUCH_SURFACE        BLAST("TouchSurfaceAndroid")
#define F_VIRTUAL_KBD_DELEGATE BLAST("VirtualKeyboardAndroidDelegate")
#define F_PHYSICAL_KBD_DELEGATE BLAST("PhysicalKeyboardAndroidDelegate")
#define F_VIEW_DELEGATE        BLAST("ViewAndroidDelegate")
#define F_DISPLAY_DELEGATE     BLAST("DisplayAndroidDelegate")
#define F_SYSTEM_DELEGATE      BLAST("SystemAndroidDelegate")
#define F_ACCEL_DELEGATE       BLAST("AccelerometerAndroidDelegate")
#define F_BATTERY_DELEGATE     BLAST("BatteryAndroidDelegate")
#define F_VIBRATOR_DELEGATE    BLAST("VibratorAndroidDelegate")
#define F_POWER_MANAGER        BLAST("PowerManagerAndroid")
#define F_NOTIFICATION         BLAST("NotificationAndroid")
#define F_NFC                  BLAST("NearFieldCommunicationAndroid")
#define F_DEVICE_ORIENTATION   BLAST("DeviceOrientationHandlerAndroidDelegate")
#define F_TOUCHPAD_XPERIA      BLAST("TouchPadAndroidXperiaPlay")
#define F_C2DM_RECEIVER        BLAST("C2DMReceiver")
#define F_GETAPPDATA_DELEGATE  BLAST("GetAppDataDirectoryDelegate")
#define F_MODULE_CATALOG       BLAST("ModuleCatalog")
#define F_KEYBOARD_ANDROID     BLAST("KeyboardAndroid")
#define F_MOUSE_ANDROID        BLAST("MouseAndroid")
#define F_LOCAL_NOTIF_RECEIVER BLAST("LocalNotificationReceiver")
#define F_AUDIOCORE            EAP("EAAudioCore/AndroidEAAudioCore")
#define F_EAMIO_STORAGE        EAP("EAMIO/StorageDirectory")
/* com/ea/EAMIO/StorageDirectory's own methods (classes.dex:
 * StorageDirectory.java). The four getters are the ones the class's native
 * StartupNativeImpl asks for BY NAME (the strings are in libFIFA14.so:
 * 'GetInternalStorageDirectory', 'GetPrimaryExternalStorageDirectory',
 * 'GetPrimaryExternalStorageState', 'GetDedicatedDirectory', all
 * "()Ljava/lang/String;"), and Startup/Shutdown are the Java wrappers of the
 * two natives the port routes to the engine (fifa14_jni_tables.c). */
#define F_EAMIO_STARTUP        "Startup"      /* (Landroid/app/Activity;)V -> StartupNativeImpl() */
#define F_EAMIO_SHUTDOWN       "Shutdown"     /* ()V -> ShutdownNativeImpl()                   */
#define F_EAMIO_DEDICATED      "GetDedicatedDirectory"
#define F_EAMIO_INTERNAL       "GetInternalStorageDirectory"
#define F_EAMIO_EXTERNAL       "GetPrimaryExternalStorageDirectory"
#define F_EAMIO_STATE          "GetPrimaryExternalStorageState"
#define F_EAMAUDIO_WRAPPER     EAP("EAMAudio/EAMAudioCoreWrapper")
#define F_EAIO                 EAP("EAIO/EAIO")
#define F_EATHREAD             EAP("EAThread/EAThread")
#define F_BROWSER              EAP("InAppWebBrowser/BrowserAndroid")
#define F_VIDEO_PLAYER         EAP("VideoPlayer/PlayerAndroid")
#define F_RWFILESYSTEM         EAP("rwfilesystem/rwfilesystem")
#define F_FIFA14_ACTIVITY      EAP("game/fifa14/Fifa14Activity")
#define F_GOOGLEPLUS_HANDLER   EAP("game/fifa14/GooglePlusHandler")
#define F_YOUTUBE_HANDLER      EAP("game/fifa14/YouTubeHandler")
/* `Java_com_ea_FIFA14_FIFA14_runEntryPoint`: the class the title's own entry
 * point is declared on (its native is called by the port, fifa14_boot.c). */
#define F_FIFA14_ENGINE        EAP("FIFA14/FIFA14")
/* Android framework classes with real handlers (not auto-stubbed). */
#define F_ASSET_MANAGER        ANDROID("content/res/AssetManager")
#define F_ASSET_FD             ANDROID("content/res/AssetFileDescriptor")
#define F_AUDIO_TRACK          ANDROID("media/AudioTrack")
#define F_INPUT_STREAM         JAVA("io/InputStream")
#define F_FILE_DESCRIPTOR      JAVA("io/FileDescriptor")
/* The class of the object DisplayAndroidDelegate.GetGLView() answers: on a
 * phone it IS a view (GLSurfaceView extends SurfaceView extends View), so the
 * port's stand-in is one too -- the engine wraps it in a global ref and hands
 * it to EA::Blast::ViewAndroid (CreateView @0xe0bc70). */
#define F_GL_VIEW              ANDROID("view/SurfaceView")

/* The game's activity extends blast's MainActivity (ds_java.c's supers show
 * the same for Dead Space). */
#define F_ACTIVITY_BASE        F_MAIN_ACTIVITY

/* ------------------------------------------------------------------ licence
 * The engine's Java methods that cannot work offline (EA shut the servers
 * down: Origin, Synergy, MTX, Facebook, the identity store). They are the
 * candidates for "the class does not exist" -- FindClass then answers NULL and
 * the engine takes its "no such plugin" path. LEFT OFF BY DEFAULT: an absent
 * class whose FindClass the engine does not check is a null deref. Run once
 * with the log ([jni] unhandled ...) and turn this on to see which the engine
 * probes. 1 = report the social/network classes as absent. */
#ifndef FIFA14_SOCIAL_MISSING
#define FIFA14_SOCIAL_MISSING 0
#endif

/* ------------------------------------------------------- handlers (stubs) */
/* Device / display answers (real values; from ds_java.c's SystemAndroidDelegate
 * and DisplayAndroidDelegate tables, the same blast delegates). */
JNI_H_DECL(fifa14_h_activity_instance);
JNI_H_DECL(fifa14_h_assets);
JNI_H_DECL(fifa14_h_finish);

JNI_H_DECL(fifa14_h_dev_str_one);      /* SystemAndroidDelegate: counts -> "1"  */
JNI_H_DECL(fifa14_h_dev_str_zero);     /*                            -> "0"     */
JNI_H_DECL(fifa14_h_dev_unknown);      /*                            -> "-1"    */
/* The capability counters the engine asks as ()I, and the app's own identity.
 * The String table above was the WRONG signature for every one of them: the
 * engine's GetMethodID never matched, so they came back as the runtime's
 * unhandled zero and are in the log as "[jni] unhandled
 * com/ea/blast/SystemAndroidDelegate.<name>()I" (13 lines) plus the two
 * "unhandled static ... Fifa14Activity.GetThe*() (on java/lang/Class)". Their
 * answers are the reported phone's hardware (a Galaxy S3) and the APK's own
 * manifest values; each logs the first time the engine calls it.
 * (fifa14_blast_stubs.c has the values and the reasoning.) */
JNI_H_DECL(fifa14_h_dev_accelerometer);
JNI_H_DECL(fifa14_h_dev_touchscreen);
JNI_H_DECL(fifa14_h_dev_touchpad);
JNI_H_DECL(fifa14_h_dev_camera);
JNI_H_DECL(fifa14_h_dev_compass);
JNI_H_DECL(fifa14_h_dev_gyroscope);
JNI_H_DECL(fifa14_h_dev_microphone);
JNI_H_DECL(fifa14_h_dev_trackball);
JNI_H_DECL(fifa14_h_dev_vibrator);
JNI_H_DECL(fifa14_h_dev_phys_keyboard);
JNI_H_DECL(fifa14_h_dev_virt_keyboard);
JNI_H_DECL(fifa14_h_dev_display);
JNI_H_DECL(fifa14_h_dev_firmware);      /* GetFirmware()Ljava/lang/String;      */
JNI_H_DECL(fifa14_h_dev_hw_fp_support); /* GetHardwareFloatingPointSupport()   */
JNI_H_DECL(fifa14_h_app_version_code);  /* GetApplicationVersionCode()I        */
JNI_H_DECL(fifa14_h_app_version_name);  /* GetApplicationVersion() / GetThe*   */
JNI_H_DECL(fifa14_h_app_package);       /* GetThePackage()Ljava/lang/String;   */
JNI_H_DECL(fifa14_h_dev_model);        /* GetDeviceModel/Name        -> model   */
JNI_H_DECL(fifa14_h_dev_manufacturer); /* GetManufacturer                     */
JNI_H_DECL(fifa14_h_dev_platform);     /* GetPlatformRaw/StdName               */
JNI_H_DECL(fifa14_h_dev_version);      /* GetPlatformVersion                   */
JNI_H_DECL(fifa14_h_dev_api);          /* GetApiLevel                          */
JNI_H_DECL(fifa14_h_dev_abi);          /* GetProcessorArchitecture             */
JNI_H_DECL(fifa14_h_dev_language);     /* GetLanguage                          */
JNI_H_DECL(fifa14_h_dev_locale);       /* GetLocale                            */
JNI_H_DECL(fifa14_h_dev_false);        /* IsBatteryStateAvailable/GetLocation..*/
JNI_H_DECL(fifa14_h_app_data_dir);     /* GetAppDataDirectory   -> /data/data  */
JNI_H_DECL(fifa14_h_app_ext_dir);      /* GetExternalStorageDirectory -> /sdcard */
JNI_H_DECL(fifa14_h_display_width);
JNI_H_DECL(fifa14_h_display_height);
JNI_H_DECL(fifa14_h_display_orientation);
JNI_H_DECL(fifa14_h_display_dpi);
/* DisplayAndroidDelegate.GetGLView(): on Android the GLSurfaceView the port's
 * GL context stands in for (fifa14_gl.c). Not a getter the engine parses -- it
 * is handed to EA::Blast::ViewAndroid -- but NULL means "the view has no
 * surface", so it answers a real object. */
JNI_H_DECL(fifa14_h_gl_view);

/* AssetManager / InputStream / AssetFileDescriptor over the APK (adapted from
 * ds_assets.c). */
JNI_H_DECL(fifa14_h_asset_open);
JNI_H_DECL(fifa14_h_asset_open_fd);
JNI_H_DECL(fifa14_h_asset_list);
JNI_H_DECL(fifa14_h_asset_fd_length);
JNI_H_DECL(fifa14_h_asset_fd_offset);
JNI_H_DECL(fifa14_h_stream_read);
JNI_H_DECL(fifa14_h_stream_skip);
JNI_H_DECL(fifa14_h_stream_close);
JNI_H_DECL(fifa14_h_fd_int);

/* AudioTrack over rt_audout (adapted from ds_audio.c), and the engine's own
 * AudioTrack: AndroidEAAudioCore's Java half, which the port performs. */
JNI_H_DECL(fifa14_h_track_write);
JNI_H_DECL(fifa14_h_track_rate);
JNI_H_DECL(fifa14_h_track_minbuf);
JNI_H_DECL(fifa14_h_track_play);
JNI_H_DECL(fifa14_h_track_stop);
JNI_H_DECL(fifa14_h_track_flush);
/* AndroidEAAudioCore.Startup()V / Startup(I)V / Shutdown()V: the port builds
 * the AudioTrack the Java would have built, calls the engine's native
 * Java_com_ea_EAAudioCore_AndroidEAAudioCore_Init with it (that is what starts
 * the engine's mixer), and answers the shutdown. */
JNI_H_DECL(fifa14_h_audiocore_startup);
JNI_H_DECL(fifa14_h_audiocore_startup_ch);
JNI_H_DECL(fifa14_h_audiocore_shutdown);

/* The no-op service handlers: one shape for the whole class, so the tables
 * read the same as Dead Space's. */
JNI_H_DECL(fifa14_h_noop);             /* void: a delegate's setter / a service call */
/* The same, for the class-level WILDCARD entries ({"<cls>", NULL, NULL, h}),
 * plus one log line the first time each method of that class is called -- the
 * engine's calls the port has not named yet. The runtime's own default (a
 * method with no entry) logs "[jni] unhandled ..." and answers the zero of its
 * return type; a wildcard would hide those lines, so this one logs them and
 * answers the same zero -- EXCEPT for a method that answers a String, which is
 * answered "" and never NULL (the engine reads a String's first byte at once:
 * crash.log's last block is a data abort on exactly that). That list is the
 * next round's work. */
JNI_H_DECL(fifa14_h_noop_log);
/* The doors that lead out of the app (share to YouTube/Facebook, the social
 * login, the ad/store SDKs): the same inert answer as fifa14_h_noop_log, plus
 * one "[social] ..." line naming the door, so a next run's log says which one
 * the user touched. See fifa14_blast_stubs.c: there is no other app on this
 * console, so the answer is the app's own "nothing happened" one. */
JNI_H_DECL(fifa14_h_social);
/* java.lang.Class.getClassLoader() / ClassLoader.loadClass(name): the chain the
 * engine's Nimble layer resolves EVERY class name through. Answered NULL, it
 * made Nimble's CppBridge report "Can't find class java/util/ArrayList" for
 * classes that are in classes.txt; answered from the port's own table, loadClass
 * and FindClass agree. */
JNI_H_DECL(fifa14_h_class_getloader);
JNI_H_DECL(fifa14_h_classloader_loadclass);
JNI_H_DECL(fifa14_h_ea_startup);       /* EAIO.Startup(AssetManager): logs, -> native TODO */
JNI_H_DECL(fifa14_h_ea_shutdown);

/* -------------------------------- com/ea/EAMIO/StorageDirectory ----------
 * The Java side of EA::IO::StorageDirectory. Its Startup is NOT a no-op to
 * swallow: it is what calls the native StartupNativeImpl, the only code that
 * gives the EA::Jni::Context its JavaVM -- and EA::Jni::Context::GetEnv()
 * (libFIFA14.so+0xe3a374) is `this->m_javaVM->functions->GetEnv(&env,
 * JNI_VERSION_1_4)`, i.e. a NULL dereference while m_javaVM is still 0. The
 * engine's first storage query (EA::Blast::GetExternalStorageDirectory ->
 * StorageDirectory::GetAllStorageList -> AppendPrimaryExternalStorageList, in
 * the engine's init frame) went through it and crashed in every thread that
 * asked. fifa14_storage_start() is the way in (the boot calls it; the Java
 * Startup handler calls it too, so either order works). */
JNI_H_DECL(fifa14_h_eamio_startup);
JNI_H_DECL(fifa14_h_eamio_shutdown);
JNI_H_DECL(fifa14_h_storage_dedicated); /* "Android/data/<pkg>/files/"        */
JNI_H_DECL(fifa14_h_storage_internal);  /* the app's /data/data/<pkg>/files   */
JNI_H_DECL(fifa14_h_storage_external);  /* Environment.getExternalStorage..() */
JNI_H_DECL(fifa14_h_storage_state);     /* Environment.getExternalStorageState() */

/* ---------------- com/ea/InAppWebBrowser/BrowserAndroid + PlayerAndroid ---
 * The two classes behind the OTHER NULL-JavaVM crash (crash.log: EA::Jni::
 * Context::GetEnv, called from EA::Jni::Delegate::Delegate(char const*,
 * Context*) @0xe3a458, whose only two callers are these classes'
 * constructors). Their static EA::Jni::Context (BrowserAndroid::mJniContext,
 * PlayerAndroid::mJniContext) is filled by the class's own
 * Java_com_ea_*_StartupNativeImpl, which the APK's Java calls and the port's
 * Java never would -- so the boot calls the two natives (fifa14_boot.c step
 * 5d) and these two handlers do the same if the engine's Java side ever gets
 * there first. config.h has the evidence (the GOT slots and the Context
 * addresses, both proved from the loaded image). */
JNI_H_DECL(fifa14_h_browser_startup); /* BrowserAndroid.Startup() -> the native */
JNI_H_DECL(fifa14_h_video_startup);   /* PlayerAndroid.Startup()  -> the native */

/* ------------------------------------------------------------------ setup */
/* jni_init() + the singletons (the activity, the AssetManager, the AudioTrack)
 * + the JNIEnv MonitorEnter/MonitorExit fix the engine needs (ds_java.c). */
void fifa14_java_init(void);
/* fifa14_blast_stubs.c's once-only state (the wildcard handlers' "seen" list):
 * called from fifa14_java_init(), before the engine can call anything. */
void fifa14_stubs_init(void);
extern JObj *fifa14_activity; /* the one activity (Fifa14Activity) */
/* Give EA::Jni::Context the port's JavaVM the way the engine does it: the
 * native Java_com_ea_EAMIO_StorageDirectory_StartupNativeImpl (@0x00e39bf4),
 * bound by symbol. Returns 1 (called now), 0 (already called), -1 (the symbol
 * is missing from the .so: nothing else fills that context, so the engine's
 * own storage queries would fault -- reported, not fatal). `why` is only for
 * the log ("boot", "the engine's Java call"). Idempotent: the native is called
 * once per process (the engine's Java calls Startup once, the boot once). */
int fifa14_storage_start(const char *why);
/* The same shape for EA::IO's OTHER Java-side directory service:
 * com/ea/EAIO/EAIO.Startup(AssetManager) -> the native
 * Java_com_ea_EAIO_EAIO_StartupNativeImpl (@0x00d8575c), which forwards
 * (am, s1, s2, s3) to EA::IO::AssetManagerJNI::Startup(env, am, s1, s2, s3)
 * (@0x00d832d0). That Startup is what gives AssetManagerJNI its JavaVM
 * (`env->GetJavaVM(&sVM)`, sVM @0x12432c0), its AssetManager object
 * (`NewGlobalRef(am)`), the four Android roots and the method ids
 * (InputStream.read/skip/close, AssetManager.open/list/openFd,
 * AssetFileDescriptor.getLength). FileStream::Open reads sVM for every
 * 'appbundle:/' path -- the engine's first data file, 'appbundle:/EAMCore.ini'
 * from Blast::System::LoadProperties, crashed on it (fifa14_boot.c's
 * asset_manager_vm_seed has the whole chain). Returns 1 (called now), 0
 * (already called), -1 (the native or the AssetManager is missing: reported,
 * not fatal). Idempotent, `why` only for the log ("boot 5c", "the engine's Java
 * call"). */
int fifa14_assetmanager_start(const char *why);
/* Index the APK's assets/ once: > 0, or negative (logged). */
int fifa14_assets_init(void);
/* audout open: 0, or negative; the pause/shutdown pair beside it. */
int fifa14_audio_init(void);
void fifa14_audio_shutdown(void);
void fifa14_audio_pause(int paused);
/* One [audio] line: blocks written, samples, frames, the peak, the underruns and
 * whether the engine's mixer is playing. The boot's frame loop calls it with the
 * same window as its own [loop] line. */
void fifa14_audio_report(void);
/* The inventory of the engine's own Java classes (documentation / a future
 * classes.txt): the 26 of libFIFA14.so + the 101 of libnimble.so. External
 * linkage so an unreferenced array does not warn. */
extern const char *const fifa14_game_classes[];   /* libFIFA14.so's 26 */
extern const int fifa14_game_class_count;
extern const char *const fifa14_nimble_classes[]; /* libnimble.so's 101 */
extern const int fifa14_nimble_class_count;

#endif /* FIFA14_JNI_TABLES_H */
