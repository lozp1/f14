/* fifa14_input.c -- the input: the console's touchscreen and the pad, delivered
 * to the engine on the same native entry Android's Java uses.
 *
 * THE CHAIN, AS MEASURED (not guessed: the APK's classes.dex and the
 * disassembly of libFIFA14.so; the RVAs are the file's own).
 *
 *   On Android, the touchscreen belongs to the Java view. The APK's
 *   com/ea/blast/MotionEventAndroid.java is that view's onTouchEvent:
 *
 *     action        = event.getAction() & 0xff        (0 down, 1 up, 2 move, 3
 *                                                      cancel, 5 pointer-down,
 *                                                      6 pointer-up)
 *     blastModuleId = GetBlastModuleId(event): MotionEvent.getSource() ==
 *                     0x1002 (SOURCE_TOUCHSCREEN) or 0x5002 -> kModuleTypeId
 *                     TouchScreen, 0x100008 -> TouchPad, 0x2002/0x6002/0x4002
 *                     -> Mouse; below SDK 9 always TouchScreen
 *     blastRawMsgId = ActionToBlastRawMsgId(blastModuleId, action)
 *     x, y          = event.getX(i), event.getY(i)    <-- PIXELS OF THE VIEW,
 *                                                         not normalised
 *
 *     then SendMotionEventInternal:
 *       TouchSurfaceAndroid.NativeOnPointerEvent(blastRawMsgId, blastModuleId,
 *                                                event.getPointerId(i), x, y)
 *
 *   Its Java declaration is `protected static native NativeOnPointerEvent
 *   (IIIFF)V` -- four parameters for the five values the call site passes, so
 *   the fifth (y) lands in the extra argument slot. The native reads it
 *   anyway: it sees (in AArch32 softfp order, which is what both the game's
 *   .so and this port are built for)
 *
 *     r0, r1 = JNIEnv*, jclass       (never touched: the method is static and
 *                                     uses no Java object at all)
 *     r2     = the raw message id    -> the dispatch argument
 *     r3     = the module id         -> MessagePointer+12
 *     [sp+48]= the pointer id        -> MessagePointer+28
 *     [sp+52]= x                     -> MessagePointer+20
 *     [sp+56]= y                     -> MessagePointer+24
 *
 *   So this file calls it with a C prototype of exactly that shape
 *   (Fifa14PointerFn below) and with the same five values the Java passes.
 *   Nothing else about the platform event is invented here.
 *
 *   The action codes are the engine's own, read at run time out of its two
 *   instructions each (NativeGetIdRawPointer*): down 0x0006000E, up 0x0008000E,
 *   move 0x0004000E, cancel 0x0000000E; and the touch module's id out of
 *   NativeGetModuleTypeIdTouchScreen: 1000.
 *
 *   Inside the engine, that native does NOTHING but post a message: it first
 *   checks IsMainLoopAlive() (the engine's state word == 2, i.e. after the
 *   first NativeOnDrawFrame) and drops the event when it is not, then
 *   allocates a 32-byte MessagePointer out of the engine's allocator, fills the
 *   five fields above and dispatches it. The engine consumes it during its own
 *   loop, which is the one this port runs on the frame thread, so a pointer
 *   posted from fifa14_input_poll() is serviced by the very next frame.
 *
 *   The message dispatch routes by module id to the TouchScreen module, which
 *   owns an EA::Blast::TouchSurface at +36 and calls
 *   TouchSurface::OnRawTouchDown/Move/Up/Cancel @0x00dfadbc (and the std
 *   variants) with the raw pointer id: that is where the raw id becomes a std
 *   pointer id (a counter, TouchSurface+104) and where the orientation
 *   transform happens. RawToStdPointerId @0x00dfad68 is a map lookup, so ANY
 *   stable value can be a raw id -- the port's pad pointer uses 100/101, well
 *   away from the panel's finger ids.
 *
 *   NOTE ON THE COORDINATE SPACE: OnRawTouchDown uses the coordinates as
 *   PIXELS (its flip is (size - 1) - y with size from TouchSurface::SetSize)
 *   and the Java passes the view's pixels, so what the engine wants is the
 *   view's pixel space -- for this port the 1280x720 the boot hands
 *   NativeOnSurfaceChanged. The Switch's panel reports the handheld screen's
 *   own pixels (libnx: "the touchscreen is the handheld screen, 1280x720, the
 *   same pixels the framebuffer uses"), so the panel needs no scaling at all
 *   unless config.ini changed the window's resolution, in which case the panel
 *   is scaled into the engine's.
 *
 * WHY THIS WAY, AND NOT ANOTHER:
 *   - The engine has no InputQueue to serve: its Java side is a plain View
 *     (com/ea/blast/MotionEventAndroid) whose onTouchEvent calls one static
 *     native. There is no listener to call back (the port's fake
 *     SetOnTouchListener would have nothing to deliver to) and no
 *     dispatchTouchEvent/dispatchGenericMotionEvent on the Java side either.
 *   - The port's own GetGLView() object (DisplayAndroidDelegate) is the GL
 *     view, NOT the input view: on Android those are two objects and the
 *     engine only ever asks MotionEventAndroid for pointers. So registering a
 *     Java-side listener on the port's SurfaceView object would deliver
 *     nothing.
 *   - Calling the engine's own native is therefore the shortest path that adds
 *     no engine knowledge: it is the exact call the game makes on a phone.
 *     Driving EA::Blast::TouchSurface directly would mean re-implementing the
 *     message dispatch AND the module lookup.
 *
 * WHAT THE PAD'S BUTTONS ARE, AND WHY:
 *   - left stick: the port's cursor (the engine's pointer moves with it).
 *   - A: a click -- DOWN when pressed, UP when released (a hold drags, which is
 *     what the engine's UI needs for sliders and for its own virtual joystick).
 *   - B: the second finger (multi-touch pointer 101). The engine's Android
 *     input has a right-mouse-button path too
 *     (Java_com_ea_blast_MouseAndroid_NativeOnMouseEvent @0x00e01bcc, module
 *     Mouse = 2200), but the port's device profile answers TouchScreen = 1 and
 *     TouchPad = 0 and the engine NEVER asks for a mouse (no GetMouseCount
 *     anywhere in the run log, and the port has no handler for it), so a mouse
 *     event would be posted for a module the engine has no reason to have
 *     built. The second finger lands on the module that does exist, and on this
 *     game a second finger is the right-hand button.
 *   - + and -: Android's BACK (MainActivity.NativeOnBackPressed, the port's own
 *     bound entry point) -- the context key the menus use.
 *   - the engine's KEY path is the PhysicalKeyboard module (fifa14_blast_stubs.c
 *     answers GetPhysicalKeyboardCount = 1 so the module is built at all), and
 *     the engine only believes a key once the module is ENABLED: its
 *     OnRawKeyDown (@0x00df0ba8) requires vtable[34]() == 1 after mapping the
 *     key (measured in the .so), and the only enable is the module's
 *     OnLifeCycleFocusGained (@0x00df12ec). fifa14_boot.c's step 10b delivers
 *     Android's onResume for exactly that reason.
 *
 * LOGGING: one line the first time each kind of event is delivered (DOWN, MOVE,
 * UP, CANCEL, KEY) with the coordinates, and a summary every
 * FIFA14_INPUT_SUMMARY_FRAMES frames with the count and the pointer's last
 * state, so the next run's debug.log says whether the engine is being served.
 * The per-frame cost is two service reads and, only on a real event, one call
 * into the engine: nothing is added to the render path.
 *
 * MIT.
 */
#include <string.h>
#include <switch.h>

#include "config.h"
#include "fifa14_boot.h"
#include "fifa14_input.h"
#include "fifa14_jni_tables.h"
#include "fifa14_padact.h"
#include "fifa14_padscope.h"
#include "fifa14_physkbd.h"
#include "jni.h"
#include "rt_cfg.h"
#include "rt_pad.h"
#include "rt_window.h"
#include "so_util.h"
#include "util.h"

/* fifa14_main.c's module (the game), and the JNIEnv the port answers with. */
extern so_module game_mod;
extern void *g_jni_env;

