/* fifa14_boot.c -- FIFA 14's boot: the JNI environment, the game's natives
 * bound by symbol, the order the engine's own state machine demands, and the
 * frame loop that drives it.
 *
 * THE ORDER, AND WHY IT IS THIS ONE. It is read out of libFIFA14.so (the
 * disassembly, not a guess; the RVAs are in config.h and NOTAS-ENTRADA-Y-JNI.md):
 *
 *   1. fifa14_java_init() -- the JNIEnv and the four tables of
 *      fifa14_jni_tables.c, the MonitorEnter/Exit fix, the activity and
 *      AssetManager singletons. Everything below goes through it.
 *   2. the natives, by symbol: so_try_find_addr_rx over libFIFA14.so's exports.
 *   2b. libnimble.so's OWN JavaVM. The port loads that module itself
 *      (fifa14_main.c, so_load + so_execute_init_array) and had only ever
 *      called libFIFA14.so's JNI_OnLoad (step 4), so EA::Nimble::getEnv() read
 *      a NULL static (libnimble.so+0x56d80) and every thread that built a
 *      Nimble object died in `ldr r3,[r0]` with r0 = 0 -- crash.log's LAST
 *      block, thread 694, inside Base::NotificationListener's constructor.
 *      Android's System.loadLibrary("nimble") runs that module's own
 *      JNI_OnLoad(vm, NULL), which is the only writer of that word; the port
 *      does the same at this step, and nimble_vm_seed() before it is the belt
 *      to those braces, as everywhere else here (config.h has the whole
 *      disassembly). It must come BEFORE step 3: libFIFA14.so's own
 *      constructors are the first engine code that can build a Nimble object.
 *   3. libFIFA14.so's constructors (so_execute_init_array): Android runs a
 *      library's .init_array at System.loadLibrary / dlopen, before anything
 *      calls into it. 197 entries; traced when FIFA14_TRACE_CTORS is 1.
 *   4. JNI_OnLoad(vm, NULL) @0x00e005d8. Twenty bytes: it stores `vm` into the
 *      global EA::Blast::JniContext::GetEnv() @0x00dff764 reads --
 *      vm->functions[6] == GetEnv(vm, &env, 0x10004), i.e. JNIEnv slot 6 of our
 *      JavaVM table, which answers g_jni_env -- and returns 0x00010004
 *      (JNI_VERSION_1_4). NOTHING may run before it that reaches GetEnv.
 *   5. initJNI @0x0033eb1c (Java_com_ea_game_fifa14_Fifa14Activity_initJNI):
 *      JNIEnv slot 219 == GetJavaVM(env, &vm) into the same kind of global, and
 *      it logs (android_log_print) when that fails. This is the Java side's
 *      first call into the game; the port makes it.
 *   5b/5c/5d. the four EA::Jni::Contexts whose JavaVM only a Java-side Startup
 *      fills, and which Context::GetEnv() (@0xe3a374) dereferences as
 *      `ldr r1,[r0]` with r0 = m_javaVM -- a data abort at +0xe3a390 while it
 *      is still 0 (three separate crashes brought these here):
 *        5b StorageDirectory (EAMIO)   -> fifa14_storage_start
 *        5c EA::IO::AssetManagerJNI    -> fifa14_assetmanager_start (FileStream
 *                                        ::Open's 'appbundle:/' branch reads sVM)
 *        5d InAppWebBrowser::BrowserAndroid and VideoPlayer::PlayerAndroid ->
 *           fifa14_jni_class_start (the two classes building an
 *           EA::Jni::Delegate in their constructors: 0xe357ec, 0xe3bdac)
 *      Each calls the engine's OWN native (the one the APK's Java calls) and,
 *      first, a seed that decodes the native out of the loaded image and fills
 *      the same field if the native is not bound. config.h has the evidence.
 *   6. NativeOnCreate @0x00dffc8c: EA::Blast::InitTraceSystem(), then the
 *      EA::Blast::SystemAndroid singleton (SystemAndroid::SystemAndroid
 *      @0x00e00604), its Init, two more vtable calls whose results it keeps,
 *      and LAST: the engine's state word = 1. SystemAndroid's constructor is
 *      what asks Java for the activity:
 *      EA::Blast::SystemAndroid::GetMainActivityInstance @0x00e00710 does
 *      FindClass("com/ea/blast/MainActivity") then
 *      GetStaticMethodID(cls, "GetInstance", "()Lcom/ea/blast/MainActivity;")
 *      then CallStaticObjectMethod -- which is exactly the table entry the port
 *      already has (F_MAIN_ACTIVITY / GetInstance / fifa14_h_activity_instance).
 *   7. runEntryPoint @0x00338fd8 (Java_com_ea_FIFA14_FIFA14_runEntryPoint):
 *      it calls main(0, NULL) @0x00dff844, and in this build main is
 *      `mov r0, #0; bx lr` -- a stub. So the call is harmless and its 0 is
 *      logged: the title's real entry in this build is the render loop below.
 *   7b. the GL CONTEXT (fifa14_gl_boot, fifa14_gl.c): the EGL
 *      display/config/GLES2 context/window surface, made current, which on
 *      Android is the Java GLSurfaceView's job and which the game cannot do
 *      itself (it imports only eglGetCurrentDisplay/Surface/SwapBuffers). It
 *      must come before step 8: the engine's Init asks the GL for
 *      glGetString(GL_RENDERER) for its GPU table, and Mesa's glGetString
 *      returns NULL with no current context -- the Strcmp(NULL, "Mali-T604")
 *      data abort at libFIFA14.so+0xe5c8b8 that stopped the module manager.
 *   8. the ENGINE'S INIT FRAME: NativeOnDrawFrame @0x00e003ec. It locks the
 *      engine's mutex and, the FIRST time, calls EA::Blast::LoopLocked()
 *      @0x00dffa98, which with the state word at 1 calls the SystemAndroid
 *      vtable slot 0x6c (the engine's Init) and sets the state word to 2.
 *      That is the ONLY way the state reaches 2.
 *   9. NativeOnSurfaceCreated @0x00e00548 is 72 bytes and starts
 *      `if (state != 2) return;`, so it does NOTHING before step 8 -- this is
 *      why the first NativeOnDrawFrame is a step of its own. Once the state is
 *      2 it latches a byte that LoopLocked() turns into the engine's
 *      "surface created" message on the next frame.
 *  10. NativeOnSurfaceChanged @0x00e00468 (env, renderer, width, height): it
 *      builds a 28-byte EA::Blast::MessageCoord2i (width at +20, height at +24)
 *      and posts it through the SystemAndroid singleton, so it too must come
 *      after NativeOnCreate. The size is the runtime's window (dcr_window_size).
 *  11. the loop: NativeOnDrawFrame once per frame. After the first call each
 *      one walks LoopLocked()'s state == 2 path: the surface flags, the
 *      SystemAndroid vtable slot 0x20 with message 16, slot 0x38,
 *      SystemAndroid::EndMainLoopSync() and slot 0x8c -- one iteration of the
 *      engine's loop, on THIS thread. The port's thread is therefore the game's
 *      thread; the runtime makes it a guest thread (dcr_sched_init, main.c).
 *  12. out: NativeOnSurfaceDestroyed, NativeOnPause and shutdownJNI --
 *      Android's onPause/onDestroy order. (EAIO's Shutdown *native* is not
 *      called directly: it dereferences globals only EAIO.Startup sets, and
 *      fifa14_blast_stubs.c already routes the engine's own EAIO.Shutdown to
 *      it.)
 *
 * WHAT THIS DOES NOT DO YET: the engine's own Java surface beyond the tables
 * (the log lists every call nothing answers -- "[jni] unhandled ..."), the
 * ModuleCatalog natives (ds_loader.c's pattern: the port tells the engine which
 * input modules exist), the engine offsets/patches (fifa14_offsets.h), GL
 * hooks, and input. See NOTES.md and the [jni] lines of the first run's log.
 *
 * MIT.
 */
#include <stdio.h>
#include <string.h>
#include <switch.h>

#include "config.h"
#include "dcr_boost.h"
#include "fifa14_assets.h"
#include "fifa14_boot.h"
#include "fifa14_ctlcfg.h"
#include "fifa14_gl.h"
#include "fifa14_input.h"
#include "fifa14_jni_tables.h"
#include "fifa14_spch.h"
#include "gl_layer.h"
#include "jni.h"
#include "rt_applet.h"
#include "rt_window.h"
#include "so_util.h"
#include "util.h"
#include "watchdog.h"

/* The game's module (fifa14_main.c). */
extern so_module game_mod;
/* libnimble.so's, for the JavaVM seed below (fifa14_main.c loads it). */
extern so_module nimble_mod;

/* ------------------------------------------------------------- the natives
 * One row per name in config.h's list; the address is filled in by
 * fifa14_boot_bind(). `used` is only for the log (what the boot drives, what
 * is bound for the stubs or the next round). */
typedef struct {
  const char *sym;
  const char *used;
  uintptr_t addr;
} Fifa14Entry;

