/* fifa14_physkbd.c -- THE LAST LINK. The engine's PhysicalKeyboard module,
 * which is where an Android gamepad's KEYCODE_BUTTON_* end up, found by the
 * port, watched while it runs, turned on when the engine has not, and -- when
 * the configuration asks for it -- called directly instead of through the
 * engine's message dispatch.
 *
 * ==========================================================================
 * WHY THE PAD'S KEYS WERE DROPPED (the round before this one, measured)
 *
 * The port reads the pad and hands every button to the engine, and the count
 * in the log says so: "[input] ... key 238 = 86 down + 85 up". The engine's
 * side of that call is
 *
 *   Java_com_ea_blast_KeyboardAndroid_NativeOnKeyDown @0x00e0c498
 *     -> posts, with a HARDCODED raw id 0x00040008, an EA::Blast::MessageKeyData
 *        whose +12 is the module id it was given (600) and whose +20/+24 are the
 *        Android keycode and the alt flag
 *     -> PhysicalKeyboard::HandleMessage @0x00df0864 routes it (its +12 == 600,
 *        then 0x40008) to PhysicalKeyboard::OnRawKeyDown @0x00df0ba8
 *
 * and OnRawKeyDown, read instruction by instruction, is
 *
 *   df0ba8  push {r4,r5,r6,lr}; sub sp,#24; mov r4,r0        ; this = the module
 *   df0bb4  ldr r0,[r4,#36]                                  ; the mapper it owns
 *   df0bb8  ldr r3,[r0]; ldr r3,[r3,#32]; blx r3             ; mapper->vtable[8]
 *   df0bc4  mov r5,r0                                        ; r5 = RawToStdKey(raw, alt)
 *   df0bc8  cmp r5,#0; beq <return>                          ; 0 = a key it does not know
 *   df0bd0  ldr r0,[r4]; ldr r1,[r0,#136]; blx r1            ; vtable[34] = IsEnabled()
 *   df0be0  cmp r0,#1; bne <return>                          ; ** 1 or the key is DROPPED **
 *   ...     (then the pressed-key set, and Keyboard::NotifyKey(mapper, 0x40108, key))
 *
 * vtable[34] is PhysicalKeyboard::IsEnabled @0x00df17c4, which is literally
 *
 *   df17c4  ldrb r0,[r0,#144]; bx lr        ; the byte at the object's +0x90
 *   df17cc  strb r1,[r0,#144]; bx lr        ; and SetEnabled is what writes it
 *
 * and the constructor (0x00df0018) writes that byte 0. Only
 * PhysicalKeyboard::OnLifeCycleFocusGained @0x00df12ec sets it:
 *
 *   df12f4  mov r1,#1; ldr r2,[r0,#140]; blx r2   ; vtable[35] = SetEnabled(true)
 *
 * So a module that never hears the life cycle swallows every key in silence,
 * with the port's counters running -- exactly what the logs show.
 *
 * ==========================================================================
 * THE MODULE'S INSTANCE, FROM THE ENGINE'S OWN SINGLETON
 *
 * The module's instance is the one thing the port never had. The engine keeps
 * it in its own singleton:
 *
 *   gSystem (an exported OBJECT symbol of libFIFA14.so, i.e. by name, no RVA)
 *     = the EA::Blast::SystemAndroid* the engine's NativeOnCreate built
 *     (NativeOnCreate @0x00dffc8c does exactly `gSystem = SystemAndroid*`:
 *      str r4,[r6] with r6 = the gSystem GOT slot, the word this reads)
 *   *(SystemAndroid* + 20) = its ModuleManager
 *     (System::GetModuleManager @0x00e01ad4 is `ldr r0,[r0,#20]; bx lr`, and
 *      System::Init @0xe0757c is the store: `str r5,[r4,#20]` right after
 *      ModuleManager::Create)
 *   ModuleManager::GetModule(600, 0) @0x00e084d0
 *     = the module, CREATED ON DEMAND -- it scans the registry for the entry
 *       whose type id is 600 and index 0 and instantiates it through
 *       ModuleManager::GetModule(const IModuleInfo*) @0x00e08558, which calls
 *       the module's own Init (0xe08678). 600 is the engine's own
 *       NativeGetModuleTypeIdPhysicalKeyboard, and 0 is the index
 *       RegisterPhysicalKeyboardModule @0x00df143c registers (`mov r2,#600;
 *       mov r3,#0`).
 *
 * WHY THE MODULE MAY SIMPLY NOT BE THERE. RegisterPhysicalKeyboardModule fills
 * a ModuleRegistryEntry whose InstantiationMode argument is 0, and
 * ModuleManager::InitAutoInstantiatedModules @0x00e08268 only instantiates the
 * entries whose mode is 1 (`ldr r0,[sl,#48]; cmp r0,#1; bne <next>`). The
 * PhysicalKeyboard is therefore a module the game has to ASK for -- and a port
 * that only answers GetPhysicalKeyboardCount = 1 has given the engine nowhere
 * to put a key. GetModule(600, 0) is that ask, made with the engine's own API.
 *
 * ==========================================================================
 * THE ROUND AFTER THAT: THE MODULE WAS IN HAND AND THE PORT REFUSED IT
 * (the console log of 2026-10-13, and what the .so says about it)
 *
 * The lookup WORKED -- and the port threw the answer away:
 *
 *   [input] PhysicalKeyboard: ModuleManager::GetModule(600, 0) -> 0x9560a574
 *           (the module is there)
 *   [input] PhysicalKeyboard: 0x9560a574 is NOT a PhysicalKeyboard (its first
 *           word is 0x29ba2234, the engine's vtable address point is
 *           0x29ba0798): the pad's keys cannot be delivered ...
 *
 * The test that refused it was
 *
 *   *(uint32_t *)obj == (uint32_t)(_ZTVN2EA5Blast16PhysicalKeyboardE + 8)
 *
 * i.e. "the object's first word is the BASE class's address point". It is a
 * test that could never pass, and the two numbers in the log say why:
 *
 *   0x29ba2234 - 0x29ba0798 = 0x1A9C
 *
 * and the .so's own tables (arm-none-eabi-objdump over
 * _ZTVN2EA5Blast23PhysicalKeyboardAndroidE @0x010de190, the DERIVED class,
 * against _ZTVN2EA5Blast16PhysicalKeyboardE @0x010dc790) say exactly what
 * 0x1A9C is:
 *
 *   - the derived class's PRIMARY table starts 0x1A00 after the base's, so its
 *     address point (table + 8) is 0x1A08 after the base's -- not 0x1A9C;
 *   - every one of these tables ends with a SECONDARY table for the +4
 *     subobject: raw slot 39 = 0xfffffffc (the offset-to-top of a subobject 4
 *     bytes in), raw slot 40 = the typeinfo, raw slot 41 = the address point of
 *     that secondary table (table + 0xA4), whose entries are the Thn4_ adjustor
 *     thunks (measured in both classes: _ZThn4_...GetId, GetTypeId, GetName,
 *     GetDescription, GetModuleInfo, IsAvailable at slots 41..46);
 *   - and 0x010de190 + 0xA4 - 0x010dc790 - 8 = 0x1A9C, the exact number the
 *     console printed.
 *
 * So ModuleManager::GetModule() hands back the module's **+4 INTERFACE**
 * pointer (an EA::Blast::IPhysicalKeyboard / IModule subobject) whose first
 * word is the class's SECONDARY address point; the object itself is 4 bytes
 * BEFORE it, and its own first word is the primary address point. The port was
 * comparing an interface pointer's first word against a class's primary
 * address point -- on top of only knowing the base class.
 *
 * WHAT THE PORT DOES NOW -- AND WHY IT IS THE ENTRIES, NOT THE FIRST WORD.
 * The class is identified the way the engine's own linker left it: by the
 * METHODS THE TABLE HOLDS, each at the offset it was measured at. The port
 * resolves the engine's own entry points BY NAME (all six are exported) and
 * asks a candidate table which of them it holds:
 *
 *        AP+52   HandleMessage             AP+132  OnLifeCycleFocusGained
 *        AP+112  OnRawKeyDown              AP+136  IsEnabled
 *        AP+116  OnRawKeyUp                AP+140  SetEnabled
 *
 * (AP = address point = the word an object's first word points at; 8 + 4*slot,
 * i.e. the raw slots 15/30/31/35/36/37.) The same six sit at the same six
 * offsets in the BASE class's table and in the DERIVED one (read out of the
 * .so: the derived class overrides D1/D0/Init/IsVisible and nothing else) --
 * so three or more of them is the answer for either class, and for a class the
 * .so's symbol table does not even name.
 *
 * The object's own pointer is found the same way: the primary address point is
 * the table whose +136 is IsEnabled, and the object is the pointer whose first
 * word is that table (4 bytes before what GetModule returned, when what it
 * returned is the interface pointer -- checked, not assumed).
 *
 * WHAT THE LOG WILL SAY, AND WHAT EACH OUTCOME MEANS
 *
 *   [input] the module object 0x...: its vtable slots match OnRawKeyDown/
 *       HandleMessage/IsEnabled = YES (4 of 6: ...)      <- the class check
 *   [input] PhysicalKeyboard: the manager hands back the +4 INTERFACE pointer
 *       ... the object is 4 B before it (0x...)           <- which pointer it was
 *   [input] PhysicalKeyboard: the object 0x... -> its class, per the engine's
 *       own vtable symbols: PhysicalKeyboardAndroid (the DERIVED class: ...)
 *   [input] PhysicalKeyboard enabled field = 1            <- the task's own line
 *   [input] direct calls N = D down + U up                <- one path's counters
 *   [input] routed calls N = D down + U up                <- the other path's
 *
 * MIT.
 */