/* ------------------------------------------------------------------ the engine
 * The one entry Android's Java calls for every touch (see the header), and the
 * engine's own constants for it. The C prototype is the native's own argument
 * layout, which the Java declaration only half describes: two words in r2/r3,
 * three on the stack. */
typedef void (*Fifa14PointerFn)(void *env, void *cls, int msg, int module, int pointer, float x,
                                float y);
/* The four constants natives: they read no argument at all, but they are JNI
 * natives, so they are called with (env, cls) all the same. */
typedef int (*Fifa14ConstFn)(void *env, void *cls);

/* The engine's KEY entry (the pad's half of the same class): the argument
 * layout the native actually reads is r2 = module id, r3 = the Android
 * keycode, and the alt flag in the stack slot its Java declaration has no
 * parameter for. The whole chain is in the header, below. */
typedef void (*Fifa14KeyFn)(void *env, void *cls, int module, int androidKey, int alt);

#define FIFA14_PTR_EVENT_SYM "Java_com_ea_blast_TouchSurfaceAndroid_NativeOnPointerEvent"
#define FIFA14_PTR_DOWN_SYM "Java_com_ea_blast_TouchSurfaceAndroid_NativeGetIdRawPointerDown"
#define FIFA14_PTR_UP_SYM "Java_com_ea_blast_TouchSurfaceAndroid_NativeGetIdRawPointerUp"
#define FIFA14_PTR_MOVE_SYM "Java_com_ea_blast_TouchSurfaceAndroid_NativeGetIdRawPointerMove"
#define FIFA14_PTR_CANCEL_SYM "Java_com_ea_blast_TouchSurfaceAndroid_NativeGetIdRawPointerCancel"
#define FIFA14_PTR_MODULE_SYM "Java_com_ea_blast_ModuleCatalog_NativeGetModuleTypeIdTouchScreen"

/* The measured values, used only when the symbol lookup above fails (they are
 * what the natives return: read out of their 8/12 bytes). */
#define FIFA14_PTR_DOWN_FALLBACK 0x0006000E
#define FIFA14_PTR_UP_FALLBACK 0x0008000E
#define FIFA14_PTR_MOVE_FALLBACK 0x0004000E
#define FIFA14_PTR_CANCEL_FALLBACK 0x0000000E
#define FIFA14_PTR_MODULE_FALLBACK 1000

/* The panel reports the handheld screen: 1280x720, the engine's own space. */
#define FIFA14_PANEL_W 1280
#define FIFA14_PANEL_H 720

/* The pad's own raw pointer ids: the engine's RawToStdPointerId is a map
 * lookup, so any stable value works as long as it is not a panel finger id. */
#define FIFA14_PAD_RAW_A 100u
#define FIFA14_PAD_RAW_B 101u

/* One held finger, as read off the panel. */
typedef struct {
  int used;
  uint32_t raw; /* the panel's finger id: the engine's raw pointer id */
  float x, y;   /* the last position delivered, in the engine's pixels */
} Fifa14Finger;

/* The panel reports 16 fingers; four is plenty (and keeps the per-frame scan
 * and the table small). */
#define FIFA14_INPUT_MAX_FINGERS 4

typedef struct {
  int ready;
  int trace; /* log every event, not only the first of each kind (config.ini) */

  /* the engine's entry point and its constants */
  Fifa14PointerFn ptr;
  int id_down, id_up, id_move, id_cancel, module;
  int ids_from_engine; /* the four codes came from the natives, not fallbacks */

  /* the engine's KEY entry (the pad), and the id of the module it lands on */
  Fifa14KeyFn key_down, key_up;
  int key_module;
  int keys_from_engine; /* both natives resolved, so the pad can reach the engine */
  int keys_logged_map;  /* the one-line map has been written */

  int panel; /* the touchscreen came up */
  Fifa14Finger fingers[FIFA14_INPUT_MAX_FINGERS];

  int pad; /* the pad is up */
  PadState padstate;
  uint64_t buttons; /* the last raw button word (logged when it changes) */
  uint64_t keys_held; /* the button bits already delivered as keys (the edges) */
  unsigned stick_dpad_on; /* the bits of k_stick_dpad the left stick is holding */
  int a_down, b_down;
  float cx, cy; /* the pad's cursor, in the engine's pixels */

  /* the log's bookkeeping */
  unsigned n_ev, n_down, n_move, n_up, n_cancel, n_key;
  unsigned n_key_down, n_key_up; /* the two halves of n_key */
  /* THE ROUTED PATH'S OWN COUNTERS (2026-10-13, with the module found and
   * enabled): the keys delivered the Android way, through the engine's own
   * KeyboardAndroid natives and its message routing. "direct calls" (the
   * module's own, in fifa14_physkbd.c) and "routed calls" are printed next to
   * each other so one run says which path the game answered to. */
  unsigned n_routed, n_routed_down, n_routed_up;
  int logged_key, logged_key_routed, logged_no_path;
  int logged_move, logged_cancel;
  int logged_down, logged_up;
  int logged_buttons;
  /* THE PAD'S OWN PROOF (added 2026-10-13, after a whole round in which the
   * log said "key 0 = 0 down + 0 up" and nothing said WHY): how many times the
   * pad was read, the largest raw button word ever seen, and whether the pad
   * has EVER reported a button. A port whose mask does not match the id the
   * console reports under reads 0 forever, and this is the line that says so. */
  unsigned n_pad_samples;
  uint64_t pad_max_buttons;
  int pad_saw_a_button;
  int logged_mask;
  uint64_t frames;

  /* THE PAD'S KEY SCOPE (2026-10-13, config.ini [controls] pad_keys_scope,
   * fifa14_padscope.c): 1 = "match" -- the pad's KEYS are delivered only while
   * the engine has a live game object. scope_open is the gate's last decision,
   * scope_edges the last raw button word (so a press made while the gate was
   * closed is not delivered late when it opens), n_withheld the key edges that
   * were held back. The touch layer and +/- (Android BACK) are never scoped. */
  int scope_match;
  int scope_open;
  int scope_logged;
  uint64_t scope_edges;
  unsigned n_withheld;
  int last_msg, last_pointer; /* the pointer's last delivered state */
  float last_x, last_y;
  int held; /* pointers the engine has been told are down */

  int size_w, size_h; /* the engine's surface: what it was told, 1280x720 */
} Fifa14Input;

static Fifa14Input s_in;

/* ------------------------------------------------------------------ the call */

/* The one place the engine is called. The native posts a message the engine's
 * loop consumes in the frame it is in. */
static void deliver(const char *kind, int msg, int pointer, float x, float y) {
  if (!s_in.ptr || !msg)
    return;
  s_in.ptr(g_jni_env, NULL, msg, s_in.module, pointer, x, y);
  s_in.n_ev++;
  s_in.last_msg = msg;
  s_in.last_pointer = pointer;
  s_in.last_x = x;
  s_in.last_y = y;
  const int px = (int)(x + 0.5f), py = (int)(y + 0.5f);
  int *logged = NULL;
  if (msg == s_in.id_down) {
    s_in.n_down++;
    logged = &s_in.logged_down;
  } else if (msg == s_in.id_up) {
    s_in.n_up++;
    logged = &s_in.logged_up;
  } else if (msg == s_in.id_move) {
    s_in.n_move++;
    logged = &s_in.logged_move;
  } else if (msg == s_in.id_cancel) {
    s_in.n_cancel++;
    logged = &s_in.logged_cancel;
  }
  if (logged && !*logged) {
    *logged = 1;
    debugPrintf("[input] %s (the first one): %s(pointer %d, x %d, y %d) -- module %d, "
                "the engine's raw message id 0x%05x\n",
                kind, kind, pointer, px, py, s_in.module, (unsigned)msg);
  } else if (s_in.trace) {
    debugPrintf("[input] %s pointer %d at %d,%d\n", kind, pointer, px, py);
  }
}

/* THE PAD'S ACTIONS IN A MATCH (fifa14_padact.c): a touch on one of the
 * engine's own on-screen buttons, sent through the same native the panel
 * uses. `raw_id` is the bridge's own pointer id (120+button), `down` picks the
 * engine's down/up action code. Logged once per button, then only with
 * [input] trace. */