static Fifa14Entry g_entries[] = {
    {FIFA14_ENTRY_JNI_ONLOAD, "boot: the JavaVM cache (GetEnv needs it)"},
    {FIFA14_ENTRY_INIT_JNI, "boot: the Java side's first call"},
    {FIFA14_ENTRY_SHUTDOWN_JNI, "shutdown"},
    {FIFA14_ENTRY_RUN_ENTRY_POINT, "boot: calls main() (a stub in this build)"},
    {FIFA14_ENTRY_NATIVE_ONCREATE, "boot: the engine's system + state = 1"},
    {FIFA14_ENTRY_NATIVE_ONPAUSE, "focus lost"},
    {FIFA14_ENTRY_NATIVE_ONRESUME, "focus back"},
    {FIFA14_ENTRY_NATIVE_ONBACK, "the system's back button"},
    {FIFA14_ENTRY_NATIVE_ONDRAWFRAME, "the frame tick (the engine's loop)"},
    {FIFA14_ENTRY_NATIVE_SURFACE_CREATED, "boot: the surface exists"},
    {FIFA14_ENTRY_NATIVE_SURFACE_CHANGED, "boot: the surface's size"},
    {FIFA14_ENTRY_NATIVE_SURFACE_DESTROYED, "shutdown"},
    {FIFA14_ENTRY_EAIO_STARTUP, "fifa14_blast_stubs.c: EAIO.Startup -> the native"},
    {FIFA14_ENTRY_EAIO_SHUTDOWN, "fifa14_blast_stubs.c: EAIO.Shutdown -> the native"},
    {FIFA14_ENTRY_EAAUDIOCORE_INIT, "next round: the audio core"},
    {FIFA14_ENTRY_EAAUDIOCORE_RELEASE, "next round: the audio core"},
    {FIFA14_ENTRY_EAMIO_STARTUP, "boot 5b: the EA::Jni context's JavaVM (the storage list needs it)"},
    {FIFA14_ENTRY_EAMIO_SHUTDOWN, "shutdown: EAMIO/StorageDirectory.ShutdownNativeImpl"},
    {FIFA14_ENTRY_BROWSER_JNI_STARTUP,
     "boot 5d: BrowserAndroid's EA::Jni context (its Delegate ctor's GetEnv)"},
    {FIFA14_ENTRY_VIDEO_JNI_STARTUP,
     "boot 5d: PlayerAndroid's EA::Jni context (its Delegate ctor's GetEnv)"},
};
static const int g_nentries = (int)(sizeof g_entries / sizeof g_entries[0]);

static uintptr_t entry(const char *sym) {
  for (int i = 0; i < g_nentries; i++)
    if (!strcmp(g_entries[i].sym, sym))
      return g_entries[i].addr;
  return 0;
}

void *fifa14_native_addr(const char *symbol) { return (void *)entry(symbol); }

void fifa14_boot_bind(void) {
  int found = 0;
  debugPrintf("[boot] ==== the game's natives, by symbol ====\n");
  for (int i = 0; i < g_nentries; i++) {
    Fifa14Entry *e = &g_entries[i];
    e->addr = so_try_find_addr_rx(&game_mod, e->sym);
    found += e->addr != 0;
    if (e->addr)
      debugPrintf("[bind] %-58s @ 0x%08lx  (%s)\n", e->sym, (unsigned long)e->addr, e->used);
    else
      debugPrintf("[bind] %-58s MISSING from %s  (%s)\n", e->sym, FIFA14_LIB_GAME, e->used);
  }
  debugPrintf("[boot] %d of %d entry points bound\n", found, g_nentries);
}

/* ---------------------------------------------------------------- the loop
 * The watchdog's frame counter (watchdog.h: dcr_boot_frames is required from
 * the port; read on the watchdog's thread, so it takes no lock). */
static volatile uint64_t g_frames;

uint64_t dcr_boot_frames(void) { return g_frames; }

static uint32_t frames_u32(void) { return (uint32_t)g_frames; }

static int g_booted;      /* the natives may be called by rt_applet_poll's callbacks */
static int g_paused;

static uint64_t now_ns(void) { return armTicksToNs(armGetSystemTick()); }

/* The frame the engine is asked to draw. Its first call is the engine's Init
 * (see step 8 above), so the loop must not start before the surface steps. */
#define FIFA14_FPS_LOG_FRAMES 300u

static void draw_one_frame(uintptr_t draw) {
  ((void (*)(void *, void *))draw)(g_jni_env, jni_singleton(F_ANDROID_RENDERER));
}

static void frame_loop(uintptr_t draw) {
  /* The engine's init frame (step 8) was frame 1: the count carries on, so
   * the watchdog's counter and the [loop] lines are one monotonic series. */
  uint64_t frames = 1, last = now_ns(), counted = 1;
  int first = 1;
  debugPrintf("[loop] the engine's frame loop starts (NativeOnDrawFrame on this thread)\n");
  while (!rt_exit_requested() && appletMainLoop()) {
    rt_applet_poll();
    if (!rt_focused()) {
      svcSleepThread(50000000ll);
      continue;
    }
    dcr_boost_frame_begin();
    /* THE INPUT, before the frame: the engine's pointer native only POSTS a
     * message (fifa14_input.c has the chain), and the engine's loop -- this
     * same thread, inside NativeOnDrawFrame below -- consumes it, so a touch
     * read now is serviced by the frame it belongs to. Two service reads and,
     * on a real event, one call: nothing added to the render path. */
    fifa14_input_poll();
    /* THE GAME'S CONTROL SCHEME, from time to time (fifa14_ctlcfg.c): the
     * engine's own registry values for the CONTROL_CONFIG keys, and a re-report
     * of the two ini files whenever the game rewrites one of them. Nothing is
     * written here: the patch ran at boot. */
    fifa14_ctlcfg_frame(frames);
    /* THE COMMENTATORS (fifa14_spch.c): with [audio] force_speech = 1, the
     * engine's own speech start is called by the port -- once, words verified,
     * logged. With 0 (the default) this is one function call and a return. */
    fifa14_spch_frame(frames);
    draw_one_frame(draw);
    /* THE PRESENT -- Android's Java, done here. On the phone eglSwapBuffers is
     * NOT the engine's call: the activity's GLSurfaceView (its GLThread's
     * EGLHelper.swap()) presents after the Renderer's onDrawFrame, and the
     * native engine only paints the back buffer. The port makes the context
     * (fifa14_gl_boot) but nothing was ever presenting: the engine ran ~4500
     * frames at 30 fps with "engine swaps: 0" and the screen stayed black.
     * fifa14_gl_swap() is that Java step, once per engine frame, exactly as the
     * GLSurfaceView's render thread does after every onDrawFrame; it reaches
     * the real eglSwapBuffers and counts the frame (dcr_gl_frames, the "engine
     * swaps" below and the watchdog's). The engine's own pacing is untouched:
     * RT_GL_SWAP_ENDS_FRAME is 0, so the swap does not re-bracket the boost. */
    fifa14_gl_swap();
    frames++;
    g_frames = frames;
    dcr_boost_frame_end(frames);
    dcr_boost_poll(); /* a frame that has run past 50 ms is CPU-boosted */
    if (first) {
      first = 0;
      dcr_boost_first_picture(); /* the start-up boost can end */
    }
    dcr_boost_launch_tick(); /* ends the start-up boost once the game is up */
    dcr_boost_report();
    if (frames - counted >= FIFA14_FPS_LOG_FRAMES) {
      const uint64_t ms = (now_ns() - last) / 1000000ull;
      debugPrintf("[loop] %llu frames in %llu ms (%llu fps), engine swaps: %u, "
                  "java objects: %d\n",
                  (unsigned long long)frames, (unsigned long long)ms,
                  (unsigned long long)(ms ? (frames - counted) * 1000u / ms : 0),
                  (unsigned)dcr_gl_frames(), jni_live_objects());
      /* did the engine ask the GL to paint in that window? (a swap of an empty
       * back buffer is a black screen just the same) -- once per window only */
      fifa14_gl_draw_window((unsigned)(frames - counted));
      /* ...and is its mixer feeding the device? One [audio] line per window, so
       * a run's log says whether write() ever arrives and how loud it is. */
      fifa14_audio_report();
      counted = frames;
      last = now_ns();
    }
  }
  debugPrintf("[loop] the loop ended after %llu frames (exit requested: %d, "
              "engine swaps: %u)\n",
              (unsigned long long)frames, rt_exit_requested(), (unsigned)dcr_gl_frames());
}

/* ------------------------------------------------------- the focus (Android's
 * onPause / onResume). rt_applet_poll calls these on the frame's thread; the
 * engine is told only once the boot has reached it. */
void port_focus_lost(void) {
  if (!g_booted)
    return;
  /* Android hands the view an ACTION_CANCEL when it loses the window: the
   * engine must not keep a pointer down across a pause (its UI would stay
   * pressed). fifa14_input.c sends the engine's cancel code per pointer. */
  fifa14_input_release_all();
  const uintptr_t pause = entry(FIFA14_ENTRY_NATIVE_ONPAUSE);
  debugPrintf("[boot] focus lost -> NativeOnPause\n");
  if (pause)
    ((void (*)(void *, void *))pause)(g_jni_env, fifa14_activity);
  g_paused = 1;
}

void port_focus_gained(void) {
  if (!g_booted)
    return;
  const uintptr_t resume = entry(FIFA14_ENTRY_NATIVE_ONRESUME);
  if (!g_paused) /* the first call of port_focus_gained comes with the boot */
    return;
  debugPrintf("[boot] focus back -> NativeOnResume(true)\n");
  if (resume)
    ((void (*)(void *, void *, int))resume)(g_jni_env, fifa14_activity, 1);
  g_paused = 0;
}