#include <stdio.h>
#include <string.h>
#include <switch.h>

#include "config.h"
#include "fifa14_physkbd.h"
#include "so_util.h"
#include "util.h"

/* The game's module (fifa14_main.c). */
extern so_module game_mod;
/* The JNIEnv the port answers with, for the engine's constant natives. */
extern void *g_jni_env;

/* ---------------------------------------------------------------- symbols
 * All by NAME: the .so exports every one of them, so nothing here is an RVA
 * that a different build of the game could move. */
#define PK_SYM_SYSTEM_GLOBAL "gSystem" /* DO: EA::Blast::System* gSystem */
#define PK_SYM_VT_SYS "_ZTVN2EA5Blast13SystemAndroidE"
#define PK_SYM_VT_MM "_ZTVN2EA5Blast13ModuleManagerE"
#define PK_SYM_VT_PK "_ZTVN2EA5Blast16PhysicalKeyboardE"
#define PK_SYM_VT_PK_ANDROID "_ZTVN2EA5Blast23PhysicalKeyboardAndroidE"
#define PK_SYM_GETMODULE "_ZN2EA5Blast13ModuleManager9GetModuleEij"
#define PK_SYM_HANDLEMSG "_ZN2EA5Blast16PhysicalKeyboard13HandleMessageEjPv"
#define PK_SYM_RAW_DOWN "_ZN2EA5Blast16PhysicalKeyboard12OnRawKeyDownEji"
#define PK_SYM_RAW_UP "_ZN2EA5Blast16PhysicalKeyboard10OnRawKeyUpEji"
#define PK_SYM_FOCUS "_ZN2EA5Blast16PhysicalKeyboard22OnLifeCycleFocusGainedEv"
#define PK_SYM_SETENABLED "_ZN2EA5Blast16PhysicalKeyboard10SetEnabledEb"
#define PK_SYM_MODULE_ID \
  "Java_com_ea_blast_ModuleCatalog_NativeGetModuleTypeIdPhysicalKeyboard"