static int s_act_logged[8];
static unsigned s_n_act_down, s_n_act_up;

void fifa14_input_inject_touch(int raw_id, int down, float x, float y) {
  if (!s_in.ptr || !s_in.id_down || !s_in.id_up)
    return;
  s_in.ptr(g_jni_env, NULL, down ? s_in.id_down : s_in.id_up, s_in.module, raw_id, x, y);
  s_in.n_ev++;
  s_in.last_msg = down ? s_in.id_down : s_in.id_up;
  s_in.last_pointer = raw_id;
  s_in.last_x = x;
  s_in.last_y = y;
  if (down)
    s_n_act_down++;
  else
    s_n_act_up++;
  const int slot = raw_id - 120;
  if (slot >= 0 && slot < (int)(sizeof s_act_logged / sizeof s_act_logged[0]) &&
      (!s_act_logged[slot] || s_in.trace)) {
    s_act_logged[slot] = 1;
    debugPrintf("[input] the pad's ACTION touch (pointer %d): %s at %d,%d -- the engine's own "
                "on-screen button, through the same native the panel uses\n",
                raw_id, down ? "DOWN" : "UP", (int)(x + 0.5f), (int)(y + 0.5f));
  }
}

/* Android's BACK: the engine's own entry point (the port bound it in the boot),
 * called the way the boot calls onPause: (env, activity). */
static void send_back(void) {
  void *fn = fifa14_native_addr(FIFA14_ENTRY_NATIVE_ONBACK);
  if (!fn)
    return;
  s_in.n_key++;
  if (!s_in.logged_key) {
    s_in.logged_key = 1;
    debugPrintf("[input] KEY (the first one): Android's BACK -> MainActivity.NativeOnBackPressed\n");
  } else if (s_in.trace) {
    debugPrintf("[input] KEY: Android's BACK\n");
  }
  ((void (*)(void *, void *))fn)(g_jni_env, fifa14_activity);
}

/* ------------------------------------------------------------------ the touch
 * The console's panel, straight through: libnx reports the handheld screen's
 * own pixels (0..1280 x 0..720) and that is the space the engine's TouchSurface
 * is sized in, so this is a scale only when config.ini changed the window. */
static void poll_touchscreen(void) {
  HidTouchScreenState st;
  uint32_t seen[FIFA14_INPUT_MAX_FINGERS];
  int nseen = 0, count;
  if (!s_in.panel)
    return;
  memset(&st, 0, sizeof st);
  if (hidGetTouchScreenStates(&st, 1) < 1)
    return;
  count = st.count;
  if (count > (int)FIFA14_INPUT_MAX_FINGERS)
    count = (int)FIFA14_INPUT_MAX_FINGERS;
  for (int i = 0; i < count; i++) {
    const HidTouchState *t = &st.touches[i];
    const uint32_t raw = t->finger_id;
    const float x = (float)t->x * (float)s_in.size_w / (float)FIFA14_PANEL_W;
    const float y = (float)t->y * (float)s_in.size_h / (float)FIFA14_PANEL_H;
    Fifa14Finger *f = NULL;
    int free_slot = -1;
    if (x < 0.0f || y < 0.0f || x > (float)(s_in.size_w - 1) || y > (float)(s_in.size_h - 1))
      continue; /* off the window: not a touch as far as the engine's view is concerned */
    for (int j = 0; j < (int)FIFA14_INPUT_MAX_FINGERS; j++) {
      if (s_in.fingers[j].used && s_in.fingers[j].raw == raw)
        f = &s_in.fingers[j];
      else if (!s_in.fingers[j].used && free_slot < 0)
        free_slot = j;
    }
    if (!f) {
      if (free_slot < 0)
        continue; /* more fingers than the table: the extra ones are dropped */
      f = &s_in.fingers[free_slot];
      f->used = 1;
      f->raw = raw;
      f->x = x;
      f->y = y;
      s_in.held++;
      deliver("DOWN", s_in.id_down, (int)raw, x, y);
    } else if ((int)x != (int)f->x || (int)y != (int)f->y) {
      f->x = x;
      f->y = y;
      deliver("MOVE", s_in.id_move, (int)raw, x, y);
    }
    seen[nseen++] = raw;
  }
  /* A finger that was down and is not in this sample has lifted. */
  for (int j = 0; j < (int)FIFA14_INPUT_MAX_FINGERS; j++) {
    Fifa14Finger *f = &s_in.fingers[j];
    int held = 0;
    if (!f->used)
      continue;
    for (int k = 0; k < nseen; k++)
      held += seen[k] == f->raw;
    if (held)
      continue;
    deliver("UP", s_in.id_up, (int)f->raw, f->x, f->y);
    f->used = 0;
    if (s_in.held)
      s_in.held--;
  }
}

/* The touchscreen is the applet's (handheld only): hide it when it is not
 * there instead of aborting the way libnx's hidInitializeTouchScreen does. */
static Result activate_touchscreen(void) {
  const u64 aruid = appletGetAppletResourceUserId();
  return serviceDispatchIn(hidGetServiceSession(), 11, aruid, .in_send_pid = true);
}