/* ------------------------------------ the EA::Jni context's JavaVM (the seed)
 *
 * WHY THIS EXISTS. EA::Jni::Context::GetEnv() (libFIFA14.so+0xe3a374) is
 *
 *     e3a38c  ldr r0, [r0]       ; r0 = this->m_javaVM      (Context+0)
 *     e3a390  ldr r1, [r0]       ; r1 = vm->functions       <-- THE CRASH
 *     e3a394  ldr r3, [r1, #24]  ; r3 = functions[6] = GetEnv (JNIInvokeInterface)
 *     e3a398  add r1, sp, #4     ; &env
 *     e3a39c  blx r3             ; GetEnv(vm, &env, JNI_VERSION_1_4)
 *
 * so a Context that never got a JavaVM faults on the SECOND instruction, with
 * r0 = 0 -- the reported data abort (pc +0xe3a390, far 0, esr 0x92000006,
 * r2 0x00010004). The Context is the object AppendPrimaryExternalStorageList
 * hands it as `this`, named from the load-time literals:
 *
 *     e3942c  ldr r0, [pc, #876]  ; (the GOT anchor - pc - 8)
 *     e39430  add r8, pc, r0      ; r8 = the anchor
 *     e39434  ldr r0, [pc, #872]  ; (the object - the anchor)
 *     e39438  add r7, r0, r8      ; r7 = the object
 *     e3943c  add r5, r7, #4      ; r5 = `this` (the vm field is at +0)
 *     e39440  mov r0, r5
 *     e39444  bl  EA::Jni::Context::GetEnv()
 *
 * WHAT IT DOES. The engine's own native is what fills that field, and step 5b
 * calls it (fifa14_storage_start): that is the fix. This is the belt to those
 * braces -- it decodes the seven instructions above OUT OF THE LOADED IMAGE, so
 * it can only ever write where THIS build really keeps the object, checks that
 * the branch lands on GetEnv, that the field is inside the game module's
 * writable segment and that it is still 0, and puts the port's JavaVM there.
 * Anything else is logged and left alone: a wrong guess can never write.
 *
 * THE POINT: with the JavaVM in place, GetEnv() goes through the runtime's
 * JavaVM and answers g_jni_env -- the same valid JNIEnv for any thread, since
 * the fake VM has no per-thread attach bookkeeping (jni_core.c: vm_GetEnv and
 * vm_AttachCurrentThread both answer g_jni_env). No thread can get a NULL env
 * from there again. */
#define FIFA14_ARM_LDR_PC(w) (((w) & 0xffff0000u) == 0xe59f0000u)  /* ldr Rd, [pc, #imm12]  */
#define FIFA14_ARM_ADD_PC(w) (((w) & 0xffff0000u) == 0xe08f0000u)  /* add Rd, pc, Rm        */
#define FIFA14_ARM_ADD_RR(w) (((w) & 0xfff00000u) == 0xe0800000u)  /* add Rd, Rn, Rm */
#define FIFA14_ARM_ADD_RI(w) (((w) & 0xfff00000u) == 0xe2800000u)  /* add Rd, Rn, #imm12 */
#define FIFA14_ARM_MOV_RR(w) (((w) & 0xfff00000u) == 0xe1a00000u)  /* mov Rd, Rm */
#define FIFA14_ARM_BL(w)     (((w) & 0xff000000u) == 0xeb000000u)  /* bl label */
/* (the two 0xffff0000 masks keep bits 31-16 only: Rd lives in bits 15-12 and
 * must be free to differ -- masking it in would make the test never match.) */

static void jni_context_vm_seed(void) {
  uintptr_t base = (uintptr_t)game_mod.load_virtbase;
  const uint32_t *fn = (const uint32_t *)so_try_find_addr_rx(
      &game_mod, FIFA14_SYM_STORAGE_APPEND_EXTERNAL);
  const uintptr_t getenv = so_try_find_addr_rx(&game_mod, FIFA14_SYM_JNI_GETENV);
  if (!fn || !getenv) {
    debugPrintf("[jni] the NULL-env safety net is off: %s and/or %s is not exported\n",
                FIFA14_SYM_STORAGE_APPEND_EXTERNAL, FIFA14_SYM_JNI_GETENV);
    return;
  }
  /* insn[0..6] = the seven instructions quoted above, from fn+0x0c. */
  const uint32_t *insn = &fn[3];
  if (!FIFA14_ARM_LDR_PC(insn[0]) || !FIFA14_ARM_ADD_PC(insn[1]) ||
      !FIFA14_ARM_LDR_PC(insn[2]) || !FIFA14_ARM_ADD_RR(insn[3]) ||
      !FIFA14_ARM_ADD_RI(insn[4]) || (insn[4] & 0xfffu) != 4u ||
      !FIFA14_ARM_MOV_RR(insn[5]) || !FIFA14_ARM_BL(insn[6])) {
    debugPrintf("[jni] the safety net does not recognise the first instructions of %s "
                "(not the anchor/object/...GetEnv sequence): nothing written\n",
                FIFA14_SYM_STORAGE_APPEND_EXTERNAL);
    return;
  }
  const uintptr_t a = (uintptr_t)insn; /* the ldr of the anchor */
  /* Both literals are `target - the address the add is relative to`, so they
   * are signed: on this build both are positive (the anchor is in .got, below
   * the object in .bss), but a build that put them the other way round must
   * still come out right. */
  const int32_t d1 = (int32_t)*(const uint32_t *)(a + 8 + (insn[0] & 0xfffu));
  const uintptr_t anchor = a + 12 + (intptr_t)d1;
  const int32_t d2 = (int32_t)*(const uint32_t *)(a + 16 + (insn[2] & 0xfffu));
  const uintptr_t obj = anchor + (intptr_t)d2;
  const uintptr_t ctx = obj + 4; /* add r5, r7, #4: the Context, the vm field at +0 */
  int32_t imm = (int32_t)(insn[6] & 0x00ffffffu);
  if (imm & 0x00800000)
    imm -= 0x01000000;
  const uintptr_t target = a + 24 + 8 + ((uintptr_t)imm << 2); /* insn[6] is the bl */
  if (target != getenv) {
    debugPrintf("[jni] the safety net: the branch after that object does not land on GetEnv "
                "(libFIFA14.so+0x%lx, GetEnv is +0x%lx): nothing written\n",
                (unsigned long)(target - base), (unsigned long)(getenv - base));
    return;
  }
  const Elf32_Phdr *seg = so_segment_of(&game_mod, ctx);
  if (!seg || !(seg->p_flags & PF_W)) {
    debugPrintf("[jni] the safety net: libFIFA14.so+0x%lx is not in a writable segment: "
                "nothing written\n",
                (unsigned long)(ctx - base));
    return;
  }
  volatile uint32_t *field = (volatile uint32_t *)ctx;
  if (*field) {
    debugPrintf("[jni] the EA::Jni context (libFIFA14.so+0x%lx) already holds %p: left alone\n",
                (unsigned long)(ctx - base), (void *)(uintptr_t)*field);
    return;
  }
  *field = (uint32_t)(uintptr_t)g_jni_vm;
  debugPrintf("[jni] the EA::Jni context's JavaVM field (libFIFA14.so+0x%lx) was NULL: set to "
              "%p, so no thread can dereference NULL in GetEnv\n",
              (unsigned long)(ctx - base), g_jni_vm);
}

/* -------- the second seed: EA::IO::AssetManagerJNI::sVM (the 'appbundle:/'
 * branch of FileStream::Open)
 *
 * WHY THIS EXISTS. FileStream::Open (libFIFA14.so+0xd84b4c) compares the first
 * ten bytes of the path against the .rodata literal 'appbundle:/' and, when it
 * matches, opens the file through the engine's JAVA AssetManager instead of
 * open(2). That branch is
 *
 *     d84d1c  add r0, pc, r0       ; the GOT anchor
 *     d84d20  ldr r0, [r5, r0]     ; r0 = &AssetManagerJNI::sVM   <- the GOT slot
 *     d84d24  ldr r0, [r0]         ; r0 = sVM
 *     d84d28  ldr r1, [r0]         ; r1 = (*sVM)->functions   <-- THE CRASH
 *     d84d2c  ldr r3, [r1, #24]    ; r3 = functions[6] = JavaVM->GetEnv
 *     d84d30  add r1, sp, #8       ; &env  (an AssetManagerJNI::AutoJNIEnv)
 *     d84d34  blx r3               ; GetEnv(sVM, &env, 0x10002)
 *
 * so a process whose sVM is still NULL faults on `ldr r1,[r0]` with r0 = 0 --
 * the reported data abort (pc +0xd84d28, lr AssetStream::
 * CreateMemoryStreamFromFile+0x9c, insn e5901000). sVM is a 4-byte .bss field
 * at +0x12432c0, filled by EA::IO::AssetManagerJNI::Startup with
 * `env->GetJavaVM(&sVM)`; the engine's own Startup is only reachable from the
 * Java_com_ea_EAIO_EAIO_StartupNativeImpl the APK's Java calls (see
 * fifa14_blast_stubs.c's fifa14_assetmanager_start, which the boot calls in
 * step 5c) -- and that is the FIX. This is the belt to those braces, exactly
 * as jni_context_vm_seed() is to fifa14_storage_start():
 *
 *   - the two addresses come from the .so's own symbol table, BY NAME
 *     (so_try_find_addr_rx): FileStream::Open and AssetManagerJNI::sVM;
 *   - the FIELD-address arithmetic is decoded out of the LOADED image, so it
 *     can only ever write where THIS build really keeps the field: the shape
 *     above is recognised instruction by instruction (register chains
 *     included), the two pc-relative literals that make the GOT slot are read
 *     from the code, and the slot must resolve to the address the symbol table
 *     gave for sVM. Anything else is logged and left alone;
 *   - the field must be inside a writable segment and still 0.
 *
 * Only then is the port's JavaVM written. With it in place, GetEnv() goes
 * through the runtime's fake JavaVM (jni_core.c's g_vm_fn, whose slot 6 is
 * GetEnv -- the same slot this code calls) and answers g_jni_env, and the
 * AssetManagerJNI::* calls that follow reach the port's AssetManager (the
 * APK's assets/, fifa14_blast_stubs.c). */
#define FIFA14_ARM_LDR_RR(w)  (((w) & 0x0ff00010u) == 0x07900000u) /* ldr Rd,[Rn,Rm]      */
#define FIFA14_ARM_LDR_OFF(w) (((w) & 0x0ff00000u) == 0x05900000u) /* ldr Rd,[Rn,#imm12]  */
/* (both masks cover bits 31-20 only -- the condition, the 01 transfer opcode,
 * I, P, U, B, W and L. Rd, Rn and Rm are checked separately, by insn_rd/
 * insn_rn/insn_rm, so a look-alike with another register does not match.) */
#define FIFA14_SEED_WINDOW 320u /* instructions scanned from FileStream::Open's entry */