#define PK_SYM_ISENABLED "_ZN2EA5Blast16PhysicalKeyboard9IsEnabledEv"

/* The measured offsets. Each one is an instruction of the engine's, quoted in
 * this file's header. */
#define PK_OFF_MAPPER 36u          /* ldr r0,[r4,#36]      (OnRawKeyDown) */
#define PK_OFF_ENABLED 144u        /* ldrb r0,[r0,#144]    (IsEnabled)   */
#define PK_OFF_VISIBLE 145u        /* strb r0,[r4,#145]    (Init)        */
#define PK_VT_ISENABLED_OFF 136u   /* ldr r1,[r0,#136]     (vtable[34])  */
#define PK_VT_SETENABLED_OFF 140u  /* ldr r2,[r0,#140]     (vtable[35])  */
#define SYS_OFF_MODULEMANAGER 20u  /* ldr r0,[r0,#20]      (GetModuleManager) */

/* The engine's module type id for the physical keyboard, and the index its
 * registry entry is registered under (`mov r2,#600; mov r3,#0` in
 * RegisterPhysicalKeyboardModule @0x00df143c). 600 is also what its own
 * NativeGetModuleTypeIdPhysicalKeyboard returns. */
#define PK_MODULE_TYPE 600u
#define PK_MODULE_INDEX 0u

/* The engine's own constant native, and the egl-like shape of the natives that
 * ignore both arguments (`mov r0,#600; bx lr`). */
typedef unsigned (*Fifa14ConstFn)(void *env, void *cls);

/* ModuleManager::GetModule(unsigned typeId, unsigned index) -> Module* */
typedef void *(*PkGetModuleFn)(void *self, unsigned type, unsigned index);
/* PhysicalKeyboard::OnRawKeyDown/Up(unsigned rawKey, int alt) */
typedef void (*PkRawFn)(void *self, unsigned key, int alt);
/* PhysicalKeyboard::OnLifeCycleFocusGained() */
typedef void (*PkVoidFn)(void *self);
/* PhysicalKeyboard::SetEnabled(bool) */
typedef void (*PkSetEnabledFn)(void *self, int on);
/* PhysicalKeyboard::IsEnabled() -> int */
typedef int (*PkIsEnabledFn)(void *self);

/* ----------------------------------------------------------------- the table
 * The engine's own methods and where each sits in the class's primary table,
 * AP-relative: MEASURED out of _ZTVN2EA5Blast16PhysicalKeyboardE and out of
 * _ZTVN2EA5Blast23PhysicalKeyboardAndroidE (this file's header has the raw
 * slots: 15, 30, 31, 35, 36, 37 -- the offset is 8 less than 4*slot, because
 * the address point comes after the offset-to-top and the typeinfo). The
 * derived class overrides D1/D0/Init/IsVisible and nothing else, so both
 * classes hold all six at these offsets -- which is what makes the count a
 * legitimate answer for either of them, and for a class no symbol names. */
typedef struct {
  unsigned off;
  const char *sym; /* the .so's exported name of the entry */
  const char *what;
} PkEntry;

static const PkEntry k_entry[] = {
    {52u, PK_SYM_HANDLEMSG, "HandleMessage"},
    {112u, PK_SYM_RAW_DOWN, "OnRawKeyDown"},
    {116u, PK_SYM_RAW_UP, "OnRawKeyUp"},
    {132u, PK_SYM_FOCUS, "OnLifeCycleFocusGained"},
    {136u, PK_SYM_ISENABLED, "IsEnabled"},
    {140u, PK_SYM_SETENABLED, "SetEnabled"},
};
#define PK_ENTRY_COUNT (int)(sizeof k_entry / sizeof k_entry[0])