/* ==================================================== THE PAD IS A GAMEPAD ====
 *
 * THE MEASURED CHAIN (the APK's classes.dex + the disassembly of libFIFA14.so;
 * nothing here is guessed):
 *
 *   MainActivity.onKeyDown(keyCode, event)   (com/ea/blast/MainActivity.java)
 *     -> mPhysicalKeyboard.OnKeyDown(keyCode, event)
 *        (PhysicalKeyboardAndroid extends KeyboardAndroid)
 *        IsSystemKey(keyCode)? 3,5,6,0x18,0x19,0x1a,0x1b,0x5b are the system
 *        keys; repeatCount 0 only (no auto-repeat); isAltPressed() is passed.
 *        -> KeyboardAndroid.NativeOnKeyDown(GetModuleId(), keyCode, alt)
 *           GetModuleId() = ModuleCatalog.kModuleTypeIdPhysicalKeyboard
 *   MainActivity.onKeyUp -> mPhysicalKeyboard.OnKeyUp -> NativeOnKeyUp(same).
 *
 *   `private native NativeOnKeyDown(III)V` -- but the call site passes
 *   (this, moduleId, androidKey, alt): FOUR values for three parameters, so the
 *   alt flag lands in the extra argument slot, exactly like the pointer native.
 *   The native reads it anyway (@0x00e0c498, 220 B, read instruction by
 *   instruction):
 *
 *     r0, r1 = JNIEnv*, jobject      (never touched)
 *     r2     = moduleId              -> MessageKeyData+12
 *     r3     = androidKey            -> MessageKeyData+20
 *     [sp+40]= altPressed            -> MessageKeyData+24
 *   then dispatches it with a HARDCODED raw message id: 0x00040008 for
 *   NativeOnKeyDown (8 | 262144) and 0x00080008 for NativeOnKeyUp (8 |
 *   524288). (: "0x40000 = set/down, 0x80000 = clear/up" for the key class 8;
 *   the pointer class 0xE uses 0x60000/0x80000/0x40000 instead.)
 *   NativeOnCharacter(III) is the same shape with id 3; visibility uses 9.
 *
 *   PhysicalKeyboard::HandleMessage (@0x00df0864) routes on the message's
 *   +12 == 600 and then on the id: 0x40008 -> OnRawKeyDown(rawKey, alt),
 *   0x80008 -> OnRawKeyUp, 0x20008 -> CancelPressedKeys.
 *   OnRawKeyDown (@0x00df0ba8) calls the mapper it owns at +36 and drops the
 *   event when the mapped key is 0 -- and it first requires IsEnabled() (byte
 *   +0x90). The constructor writes that byte 0, so NOTHING works until the
 *   module is enabled: PhysicalKeyboard::OnLifeCycleFocusGained (@0x00df12ec)
 *   is what enables it (`this->vtable[140](1)` = SetEnabled(true)), and
 *   PhysicalKeyboard::Init ends by calling it when the system's life-cycle call
 *   answers 5. THE ENABLE IS AUTOMATIC -- but only for a module that EXISTS,
 *   and AllocPhysicalKeyboardModule (@0x00df13e4) refuses to build one when the
 *   platform's count is < 1. That is why this port now answers
 *   GetPhysicalKeyboardCount = 1 (fifa14_blast_stubs.c): the answer was 0 and
 *   the module was never built, so the engine had nowhere to put a key.
 *
 *   THE MAPPER: KeyboardAndroid::RawToStdKey(rawKey, alt) (@0x00e0c448, the
 *   vtable slot the mapper object is built from) is, literally:
 *
 *     if (raw -  7 <=  9u) return raw + 41;   // KEYCODE_0..9   -> '0'..'9'
 *     if (raw - 29 <= 25u) return raw + 68;   // KEYCODE_A..Z   -> 'a'..'z'
 *     if (raw > 164)       return 0;          // not a key the engine knows
 *     return kRawToStdKey[raw];               // a 165-entry u32 table
 *
 *   and the table (165 u32 at file offset 0x0107ae60, read out of the .so) has,
 *   for the gamepad codes Android uses, these EXACT values -- this is the
 *   engine's answer to "does it know a gamepad", and it does, first-class:
 *
 *     KEYCODE_BUTTON_A 96 -> 0xF026      BUTTON_L1  102 -> 0xF022
 *     KEYCODE_BUTTON_B 97 -> 0xF027      BUTTON_R1  103 -> 0xF024
 *     KEYCODE_BUTTON_C 98 -> 0xF028      BUTTON_L2  104 -> 0xF023
 *     KEYCODE_BUTTON_X 99 -> 0xF029      BUTTON_R2  105 -> 0xF025
 *     BUTTON_Y        100 -> 0xF02A      BUTTON_THUMBL 106 -> 0xF030
 *     BUTTON_Z        101 -> 0xF02B      BUTTON_THUMBR 107 -> 0xF031
 *     BUTTON_START    108 -> 0xF032      BUTTON_SELECT 109 -> 0xF033
 *     BUTTON_MODE     110 -> 0xF034
 *     DPAD_UP 19 -> 0x16  DPAD_DOWN 20 -> 0x17  DPAD_LEFT 21 -> 0x18
 *     DPAD_RIGHT 22 -> 0x19  DPAD_CENTER 23 -> 0x1A
 *     BACK 4 -> 0x10   HOME 3 -> 0x0F   MENU 82 -> 0x0E   ENTER 66 -> 0x0D
 *     DEL 67 -> 0x08   ESCAPE 111 -> 0x1B   TAB 61 -> 0x09   SPACE 62 -> 0x20
 *
 *   So the engine's gamepad support IS this module: an Android gamepad is a
 *   "physical keyboard" whose keycodes happen to be BUTTON_*. (Keyboard-
 *   AndroidXperiaPlay::RawToStdKey narrows it further for that one phone:
 *   DPAD_CENTER -> 0xF02D, BUTTON_X -> 0xF02E, BUTTON_Y -> 0xF02F.)
 *
 * NO AXES, AND THAT IS MEASURED TOO: the APK's MotionEventAndroid.
 * onGenericMotionEvent() -- the only place a joystick's axes could enter --
 * does nothing but `getSource() & 2`, a packed-switch on action 7/8, a 12 ms
 * sleep and `return true`. It never calls a native and there is NO
 * NativeOnMotion/NativeOnAxis anywhere in libFIFA14.so; ModuleCatalog has no
 * joystick/analog module and no AXIS_* constant is referenced by the game.
 * So an analog stick can only reach the engine as the digital D-pad: the left
 * stick below is turned into DPAD_UP/DOWN/LEFT/RIGHT by a threshold with
 * hysteresis. ZL/ZR have no analog path either -- they go in as BUTTON_L2/R2,
 * which is what the engine's table knows them as.
 *
 * WHAT IS WHERE (nothing that worked is lost):
 *   - A/B/X/Y, L/R, ZL/ZR, the D-pad, +, - and the two stick presses become
 *     the engine's own keys (the table above).
 *   - the left stick is the D-pad as well (it duplicates the cross).
 *   - the RIGHT stick is still the touch cursor, StickR its click and StickL
 *     the second finger: screens that only answer to the screen (the terms
 *     dialog among them) stay reachable with the pad, and the console's own
 *     panel is untouched.
 */
static float axis(float v) {
  const float dz = FIFA14_INPUT_CURSOR_DEADZONE;
  if (v > -dz && v < dz)
    return 0.0f;
  v = v > 0.0f ? (v - dz) / (1.0f - dz) : (v + dz) / (1.0f - dz);
  return v * (v < 0.0f ? -v : v);
}

/* The engine's key entry, called exactly the way Android's Java calls it. (The
 * native's own argument layout: r2 = module, r3 = the Android keycode, and the
 * one flag the Java declaration has no parameter for in the stack slot -- see
 * this file's header.) */
#define FIFA14_KEY_DOWN_SYM "Java_com_ea_blast_KeyboardAndroid_NativeOnKeyDown"
#define FIFA14_KEY_UP_SYM "Java_com_ea_blast_KeyboardAndroid_NativeOnKeyUp"
#define FIFA14_KEY_MODULE_SYM \
  "Java_com_ea_blast_ModuleCatalog_NativeGetModuleTypeIdPhysicalKeyboard"
/* What NativeGetModuleTypeIdPhysicalKeyboard returns, read out of its 8 bytes
 * (`mov r0,#600`), for when the symbol lookup fails. */
#define FIFA14_KEY_MODULE_FALLBACK 600

/* Android's KeyEvent codes, and what KeyboardAndroid::RawToStdKey makes of them
 * (the table in this file's header). The Android names are the contract the
 * engine's Java side speaks, so they are what goes in. */
#define FIFA14_K_A 96
#define FIFA14_K_B 97
#define FIFA14_K_X 99
#define FIFA14_K_Y 100
#define FIFA14_K_L1 102
#define FIFA14_K_R1 103
#define FIFA14_K_L2 104 /* the analog triggers: there is no axis to send them on */
#define FIFA14_K_R2 105
#define FIFA14_K_START 108
#define FIFA14_K_SELECT 109
#define FIFA14_K_DUP 19
#define FIFA14_K_DDOWN 20
#define FIFA14_K_DLEFT 21
#define FIFA14_K_DRIGHT 22

/* One pad button: the bit libnx reports, the Android keycode, and the label the
 * log uses. The left stick is not here (it is a threshold, below). */
typedef struct {
  uint64_t bit;
  int code;
  const char *what;
} Fifa14KeyDef;

static const Fifa14KeyDef k_keys[] = {
    {HidNpadButton_A, FIFA14_K_A, "A -> KEYCODE_BUTTON_A -> the engine's key 0xF026"},
    {HidNpadButton_B, FIFA14_K_B, "B -> KEYCODE_BUTTON_B -> 0xF027"},
    {HidNpadButton_X, FIFA14_K_X, "X -> KEYCODE_BUTTON_X -> 0xF029"},
    {HidNpadButton_Y, FIFA14_K_Y, "Y -> KEYCODE_BUTTON_Y -> 0xF02A"},
    {HidNpadButton_L, FIFA14_K_L1, "L -> KEYCODE_BUTTON_L1 -> 0xF022"},
    {HidNpadButton_R, FIFA14_K_R1, "R -> KEYCODE_BUTTON_R1 -> 0xF024"},
    {HidNpadButton_ZL, FIFA14_K_L2, "ZL -> KEYCODE_BUTTON_L2 -> 0xF023"},
    {HidNpadButton_ZR, FIFA14_K_R2, "ZR -> KEYCODE_BUTTON_R2 -> 0xF025"},
    {HidNpadButton_Plus, FIFA14_K_START, "+ -> KEYCODE_BUTTON_START -> 0xF032"},
    {HidNpadButton_Minus, FIFA14_K_SELECT, "- -> KEYCODE_BUTTON_SELECT -> 0xF033"},
    {HidNpadButton_Up, FIFA14_K_DUP, "the cross up -> KEYCODE_DPAD_UP -> 0x16"},
    {HidNpadButton_Down, FIFA14_K_DDOWN, "the cross down -> KEYCODE_DPAD_DOWN -> 0x17"},
    {HidNpadButton_Left, FIFA14_K_DLEFT, "the cross left -> KEYCODE_DPAD_LEFT -> 0x18"},
    {HidNpadButton_Right, FIFA14_K_DRIGHT, "the cross right -> KEYCODE_DPAD_RIGHT -> 0x19"},
};
#define FIFA14_KEY_COUNT (int)(sizeof k_keys / sizeof k_keys[0])