static uint32_t insn_rd(uint32_t w) { return (w >> 12) & 0xfu; } /* Rd */
static uint32_t insn_rn(uint32_t w) { return (w >> 16) & 0xfu; } /* Rn */
static uint32_t insn_rm(uint32_t w) { return w & 0xfu; }         /* Rm */

/* The latest `ldr Rd, [pc, #imm]` before `upto` that loads into register `rd`,
 * as the 32-bit word it loads (0 when there is none). */
static int lit_last_into(const uint32_t *fn, unsigned upto, uint32_t rd, uint32_t *out) {
  int found = 0;
  for (unsigned i = 0; i < upto; i++) {
    if (!FIFA14_ARM_LDR_PC(fn[i]) || insn_rd(fn[i]) != rd)
      continue;
    *out = *(const uint32_t *)((uintptr_t)&fn[i] + 8 + (fn[i] & 0xfffu));
    found = 1;
  }
  return found;
}

static uintptr_t asset_manager_vm_seed(void) {
  const uintptr_t base = (uintptr_t)game_mod.load_virtbase;
  const uint32_t *fn = (const uint32_t *)so_try_find_addr_rx(&game_mod, FIFA14_SYM_FILE_STREAM_OPEN);
  const uintptr_t svm = so_try_find_addr_rx(&game_mod, FIFA14_SYM_ASSET_MANAGER_SVM);
  if (!fn || !svm) {
    debugPrintf("[assets] the AssetManagerJNI safety net is off: %s and/or %s is not "
                "exported by %s\n",
                FIFA14_SYM_FILE_STREAM_OPEN, FIFA14_SYM_ASSET_MANAGER_SVM, FIFA14_LIB_GAME);
    return 0;
  }
  debugPrintf("[assets] AssetManagerJNI::sVM @ libFIFA14.so+0x%lx, %s @ +0x%lx: looking for the "
              "'appbundle:/' branch that reads it\n",
              (unsigned long)(svm - base), FIFA14_SYM_FILE_STREAM_OPEN, (unsigned long)((uintptr_t)fn - base));
  for (unsigned i = 3; i + 4 < FIFA14_SEED_WINDOW; i++) {
    if (!FIFA14_ARM_ADD_PC(fn[i]) || insn_rn(fn[i]) != 15u) /* add Rd, pc, Rm */
      continue;
    if (!FIFA14_ARM_LDR_RR(fn[i + 1]) || insn_rd(fn[i + 1]) != insn_rd(fn[i]))
      continue; /* ldr Rd, [Rgot, Rd]: the anchor, addressed through the GOT base */
    if (!FIFA14_ARM_LDR_OFF(fn[i + 2]) || !FIFA14_ARM_LDR_OFF(fn[i + 3]) ||
        !FIFA14_ARM_LDR_OFF(fn[i + 4]) || (fn[i + 4] & 0xfffu) != 24u)
      continue; /* ldr Rd,[Rd] ; ldr Rx,[Rd] ; ldr Ry,[Rx,#24] */
    const uint32_t rd = insn_rd(fn[i + 1]);
    if (insn_rn(fn[i + 2]) != rd || insn_rd(fn[i + 2]) != rd || insn_rn(fn[i + 3]) != rd ||
        insn_rn(fn[i + 4]) != insn_rd(fn[i + 3]))
      continue; /* the register chain of the vm->functions[6] call, not a look-alike */
    /* The two literals: the add's Rm holds the anchor delta, insn[i+1]'s Rn the
     * GOT-base one. Both are pc-relative loads in this same function. */
    uint32_t lit_a = 0, lit_b = 0;
    if (!lit_last_into(fn, i + 1, insn_rm(fn[i]), &lit_a))
      continue;
    if (!lit_last_into(fn, i + 1, insn_rn(fn[i + 1]), &lit_b))
      continue;
    const uintptr_t insn_addr = (uintptr_t)&fn[i];
    /* `add Rd, pc, Rm` reads pc as insn_addr + 8 (ARM) and adds the anchor
     * literal; the GOT base literal is then added to it. The module is mapped
     * below 4 GB (its code uses 32-bit addresses), so this is the 32-bit sum
     * the guest itself computes: the address of the GOT slot the code reads. */
    const uint32_t slot = (uint32_t)(insn_addr + 8) + lit_a + lit_b;
    if (!so_segment_of(&game_mod, (uintptr_t)slot)) {
      debugPrintf("[assets] the safety net: the 'appbundle:/' branch at +0x%lx works out a GOT slot at "
                  "0x%lx, which is not inside %s: nothing written\n",
                  (unsigned long)(insn_addr - base), (unsigned long)slot, FIFA14_LIB_GAME);
      return 0;
    }
    /* The slot's (relocated) content must be the address the symbol table gave
     * for sVM: only then does THIS code read THAT field. */
    const uintptr_t got_field = (uintptr_t)*(volatile const uint32_t *)slot;
    if (got_field != svm) {
      debugPrintf("[assets] the safety net: the 'appbundle:/' branch at +0x%lx reads the GOT slot 0x%lx, "
                  "which holds 0x%lx, not sVM (0x%lx): nothing written\n",
                  (unsigned long)(insn_addr - base), (unsigned long)slot, (unsigned long)got_field,
                  (unsigned long)svm);
      return 0;
    }
    debugPrintf("[assets] the 'appbundle:/' branch at +0x%lx (inside %s) reads AssetManagerJNI::sVM "
                "(+0x%lx) through the GOT slot 0x%lx (content 0x%lx) and calls (*sVM)->functions[6] = "
                "JavaVM->GetEnv\n",
                (unsigned long)(insn_addr - base), FIFA14_SYM_FILE_STREAM_OPEN,
                (unsigned long)(svm - base), (unsigned long)(slot - base),
                (unsigned long)(got_field - base));
    const Elf32_Phdr *seg = so_segment_of(&game_mod, svm);
    if (!seg || !(seg->p_flags & PF_W)) {
      debugPrintf("[assets] the safety net: libFIFA14.so+0x%lx is not in a writable segment: "
                  "nothing written\n",
                  (unsigned long)(svm - base));
      return svm;
    }
    volatile uint32_t *field = (volatile uint32_t *)svm;
    if (*field) {
      debugPrintf("[assets] AssetManagerJNI::sVM already holds %p (%s): left alone\n",
                  (void *)(uintptr_t)*field,
                  *field == (uint32_t)(uintptr_t)g_jni_vm ? "the port's JavaVM" : "something else");
      return svm;
    }
    *field = (uint32_t)(uintptr_t)g_jni_vm;
    debugPrintf("[assets] AssetManagerJNI::sVM (libFIFA14.so+0x%lx) was NULL: set to %p, the port's "
                "JavaVM -- the 'appbundle:/' path now gets its JNIEnv from GetEnv instead of "
                "dereferencing NULL\n",
                (unsigned long)(svm - base), g_jni_vm);
    return svm;
  }
  debugPrintf("[assets] the safety net does not recognise the first %u instructions of %s (not the "
              "GOT-anchor/.../GetEnv sequence): sVM is left alone\n",
              (unsigned)FIFA14_SEED_WINDOW, FIFA14_SYM_FILE_STREAM_OPEN);
  return 0;
}

/* -------- the third and fourth seeds: the OTHER two EA::Jni::Contexts (the
 * InAppWebBrowser and VideoPlayer delegates' GetEnv crash)
 *
 * WHY THIS EXISTS. The crash report (crash.log, in two threads):
 *
 *     pc +0xe3a390  EA::Jni::Context::GetEnv+0x1c   (`ldr r1,[r0]`, r0 = 0)
 *     lr +0xe3a490  EA::Jni::Delegate::Delegate(char const*, Context*)+0x38
 *     far 0, esr 0x92000006, insn e5901000
 *
 * -- the same NULL m_javaVM as the two crashes already fixed, in the Context of
 * EA::InAppWebBrowser::BrowserAndroid or EA::VideoPlayer::PlayerAndroid. Each
 * class's own Java_com_ea_*_StartupNativeImpl fills it (config.h has the whole
 * chain), and the boot calls both of them (step 5d, fifa14_jni_class_start);
 * THIS is the belt to those braces, exactly as jni_context_vm_seed() is to
 * fifa14_storage_start() and asset_manager_vm_seed() is to
 * fifa14_assetmanager_start().
 *
 * HOW IT KNOWS WHERE THE FIELD IS. It decodes the native OUT OF THE LOADED
 * IMAGE (so it can only ever write where THIS build really keeps the field).
 * The native is 72 bytes and ends with these six instructions:
 *
 *     ldr Rd, [pc, #a]     ; Rd  = the anchor delta (a .got-relative literal)
 *     ldr Rn, [pc, #b]     ; Rn  = the GOT slot's delta (the second literal)
 *     ldr Rv, [sp, #4]     ; Rv  = the JavaVM env->GetJavaVM() just left on the stack
 *     add Rd, pc, Rd       ; Rd  = pc+8 of the add + a      (the .got base)
 *     ldr Rd, [Rn, Rd]     ; Rd  = *(b + the .got base)     (the FIELD's address)
 *     str Rv, [Rd]         ; *field = the JavaVM            <- m_javaVM at Context+0
 *
 * so the field is `*(anchor + b)`, and it must be the address the symbol table
 * gives for that class's mJniContext -- only then does THIS code write THAT
 * Context. Every instruction of the shape is checked (registers included, and
 * that the stored register is the one GetJavaVM wrote to the stack), the two
 * pc-relative literals are read from the code, and anything else is logged and
 * left alone.
 *
 * WHY THE CALL SITE AGREES (measured, not assumed): the two callers of
 * Delegate::Delegate(char const*, Context*) -- 0xe357ec in BrowserAndroid's
 * constructor and 0xe3bdac in PlayerAndroid's -- resolve the SAME GOT slot this
 * native writes through (0x010e6e40 / 0x010e6e9c), i.e. the constructor is
 * handed the very Context the native fills. That is why the seed still covers
 * the crash when the native call is skipped or its symbol is not bound.
 *
 * THE WRITE, as in the other two seeds: only into a field inside a writable
 * segment, only while it is still 0, and only the port's JavaVM -- so GetEnv()
 * answers g_jni_env (jni_core.c's fake JavaVM, slot 6: one JNIEnv for every
 * thread) and no thread can dereference NULL there again. Returns the Context
 * the seed resolved (0 = it could not: the log says why), so the caller can
 * read the field back. */