/* The three the task's own log line names: OnRawKeyDown (1), HandleMessage (0)
 * and IsEnabled (4). */
#define PK_ENTRY_BIT(i) (1u << (i))
#define PK_ENTRY_NAMED (PK_ENTRY_BIT(0) | PK_ENTRY_BIT(1) | PK_ENTRY_BIT(4))

/* How many of the six make a table the module's. */
#define PK_ENTRY_MIN 3

static struct {
  int tried;   /* the lookup has been attempted (once) */
  int ok;      /* the module is in hand */
  void *system, *manager;
  void *module_arg;  /* exactly what GetModule returned (the interface pointer) */
  void *module_base; /* the object itself (what `this` has to be) */
  int module_shift;  /* 0, or -4 when the engine handed back the +4 interface */
  PkRawFn raw_down, raw_up;
  PkVoidFn focus_gained;
  PkSetEnabledFn set_enabled;
  unsigned module_id;

  /* the log's bookkeeping */
  int last_enabled;      /* the last value written (2 = never written) */
  unsigned frames;       /* frames, for the report's rate */
  int logged_first_key;  /* the direct path has said what it is */
  unsigned n_calls, n_down, n_up;

  /* the six entry points, resolved once (0 = the symbol did not resolve) */
  uintptr_t entry_addr[PK_ENTRY_COUNT];
} s_pk;

static int enabled_of(void *module) {
  if (!module)
    return -1;
  return (int)((const uint8_t *)module)[PK_OFF_ENABLED];
}

/* The engine's own IsEnabled, called through the module's vtable slot the way
 * OnRawKeyDown calls it (vtable[34], offset 136). This is the gate itself, not
 * a copy of it: whatever this answers is what a key would be judged by. */
static int isenabled_via_vtable(void *module) {
  if (!module)
    return -1;
  const uintptr_t *vt = *(const uintptr_t *const *)module;
  if (!vt)
    return -1;
  const PkIsEnabledFn fn = (PkIsEnabledFn)vt[PK_VT_ISENABLED_OFF / 4];
  if (!fn)
    return -1;
  return fn(module);
}

/* An object of the engine, identified by the vtable its own constructor put in
 * its first word: &_ZTV<class> + 8 (the two words before an address point are
 * the offset-to-top and the typeinfo pointer). 1 when `obj` is one. Used only
 * for the two singletons (SystemAndroid, ModuleManager), which the engine's own
 * API returns as the objects themselves. */
static int is_engine_object(const void *obj, uintptr_t vt_symbol) {
  if (!obj || !vt_symbol)
    return 0;
  return *(const uint32_t *)obj == (uint32_t)(vt_symbol + 8);
}

/* ------------------------------------------------------- the class, by entries
 * Is `p` inside the engine's mapped image? Every address point the port reads
 * comes from the engine, but a wrong one must not be dereferenced: the vtable
 * symbols are IN that image, so a candidate that is not can never be a table of
 * the module's class. */
static int in_engine_image(uint32_t p) {
  const uintptr_t lo = (uintptr_t)game_mod.load_virtbase;
  const uintptr_t hi = lo + game_mod.load_size;
  return lo && (uintptr_t)p >= lo && (uintptr_t)p < hi;
}

/* Which of the engine's own entries the table at `ap` holds, each at its
 * measured offset. Returns the count and, in *mask, which ones they were; 0
 * when `ap` is not in the engine's image (nothing is read then). */
static int table_entries(uint32_t ap, unsigned *mask) {
  unsigned m = 0;
  int n = 0;
  if (mask)
    *mask = 0;
  if (!ap || !in_engine_image(ap))
    return 0;
  for (int i = 0; i < PK_ENTRY_COUNT; i++) {
    if (!s_pk.entry_addr[i])
      continue;
    const uint32_t v = ((const uint32_t *)ap)[k_entry[i].off / 4];
    if (v && v == (uint32_t)s_pk.entry_addr[i]) {
      m |= PK_ENTRY_BIT(i);
      n++;
    }
  }
  if (mask)
    *mask = m;
  return n;
}

/* The matched names, space-separated, into `out` (for the log). */
static void matched_names(unsigned mask, char *out, size_t cap) {
  size_t used = 0;
  out[0] = 0;
  for (int i = 0; i < PK_ENTRY_COUNT; i++) {
    if (!(mask & PK_ENTRY_BIT(i)))
      continue;
    const size_t room = cap > used ? cap - used : 0;
    const int n = snprintf(out + used, room, "%s%s", used ? " " : "", k_entry[i].what);
    if (n < 0 || (size_t)n >= room)
      break;
    used += (size_t)n;
  }
}

/* THE TASK'S OWN LINE. `what` is the pointer being judged, `n` the count
 * table_entries() returned for it: a table of the module's class (base or
 * derived, primary or secondary address point) holds 3 or more of the six
 * entries, each at its measured offset. */
