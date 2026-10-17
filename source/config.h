/* config.h -- FIFA 14's own build-wide constants (the runtime's settings are in
 * port_config.h).
 *
 * FIFA 14 (EA Mobile), engine: EA NIMBLE. armeabi-v7a (ELF32 ARM, softfp),
 * version 1.3.6 / versionCode 136. MIT.
 */
#ifndef FIFA14_CONFIG_H
#define FIFA14_CONFIG_H

#include "rt_settings.h"

/* --------------------------------------------------------------- the modules */
/* The game's three libraries, in DT_NEEDED order inside libFIFA14.so:
 *   libFIFA14.so        NEEDED libgnustl_shared.so, libnimble.so
 *   libnimble.so        NEEDED libgnustl_shared.so
 * Load order: gnustl -> nimble -> FIFA14 (see source/fifa14_main.c). */
#define FIFA14_LIB_GNUSTL "libgnustl_shared.so"
#define FIFA14_LIB_NIMBLE "libnimble.so"
#define FIFA14_LIB_GAME   "libFIFA14.so"

/* The engine, EA NIMBLE (system + game framework), lives in libnimble.so.
 * libFIFA14.so imports 122 undefined EA::Nimble* symbols:
 *   77  _ZN2EA6Nimble*    (methods)
 *   44  _ZNK2EA6Nimble*   (const methods)
 *    1  _ZTVN2EA6Nimble*  (a vtable)
 * THEY CANNOT BE STUBBED: gen_imports.py leaves them out of dcr_imports[], and
 * the loader binds them at run time to the exports of the REAL libnimble.so,
 * which is therefore loaded as a module (NOT shimmed). See tools/imports.cfg. */
#define FIFA14_NIMBLE_IMPORT_COUNT 122

/* --------------------------------------------------------- engine entry points
 * The 14 lifecycle natives of libFIFA14.so, by symbol (the .so exports 118
 * Java_*; these govern the boot, the loop and the shutdown). The loader finds
 * them by NAME (so_try_find_addr_rx), which is why the names, not the RVAs, are
 * what the port uses; the RVAs are recorded for the symbol-table-free path
 * (fifa14_offsets.h, not derived yet).
 *   JNI_OnLoad                                          @0x00e005d8 (20 B)
 *   Java_com_ea_game_fifa14_Fifa14Activity_initJNI       @0x0033eb1c (188 B)
 *   Java_com_ea_game_fifa14_Fifa14Activity_shutdownJNI   @0x0033ebd8 (68 B)
 *   Java_com_ea_FIFA14_FIFA14_runEntryPoint              @0x00338fd8 (48 B)
 *   Java_com_ea_blast_MainActivity_NativeOnCreate        @0x00dffc8c (168 B)
 *   Java_com_ea_blast_MainActivity_NativeOnPause         @0x00dffeac (236 B)
 *   Java_com_ea_blast_MainActivity_NativeOnResume        @0x00dfff98 (236 B)
 *   Java_com_ea_blast_MainActivity_NativeOnBackPressed   @0x00e00378 (116 B)
 *   Java_com_ea_blast_AndroidRenderer_NativeOnDrawFrame  @0x00e003ec (124 B)
 *   Java_com_ea_blast_AndroidRenderer_NativeOnSurfaceCreated  @0x00e00548 (72 B)
 *   Java_com_ea_blast_AndroidRenderer_NativeOnSurfaceChanged  @0x00e00468 (224 B)
 *   Java_com_ea_blast_AndroidRenderer_NativeOnSurfaceDestroyed @0x00e00590 (72 B)
 *   Java_com_ea_EAIO_EAIO_StartupNativeImpl              @0x00d8575c (44 B)
 *   Java_com_ea_EAIO_EAIO_Shutdown                       @0x00d85788 (88 B)
 *   Java_com_ea_EAAudioCore_AndroidEAAudioCore_Init      @0x00ccef5c (440 B)
 *   Java_com_ea_EAAudioCore_AndroidEAAudioCore_Release   @0x00ccf114 (136 B)
 * The order the boot calls them in, with the reason for each, is the header of
 * source/fifa14_boot.c (read out of the disassembly).
 */