/* Every bit k_keys[] covers: the pad's KEYS, and nothing else (the two stick
 * presses and the right stick belong to the touch layer). */
#define FIFA14_KEY_BITS                                                                             \
  (HidNpadButton_A | HidNpadButton_B | HidNpadButton_X | HidNpadButton_Y | HidNpadButton_L |        \
   HidNpadButton_R | HidNpadButton_ZL | HidNpadButton_ZR | HidNpadButton_Plus |                     \
   HidNpadButton_Minus | HidNpadButton_Up | HidNpadButton_Down | HidNpadButton_Left |               \
   HidNpadButton_Right)

/* How far the left stick has to go for a D-pad direction, and how far back for
 * it to let go (the gap is the hysteresis: a stick resting on the threshold
 * must not chatter). */
#define FIFA14_STICK_DPAD_ON 0.55f
#define FIFA14_STICK_DPAD_OFF 0.38f

/* The four D-pad directions the STICK produces, as the same keycodes the cross
 * uses. The two sources are OR-ed, so holding both changes nothing. */
typedef struct {
  int code;
  const char *what;
  int axis;  /* which stick entry: 0 = x, 1 = -y */
  int sign;  /* which way */
} Fifa14StickDpad;

static const Fifa14StickDpad k_stick_dpad[4] = {
    {FIFA14_K_DUP, "the left stick up -> KEYCODE_DPAD_UP", 1, +1},
    {FIFA14_K_DDOWN, "the left stick down -> KEYCODE_DPAD_DOWN", 1, -1},
    {FIFA14_K_DLEFT, "the left stick left -> KEYCODE_DPAD_LEFT", 0, -1},
    {FIFA14_K_DRIGHT, "the left stick right -> KEYCODE_DPAD_RIGHT", 0, +1},
};

/* One call into the engine, and the counters. `what` is only for the log.
 *
 * TWO PATHS (2026-10-13, the round in which the module was found, identified
 * by its table's entries and ENABLED). The Android way is KeyboardAndroid's
 * native: the engine posts the 0x00040008 message and its ModuleManager routes
 * it to the PhysicalKeyboard module -- which now exists and is enabled, so the
 * routing has a destination for the first time. The direct way
 * (FIFA14_INPUT_KEY_DIRECT, fifa14_physkbd.c) is the same call the dispatch
 * would end in (the module's OnRawKeyDown/OnRawKeyUp), made by the port, with
 * the routing taken out. Each path has its own counter and its own first-key
 * line, so one run says what each delivered; both are on by default (the
 * experiment, and it is SAFE: the engine's own OnRawKeyDown @0x00df0ba8
 * inserts the key into the pressed set and returns early when it was already
 * there -- `df0bfc ldrb r0,[sp,#16]; df0c00 cmp r0,#0; beq <return>` -- so a
 * key arriving twice notifies the game once). Either path is a compile-time
 * switch in source/fifa14_physkbd.h (FIFA14_INPUT_KEY_DIRECT /
 * FIFA14_INPUT_KEY_ROUTED: config.ini has no row for them). */
static void deliver_key(int down, int code, const char *what) {
  int delivered = 0;
#if FIFA14_INPUT_KEY_DIRECT
  if (fifa14_physkbd_ready()) {
    fifa14_physkbd_raw_key(down, code, 0);
    delivered++;
    s_in.n_key++;
    if (down)
      s_in.n_key_down++;
    else
      s_in.n_key_up++;
    if (!s_in.logged_key) {
      s_in.logged_key = 1;
      debugPrintf("[input] KEY (the first one): %s -- the DIRECT call on the engine's "
                  "PhysicalKeyboard module (FIFA14_INPUT_KEY_DIRECT %d); no native, no "
                  "message, no routing\n",
                  what, (int)FIFA14_INPUT_KEY_DIRECT);
    } else if (s_in.trace) {
      debugPrintf("[input] KEY %s (%s) direct\n", down ? "down" : "up", what);
    }
  }
#endif
#if FIFA14_INPUT_KEY_ROUTED
  {
    Fifa14KeyFn fn = down ? s_in.key_down : s_in.key_up;
    if (fn) {
      fn(g_jni_env, NULL, s_in.key_module, code, 0);
      delivered++;
      s_in.n_routed++;
      if (down)
        s_in.n_routed_down++;
      else
        s_in.n_routed_up++;
      if (!s_in.logged_key_routed) {
        s_in.logged_key_routed = 1;
        debugPrintf("[input] KEY (the first one on the ENGINE'S OWN PATH): %s -- module %d, %s, "
                    "alt 0 (FIFA14_INPUT_KEY_ROUTED %d): the engine posts it and its "
                    "ModuleManager routes it to the module\n",
                    what, s_in.key_module,
                    down ? "0x00040008, the id the native itself dispatches" : "0x00080008",
                    (int)FIFA14_INPUT_KEY_ROUTED);
      } else if (s_in.trace) {
        debugPrintf("[input] KEY %s (%s) routed\n", down ? "down" : "up", what);
      }
    }
  }
#endif
  if (!delivered) {
    /* Neither path is live: say it once, so a log with keys that go nowhere
     * names the reason instead of staying silent. */
    if (!s_in.logged_no_path) {
      s_in.logged_no_path = 1;
      debugPrintf("[input] the pad's keys have NO path into the engine right now "
                  "(FIFA14_INPUT_KEY_DIRECT %d, FIFA14_INPUT_KEY_ROUTED %d, the module %s, the "
                  "natives %s): the buttons are read and dropped\n",
                  (int)FIFA14_INPUT_KEY_DIRECT, (int)FIFA14_INPUT_KEY_ROUTED,
                  fifa14_physkbd_ready() ? "is in hand" : "is NOT in hand",
                  s_in.keys_from_engine ? "resolved" : "are MISSING");
    }
  }
}

/* The pad's buttons, by edge: every change is one call into the engine. */
static void poll_pad_keys(uint64_t buttons) {
  for (int i = 0; i < FIFA14_KEY_COUNT; i++) {
    const uint64_t on = (buttons & k_keys[i].bit) != 0;
    const uint64_t was = (s_in.keys_held & k_keys[i].bit) != 0;
    if (on == was)
      continue;
    deliver_key((int)on, k_keys[i].code, k_keys[i].what);
  }
  s_in.keys_held = buttons & FIFA14_KEY_BITS;
}

/* THE SCOPE'S RELEASE: when the key gate closes with a key held down, the
 * engine must be told the key went up -- the same thing Android's Activity
 * gets as its window goes away. The release is delivered even while the gate
 * is closed (a stuck key is worse than a withheld one). */
static void release_pad_keys(void) {
  for (int i = 0; i < FIFA14_KEY_COUNT; i++) {
    if (!(s_in.keys_held & k_keys[i].bit))
      continue;
    deliver_key(0, k_keys[i].code, k_keys[i].what);
  }
  s_in.keys_held = 0;
  for (int i = 0; i < 4; i++) {
    if (!((s_in.stick_dpad_on >> i) & 1))
      continue;
    deliver_key(0, k_stick_dpad[i].code, k_stick_dpad[i].what);
  }
  s_in.stick_dpad_on = 0;
}

/* ================================================== THE KEY GATE (the scope) ==
 * config.ini [controls] pad_keys_scope = match | always (fifa14_config.c).
 *
 *   always -- exactly what this port has always done: every key, every frame.
 *   match  -- the pad's KEYS go in only while the engine has a live game
 *             object (fifa14_padscope.c: the engine's own pointer, verified
 *             before it is read). While it does not, every key edge is held
 *             back and counted, and anything held is released.
 *
 * The gate is FAIL-OPEN by construction: fifa14_padscope_match() answers -1
 * when the chain could not be verified, and -1 opens the gate. The touch layer
 * (right stick cursor, StickR/StickL) and +/- (Android's BACK) are NOT scoped:
 * the menus are worked with those. Returns 1 when the keys may go in. */