static void log_match(const char *line, uint32_t what, int n, unsigned mask) {
  char names[192];
  matched_names(mask, names, sizeof names);
  debugPrintf("[input] %s 0x%08x: its vtable slots match OnRawKeyDown/HandleMessage/"
              "IsEnabled = %s (%d of %d known entries at their measured offsets%s%s)\n",
              line, (unsigned)what,
              (mask & PK_ENTRY_NAMED) == PK_ENTRY_NAMED ? "YES" : "NO", n, PK_ENTRY_COUNT,
              names[0] ? ": " : "", names);
}

/* The class named by the engine's own vtable symbol whose address point the
 * object's first word is -- the base, the phone's derived class, or neither. */
static const char *class_of_table(uint32_t ap, uintptr_t vt_pk, uintptr_t vt_android) {
  if (vt_android && ap == (uint32_t)(vt_android + 8))
    return "PhysicalKeyboardAndroid (the DERIVED class: the phone's own keyboard module)";
  if (vt_pk && ap == (uint32_t)(vt_pk + 8))
    return "PhysicalKeyboard (the base class)";
  return "a class the .so exports no _ZTV symbol for (another build's, or a subclass)";
}

int fifa14_physkbd_init(void) {
  if (s_pk.tried)
    return s_pk.ok;
  s_pk.tried = 1;
  s_pk.last_enabled = 2;

  const uintptr_t gsym = so_try_find_addr_rx(&game_mod, PK_SYM_SYSTEM_GLOBAL);
  const uintptr_t vt_sys = so_try_find_addr_rx(&game_mod, PK_SYM_VT_SYS);
  const uintptr_t vt_mm = so_try_find_addr_rx(&game_mod, PK_SYM_VT_MM);
  const uintptr_t vt_pk = so_try_find_addr_rx(&game_mod, PK_SYM_VT_PK);
  const uintptr_t vt_android = so_try_find_addr_rx(&game_mod, PK_SYM_VT_PK_ANDROID);
  const uintptr_t getmod = so_try_find_addr_rx(&game_mod, PK_SYM_GETMODULE);

  debugPrintf("[input] ==== the engine's PhysicalKeyboard module: the last link ====\n");
  if (!gsym || !vt_sys || !vt_mm || !vt_pk || !getmod) {
    debugPrintf("[input] PhysicalKeyboard: the lookup failed (gSystem 0x%08lx, the "
                "three vtables 0x%08lx/0x%08lx/0x%08lx, GetModule 0x%08lx): the module "
                "cannot be found, the pad's keys stay on the engine's message path\n",
                (unsigned long)gsym, (unsigned long)vt_sys, (unsigned long)vt_mm,
                (unsigned long)vt_pk, (unsigned long)getmod);
    return 0;
  }

  /* 1. gSystem: the engine's own SystemAndroid singleton (NativeOnCreate stores
   *    it there: `str r4,[r6]` with r6 = the gSystem slot). */
  s_pk.system = *(void **)gsym;
  debugPrintf("[input] PhysicalKeyboard: %s @ 0x%08lx -> the engine's SystemAndroid "
              "0x%08x\n",
              PK_SYM_SYSTEM_GLOBAL, (unsigned long)gsym, (unsigned)(uintptr_t)s_pk.system);
  if (!is_engine_object(s_pk.system, vt_sys)) {
    debugPrintf("[input] PhysicalKeyboard: 0x%08x is NOT a SystemAndroid (its first word "
                "is 0x%08x, the engine's vtable address point is 0x%08lx): refusing to "
                "follow it\n",
                (unsigned)(uintptr_t)s_pk.system,
                (unsigned)(s_pk.system ? *(const uint32_t *)s_pk.system : 0),
                (unsigned long)(vt_sys + 8));
    s_pk.system = NULL;
    return 0;
  }

  /* 2. SystemAndroid + 20 = its ModuleManager (System::GetModuleManager). */
  s_pk.manager = *(void **)((char *)s_pk.system + SYS_OFF_MODULEMANAGER);
  debugPrintf("[input] PhysicalKeyboard: SystemAndroid+20 = its ModuleManager 0x%08x\n",
              (unsigned)(uintptr_t)s_pk.manager);
  if (!is_engine_object(s_pk.manager, vt_mm)) {
    debugPrintf("[input] PhysicalKeyboard: 0x%08x is NOT a ModuleManager (its first word "
                "is 0x%08x, the engine's vtable address point is 0x%lx): the engine had "
                "not built one yet (System::Init does) -- no module to talk to\n",
                (unsigned)(uintptr_t)s_pk.manager,
                (unsigned)(s_pk.manager ? *(const uint32_t *)s_pk.manager : 0),
                (unsigned long)(vt_mm + 8));
    s_pk.manager = NULL;
    return 0;
  }

  /* 3. The engine's own type id (it ignores both arguments: `mov r0,#600`). */
  s_pk.module_id = PK_MODULE_TYPE;
  const Fifa14ConstFn cnst =
      (Fifa14ConstFn)so_try_find_addr_rx(&game_mod, PK_SYM_MODULE_ID);
  if (cnst)
    s_pk.module_id = cnst(g_jni_env, NULL);

  /* 3b. THE ENTRIES, resolved once: the engine's own methods, by name. These,
   *     and not a vtable symbol, are what identifies the module's class. */
  {
    int missing = 0;
    for (int i = 0; i < PK_ENTRY_COUNT; i++) {
      s_pk.entry_addr[i] = so_try_find_addr_rx(&game_mod, k_entry[i].sym);
      if (!s_pk.entry_addr[i])
        missing++;
    }
    debugPrintf("[input] PhysicalKeyboard: the class check's entries, resolved by name in "
                "%s: ", FIFA14_LIB_GAME);
    for (int i = 0; i < PK_ENTRY_COUNT; i++)
      debugPrintf("%s%s@+%u=0x%08lx", i ? ", " : "", k_entry[i].what, k_entry[i].off,
                  (unsigned long)s_pk.entry_addr[i]);
    debugPrintf("%s\n", missing ? "  <- SOME DID NOT RESOLVE: the count is out of fewer "
                                  "entries than six" : " (all six)");
    if (missing == PK_ENTRY_COUNT) {
      debugPrintf("[input] PhysicalKeyboard: NONE of the %d entry points resolved: the "
                  "class cannot be identified by its entries, and the module is left "
                  "alone (the pad stays on the engine's message path)\n",
                  PK_ENTRY_COUNT);
      return 0;
    }
  }

  /* 4. THE ASK. On demand: this is what creates the module when the engine has
   *    not (see this file's header). */
  const PkGetModuleFn get = (PkGetModuleFn)getmod;
  s_pk.module_arg = get(s_pk.manager, s_pk.module_id, PK_MODULE_INDEX);
  debugPrintf("[input] PhysicalKeyboard: ModuleManager::GetModule(%u, %u) -> 0x%08x (%s)\n",
              s_pk.module_id, PK_MODULE_INDEX, (unsigned)(uintptr_t)s_pk.module_arg,
              s_pk.module_arg ? "the engine has such an entry"
                              : "the engine has no such entry");
  if (!s_pk.module_arg) {
    /* The task's own fallback: the registry may hold the module under another
     * index (the engine registers index 0; another build may register more). */
    for (unsigned idx = 1; idx <= 4 && !s_pk.module_arg; idx++) {
      void *alt = get(s_pk.manager, s_pk.module_id, idx);
      debugPrintf("[input] PhysicalKeyboard: GetModule(%u, %u) -> 0x%08x\n", s_pk.module_id,
                  idx, (unsigned)(uintptr_t)alt);
      s_pk.module_arg = alt;
    }
    if (!s_pk.module_arg)
      return 0;
  }

  /* 5. THE CLASS, BY THE TABLE'S ENTRIES -- not by the first word against one
   *    class's symbol (which is what refused the module: see the header). The
   *    engine's GetModule hands back the module's +4 interface pointer, so the
   *    object is 4 bytes before it; BOTH candidates are judged, and the one
   *    whose table holds the engine's own entries, at their measured offsets,
   *    is the answer. */
  {
    const uint32_t p = (uint32_t)(uintptr_t)s_pk.module_arg;
    const uint32_t v_at_0 = *(const uint32_t *)(uintptr_t)p;
    const uint32_t v_at_m4 = *(const uint32_t *)((uintptr_t)p - 4);
    unsigned mask0 = 0, mask4 = 0;
    const int n0 = table_entries(v_at_0, &mask0);
    const int n4 = table_entries(v_at_m4, &mask4);
    log_match("the module object", p, n0, mask0);
    debugPrintf("[input] PhysicalKeyboard: the same check 4 B before it (0x%08x, the object "
                "itself when the manager hands back the +4 interface): %d of %d entries%s\n",
                (unsigned)v_at_m4, n4, PK_ENTRY_COUNT,
                n4 >= PK_ENTRY_MIN ? "  <- THIS is the object's own first word" : "");
    if (n4 >= PK_ENTRY_MIN) {
      debugPrintf("[input] PhysicalKeyboard: the manager hands back the +4 INTERFACE "
                  "pointer (its table is the class's SECONDARY address point: the -4 "
                  "offset-to-top, the typeinfo, then the Thn4_ thunks) -- the object is "
                  "4 B before it, 0x%08x\n",
                  (unsigned)(p - 4));
      s_pk.module_base = (char *)s_pk.module_arg - 4;
      s_pk.module_shift = -4;
    } else if (n0 >= PK_ENTRY_MIN) {
      debugPrintf("[input] PhysicalKeyboard: the manager hands back the object itself (its "
                  "first word is the class's PRIMARY address point)\n");
      s_pk.module_base = s_pk.module_arg;
    } else {
      /* Neither candidate is the module: SAY WHAT THE TWO TABLES ARE (the
       * task's own requirement) rather than follow a pointer that is not one. */
      debugPrintf("[input] PhysicalKeyboard: 0x%08x is NOT the PhysicalKeyboard module by "
                  "its entries (0x%08x holds %d of %d, and 0x%08x holds %d of %d): the "
                  "pad's keys cannot be delivered this way\n",
                  (unsigned)p, (unsigned)v_at_0, n0, PK_ENTRY_COUNT, (unsigned)v_at_m4, n4,
                  PK_ENTRY_COUNT);
      debugPrintf("[input] PhysicalKeyboard: what they are: 0x%08x -> 0x%08x (%s), "
                  "0x%08x -> 0x%08x (%s)\n",
                  (unsigned)p, (unsigned)v_at_0,
                  in_engine_image(v_at_0) ? "inside the engine's image" : "NOT the engine's",
                  (unsigned)v_at_m4,
                  (unsigned)(in_engine_image(v_at_m4) ? ((const uint32_t *)v_at_m4)[0] : 0),
                  in_engine_image(v_at_m4) ? "inside the engine's image" : "NOT the engine's");
      for (unsigned idx = 1; idx <= 4; idx++) {
        void *alt = get(s_pk.manager, s_pk.module_id, idx);
        unsigned m = 0;
        const int n = alt ? table_entries(*(const uint32_t *)alt, &m) : 0;
        debugPrintf("[input] PhysicalKeyboard: GetModule(%u, %u) -> 0x%08x: %d of %d "
                    "entries%s\n",
                    s_pk.module_id, idx, (unsigned)(uintptr_t)alt, n, PK_ENTRY_COUNT,
                    n >= PK_ENTRY_MIN ? "  <- THE MODULE" : "");
        if (alt && n >= PK_ENTRY_MIN) {
          s_pk.module_arg = alt;
          s_pk.module_base = alt;
          mask0 = m;
          break;
        }
      }
      if (!s_pk.module_base) {
        s_pk.module_arg = NULL;
        return 0;
      }
    }
  }

  /* 6. WHICH CLASS: named out of the engine's own vtable symbols, so the log
   *    says what was accepted rather than that something was. */
  {
    const uint32_t obj_vt = *(const uint32_t *)s_pk.module_base;
    debugPrintf("[input] PhysicalKeyboard: the object 0x%08x -> its class, per the engine's "
                "own vtable symbols: %s\n",
                (unsigned)(uintptr_t)s_pk.module_base,
                class_of_table(obj_vt, vt_pk, vt_android));
    if (vt_android)
      debugPrintf("[input] PhysicalKeyboard: (the address points the port knows: "
                  "PhysicalKeyboard 0x%08x, PhysicalKeyboardAndroid 0x%08x; the object's is "
                  "0x%08x%s)\n",
                  (unsigned)(vt_pk + 8), (unsigned)(vt_android + 8), (unsigned)obj_vt,
                  s_pk.module_shift ? " -- the manager's own pointer held the +4 "
                                      "interface's SECONDARY table instead" : "");
  }

  /* 7. The two calls, and whether Init has run (the mapper it builds). */
  s_pk.raw_down = (PkRawFn)so_try_find_addr_rx(&game_mod, PK_SYM_RAW_DOWN);
  s_pk.raw_up = (PkRawFn)so_try_find_addr_rx(&game_mod, PK_SYM_RAW_UP);
  s_pk.focus_gained = (PkVoidFn)so_try_find_addr_rx(&game_mod, PK_SYM_FOCUS);
  s_pk.set_enabled = (PkSetEnabledFn)so_try_find_addr_rx(&game_mod, PK_SYM_SETENABLED);
  {
    void *mapper = *(void **)((char *)s_pk.module_base + PK_OFF_MAPPER);
    const int en = enabled_of(s_pk.module_base);
    const int en_vt = isenabled_via_vtable(s_pk.module_base);
    debugPrintf("[input] PhysicalKeyboard: the module 0x%08x, its mapper at +%u = 0x%08x "
                "(Init ran: Keyboard::Create), the IsEnabled byte +%u = %d and the "
                "engine's own vtable[34] IsEnabled() = %d\n",
                (unsigned)(uintptr_t)s_pk.module_base, PK_OFF_MAPPER,
                (unsigned)(uintptr_t)mapper, PK_OFF_ENABLED, en, en_vt);
    debugPrintf("[input] PhysicalKeyboard: the calls: OnRawKeyDown @ 0x%08lx, OnRawKeyUp @ "
                "0x%08lx, OnLifeCycleFocusGained @ 0x%08lx, SetEnabled @ 0x%08lx%s\n",
                (unsigned long)s_pk.raw_down, (unsigned long)s_pk.raw_up,
                (unsigned long)s_pk.focus_gained, (unsigned long)s_pk.set_enabled,
                s_pk.raw_down && s_pk.raw_up ? ""
                                             : " -- ONE IS MISSING: the direct path is off");
    s_pk.last_enabled = en;

    /* 8. THE ENABLE. On a phone, onResume reaches the module and it enables
     *    itself. The port already delivers NativeOnResume(true) (fifa14_boot.c's
     *    step 10b), so this only fires when that did NOT reach this module --
     *    which is itself the measurement the task asks for. Both of the
     *    engine's own ways in are tried, in the order the engine uses them. */
    if (en != 1 && s_pk.focus_gained) {
      debugPrintf("[input] PhysicalKeyboard: the module is DISABLED (byte +%u = %d) -- "
                  "OnRawKeyDown would drop every key here; delivering the life-cycle call "
                  "the engine listens for (OnLifeCycleFocusGained = SetEnabled(true))\n",
                  PK_OFF_ENABLED, en);
      s_pk.focus_gained(s_pk.module_base);
      int after = enabled_of(s_pk.module_base);
      debugPrintf("[input] PhysicalKeyboard: after OnLifeCycleFocusGained the byte +%u = %d "
                  "(vtable[34]() = %d)\n",
                  PK_OFF_ENABLED, after, isenabled_via_vtable(s_pk.module_base));
      if (after != 1 && s_pk.set_enabled) {
        debugPrintf("[input] PhysicalKeyboard: still not enabled -- calling the byte's own "
                    "writer, SetEnabled(1) @0xdf17cc (vtable[35])\n");
        s_pk.set_enabled(s_pk.module_base, 1);
        after = enabled_of(s_pk.module_base);
        debugPrintf("[input] PhysicalKeyboard: after SetEnabled(1) the byte +%u = %d "
                    "(vtable[34]() = %d)\n",
                    PK_OFF_ENABLED, after, isenabled_via_vtable(s_pk.module_base));
      }
      s_pk.last_enabled = after;
    } else if (en == 1) {
      debugPrintf("[input] PhysicalKeyboard: the module is ENABLED (byte +%u = 1): what "
                  "the pad delivers reaches OnRawKeyDown\n",
                  PK_OFF_ENABLED);
    }
    debugPrintf("[input] PhysicalKeyboard enabled field = %d\n",
                enabled_of(s_pk.module_base));
  }

  s_pk.ok = 1;
#if FIFA14_INPUT_KEY_DIRECT
  if (s_pk.raw_down && s_pk.raw_up)
    debugPrintf("[input] PhysicalKeyboard: the pad's keys ALSO go in by DIRECT calls on the "
                "module (source/fifa14_physkbd.h's FIFA14_INPUT_KEY_DIRECT %d): on that path "
                "the engine's message routing is out of the way, so the game's answer to the "
                "pad is the answer to \"was it the routing?\"\n",
                (int)FIFA14_INPUT_KEY_DIRECT);
  else
    debugPrintf("[input] PhysicalKeyboard: FIFA14_INPUT_KEY_DIRECT is on but a call is "
                "missing: that path is off\n");
#endif
  return 1;
}