#define FIFA14_ENTRY_JNI_ONLOAD        "JNI_OnLoad"
#define FIFA14_ENTRY_INIT_JNI          "Java_com_ea_game_fifa14_Fifa14Activity_initJNI"
#define FIFA14_ENTRY_SHUTDOWN_JNI      "Java_com_ea_game_fifa14_Fifa14Activity_shutdownJNI"
#define FIFA14_ENTRY_RUN_ENTRY_POINT   "Java_com_ea_FIFA14_FIFA14_runEntryPoint"
#define FIFA14_ENTRY_NATIVE_ONCREATE   "Java_com_ea_blast_MainActivity_NativeOnCreate"
#define FIFA14_ENTRY_NATIVE_ONPAUSE    "Java_com_ea_blast_MainActivity_NativeOnPause"
#define FIFA14_ENTRY_NATIVE_ONRESUME   "Java_com_ea_blast_MainActivity_NativeOnResume"
#define FIFA14_ENTRY_NATIVE_ONBACK     "Java_com_ea_blast_MainActivity_NativeOnBackPressed"
#define FIFA14_ENTRY_NATIVE_ONDRAWFRAME \
  "Java_com_ea_blast_AndroidRenderer_NativeOnDrawFrame"
#define FIFA14_ENTRY_NATIVE_SURFACE_CREATED \
  "Java_com_ea_blast_AndroidRenderer_NativeOnSurfaceCreated"
#define FIFA14_ENTRY_NATIVE_SURFACE_CHANGED \
  "Java_com_ea_blast_AndroidRenderer_NativeOnSurfaceChanged"
#define FIFA14_ENTRY_NATIVE_SURFACE_DESTROYED \
  "Java_com_ea_blast_AndroidRenderer_NativeOnSurfaceDestroyed"
#define FIFA14_ENTRY_EAIO_STARTUP      "Java_com_ea_EAIO_EAIO_StartupNativeImpl"
#define FIFA14_ENTRY_EAIO_SHUTDOWN     "Java_com_ea_EAIO_EAIO_Shutdown"
#define FIFA14_ENTRY_EAAUDIOCORE_INIT  "Java_com_ea_EAAudioCore_AndroidEAAudioCore_Init"
#define FIFA14_ENTRY_EAAUDIOCORE_RELEASE "Java_com_ea_EAAudioCore_AndroidEAAudioCore_Release"

/* ------------------------------------------------ the storage directory ---
 * com/ea/EAMIO/StorageDirectory is EA::IO::StorageDirectory's Java side. In
 * the APK it is its own class (classes.dex: StorageDirectory.java):
 *
 *   static void Startup(Activity a) { sActivity = a; StartupNativeImpl(); }
 *   static void Shutdown()          { ShutdownNativeImpl(); }
 *   static String GetInternalStorageDirectory()  { sActivity.getFilesDir().getAbsolutePath(); }
 *   static String GetPrimaryExternalStorageDirectory() {
 *         Environment.getExternalStorageDirectory().getAbsolutePath(); }
 *   static String GetPrimaryExternalStorageState() { Environment.getExternalStorageState(); }
 *   static String GetDedicatedDirectory() {
 *         "Android/data/" + getClass().getPackage().getName() + "/files/"; }
 *
 * StartupNativeImpl is NOT optional: it is the only code that gives the
 * EA::Jni::Context its JavaVM (`env->GetJavaVM()` -> the context's +0 field),
 * and EA::Jni::Context::GetEnv() @0x00e3a374 is
 *
 *     ldr r0, [r0]      ; this->m_javaVM      <- 0 = the crash (pc +0xe3a390)
 *     ldr r1, [r0]      ; vm->functions
 *     ldr r3, [r1, #24] ; JNIInvokeInterface[6] = GetEnv
 *     blx r3            ; GetEnv(vm, &env, JNI_VERSION_1_4)
 *
 * The engine's first storage enumeration (EA::Blast::GetExternalStorageDirectory
 * -> StorageDirectory::GetAllStorageList -> AppendPrimaryExternalStorageList)
 * is what walked into it, on whatever thread asked. The port therefore calls
 * the native itself at boot (fifa14_boot.c, step 5b) and routes the class's
 * Java Startup/Shutdown to it (fifa14_jni_tables.c). */