#define FIFA14_ARM_STR_RR(w) (((w) & 0x0ff00000u) == 0x05800000u) /* str Rd,[Rn,Rm] */

static uintptr_t jni_class_context_seed(const char *startup_sym, const char *ctx_sym,
                                        const char *label) {
  const uintptr_t base = (uintptr_t)game_mod.load_virtbase;
  const uint32_t *fn = (const uint32_t *)so_try_find_addr_rx(&game_mod, startup_sym);
  const uintptr_t ctx = so_try_find_addr_rx(&game_mod, ctx_sym);
  if (!fn || !ctx) {
    debugPrintf("[jni] the %s safety net is off: %s and/or %s is not exported by %s\n", label,
                startup_sym, ctx_sym, FIFA14_LIB_GAME);
    return 0;
  }
  debugPrintf("[jni] %s's Context is %s @ libFIFA14.so+0x%lx; %s @ +0x%lx is what fills it\n",
              label, ctx_sym, (unsigned long)(ctx - base), startup_sym,
              (unsigned long)((uintptr_t)fn - base));
  for (unsigned i = 4; i + 2 < 20; i++) { /* the 72-byte body: 18 instructions */
    /* add Rd, pc, Rd -- the .got base (pc = this instruction + 8) */
    if (!FIFA14_ARM_ADD_PC(fn[i]) || insn_rn(fn[i]) != 15u || insn_rd(fn[i]) != insn_rm(fn[i]))
      continue;
    const uint32_t rd = insn_rd(fn[i]);
    /* ldr Rd, [Rn, Rd] -- the field's address, the GOT delta in Rn */
    if (!FIFA14_ARM_LDR_RR(fn[i + 1]) || insn_rd(fn[i + 1]) != rd || insn_rm(fn[i + 1]) != rd)
      continue;
    const uint32_t rgot = insn_rn(fn[i + 1]);
    /* str Rv, [Rd] -- the JavaVM into the field. The BASE is the field's own
     * register (Rd of the ldr above, so Rd here is Rn): `str Rd_stored, [Rn]`
     * -- the stored register is Rd and it is NOT the field's. */
    if (rgot == rd || !FIFA14_ARM_STR_RR(fn[i + 2]) || insn_rn(fn[i + 2]) != rd ||
        (fn[i + 2] & 0xfffu) != 0)
      continue;
    /* ...and the stored register must be the one the GetJavaVM result reached
     * through the stack (`ldr Rv, [sp, #4]`, the instruction right before the
     * anchor). */
    const uint32_t rv = insn_rd(fn[i + 2]);
    if (!FIFA14_ARM_LDR_OFF(fn[i - 1]) || insn_rd(fn[i - 1]) != rv || insn_rn(fn[i - 1]) != 13u ||
        (fn[i - 1] & 0xfffu) != 4u)
      continue;
    /* The two literals: the last `ldr pc` into Rd (the anchor delta) and into
     * Rn (the GOT delta), both before the add. */
    uint32_t lit_a = 0, lit_b = 0;
    if (!lit_last_into(fn, i, rd, &lit_a) || !lit_last_into(fn, i, rgot, &lit_b))
      continue;
    const uintptr_t insn_addr = (uintptr_t)&fn[i];
    /* `add Rd, pc, Rd` reads pc as insn_addr + 8 (ARM) and adds the anchor
     * literal; the GOT delta is then added to it. The module is mapped below
     * 4 GB, so this is the 32-bit sum the guest itself computes: the address of
     * the GOT slot the code reads. */
    const uint32_t slot = (uint32_t)(insn_addr + 8) + lit_a + lit_b;
    if (!so_segment_of(&game_mod, (uintptr_t)slot)) {
      debugPrintf("[jni] the %s safety net: the write at +0x%lx works out a GOT slot at 0x%lx, "
                  "which is not inside %s: nothing written\n",
                  label, (unsigned long)(insn_addr - base), (unsigned long)slot, FIFA14_LIB_GAME);
      return 0;
    }
    /* The slot's (relocated) content must be the address the symbol table gave
     * for mJniContext: only then does THIS code write THAT Context. */
    const uintptr_t field = (uintptr_t)*(volatile const uint32_t *)slot;
    if (field != ctx) {
      debugPrintf("[jni] the %s safety net: the write at +0x%lx reads the GOT slot 0x%lx, which "
                  "holds 0x%lx, not %s (0x%lx): nothing written\n",
                  label, (unsigned long)(insn_addr - base), (unsigned long)slot,
                  (unsigned long)field, ctx_sym, (unsigned long)ctx);
      return 0;
    }
    debugPrintf("[jni] %s: +0x%lx writes the JavaVM through the GOT slot 0x%lx, which holds 0x%lx "
                "= %s -- the same slot the class's Delegate constructor reads\n",
                label, (unsigned long)(insn_addr - base), (unsigned long)slot,
                (unsigned long)field, ctx_sym);
    const Elf32_Phdr *seg = so_segment_of(&game_mod, ctx);
    if (!seg || !(seg->p_flags & PF_W)) {
      debugPrintf("[jni] the %s safety net: libFIFA14.so+0x%lx is not in a writable segment: "
                  "nothing written\n",
                  label, (unsigned long)(ctx - base));
      return ctx;
    }
    volatile uint32_t *f = (volatile uint32_t *)ctx;
    if (*f) {
      debugPrintf("[jni] %s's mJniContext (libFIFA14.so+0x%lx) already holds %p (%s): left alone\n",
                  label, (unsigned long)(ctx - base), (void *)(uintptr_t)*f,
                  *f == (uint32_t)(uintptr_t)g_jni_vm ? "the port's JavaVM" : "something else");
      return ctx;
    }
    *f = (uint32_t)(uintptr_t)g_jni_vm;
    debugPrintf("[jni] %s's mJniContext (libFIFA14.so+0x%lx) was NULL: set to %p, the port's "
                "JavaVM -- the class's Delegate constructor now gets its JNIEnv from GetEnv "
                "instead of dereferencing NULL\n",
                label, (unsigned long)(ctx - base), g_jni_vm);
    return ctx;
  }
  debugPrintf("[jni] the %s safety net does not recognise the first 20 instructions of %s (not "
              "the anchor/GOT-slot/str sequence): its Context is left alone\n",
              label, startup_sym);
  return 0;
}

/* The read-back after the seeds and the Startups: what the two Contexts hold
 * now. 0 = the seed could not resolve the field (the [jni] lines above say
 * why), and then a Delegate built for that class would fault in GetEnv. */
static void jni_class_context_report(uintptr_t ctx, const char *label) {
  if (!ctx) {
    debugPrintf("[boot] 5d/10 %s: the Context field was NOT resolved (see the [jni] lines above); "
                "if the engine builds that class's Delegate, GetEnv faults on the NULL\n",
                label);
    return;
  }
  const void *vm = (const void *)(uintptr_t)*(volatile const uint32_t *)ctx;
  debugPrintf("[boot] 5d/10 %s::mJniContext (libFIFA14.so+0x%lx) = %p%s\n", label,
              (unsigned long)(ctx - (uintptr_t)game_mod.load_virtbase), vm,
              vm == g_jni_vm
                  ? " -- the port's JavaVM: GetEnv answers an env"
                  : (vm ? "  <- NOT the port's JavaVM" : "  <- STILL NULL: the Delegate constructor "
                                                          "will fault again"));
}

/* ----------------------------- libnimble.so's JavaVM (the fifth seed)
 *
 * WHY THIS EXISTS. crash.log's LAST block:
 *
 *     pc  0481bbbc  libnimble.so+0x34bbc  (EA::Nimble::getEnv()+0xc)  (Thumb)
 *     lr  04820b59  libnimble.so+0x39b59
 *                    (EA::Nimble::Base::NotificationListener::NotificationListener(
 *                     fastdelegate::FastDelegate3<...> const&)+0x41)
 *     r0  00000000   far 00000000   esr 92000006   r2 00010006
 *
 * r0 = 0, far = 0 and r2 = 0x00010006 (JNI_VERSION_1_6) name the fault exactly:
 * getEnv's `ldr r3,[r0]` on the JavaVM it loaded from libnimble.so+0x56d80 --
 * which in the port is still 0. config.h has the whole disassembly of getEnv
 * and of JNI_OnLoad (the module's only writer of that word): the two
 * pc-relative literals, the record layout, and the four instructions in the
 * module that reach the block at all.
 *
 * WHAT THIS DOES -- the module's OWN entry point, and the same belt-and-braces
 * order the three EA::Jni seeds above use:
 *
 *   - nimble_vm_seed() decodes the field OUT OF THE LOADED IMAGE by two
 *     independent paths (getEnv's read and JNI_OnLoad's write, which must agree
 *     on the same address) and, only if it is inside a writable segment and
 *     still 0, puts the port's JavaVM there;
 *   - fifa14_nimble_onload() then runs libnimble.so's JNI_OnLoad(g_jni_vm, NULL)
 *     -- exactly what Android's System.loadLibrary("nimble") runs -- which sets
 *     the same field itself and creates the pthread key beside it;
 *   - the boot reads the field back and says in one line what it holds.
 *
 * THE THREAD. The crash is in thread 694, not on the boot's thread, so the
 * value must answer from ANY thread -- and it does: the port's JavaVM is the
 * runtime's fake one (jni_core.c's g_vm_fn / g_jni_vm, answering g_jni_env),
 * whose slot 6 is GetEnv, the very slot getEnv calls, with no per-thread attach
 * bookkeeping. Once this field holds it, no thread can get a NULL env there. */