int fifa14_physkbd_ready(void) {
  return s_pk.ok;
}

int fifa14_physkbd_enabled(void) {
  return s_pk.ok ? enabled_of(s_pk.module_base) : -1;
}

/* The object exactly as the engine's own API hands it back (the +4 interface
 * pointer), and the object itself (what OnRawKeyDown's `this` has to be).
 * NULL until init() has the module. */
void *fifa14_physkbd_module(void) {
  return s_pk.ok ? s_pk.module_arg : NULL;
}
void *fifa14_physkbd_object(void) {
  return s_pk.ok ? s_pk.module_base : NULL;
}
int fifa14_physkbd_shift(void) {
  return s_pk.ok ? s_pk.module_shift : 0;
}

void fifa14_physkbd_raw_key(int down, int android_key, int alt) {
  if (!s_pk.ok)
    return;
  PkRawFn fn = down ? s_pk.raw_down : s_pk.raw_up;
  if (!fn)
    return;
  fn(s_pk.module_base, (unsigned)android_key, alt);
  s_pk.n_calls++;
  if (down)
    s_pk.n_down++;
  else
    s_pk.n_up++;
  if (!s_pk.logged_first_key) {
    s_pk.logged_first_key = 1;
    debugPrintf("[input] the first DIRECT key: %s(%u, alt %d) on the module 0x%08x -- "
                "the engine's own call, no message in between\n",
                down ? "OnRawKeyDown" : "OnRawKeyUp", (unsigned)android_key, alt,
                (unsigned)(uintptr_t)s_pk.module_base);
  }
}