#define FIFA14_ENTRY_EAMIO_STARTUP \
  "Java_com_ea_EAMIO_StorageDirectory_StartupNativeImpl"
#define FIFA14_ENTRY_EAMIO_SHUTDOWN \
  "Java_com_ea_EAMIO_StorageDirectory_ShutdownNativeImpl"

/* ------------------------------------ the LAST TWO EA::Jni::Contexts ------
 * The crash that brought these here (crash.log, two threads):
 *
 *   [crash] pc  1a8bb390  libFIFA14.so+0xe3a390 (_ZN2EA3Jni7Context6GetEnvEv+0x1c)
 *   [crash] lr  1a8bb490  libFIFA14.so+0xe3a490 (_ZN2EA3Jni8DelegateC2EPKcPNS0_7ContextE+0x38)
 *   [crash] far 00000000  esr 92000006  insn e5901000   <- ldr r1,[r0] with r0 = 0
 *
 * EA::Jni::Delegate::Delegate(char const*, Context*) (@0xe3a458) stores the
 * Context it is handed into Delegate+4 and then calls
 * EA::Jni::Context::GetEnv() on it -- `ldr r0,[r2]` = that Context's m_javaVM
 * (Context+0). The Context was still 0, so GetEnv faulted on its second
 * instruction (+0xe3a390), exactly as it did twice before with other Contexts.
 *
 * WHICH Context, and how it is proved. The ctor has exactly TWO callers in the
 * whole image (a BL scan of the executable segment; both are in the .so):
 *
 *   0xe357ec  inside EA::InAppWebBrowser::BrowserAndroid::BrowserAndroid(ICoreAllocator*)
 *             (@0xe357a4): the Context is `ldr r2,[r0,r7]` off the GOT slot
 *             0x010e6e40, whose (relocated) content is 0x0138e134 =
 *             _ZN2EA15InAppWebBrowser14BrowserAndroid11mJniContextE.
 *   0xe3bdac  inside EA::VideoPlayer::PlayerAndroid::PlayerAndroid(...)
 *             (@0xe3bd50): GOT slot 0x010e6e9c -> 0x0138ea64 =
 *             _ZN2EA11VideoPlayer13PlayerAndroid11mJniContextE.
 *
 * and each class's OWN native fills exactly those two objects -- the slot is
 * the same one for both the ctor and the native (0x010e6e40 / 0x010e6e9c):
 *
 *   Java_com_ea_InAppWebBrowser_BrowserAndroid_StartupNativeImpl @0xe356e8 (72 B)
 *   Java_com_ea_VideoPlayer_PlayerAndroid_StartupNativeImpl      @0xe3bc5c (72 B)
 *       env->GetJavaVM(&vm)                       (r1=[r0]; r2=[r1,#876]; slot 219)
 *       ldr r0,[pc,#24] / ldr r1,[pc,#24]         the two pc-relative literals
 *       ldr r2,[sp,#4]                            vm
 *       add r0, pc, r0 / ldr r0,[r0,r1]           the field's address (the GOT slot above)
 *       str r2,[r0]                               *field = vm      <- m_javaVM
 *
 * In the APK the Java side calls those two natives from
 * com/ea/InAppWebBrowser/BrowserAndroid.java and com/ea/VideoPlayer/
 * PlayerAndroid.java (Startup); the port's Java never would, so -- exactly like
 * EAIO.Startup and StorageDirectory.Startup -- the port calls them itself at
 * boot (fifa14_boot.c step 5d, fifa14_blast_stubs.c fifa14_jni_class_start())
 * and a seed beside that (fifa14_boot.c jni_class_context_seed()) fills the
 * same field if the native is unbound. GetEnv() then goes through the runtime's
 * fake JavaVM (jni_core.c's slot 6) for these Contexts too.
 *
 * The remaining GetEnv users are covered by the fixes already in place:
 * StorageDirectory (step 5b), AssetManagerJNI (step 5c) and EA::Audio's
 * JavaHelperInit, which is handed the JavaVM as its argument by
 * AndroidEAAudioCore's own Init and is not reached yet (the port does not call
 * EAAudioCore.Init -- see fifa14_boot.c step 5c/next round). */