/* Thumb-1, which is what this module is: the high-register ADD (0x44xx; Rm = 15
 * is the pc), `ldr Rd,[pc,#imm8*4]`, and the immediate forms of ldr/str. Note
 * that the module uses BOTH pc-relative rules: `add Rd, pc` adds its literal to
 * insn + 4 with nothing rounded (that is what makes JNI_OnLoad's block land on
 * a word boundary), while `ldr Rd,[pc,#imm]` reads from insn + 4 rounded down to
 * 4. Off by two bytes either way and the field is the wrong word -- which is why
 * the seed cross-checks its two paths and the segment before writing. */
#define FIFA14_THUMB_ADD_PC(w) (((w) & 0xff00u) == 0x4400u && (((w) >> 3) & 0xfu) == 15u)
#define FIFA14_THUMB_LDR_PC(w) (((w) & 0xf800u) == 0x4800u)  /* ldr Rd,[pc,#imm8*4] */
#define FIFA14_THUMB_LDR_IMM(w) (((w) & 0xf800u) == 0x6800u) /* ldr Rd,[Rn,#imm5*4] */
#define FIFA14_THUMB_STR_IMM(w) (((w) & 0xf800u) == 0x6000u) /* str Rd,[Rn,#imm5*4] */

static uint32_t thumb_add_rd(uint32_t w) { return ((w >> 4) & 0x8u) | (w & 0x7u); }
static uint32_t thumb_lo_rd(uint32_t w) { return (w >> 8) & 0x7u; }
static uint32_t thumb_lo_rn(uint32_t w) { return (w >> 3) & 0x7u; }
static uint32_t thumb_ldr_pc_off(uint32_t w) { return (w & 0xffu) << 2; }
static uint32_t thumb_imm5_off(uint32_t w) { return ((w >> 6) & 0x1fu) << 2; }

/* The latest `ldr Rd,[pc,#imm]` before `upto` that loads into `rd`, as the word
 * it loads (0 when there is none). */
static int thumb_lit_last_into(const uint16_t *fn, unsigned upto, uint32_t rd, uint32_t *out) {
  int found = 0;
  for (unsigned i = 0; i < upto; i++) {
    if (!FIFA14_THUMB_LDR_PC(fn[i]) || thumb_lo_rd(fn[i]) != rd)
      continue;
    const uintptr_t lit = ((uintptr_t)&fn[i] + 4) & ~(uintptr_t)3;
    *out = *(const uint32_t *)(lit + thumb_ldr_pc_off(fn[i]));
    found = 1;
  }
  return found;
}

static uintptr_t nimble_vm_seed(void) {
  const uintptr_t base = (uintptr_t)nimble_mod.load_virtbase;
  const uint16_t *getenv_fn = (const uint16_t *)so_try_find_addr_rx(&nimble_mod, FIFA14_SYM_NIMBLE_GETENV);
  const uint16_t *onload_fn = (const uint16_t *)so_try_find_addr_rx(&nimble_mod,
                                                                   FIFA14_SYM_NIMBLE_JNI_ONLOAD);
  if (!getenv_fn || !onload_fn) {
    debugPrintf("[nimble] the JavaVM safety net is off: %s and/or %s is not exported by %s\n",
                FIFA14_SYM_NIMBLE_GETENV, FIFA14_SYM_NIMBLE_JNI_ONLOAD, FIFA14_LIB_NIMBLE);
    return 0;
  }
  debugPrintf("[nimble] %s @ libnimble.so+0x%lx and %s @ +0x%lx: decoding the JavaVM word they "
              "share\n",
              FIFA14_SYM_NIMBLE_GETENV, (unsigned long)((uintptr_t)getenv_fn - base),
              FIFA14_SYM_NIMBLE_JNI_ONLOAD, (unsigned long)((uintptr_t)onload_fn - base));

  /* Path 1, getEnv's READ: `add Rd, pc` (the anchor) and, right after it,
   * `ldr R0,[Rd]` with offset 0 -- the JavaVM word itself, the one the next
   * instruction dereferences (the fault in crash.log). */
  uintptr_t from_read = 0;
  for (unsigned i = 1; i + 1 < 24u; i++) {
    if (!FIFA14_THUMB_ADD_PC(getenv_fn[i]))
      continue;
    const uint32_t rd = thumb_add_rd(getenv_fn[i]);
    if (!FIFA14_THUMB_LDR_IMM(getenv_fn[i + 1]) || thumb_lo_rn(getenv_fn[i + 1]) != rd ||
        thumb_lo_rd(getenv_fn[i + 1]) != 0u || thumb_imm5_off(getenv_fn[i + 1]) != 0u)
      continue;
    uint32_t lit = 0;
    if (!thumb_lit_last_into(getenv_fn, i, rd, &lit))
      continue;
    from_read = (uintptr_t)&getenv_fn[i] + 4 + lit;
    break;
  }

  /* Path 2, JNI_OnLoad's WRITE: `add Rd, pc` and, right after it, `str R0,[Rd]`
   * with offset 0 -- the JavaVM ARGUMENT (r0) going into the field. The stored
   * register being r0 is what makes this the write and not a read. */
  uintptr_t from_write = 0;
  for (unsigned i = 1; i + 1 < 40u; i++) {
    if (!FIFA14_THUMB_ADD_PC(onload_fn[i]))
      continue;
    const uint32_t rd = thumb_add_rd(onload_fn[i]);
    if (!FIFA14_THUMB_STR_IMM(onload_fn[i + 1]) || thumb_lo_rn(onload_fn[i + 1]) != rd ||
        thumb_lo_rd(onload_fn[i + 1]) != 0u || thumb_imm5_off(onload_fn[i + 1]) != 0u)
      continue;
    uint32_t lit = 0;
    if (!thumb_lit_last_into(onload_fn, i, rd, &lit))
      continue;
    from_write = (uintptr_t)&onload_fn[i] + 4 + lit;
    break;
  }

  if (!from_read || !from_write) {
    debugPrintf("[nimble] the safety net does not recognise %s's read or %s's write (not the "
                "pc-anchor + ldr/str sequence): nothing written\n",
                FIFA14_SYM_NIMBLE_GETENV, FIFA14_SYM_NIMBLE_JNI_ONLOAD);
    return 0;
  }
  if (from_read != from_write) {
    debugPrintf("[nimble] the safety net: getEnv's read decodes to libnimble.so+0x%lx and "
                "JNI_OnLoad's write to +0x%lx -- they disagree, so this is not the JavaVM word: "
                "nothing written\n",
                (unsigned long)(from_read - base), (unsigned long)(from_write - base));
    return 0;
  }
  const uintptr_t field = from_read;

  /* How many places in the module reach that block at all: the run-time form of
   * config.h's "JNI_OnLoad is the only writer" -- every `add Rd, pc` in the
   * module's code, decoded the same way, that lands inside the four words.
   * The code window comes from the module's OWN symbol table (its lowest and
   * highest defined Thumb function): the section headers are gone by this point
   * (so_free_temp freed the file image) and the module's first PT_LOAD also
   * carries .dynsym/.rel.dyn/.rodata, whose halfwords would decode as
   * instructions. */
  unsigned refs = 0;
  uintptr_t lo = 0, hi = 0;
  for (int i = 0; i < nimble_mod.num_syms; i++) {
    const Elf32_Sym *s = &nimble_mod.syms[i];
    if (s->st_shndx == SHN_UNDEF || ELF32_ST_TYPE(s->st_info) != STT_FUNC || !s->st_size)
      continue;
    const uintptr_t v = s->st_value & ~(uintptr_t)1; /* Thumb symbols carry bit 0 */
    if (!lo || v < lo)
      lo = v;
    if (v + s->st_size > hi)
      hi = v + s->st_size;
  }
  if (lo && hi > lo && hi - lo < 4u * 1024u * 1024u) {
    const uint16_t *code = (const uint16_t *)(base + lo);
    const unsigned n = (unsigned)((hi - lo) / 2);
    for (unsigned i = 1; i < n; i++) {
      if (!FIFA14_THUMB_ADD_PC(code[i]))
        continue;
      const uint32_t rd = thumb_add_rd(code[i]);
      uint32_t lit = 0;
      if (!thumb_lit_last_into(code, i, rd, &lit))
        continue;
      const uintptr_t a = (uintptr_t)&code[i] + 4 + lit;
      if (a >= field && a < field + 16u) {
        refs++;
        debugPrintf("[nimble]   +0x%lx reaches the JavaVM block (word %lu)\n",
                    (unsigned long)(a - base), (unsigned long)(a - field));
      }
    }
  }

  const Elf32_Phdr *seg = so_segment_of(&nimble_mod, field);
  if (!seg || !(seg->p_flags & PF_W)) {
    debugPrintf("[nimble] the safety net: libnimble.so+0x%lx is not in a writable segment: "
                "nothing written\n",
                (unsigned long)(field - base));
    return 0;
  }
  volatile uint32_t *f = (volatile uint32_t *)field;
  if (*f) {
    debugPrintf("[nimble] the JavaVM (libnimble.so+0x%lx) already holds %p (%s): left alone\n",
                (unsigned long)(field - base), (void *)(uintptr_t)*f,
                *f == (uint32_t)(uintptr_t)g_jni_vm ? "the port's JavaVM" : "something else");
    return field;
  }
  *f = (uint32_t)(uintptr_t)g_jni_vm;
  debugPrintf("[nimble] the JavaVM (libnimble.so+0x%lx) was NULL: set to %p, the port's JavaVM "
              "(%u place(s) in the module reach that block) -- getEnv answers an env in any "
              "thread from here on\n",
              (unsigned long)(field - base), g_jni_vm, refs);
  return field;
}