unsigned fifa14_physkbd_direct_calls(unsigned *down, unsigned *up) {
  if (down)
    *down = s_pk.n_down;
  if (up)
    *up = s_pk.n_up;
  return s_pk.n_calls;
}

void fifa14_physkbd_report(void) {
  if (!s_pk.ok)
    return;
  s_pk.frames++;
  const unsigned every =
      s_pk.frames <= FIFA14_PKBD_REPORT_EARLY ? FIFA14_PKBD_REPORT_FRAMES : FIFA14_PKBD_REPORT_SLOW;
  const int en = enabled_of(s_pk.module_base);
  const int changed = en != s_pk.last_enabled;
  if (!changed && (every == 0 || s_pk.frames % every != 0))
    return;
  s_pk.last_enabled = en;
  /* THE TASK'S OWN LINE: the module's enable field, and the engine's own
   * IsEnabled() next to it (they are the same byte, read two ways). */
  debugPrintf("[input] PhysicalKeyboard enabled field = %d (byte +%u; the engine's own "
              "vtable[34]() IsEnabled = %d, visible +%u = %d); direct calls %u = %u down "
              "+ %u up, module 0x%08x%s\n",
              en, PK_OFF_ENABLED, isenabled_via_vtable(s_pk.module_base), PK_OFF_VISIBLE,
              (int)((const uint8_t *)s_pk.module_base)[PK_OFF_VISIBLE], s_pk.n_calls,
              s_pk.n_down, s_pk.n_up, (unsigned)(uintptr_t)s_pk.module_base,
              changed ? "  <- the field CHANGED since the last line" : "");
}