#define FIFA14_ENTRY_BROWSER_JNI_STARTUP \
  "Java_com_ea_InAppWebBrowser_BrowserAndroid_StartupNativeImpl"
#define FIFA14_ENTRY_VIDEO_JNI_STARTUP \
  "Java_com_ea_VideoPlayer_PlayerAndroid_StartupNativeImpl"
/* The two Context objects themselves: the seeds write their m_javaVM (the
 * object's first word) and the boot reads them back, both by NAME -- no offset
 * of this build is hard-coded in the port. */
#define FIFA14_SYM_BROWSER_JNI_CONTEXT "_ZN2EA15InAppWebBrowser14BrowserAndroid11mJniContextE"
#define FIFA14_SYM_VIDEO_JNI_CONTEXT   "_ZN2EA11VideoPlayer13PlayerAndroid11mJniContextE"

/* The two symbols the NULL-env safety net needs (fifa14_boot.c's
 * fifa14_jni_context_seed): the function whose first instructions name the
 * global object that holds that EA::Jni::Context (its `this`), and GetEnv
 * itself, which the branch inside it must land on. Both are exported by
 * libFIFA14.so, so they are found by NAME -- no offset of this build is
 * hard-coded in the seed. */
#define FIFA14_SYM_STORAGE_APPEND_EXTERNAL \
  "_ZN2EA2IO16StorageDirectory32AppendPrimaryExternalStorageListERN5eastl6vectorINS0_12StorageInfosENS2_9allocatorEEE"
#define FIFA14_SYM_JNI_GETENV "_ZN2EA3Jni7Context6GetEnvEv"

/* The two symbols the AssetManagerJNI safety net needs (fifa14_boot.c's
 * asset_manager_vm_seed): the JavaVM field the engine's FileStream::Open reads
 * for every 'appbundle:/' path -- EA::IO::AssetManagerJNI::sVM -- and that
 * function itself, whose loaded instructions name WHERE the field is. Both are
 * exported by libFIFA14.so, so both are found by NAME: no offset of this build
 * is hard-coded in the seed.
 *
 * THE EVIDENCE (the crash report, then the .so read back):
 *   crash.log: data abort, PC = libFIFA14.so+0xd84d28, LR = ...+0xe37b84
 *   (AssetStream::CreateMemoryStreamFromFile+0x9c), r0 = 0, insn e5901000 =
 *   `ldr r1,[r0]`.
 *   `arm-none-eabi-nm -D libFIFA14.so` answers
 *     `012432c0 B _ZN2EA2IO15AssetManagerJNI3sVME`   <- sVM, 4 bytes of .bss,
 *   with gpCoreAllocator @0x12432b8, sOriginalEnv @0x12432bc, sAndroidDataRoot
 *   @0x12432c4 (and sAndroidFileRoot/TempRoot/ExternalRoot @0x124434c4 /
 *   0x12436c4 / 0x12438c4, sMethodId_AssetManager_List @0x1243adc) around it.
 *   FileStream::Open's 'appbundle:/' branch (@0xd84cfc) resolves the field
 *   through the GOT entry @0x10e67c4 -- .rel.dyn R_ARM_RELATIVE, addend
 *   0x012432c0 (read out of .got) -- and then runs
 *     ldr r0,[r0]         ; r0 = sVM
 *     ldr r1,[r0]         ; r1 = (*sVM)->functions   <- the fault when sVM == 0
 *     ldr r3,[r1,#24]     ; slot 6 = JavaVM->GetEnv
 *     blx r3              ; GetEnv(vm, &env, 0x10002)
 *   which is the port's own 8-entry JavaVM table's GetEnv (runtime/source/
 *   jni_core.c: g_vm_fn[6]) -- the same field, the same call, the EA::Jni
 *   context's m_javaVM gets. sVM is filled by
 *   EA::IO::AssetManagerJNI::Startup(env, am, s1, s2, s3) @0xd832d0 with
 *   `env->GetJavaVM(&sVM)` (JNIEnv slot 219), and that Startup's only caller is
 *   the Java_com_ea_EAIO_EAIO_StartupNativeImpl @0xd8575c the APK's Java calls.
 *   The port never called it -> sVM stayed 0 -> the first data file the engine
 *   opened ('appbundle:/EAMCore.ini', Blast::System::LoadProperties) crashed. */