/* libnimble.so's own JNI entry point, exactly as Android's
 * System.loadLibrary("nimble") calls it -- and after the seed above, which is
 * the belt to this braces. It writes the same field (its only write) and
 * creates the pthread key that sits right beside it. Its answer names the
 * module: JNI_VERSION_1_6, or the log says the module is not this build's. */
static void fifa14_nimble_onload(void) {
  const uintptr_t onload = so_try_find_addr_rx(&nimble_mod, FIFA14_SYM_NIMBLE_JNI_ONLOAD);
  if (!onload) {
    debugPrintf("[nimble] %s has no %s: only the seed above is in play\n", FIFA14_LIB_NIMBLE,
                FIFA14_SYM_NIMBLE_JNI_ONLOAD);
    return;
  }
  const int ver = ((int (*)(void *, void *))onload)(g_jni_vm, NULL);
  debugPrintf("[nimble] %s JNI_OnLoad(vm=%p, NULL) -> 0x%08x%s\n", FIFA14_LIB_NIMBLE, g_jni_vm,
              (unsigned)ver,
              (unsigned)ver == FIFA14_NIMBLE_JNI_VERSION
                  ? " (JNI_VERSION_1_6: the JavaVM, the pthread key and the class cache are in "
                    "place)"
                  : "  <- NOT 0x10006! the module is not the one this port was built for");
}