static int key_gate(uint64_t buttons, const float *sticks) {
  const int live = fifa14_padscope_match(); /* 1, 0, or -1 (unknown) */
  const int open = !s_in.scope_match || live != 0;
  if (open)
    return 1;
  /* no game: count the edges that would have been delivered, then hold them */
  for (int i = 0; i < FIFA14_KEY_COUNT; i++) {
    if ((buttons & k_keys[i].bit) && !(s_in.scope_edges & k_keys[i].bit))
      s_in.n_withheld++;
  }
  for (int i = 0; i < 4; i++) {
    const float v = k_stick_dpad[i].sign > 0 ? sticks[k_stick_dpad[i].axis]
                                             : -sticks[k_stick_dpad[i].axis];
    if (!((s_in.stick_dpad_on >> i) & 1) && v > FIFA14_STICK_DPAD_ON)
      s_in.n_withheld++;
  }
  release_pad_keys();
  s_in.keys_held = buttons & FIFA14_KEY_BITS; /* held through the gate: no late edge */
  return 0;
}

/* The left stick as the D-pad, with hysteresis. What comes out is the same four
 * keycodes the cross sends, so the engine sees one D-pad with two sources. */
static void poll_pad_stick_dpad(const float *sticks) {
  for (int i = 0; i < 4; i++) {
    const float v = k_stick_dpad[i].sign > 0 ? sticks[k_stick_dpad[i].axis]
                                            : -sticks[k_stick_dpad[i].axis];
    const int on = (s_in.stick_dpad_on >> i) & 1;
    if (!on && v > FIFA14_STICK_DPAD_ON) {
      s_in.stick_dpad_on |= 1u << i;
      deliver_key(1, k_stick_dpad[i].code, k_stick_dpad[i].what);
    } else if (on && v < FIFA14_STICK_DPAD_OFF) {
      s_in.stick_dpad_on &= ~(1u << i);
      deliver_key(0, k_stick_dpad[i].code, k_stick_dpad[i].what);
    }
  }
}

/* The touch layer keeps a pad path of its own, on the parts of the pad that have
 * no game function: the right stick is the cursor (nothing in the engine reads a
 * second stick), StickR the click and StickL the second finger. So a screen that
 * only answers to the screen is still reachable without letting go of the pad. */
static void poll_pad_touch(uint64_t buttons, const float *sticks) {
  const float dx = axis(sticks[2]) * FIFA14_INPUT_CURSOR_SPEED;
  const float dy = axis(sticks[3]) * FIFA14_INPUT_CURSOR_SPEED; /* sticks are y-up */
  int moved = 0;
  if (dx != 0.0f || dy != 0.0f) {
    s_in.cx += dx;
    s_in.cy -= dy; /* the engine's y grows downwards */
    if (s_in.cx < 0.0f)
      s_in.cx = 0.0f;
    if (s_in.cy < 0.0f)
      s_in.cy = 0.0f;
    if (s_in.cx > (float)(s_in.size_w - 1))
      s_in.cx = (float)(s_in.size_w - 1);
    if (s_in.cy > (float)(s_in.size_h - 1))
      s_in.cy = (float)(s_in.size_h - 1);
    moved = 1;
  }
  /* A drag: the engine only knows the pointer that is down. */
  if (moved && s_in.a_down)
    deliver("MOVE", s_in.id_move, FIFA14_PAD_RAW_A, s_in.cx, s_in.cy);
  if (moved && s_in.b_down)
    deliver("MOVE", s_in.id_move, FIFA14_PAD_RAW_B, s_in.cx, s_in.cy);

  /* StickR: the click on the cursor (StickR is NOT one of the engine's keys). */
  if ((buttons & HidNpadButton_StickR) && !s_in.a_down) {
    s_in.a_down = 1;
    s_in.held++;
    deliver("DOWN", s_in.id_down, FIFA14_PAD_RAW_A, s_in.cx, s_in.cy);
  } else if (!(buttons & HidNpadButton_StickR) && s_in.a_down) {
    s_in.a_down = 0;
    deliver("UP", s_in.id_up, FIFA14_PAD_RAW_A, s_in.cx, s_in.cy);
    if (s_in.held)
      s_in.held--;
  }

  /* StickL: the second finger (see the file's header for why not the mouse). */
  if ((buttons & HidNpadButton_StickL) && !s_in.b_down) {
    s_in.b_down = 1;
    s_in.held++;
    deliver("DOWN", s_in.id_down, FIFA14_PAD_RAW_B, s_in.cx, s_in.cy);
  } else if (!(buttons & HidNpadButton_StickL) && s_in.b_down) {
    s_in.b_down = 0;
    deliver("UP", s_in.id_up, FIFA14_PAD_RAW_B, s_in.cx, s_in.cy);
    if (s_in.held)
      s_in.held--;
  }
}

static void poll_pad(void) {
  float sticks[4];
  uint64_t raw, buttons;
  if (!s_in.pad)
    return;
  padUpdate(&s_in.padstate);
  buttons = rt_pad_read(&s_in.padstate, sticks);
  raw = padGetButtons(&s_in.padstate);
  s_in.n_pad_samples++;
  if (raw) {
    s_in.pad_saw_a_button = 1;
    if (raw > s_in.pad_max_buttons)
      s_in.pad_max_buttons = raw;
  }
  if (raw != s_in.buttons) {
    /* The raw word, whenever it changes: the cheapest proof the pad is read at
     * all (a controller layout the port never declared reads 0 forever). */
    s_in.buttons = raw;
    s_in.logged_buttons++;
    if (s_in.logged_buttons <= 8 || s_in.trace)
      debugPrintf("[input] pad buttons -> 0x%llx (%s)\n", (unsigned long long)raw,
                  rt_pad_style_name(padGetStyleSet(&s_in.padstate)));
  }

  /* THE GAMEPAD: every button and the left stick go in as the engine's own
   * keys; the right stick and the two stick presses keep the touch layer
   * reachable (this file's header has the whole chain).
   *
   * THE KEY GATE (config.ini [controls] pad_keys_scope): with "match" the two
   * KEY paths below go in only while the engine has a live game object; the
   * touch layer is never scoped. */
  {
    const int open = key_gate(buttons, sticks);
    if (open) {
      poll_pad_keys(buttons);
      poll_pad_stick_dpad(sticks);
    }
    if (open != s_in.scope_open || !s_in.scope_logged) {
      s_in.scope_logged = 1;
      s_in.scope_open = open;
      debugPrintf("[input] the pad's keys: %s (pad_keys_scope = %s, so the gate is %s; withheld "
                  "key edges so far: %u)\n",
                  open ? "GO IN to the engine" : "are HELD BACK by the scope",
                  s_in.scope_match ? "match" : "always",
                  s_in.scope_match ? "open only while the engine has a game" : "always open",
                  s_in.n_withheld);
    }
  }
  s_in.scope_edges = buttons;

  /* THE MATCH'S ACTIONS (fifa14_padact.c): the pad's face buttons, when they
   * are mapped, become touches on the engine's own on-screen buttons -- the
   * only input a match listens to (the file's header has the measurement: the
   * keyboard module's notifications reach no listener at all). Nothing here
   * runs outside a match, and [controls] pad_touch_buttons = off turns the
   * whole bridge off and leaves the pad exactly as it was before. */
  fifa14_padact_poll(buttons);
  fifa14_padact_report(s_in.frames);

  poll_pad_touch(buttons, sticks);

  /* Android's BACK stays on the buttons it has always been on (the port's own
   * bound entry point); '-' also goes in as the engine's SELECT, which is the
   * pair the brief asks for. The [input] key counters say whether the engine
   * took them. */
  if (buttons & (HidNpadButton_Plus | HidNpadButton_Minus))
    send_back();
}

/* ------------------------------------------------------------------- the log */