#define FIFA14_SYM_FILE_STREAM_OPEN "_ZN2EA2IO10FileStream4OpenEiiii"
#define FIFA14_SYM_ASSET_MANAGER_SVM "_ZN2EA2IO15AssetManagerJNI3sVME"

/* The two symbols libnimble.so's JavaVM safety net needs (fifa14_boot.c's
 * nimble_vm_seed + fifa14_nimble_onload): the module's OWN entry point --
 * JNI_OnLoad, and the ONLY writer of the JavaVM the module caches -- and the
 * getEnv() that reads it. Both are exported by libnimble.so, so both are found
 * by NAME: no offset of this build is hard-coded in the seed.
 *
 * THE EVIDENCE (the crash report, then the .so read back with
 * arm-none-eabi-objdump -d -M force-thumb; addresses are module-relative):
 *
 *   crash.log, the LAST block: data abort, PC = libnimble.so+0x34bbc
 *   (EA::Nimble::getEnv()+0xc), LR = +0x39b59
 *   (EA::Nimble::Base::NotificationListener::NotificationListener(
 *    fastdelegate::FastDelegate3<...> const&)+0x41), r0 = 0, far = 0,
 *   esr = 0x92000006, r2 = 0x00010006.
 *
 *   EA::Nimble::getEnv() @+0x34bb0 (Thumb):
 *     34bb2  ldr  r4,[pc,#44]   ; [0x34be0] = 0x000221c4   (the anchor delta)
 *     34bb4  add  r1,sp,#4      ; &env
 *     34bb6  ldr  r2,[pc,#44]   ; [0x34be4] = 0x00010006 = JNI_VERSION_1_6
 *     34bb8  add  r4,pc         ; r4 = +0x34bb8 + 4 + 0x221c4 = +0x56d80
 *     34bba  ldr  r0,[r4,#0]    ; r0 = the cached JavaVM    <- 0 in the port
 *     34bbc  ldr  r3,[r0,#0]    ; r3 = vm->functions         <-- THE FAULT (far 0)
 *     34bbe  ldr  r3,[r3,#24]   ; functions[6] = JavaVM->GetEnv
 *     34bc0  blx  r3            ; GetEnv(vm, &env, 0x10006)
 *     34bc2  adds r0,#2 / bne   ; JNI_OK -> done; else AttachCurrentThread +
 *                               ; pthread_setspecific(key) below
 *     34bd2  ldr  r0,[r4,#4]    ; the pthread key, the word right after the vm
 *   so the static is the 4-byte .bss word at libnimble.so+0x56d80. `nm -D` has
 *   the block around it (Json::Value::null @0x56d70, JavaClassManager::
 *   s_instance @0x56dac); the field itself has NO exported symbol -- it is a
 *   file-static, which is why the seed decodes it out of the code instead of
 *   looking it up.
 *
 *   JNI_OnLoad @+0x34c60 -- the module's own JNI entry point, and what Android's
 *   System.loadLibrary("nimble") runs -- is its only writer:
 *     34c62  ldr  r4,[pc,#232]  ; [0x34d4c] = 0x00022114
 *     34c64  ldr  r3,[r0,#0]    ; r3 = vm->functions  (r0 = the JavaVM argument)
 *     34c68  add  r4,pc         ; r4 = +0x34c68 + 4 + 0x22114 = +0x56d80
 *     34c6a  str  r0,[r4,#0]    ; *+0x56d80 = vm        <-- THE ONLY WRITE
 *     34c6c  ldr  r3,[r3,#24]   ; functions[6] = GetEnv
 *     ...    blx  r3            ; GetEnv(vm, &env, 0x10006)
 *     34c78  blx  pthread_key_create ; &sKey = the same block +4
 *     34cbe.. FindClass("com/ea/nimble/bridge/BaseNativeCallback"),
 *            GetMethodID(java/lang/Class, "getClassLoader",
 *            "()Ljava/lang/ClassLoader;"), the class ref (the block +8) and the
 *            loader's loadClass (the block +12)
 *     34d2e  ldr  r0,[pc,#32]   ; [0x34d50] = 0x00010006 -> returns JNI_VERSION_1_6
 *   The block is therefore {JavaVM *sJavaVM; pthread_key_t sKey; jobject
 *   sBaseNativeCallback; jmethodID sClassLoader_loadClass;}.
 *
 *   Every `add Rd, pc` in libnimble.so's code decodes to +0x56d80 from exactly
 *   four instructions: +0x34b84 (the AttachCurrentThread helper at +0x34b80),
 *   +0x34bb8 (getEnv's read), +0x34c00 (findClass's class-cache read) and
 *   +0x34c68 (JNI_OnLoad's write). The module is Thumb-1 throughout, which is
 *   why the seed's pc-relative rule is `(insn + 4) + literal` -- NOT the ARM
 *   +8 the other seeds in fifa14_boot.c use. */