/* ------------------------------------------------------------------ the run */
void fifa14_boot_run(void) {
  /* The hang report and the 5 s log flush: from here on the first run's log
   * reaches the SD card even if the game stops dead (RT_WATCHDOG_BOOT makes a
   * stop during the boot count too). "engine frames" tells a stuck loop from a
   * loop that runs while the engine never swaps. */
  dcr_watchdog_start();
  rt_watchdog_add_counter("engine frames", frames_u32);
  rt_watchdog_add_counter("engine swaps", dcr_gl_frames);

  debugPrintf("[boot] ==== FIFA 14 starts ====\n");

  /* 1 */
  debugPrintf("[boot] 1/10 the JNI environment (fifa14_java_init)\n");
  fifa14_java_init();

  /* 2 */
  debugPrintf("[boot] 2/10 the natives, bound by symbol\n");
  fifa14_boot_bind();

  /* 2b */
  /* libnimble.so's OWN JavaVM, BEFORE any of the engine's code runs: step 3's
   * constructors are the first that can build a Nimble object. Android's
   * System.loadLibrary("nimble") runs this module's JNI_OnLoad(vm, NULL); the
   * port loads the module itself (fifa14_main.c) and had only ever called
   * libFIFA14.so's JNI_OnLoad (step 4), so EA::Nimble::getEnv() read a NULL
   * static and the NotificationListener constructor died on it (crash.log's
   * last block, thread 694, libnimble.so+0x34bbc, far 0). The seed first is the
   * belt to those braces, exactly as in 5b/5c/5d (nimble_vm_seed above). */
  debugPrintf("[boot] 2b/10 %s's own JavaVM: JNI_OnLoad -> EA::Nimble::getEnv (the "
              "NotificationListener crash)\n",
              FIFA14_LIB_NIMBLE);
  {
    const uintptr_t nvm = nimble_vm_seed();
    fifa14_nimble_onload();
    if (nvm) {
      const void *vm = (const void *)(uintptr_t)*(volatile uint32_t *)nvm;
      debugPrintf("[boot] 2b/10 libnimble.so's JavaVM (libnimble.so+0x%lx) = %p%s\n",
                  (unsigned long)(nvm - (uintptr_t)nimble_mod.load_virtbase), vm,
                  vm == g_jni_vm
                      ? " -- the port's JavaVM: getEnv answers an env in ANY thread"
                      : (vm ? "  <- NOT the port's JavaVM"
                            : "  <- STILL NULL: the next Nimble object's getEnv will fault "
                              "again"));
    } else {
      debugPrintf("[boot] 2b/10 libnimble.so's JavaVM: the field was NOT resolved (the [nimble] "
                  "lines above say why); if it is still 0, the next Nimble object's getEnv "
                  "faults on NULL\n");
    }
  }

  /* 3 */
  debugPrintf("[boot] 3/10 %s's constructors (Android's dlopen order, after the JNI)\n",
              FIFA14_LIB_GAME);
  g_so_trace_ctors = FIFA14_TRACE_CTORS;
  so_execute_init_array(&game_mod);
  /* so_free_temp(&game_mod) is deliberately NOT called: it frees the ELF
   * image, and the crash handler's so_dump_maps() (and dl_iterate_phdr) still
   * read it. 19 MB of the heap is cheap next to a lost crash report. */
  debugPrintf("[boot] 3/10 constructors done%s\n",
              FIFA14_TRACE_CTORS ? " (each was logged)" : "");

  /* 4 */
  const uintptr_t onload = entry(FIFA14_ENTRY_JNI_ONLOAD);
  if (onload) {
    const int ver = ((int (*)(void *, void *))onload)(g_jni_vm, NULL);
    debugPrintf("[boot] 4/10 JNI_OnLoad(vm=%p) -> 0x%08x%s\n", g_jni_vm, (unsigned)ver,
                (unsigned)ver == FIFA14_JNI_VERSION ? " (JNI_VERSION_1_4)"
                                                    : "  <- NOT 0x10004!");
  } else {
    debugPrintf("[boot] 4/10 JNI_OnLoad: MISSING (the engine's JniContext::GetEnv will "
                "fail and every native that asks Java will do nothing)\n");
  }

  /* 5 */
  const uintptr_t initjni = entry(FIFA14_ENTRY_INIT_JNI);
  debugPrintf("[boot] 5/10 initJNI(activity=%p)\n", (void *)fifa14_activity);
  if (initjni)
    ((void (*)(void *, void *))initjni)(g_jni_env, fifa14_activity);
  else
    debugPrintf("[boot] 5/10 initJNI: MISSING\n");

  /* 5b */
  /* The phone's storage directories, BEFORE anything that asks for one. The
   * engine's first storage query comes with its init frame (step 8): at that
   * point EA::Jni::Context must have a JavaVM or GetEnv() faults on NULL (the
   * crash that brought this step here). Android's Java does this from the
   * activity (StorageDirectory.Startup(activity) -> StartupNativeImpl()); the
   * port's Java never would, so it is done here, in the engine's own native.
   * jni_context_vm_seed() first is the belt to that braces (see above). */
  debugPrintf("[boot] 5b/10 the storage directory: EAMIO/StorageDirectory.Startup "
              "(the EA::Jni context's JavaVM)\n");
  jni_context_vm_seed();
  fifa14_storage_start("boot");

  /* 5c */
  /* The AssetManager side of the same failure, and the crash that brought this
   * step here: FileStream::Open opens every 'appbundle:/' path through the
   * engine's JAVA AssetManager, whose sVM (EA::IO::AssetManagerJNI::sVM,
   * libFIFA14.so+0x12432c0) only Java's EAIO.Startup -> StartupNativeImpl ->
   * AssetManagerJNI::Startup fills. The engine's first data file
   * ('appbundle:/EAMCore.ini', Blast::System::LoadProperties in
   * Blast::System::Init) therefore dereferenced NULL
   * (pc +0xd84d28, `ldr r1,[r0]` with r0 = (*sVM) = 0). The port calls the
   * engine's own native (fifa14_assetmanager_start), which initialises sVM,
   * the AssetManager object, the roots and the method ids; the seed before it
   * is the belt to those braces (fifa14_boot.c), and the line after is the
   * read-back: whether sVM really ended up holding the port's JavaVM is the
   * ONE thing the next run of the port has to see. */
  debugPrintf("[boot] 5c/10 the AssetManager: EAIO.Startup -> AssetManagerJNI::Startup "
              "(the 'appbundle:/' paths' JavaVM)\n");
  {
    const uintptr_t svm = asset_manager_vm_seed();
    fifa14_assetmanager_start("boot 5c");
    if (svm) {
      const void *vm = (const void *)(uintptr_t)*(volatile uint32_t *)svm;
      debugPrintf("[boot] 5c/10 AssetManagerJNI::sVM (libFIFA14.so+0x%lx) = %p%s\n",
                  (unsigned long)(svm - (uintptr_t)game_mod.load_virtbase), vm,
                  vm == g_jni_vm ? " -- the port's JavaVM: the 'appbundle:/' branch has its JNIEnv"
                                 : (vm ? "  <- NOT the port's JavaVM" : "  <- STILL NULL: the "
                                                                         "'appbundle:/' branch "
                                                                         "will fault again"));
    }
  }

  /* 5d */
  /* The other two EA::Jni::Contexts, and the crash that brought this step
   * here: EA::Jni::Delegate::Delegate(char const*, Context*) (@0xe3a458) calls
   * Context::GetEnv() on the Context it is handed, and its only two callers in
   * the whole image build the delegates of EA::InAppWebBrowser::BrowserAndroid
   * (0xe357ec, in that class's constructor) and EA::VideoPlayer::
   * PlayerAndroid (0xe3bdac, in that one's). Both Contexts were still NULL ->
   * `ldr r1,[r0]` with r0 = 0 at GetEnv+0x1c (pc +0xe3a390, lr +0xe3a490, the
   * crash in crash.log, twice: the engine builds the delegate on more than one
   * thread). Each class's Java side fills its Context by calling the class's
   * StartupNativeImpl; the port's Java never would, so the port does it here,
   * exactly as step 5b does for StorageDirectory and step 5c for the
   * AssetManager -- and the seed before each call is the belt to those braces
   * (jni_class_context_seed decodes the native in the LOADED image and writes
   * only the field whose GOT slot really is that class's mJniContext). */
  debugPrintf("[boot] 5d/10 the other two EA::Jni contexts: InAppWebBrowser/BrowserAndroid and "
              "VideoPlayer/PlayerAndroid (their Delegate constructors' GetEnv)\n");
  {
    const uintptr_t bctx =
        jni_class_context_seed(FIFA14_ENTRY_BROWSER_JNI_STARTUP, FIFA14_SYM_BROWSER_JNI_CONTEXT,
                               "BrowserAndroid");
    const uintptr_t vctx = jni_class_context_seed(FIFA14_ENTRY_VIDEO_JNI_STARTUP,
                                                  FIFA14_SYM_VIDEO_JNI_CONTEXT, "PlayerAndroid");
    fifa14_jni_class_start("boot 5d");
    jni_class_context_report(bctx, "BrowserAndroid");
    jni_class_context_report(vctx, "PlayerAndroid");
  }

  /* 6 */
  const uintptr_t oncreate = entry(FIFA14_ENTRY_NATIVE_ONCREATE);
  debugPrintf("[boot] 6/10 NativeOnCreate(activity=%p) -- the engine's system, state = 1\n",
              (void *)fifa14_activity);
  if (oncreate)
    ((void (*)(void *, void *))oncreate)(g_jni_env, fifa14_activity);
  else
    debugPrintf("[boot] 6/10 NativeOnCreate: MISSING -- the engine is not up\n");

  /* 7 */
  const uintptr_t entrypoint = entry(FIFA14_ENTRY_RUN_ENTRY_POINT);
  if (entrypoint) {
    const int rc = ((int (*)(void *, void *))entrypoint)(g_jni_env,
                                                        jni_class(F_FIFA14_ENGINE)->obj);
    debugPrintf("[boot] 7/10 runEntryPoint() -> %d (it calls the .so's main(0, NULL): a "
                "stub in this build)\n",
                rc);
  } else {
    debugPrintf("[boot] 7/10 runEntryPoint: MISSING\n");
  }

  /* 7b */
  /* THE GL CONTEXT, BEFORE THE ENGINE TOUCHES THE GL. On Android the activity's
   * Java (GLSurfaceView) creates the EGL display/config/context/window surface
   * and makes it current on its GL thread; libFIFA14.so imports only
   * eglGetCurrentDisplay/eglGetCurrentSurface/eglSwapBuffers to FIND it (no
   * eglCreateContext/eglMakeCurrent, and no such string for a dlsym either).
   * With no Java there is therefore no context at all, and the engine's very
   * first GL question dies on it: Blast::System::Init ->
   * ModuleManager::Init -> InitAutoInstantiatedModules -> GetModule -> the
   * renderer module's Initialize -> EA::Graphics::OpenGLES20Managed::
   * glGetString(GL_RENDERER = 0x1F01) -> Mesa's glGetString, which is
   * `if (!ctx) return NULL` -> EA::StdC::Strcmp(NULL, "Mali-T604") ->
   * data abort at libFIFA14.so+0xe5c8b8 (`ldrb r3,[r0]`, r0 = 0; r2 = 0x4d
   * = 'M' of the GPU-table string). fifa14_gl.c has the whole disassembly of
   * that chain; it also wraps glGetString so a NULL can never reach the
   * engine's strcmp again, and logs the pname and why it was NULL. */
  debugPrintf("[boot] 7b/10 the GL context the Java GLSurfaceView would have made "
              "(fifa14_gl_boot: EGL display/config/GLES2 context/window surface, "
              "made current)\n");
  fifa14_gl_boot();
  debugPrintf("[boot] 7b/10 current context: %s; the engine's GL_RENDERER is \"%s\"\n",
              fifa14_gl_current() ? "yes" : "NO", fifa14_gl_renderer());

  /* 8: the engine's Init, which only the first NativeOnDrawFrame runs. */
  const uintptr_t draw = entry(FIFA14_ENTRY_NATIVE_ONDRAWFRAME);
  if (!draw || !oncreate) {
    debugPrintf("[boot] the frame tick (%s) or NativeOnCreate is missing: nothing to run. "
                "The log above says what the .so has.\n",
                FIFA14_ENTRY_NATIVE_ONDRAWFRAME);
    return;
  }
  debugPrintf("[boot] 8/10 the engine's init frame: NativeOnDrawFrame (state 1 -> 2)\n");
  dcr_boost_frame_begin();
  draw_one_frame(draw);
  g_frames = 1;
  dcr_boost_frame_end(1);

  /* 9 */
  const uintptr_t created = entry(FIFA14_ENTRY_NATIVE_SURFACE_CREATED);
  debugPrintf("[boot] 9/10 NativeOnSurfaceCreated (now that the state is 2)\n");
  if (created)
    ((void (*)(void *, void *))created)(g_jni_env, jni_singleton(F_ANDROID_RENDERER));
  else
    debugPrintf("[boot] 9/10 NativeOnSurfaceCreated: MISSING\n");

  /* 10 */
  const uintptr_t changed = entry(FIFA14_ENTRY_NATIVE_SURFACE_CHANGED);
  int w = 0, h = 0;
  dcr_window_size(&w, &h);
  debugPrintf("[boot] 10/10 NativeOnSurfaceChanged(%dx%d)\n", w, h);
  if (changed)
    ((void (*)(void *, void *, int, int))changed)(g_jni_env, jni_singleton(F_ANDROID_RENDERER),
                                                 w, h);
  else
    debugPrintf("[boot] 10/10 NativeOnSurfaceChanged: MISSING\n");

  g_booted = 1;

  /* 10b: ANDROID'S onResume -- THE LIFE-CYCLE CALL THE ENGINE'S MODULES LISTEN
   * FOR (added 2026-10-13, the round in which every pad key was dropped).
   *
   * WHY IT WAS MISSING. Java's Activity always gets onCreate -> onResume before
   * the first frame, and MainActivity.onResume calls NativeOnResume; the port
   * only ever called it from port_focus_gained(), which returns early unless a
   * PAUSE happened first ("the first call of port_focus_gained comes with the
   * boot"). No pause ever happens on this console, so NativeOnResume was never
   * delivered once in any run -- the logs have no "focus back" line at all.
   *
   * WHY AFTER THE INIT FRAME AND NOT BEFORE. NativeOnResume itself checks the
   * engine's state word (measured, @0x00dfffb8): `ldr r0,[r0]; cmp r0,#2; bne
   * return` -- with the state still 1 (before the first NativeOnDrawFrame, which
   * is what runs the engine's Init) the call does NOTHING but return. So the
   * engine's own design is: the resume arrives after Init and is dispatched to
   * the modules as a life-cycle message; PhysicalKeyboard::OnLifeCycleFocusGained
   * (@0x00df12ec) then calls its own SetEnabled(true) (vtable[35](1)).
   *
   * WHY IT MATTERS. PhysicalKeyboard::HandleMessage routes the key messages
   * (module id 600, raw ids 0x40008/0x80008) to OnRawKeyDown/OnRawKeyUp, and
   * OnRawKeyDown (@0x00df0ba8) DROPS the key unless vtable[34]() answers 1 --
   * that is the module's IsEnabled, and its constructor writes it 0. A module
   * that is never told about the life cycle therefore swallows every key, which
   * is exactly the "key 0 = 0 down + 0 up" the logs show. This step is that
   * call, delivered as Android delivers it, with the state the engine demands.
   *
   * It also feeds the system's own message down the module tree (the native
   * posts 0x80006/0x20006 to SystemAndroid), which is what a phone's Activity
   * does on every foreground return. */
  {
    const uintptr_t resume = entry(FIFA14_ENTRY_NATIVE_ONRESUME);
    debugPrintf("[boot] 10b: Android's onResume -> NativeOnResume(true) (the life-cycle call "
                "the modules listen for: this is what enables the PhysicalKeyboard, without it "
                "every pad key is dropped)\n");
    if (resume)
      ((void (*)(void *, void *, int))resume)(g_jni_env, fifa14_activity, 1);
    else
      debugPrintf("[boot] 10b: %s is MISSING: the engine's key modules stay disabled\n",
                  FIFA14_ENTRY_NATIVE_ONRESUME);
  }

  debugPrintf("[boot] ==== the boot sequence is done: handing the game its thread "
              "(the frame loop) ====\n");
  /* The input goes up HERE and not earlier: the engine's pointer native checks
   * IsMainLoopAlive() (its state word == 2, set by the init frame) and drops
   * every event that arrives before the loop starts. */
  fifa14_input_init();
  frame_loop(draw);

  /* Android's onPause / onDestroy order. */
  g_booted = 0;
  const uintptr_t destroyed = entry(FIFA14_ENTRY_NATIVE_SURFACE_DESTROYED);
  const uintptr_t pause = entry(FIFA14_ENTRY_NATIVE_ONPAUSE);
  const uintptr_t shutdownjni = entry(FIFA14_ENTRY_SHUTDOWN_JNI);
  debugPrintf("[boot] shutdown: NativeOnSurfaceDestroyed, NativeOnPause, shutdownJNI\n");
  if (destroyed)
    ((void (*)(void *, void *))destroyed)(g_jni_env, jni_singleton(F_ANDROID_RENDERER));
  if (pause)
    ((void (*)(void *, void *))pause)(g_jni_env, fifa14_activity);
  /* Android's onDestroy also runs com/ea/EAMIO/StorageDirectory.Shutdown() ->
   * ShutdownNativeImpl(), the native of the pair step 5b called (a 4-byte
   * `bx lr` in this build; Delegate::Shutdown's DeleteGlobalRef in others). */
  {
    const uintptr_t storage_off = entry(FIFA14_ENTRY_EAMIO_SHUTDOWN);
    if (storage_off)
      ((void (*)(void *, void *))storage_off)(g_jni_env, jni_class(F_EAMIO_STORAGE)->obj);
  }
  if (shutdownjni)
    ((void (*)(void *, void *))shutdownjni)(g_jni_env, fifa14_activity);
  rt_applet_stop();
  debugPrintf("[boot] FIFA 14's boot is over\n");
}