static void summary(void) {
  int fingers = 0;
  for (int j = 0; j < (int)FIFA14_INPUT_MAX_FINGERS; j++)
    fingers += s_in.fingers[j].used;
  debugPrintf("[input] frame %llu: %u delivered (down %u, move %u, up %u, cancel %u, key %u "
              "= %u down + %u up), "
              "%d pointer%s held, panel %s (%d finger%s), pad %s cursor %d,%d, last pointer %s %d "
              "at %d,%d\n",
              (unsigned long long)s_in.frames, s_in.n_ev, s_in.n_down, s_in.n_move, s_in.n_up,
              s_in.n_cancel, s_in.n_key, s_in.n_key_down, s_in.n_key_up, s_in.held,
              s_in.held == 1 ? "" : "s",
              s_in.panel ? "up" : "not up", fingers, fingers == 1 ? "" : "s",
              s_in.pad ? "up" : "not up", (int)s_in.cx, (int)s_in.cy,
              s_in.last_msg == s_in.id_down    ? "DOWN"
              : s_in.last_msg == s_in.id_up    ? "UP"
              : s_in.last_msg == s_in.id_move  ? "MOVE"
              : s_in.last_msg == s_in.id_cancel ? "CANCEL"
                                               : "none",
              s_in.last_pointer, (int)(s_in.last_x + 0.5f), (int)(s_in.last_y + 0.5f));

  /* ================== THE DECISIVE LINE (added 2026-10-13) ==================
   * A run said "key 0 = 0 down + 0 up" and the log could not say whether that
   * was (a) the pad never reporting a button or (b) the engine dropping the
   * keys. This line settles it: padGetButtons is the RAW word the port reads,
   * and `a button was seen` is whether it has ever been non-zero at all. If the
   * next log shows "0x0000000000000000 ... no" the controller is not being
   * read (the mask against the id the console reports under: see rt_pad.c); if
   * it shows a non-zero word and the key counter still stands still, the pad IS
   * read and the engine is refusing the keys (the PhysicalKeyboard module's
   * IsEnabled; fifa14_boot.c's step 10b is the other half of that). */
  debugPrintf("[input] the pad's raw read: padGetButtons 0x%016llx (%s, mask 0x%llx) after %u "
              "samples, the largest word ever seen 0x%016llx, a button was seen: %s; keys "
              "delivered %u = %u down + %u up\n",
              (unsigned long long)padGetButtons(&s_in.padstate),
              rt_pad_style_name(padGetStyleSet(&s_in.padstate)),
              (unsigned long long)rt_pad_slot_mask(0), s_in.n_pad_samples,
              (unsigned long long)s_in.pad_max_buttons, s_in.pad_saw_a_button ? "yes" : "NO",
              s_in.n_key, s_in.n_key_down, s_in.n_key_up);

  /* ============ THE TWO PATHS, SIDE BY SIDE (added 2026-10-13) ==============
   * With the PhysicalKeyboard module in hand, identified by its table's entries
   * and enabled, a key now has two ways in and both are counted: the DIRECT
   * call on the module (fifa14_physkbd.c) and the engine's own ROUTED path
   * (KeyboardAndroid's native -> the engine's 0x40008 message -> the module).
   * If the game answers to the pad, the pair of counters says which way the
   * working key went; if neither moves the keys are not being read at all. */
  {
    unsigned d_dn = 0, d_up = 0;
    const unsigned direct = fifa14_physkbd_direct_calls(&d_dn, &d_up);
    debugPrintf("[input] key paths: direct calls %u = %u down + %u up, routed calls %u = %u "
                "down + %u up (module %s, enabled field %d, object 0x%08x, the engine's own "
                "pointer 0x%08x)\n",
                direct, d_dn, d_up, s_in.n_routed, s_in.n_routed_down, s_in.n_routed_up,
                fifa14_physkbd_ready() ? "in hand" : "NOT in hand",
                fifa14_physkbd_enabled(), (unsigned)(uintptr_t)fifa14_physkbd_object(),
                (unsigned)(uintptr_t)fifa14_physkbd_module());
  }
}

/* ------------------------------------------------------------------- the port */