#define FIFA14_SYM_NIMBLE_JNI_ONLOAD "JNI_OnLoad"
#define FIFA14_SYM_NIMBLE_GETENV    "_ZN2EA6Nimble6getEnvEv"

/* What libnimble.so's JNI_OnLoad answers (its return: ldr r0,[pc] = 0x10006).
 * A different value means the log is looking at another build of the module. */
#define FIFA14_NIMBLE_JNI_VERSION 0x00010006

#define FIFA14_ADDR_JNI_ONLOAD         0x00e005d8u
#define FIFA14_ADDR_NATIVE_ONCREATE    0x00dffc8cu
#define FIFA14_ADDR_NATIVE_ONDRAWFRAME 0x00e003ecu

/* What JNI_OnLoad answers (its last two instructions: mov r0, #4; orr r0, r0,
 * #0x10000). A different value here means the log is looking at another build. */
#define FIFA14_JNI_VERSION 0x00010004

/* 1: log every constructor of libFIFA14.so before it runs (its .init_array has
 * 197 entries: the engine's static initializers). The bring-up value: a stop
 * or a crash inside one of them then names itself in the log. 0 to quieten it
 * once the boot is stable. */
#define FIFA14_TRACE_CTORS 1

/* 1: fifa14_blast_stubs.c's EAIO.Startup(AssetManager) hands the engine's own
 * native StartupNativeImpl(AssetManager, dir, dir, dir) the game's folders,
 * the way the APK's Java EAIO.Startup does -- it is EA::IO's file system, the
 * layer the engine reads its data (and the APK's assets) through. 0: log and
 * do nothing. */
#define FIFA14_EAIO_STARTUP 1

/* The Java classes the native side is handed. The entry-point symbol names give
 * the package (com.ea.blast); TODO: confirm both names against the APK's
 * classes.dex and resolve the class refs through the runtime's JNI layer. */
#define FIFA14_CLASS_MAIN_ACTIVITY    "com/ea/blast/MainActivity"
#define FIFA14_CLASS_ANDROID_RENDERER "com/ea/blast/AndroidRenderer"

/* --------------------------------------------------------------- the OBB */
/* One expansion file, no patch. EA names it by versionCode (136):
 *   main.136.com.ea.game.fifa14_row.obb   1,386,236,976 B
 * The game reads it from Android/obb/<pkg>/ (the runtime's path map can send
 * that to the game folder). It is NOT a zip: it is EA's "EB" container (magic
 * 45 42 00 03), 1217 entries, the data uncompressed -- the file table is read
 * at boot and the game's own data is served from it (source/fifa14_external.c,
 * which has the layout and the evidence). */
#define FIFA14_OBB_NAME  "main.136.com.ea.game.fifa14_row.obb"
#define FIFA14_OBB_BYTES 1386236976ull

/* ---------------------------------------------------------- feature switches
 * Wired against fifa14_offsets.h once the offsets are derived (TODO). 0 now:
 * nothing of the engine's code is patched yet. */
#define FIFA14_PATCH_GL_SWAP    0
#define FIFA14_TIME_ICALL_HOOKS 0

#endif /* FIFA14_CONFIG_H */