void fifa14_input_init(void) {
  Fifa14ConstFn cnst;
  if (s_in.ready)
    return;
  s_in.ready = 1;
  s_in.trace = FIFA14_INPUT_TRACE;

  dcr_window_size(&s_in.size_w, &s_in.size_h);
  s_in.id_down = FIFA14_PTR_DOWN_FALLBACK;
  s_in.id_up = FIFA14_PTR_UP_FALLBACK;
  s_in.id_move = FIFA14_PTR_MOVE_FALLBACK;
  s_in.id_cancel = FIFA14_PTR_CANCEL_FALLBACK;
  s_in.module = FIFA14_PTR_MODULE_FALLBACK;
  s_in.cx = (float)s_in.size_w * 0.5f;
  s_in.cy = (float)s_in.size_h * 0.5f;

  debugPrintf("[input] ==== the engine's pointer entry (?the Java view's one call) ====\n");
  s_in.ptr = (Fifa14PointerFn)so_try_find_addr_rx(&game_mod, FIFA14_PTR_EVENT_SYM);
  if (!s_in.ptr) {
    debugPrintf("[input] %s MISSING from %s: the port cannot deliver any input\n",
                FIFA14_PTR_EVENT_SYM, FIFA14_LIB_GAME);
    return;
  }
  debugPrintf("[input] %s @ 0x%08lx\n", FIFA14_PTR_EVENT_SYM, (unsigned long)s_in.ptr);

  /* The engine's own constants, read from its natives. */
  cnst = (Fifa14ConstFn)so_try_find_addr_rx(&game_mod, FIFA14_PTR_DOWN_SYM);
  if (cnst)
    s_in.id_down = cnst(g_jni_env, NULL);
  cnst = (Fifa14ConstFn)so_try_find_addr_rx(&game_mod, FIFA14_PTR_UP_SYM);
  if (cnst)
    s_in.id_up = cnst(g_jni_env, NULL);
  cnst = (Fifa14ConstFn)so_try_find_addr_rx(&game_mod, FIFA14_PTR_MOVE_SYM);
  if (cnst)
    s_in.id_move = cnst(g_jni_env, NULL);
  cnst = (Fifa14ConstFn)so_try_find_addr_rx(&game_mod, FIFA14_PTR_CANCEL_SYM);
  if (cnst)
    s_in.id_cancel = cnst(g_jni_env, NULL);
  cnst = (Fifa14ConstFn)so_try_find_addr_rx(&game_mod, FIFA14_PTR_MODULE_SYM);
  if (cnst)
    s_in.module = cnst(g_jni_env, NULL);
  s_in.ids_from_engine = cnst != NULL;
  debugPrintf("[input] down 0x%05x, up 0x%05x, move 0x%05x, cancel 0x%05x, touch module %d "
              "(%s)\n",
              (unsigned)s_in.id_down, (unsigned)s_in.id_up, (unsigned)s_in.id_move,
              (unsigned)s_in.id_cancel, s_in.module,
              s_in.ids_from_engine ? "the engine's own, read from its natives"
                                   : "the measured fallbacks: the natives are missing");

  /* THE PAD'S OWN ENTRY: KeyboardAndroid's key natives, and the module the key
   * messages are routed to (see this file's header for the whole chain). */
  s_in.key_module = FIFA14_KEY_MODULE_FALLBACK;
  cnst = (Fifa14ConstFn)so_try_find_addr_rx(&game_mod, FIFA14_KEY_MODULE_SYM);
  if (cnst)
    s_in.key_module = cnst(g_jni_env, NULL);
  s_in.key_down = (Fifa14KeyFn)so_try_find_addr_rx(&game_mod, FIFA14_KEY_DOWN_SYM);
  s_in.key_up = (Fifa14KeyFn)so_try_find_addr_rx(&game_mod, FIFA14_KEY_UP_SYM);
  s_in.keys_from_engine = s_in.key_down != NULL && s_in.key_up != NULL;
  if (s_in.keys_from_engine) {
    debugPrintf("[input] the keys: %s @ 0x%08lx (down, the id 0x00040008), %s @ 0x%08lx "
                "(up, 0x00080008)\n",
                FIFA14_KEY_DOWN_SYM, (unsigned long)s_in.key_down, FIFA14_KEY_UP_SYM,
                (unsigned long)s_in.key_up);
    debugPrintf("[input] the keys' module: %s -> %d (%s)\n", FIFA14_KEY_MODULE_SYM,
                s_in.key_module,
                cnst ? "the engine's own constant" : "the measured fallback: 600");
  } else {
    debugPrintf("[input] the KEYS ARE OFF: %s / %s missing from %s -- the pad's buttons "
                "will not reach the engine (the touch layer still will)\n",
                FIFA14_KEY_DOWN_SYM, FIFA14_KEY_UP_SYM, FIFA14_LIB_GAME);
  }

#if FIFA14_INPUT_TOUCH
  if (!hidGetSharedmemAddr()) {
    debugPrintf("[input] the touchscreen: hid is not initialised (the runtime's nx_init) -- "
                "the panel is off\n");
  } else {
    const Result rc = activate_touchscreen();
    s_in.panel = R_SUCCEEDED(rc);
    debugPrintf("[input] the touchscreen: %s (hid command 11 -> 0x%x), the panel is %dx%d and "
                "the engine's view %dx%d\n",
                s_in.panel ? "up" : "REFUSED", (unsigned)rc, FIFA14_PANEL_W, FIFA14_PANEL_H,
                s_in.size_w, s_in.size_h);
  }
#else
  debugPrintf("[input] the touchscreen: off (FIFA14_INPUT_TOUCH 0)\n");
#endif

#if FIFA14_INPUT_PAD
  rt_pad_setup(1, 1);
  rt_pad_slot(&s_in.padstate, 0);
  s_in.pad = 1;
  debugPrintf("[input] the pad: player 1. If the engine's key natives resolved, every "
              "button is the engine's own gamepad key and the left stick is the cross:\n");
  /* The mask, in the log: the attached Joy-Cons report under
   * HidNpadIdType_Handheld and NOT under No1, so a slot mask without the
   * handheld bit reads zero for a whole run (rt_pad.c has the measurement). */
  debugPrintf("[input]   the mask in use: 0x%llx = player 1 (bit 0) | handheld (bit 0x20); pad "
              "style now: %s\n",
              (unsigned long long)rt_pad_slot_mask(0),
              rt_pad_style_name(padGetStyleSet(&s_in.padstate)));
  if (s_in.keys_from_engine && !s_in.keys_logged_map) {
    s_in.keys_logged_map = 1;
    for (int i = 0; i < FIFA14_KEY_COUNT; i++)
      debugPrintf("[input]   %s\n", k_keys[i].what);
    for (int i = 0; i < 4; i++)
      debugPrintf("[input]   %s (on %.2f, off %.2f)\n", k_stick_dpad[i].what,
                  (double)FIFA14_STICK_DPAD_ON, (double)FIFA14_STICK_DPAD_OFF);
  }
  debugPrintf("[input] the pad's touch layer: the RIGHT stick is the cursor at %d,%d, "
              "StickR = a click, StickL = the second finger -- the console's own panel "
              "is unchanged\n",
              (int)s_in.cx, (int)s_in.cy);

  /* THE PAD'S KEY SCOPE (config.ini [controls] pad_keys_scope, fifa14_padscope.c):
   * the lever, and the engine signal it is gated on. The signal's chain is
   * resolved and VERIFIED here (before any key has been gated), so the first
   * [pad] line of the log says whether this run's gate can act at all. */
  {
    const char *v = rt_config_get("controls", "pad_keys_scope");
    s_in.scope_match = !v || !strcmp(v, "match");
    if (v && strcmp(v, "match") != 0 && strcmp(v, "always") != 0) {
      debugPrintf("[input] config.ini [controls] pad_keys_scope = \"%s\" is not a value this port "
                  "knows: \"match\" is used (the row's own choices are match, always)\n",
                  v);
      s_in.scope_match = 1;
    }
    s_in.scope_open = 1; /* until the first poll decides */
    debugPrintf("[input] pad_keys_scope = %s: %s\n", s_in.scope_match ? "match" : "always",
                s_in.scope_match
                    ? "the pad's KEYS are delivered only while the engine has a live game object "
                      "(a match); the right stick's cursor, StickR/StickL and +/- (Android's "
                      "BACK) are NOT scoped"
                    : "every key is delivered, always -- the port's behaviour before this lever");
    fifa14_padscope_init();
  }
#else
  debugPrintf("[input] the pad: off (FIFA14_INPUT_PAD 0)\n");
#endif
  debugPrintf("[input] ==== the input is live: touch the screen, or use the stick and A ====\n");

  /* THE LAST LINK (fifa14_physkbd.c): the engine's PhysicalKeyboard module.
   * It runs HERE and not earlier because the engine's SystemAndroid singleton
   * (the port's route to the engine's ModuleManager) is built by the boot's
   * step 6, and the engine's own state is 2 by step 8 -- and because the module
   * is instantiated on demand, which is the whole point: nothing in this port
   * ever asked for it, and a module that does not exist swallows the pad in
   * silence. */
#if FIFA14_INPUT_PAD
  fifa14_physkbd_init();
#endif
}

void fifa14_input_poll(void) {
  if (!s_in.ready || !s_in.ptr)
    return;
  s_in.frames++;
#if FIFA14_INPUT_TOUCH
  poll_touchscreen();
#endif
#if FIFA14_INPUT_PAD
  poll_pad();
#endif
  if (s_in.frames % FIFA14_INPUT_SUMMARY_FRAMES == 0)
    summary();
#if FIFA14_INPUT_PAD
  /* THE MODULE'S ENABLE FLAG, ONCE IN A WHILE (fifa14_physkbd.c): the task's
   * own line, so one run says whether the engine's PhysicalKeyboard is enabled
   * -- the field OnRawKeyDown demands be 1 -- or not. */
  fifa14_physkbd_report();
  /* THE SCOPE'S SIGNAL, ONCE IN A WHILE (fifa14_padscope.c): the engine's game
   * object, written whenever it changes and on a slow heartbeat, so one run's
   * log says what the "match" scope was actually gated on. */
  fifa14_padscope_report(s_in.frames);
  /* THE PAD'S ACTIONS IN A MATCH (fifa14_padact.c): read config.ini's
   * [controls] pad_touch_buttons / pad_touch_map now, so the whole session's
   * log says what the bridge will do the moment a match is up. */
  fifa14_padact_arm();
#endif
}

void fifa14_input_release_all(void) {
  int any = 0;
  if (!s_in.ready || !s_in.ptr)
    return;
  /* Android hands the view an ACTION_CANCEL when it loses the window: the
   * engine's TouchSurface::OnRawTouchCancel is the same thing. */
  for (int j = 0; j < (int)FIFA14_INPUT_MAX_FINGERS; j++) {
    if (!s_in.fingers[j].used)
      continue;
    deliver("CANCEL", s_in.id_cancel, (int)s_in.fingers[j].raw, s_in.fingers[j].x,
            s_in.fingers[j].y);
    s_in.fingers[j].used = 0;
    any = 1;
  }
  if (s_in.a_down) {
    deliver("CANCEL", s_in.id_cancel, FIFA14_PAD_RAW_A, s_in.cx, s_in.cy);
    s_in.a_down = 0;
    any = 1;
  }
  if (s_in.b_down) {
    deliver("CANCEL", s_in.id_cancel, FIFA14_PAD_RAW_B, s_in.cx, s_in.cy);
    s_in.b_down = 0;
    any = 1;
  }
  /* The pad's keys: a focus loss must not leave one held down in the engine
   * (Android's Activity gets the key-up when the window goes away). */
  for (int i = 0; i < FIFA14_KEY_COUNT; i++) {
    if (!(s_in.keys_held & k_keys[i].bit))
      continue;
    deliver_key(0, k_keys[i].code, k_keys[i].what);
    any = 1;
  }
  /* The pad's ACTION touches (fifa14_padact.c): the same rule for the
   * pointers the bridge is holding on the engine's own buttons. */
  fifa14_padact_release_all();
  s_in.keys_held = 0;
  for (int i = 0; i < 4; i++) {
    if (!((s_in.stick_dpad_on >> i) & 1))
      continue;
    deliver_key(0, k_stick_dpad[i].code, k_stick_dpad[i].what);
  }
  s_in.stick_dpad_on = 0;
  if (any)
    debugPrintf("[input] lost the focus: every pointer was cancelled and every key "
                "released, as Android's ACTION_CANCEL does\n");
}
