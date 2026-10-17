# fifa14_nx -- NOTES

FIFA 14 (EA Mobile) on Nintendo Switch, through the **android32** runtime
(AArch32 / 32-bit; the pilot ports are `aks796/dcr_sea_nx` and
`boraeskicioglu/tasm2_nx`). This repository is the **skeleton**: the wrapper's
files and the engine's facts are in place; the parts that need the running
runtime or the engine's offsets are marked TODO here and in the sources.

This is an unofficial fan project. No game code, APK or OBB is included.

---

## 1. The game, and the runtime's identity

| field | value |
|---|---|
| Game | FIFA 14 (EA Mobile) |
| Engine | **EA NIMBLE** (system + game framework, in `libnimble.so`) |
| Package | `com.ea.game.fifa14_row` |
| APK version | 1.3.6, **versionCode 136** |
| ABI | `armeabi-v7a` (ELF32 ARM, softfp) |
| `TARGET` / `PORT_NAME` | `fifa14_nx` |
| `PORT_NPDM_PROGRAM_ID` | **`0x0100000000001014`** |
| `PORT_NPDM_VERSION` | 0.0.3 (runtime default) |
| `PORT_SO_REGION_BYTES` | `64u * 1024 * 1024` (measured sum 20.95 MiB; see below) |
| Expansion file | `main.136.com.ea.game.fifa14_row.obb` -- 1,386,236,976 B |

### 1.1 Why `0x0100000000001014`

The runtime gives every port one id in its reserved `0x01000000000010xx` block
(`runtime/runtime.mk`, `docs/notes/E_system.md`). The ids already taken by the
user's own ports are:

| id | port |
|---|---|
| `0x010000000000100E` | pvz_nx |
| `0x010000000000100F` | abspace_nx **and** a8retry_nx (they share it) |
| `0x0100000000001010` | labyrinth2_nx |
| `0x0100000000001012` | sonicracing_nx |
| `0x0100000000001013` | dcrsea_nx |
| `0x0100000000001099` | `_selfbuild` (the runtime's own self-test) |
| `0x055A3A4744DC9001` | tasm2_nx (a custom id, outside the block) |

`0x0100000000001014` is **free**: it is not among the above, it is not the
`_selfbuild` test id, and the whole `0x0100000000001000`-`0x...1FFF` range is not
used by any retail Nintendo title (retail application ids are of the
`0x0100XXXXXXXXX000` form, assigned individually; this range is the wrapper
runtime's homebrew block). If the block is ever exhausted, any non-retail id
works, but reusing one of the above would collide inside the user's own set.

---

## 2. The three libraries, and the load order

From `lib/armeabi-v7a/` in the APK:

| library | size | role |
|---|---|---|
| `libgnustl_shared.so` | 1,142,099 B | GNU libstdc++ (the C++ runtime) |
| `libnimble.so` | 352,780 B | **EA NIMBLE** (the engine), exports the `EA::Nimble` symbols |
| `libFIFA14.so` | 18,187,340 B | the game |

**Load order (verified from `DT_NEEDED`):**

```
libgnustl_shared.so  ->  libnimble.so  ->  libFIFA14.so
```

* `libFIFA14.so` NEEDED `libgnustl_shared.so` and `libnimble.so`.
* `libnimble.so` NEEDED `libgnustl_shared.so`.

This matters beyond correctness of the graph: all three must be **loaded (staged)
before any of them is resolved**, because `libFIFA14.so` binds its `EA::Nimble`
imports to a *loaded* `libnimble.so`. `port_load()` (source/fifa14_main.c) loads
all three, then resolves all three, then finalizes. Resolving the game before
the engine is in memory would leave every `Nimble` import unresolved.

---

## 3. The 122 `EA::Nimble` imports that force `libnimble.so` to be real

`libFIFA14.so` has 589 undefined dynamic symbols; **122 of them name the engine**
(`EA::Nimble`):

| mangling | count |
|---|---|
| `_ZN2EA6Nimble*` (methods) | 77 |
| `_ZNK2EA6Nimble*` (const methods) | 44 |
| `_ZTVN2EA6Nimble*` (a vtable) | 1 |
| **total** | **122** |

Examples: `_ZN2EA6Nimble8Facebook8Facebook12getComponentEv`,
`_ZNK2EA6Nimble4Json5Value4typeEv`.

They **cannot be stubbed.** They are the engine's own classes, with C++ object
layout and vtables; a hand-written shim would have to reproduce the whole NIMBLE
ABI. So `libnimble.so` is loaded as a **module** (not shimmed) and the loader
binds the 122 imports to its exported symbols. This is stated in:

* `tools/imports.cfg` (why `libnimble.so` is a `MODULE`, and must not end up a
  `b_` shim);
* `source/config.h` (`FIFA14_NIMBLE_IMPORT_COUNT 122` and the note);
* `source/fifa14_main.c` (the load-all-then-resolve-all order and why).

`gen_imports.py` leaves these symbols out of `dcr_imports[]` on purpose; a
build that somehow stubbed or dropped them would fail loudly at
`so_resolve()`.

---

## 4. Engine entry points (verified in `libFIFA14.so`)

| symbol | file offset | role |
|---|---|---|
| `Java_com_ea_blast_MainActivity_NativeOnCreate` | `0x00dffc8c` | the real boot |
| `Java_com_ea_blast_AndroidRenderer_NativeOnDrawFrame` | `0x00e003ec` | the frame tick |
| `JNI_OnLoad` | `0x00e005d8` | the JNI handshake |

The names are the exports the loader resolves; the raw offsets are recorded in
`source/config.h` for the symbol-table-independent path (`fifa14_offsets.h`, not
derived yet). The package in the symbol names is `com.ea.blast`; the runtime
package is `com.ea.game.fifa14_row`.

---

## 5. The three files the user must put on the SD card

Into `sd:/switch/fifa14_nx/`:

```
sd:/switch/fifa14_nx/
|-- fifa14_nx.nro                                   (the launcher; built later)
|-- <the FIFA 14 APK>                               (any name; adopted to FIFA14.apk)
|-- main.136.com.ea.game.fifa14_row.obb             (1,386,236,976 B)
`-- (written on the first start: FIFA14.apk, libgnustl_shared.so, libnimble.so,
     libFIFA14.so, classes.txt, .setup, config.ini, data/, debug.log)
```

So the player supplies **three** files: the **APK**, the **main OBB**, and the
**NRO** (from the release). The APK:

* package `com.ea.game.fifa14_row`, **versionCode 136**;
* `armeabi-v7a` libraries `libFIFA14.so`, `libnimble.so`, `libgnustl_shared.so`
  under `lib/armeabi-v7a/` (this is how `PORT_APK_ROLES` recognises it).

The OBB is named by the versionCode (`main.136...`); the game reads it from
`Android/obb/<pkg>/`, which the runtime's path map can route to the game folder.

---

## 6. What is NOT done yet (checklist)

Ordered roughly by how early the next step blocks the rest.

- [ ] **`source/imports.c` and `tools/imports_needed.txt`.** Not generated:
  `gen_imports.py --libs <apk>/lib/armeabi-v7a` must be run on the user's APK
  first. Its leftovers (missing `b_` shims, ABI-compatible `PASSTHROUGH`,
  bionic `DATA` objects, weak `WEAK_NULL`) get added to `tools/imports.cfg` and
  re-run until it is clean.
- [ ] **The 32-bit bionic shims the engine needs.** The runtime's `bionic_*.c`
  cover the common set, but NIMBLE/EABlast's particular libc/libm/libdl/liblog/
  libEGL/libGLESv2 imports must be checked against them and any missing `b_`
  shim (or port overlay in `port_imports[]`) written. Nothing is verified yet.
- [ ] **Mesa / the GL layer.** `DCR_GL_MESA=1` (mesa32 in `portlibs32/`) serves
  the `gl*`/EGL imports the runtime's `gl_mesa.c`/`gl_layer.h` know; the engine's
  own GL entry points reached another way need port hooks, and the renderer
  choice is untested. The `gl*` set the engine imports is not audited yet.
- [ ] **`fifa14_offsets.h` (the engine offsets/patches).** None derived. Needs a
  symbol-table-independent reading of `libFIFA14.so` 1.3.6 (arm32 offsets), the
  `dcr_offsets.h`/`tasm2_offsets`-style file, and the patches it enables
  (`FIFA14_PATCH_GL_SWAP`, `FIFA14_TIME_ICALL_HOOKS`, ...). Not applied anywhere.
- [ ] **The OBB** (`main.136.com.ea.game.fifa14_row.obb`, 1.39 GB). Not mapped,
  not cached, not read. Needs the path map (`RT_PATH_EXTRA_DIRS` already routes
  `Android/obb/<pkg>`), a direct reader, and the size check
  (`FIFA14_OBB_BYTES`) so a truncated transfer cannot boot.
- [ ] **`__gnu_Unwind_Find_exidx`.** `libgnustl_shared.so` is ARM EABI with
  exception handling; Horizon has no `dl_iterate_phdr`, so the C++ unwinder's
  exidx lookup must be served (a port hook over the loaded module, as the
  pilot's Mono/Unity ports needed). Not written.
- [ ] **The JNI environment and the class refs.** `JNI_OnLoad`, `NativeOnCreate`
  and the `NativeOnDrawFrame` loop are sketched in `source/fifa14_main.c` but
  need the runtime's JNI layer (`jni_proxy.c`), the `com/ea/blast/MainActivity`
  and `com/ea/blast/AndroidRenderer` class refs, and a `JNI_OnLoad` check
  (`0x00010004`). `port_run()` currently only runs the gnustl/nimble
  constructors and prints a skeleton line.
- [ ] **Audio.** `RT_OPENSLES` is provisionally 0 (the Java/AudioTrack fallback);
  the engine's real audio path is not traced. `port_audio_selftest()` is the
  runtime default.
- [ ] **Input.** The Switch pad -> the engine's touch/pad input is not mapped.
  `RT_PAD_MAX_PLAYERS 1` (co-op unconfirmed).
- [ ] **Focus / lifecycle callbacks.** `port_focus_lost/gaining/gained`,
  `port_watchdog_hold` are the runtime defaults; the engine's pause/resume and
  save on HOME are not implemented.
- [ ] **The launcher.** `launcher/Makefile` (`APP_*`, `TARGET` = `PORT_NAME`,
  `PORT_PAYLOAD` = the wrapper's `TARGET`, then `include
  $(A32)/launcher/launcher.mk`), `launcher/build.sh` and `icon.jpg` are not
  present yet.
- [ ] **`PORT_SO_REGION_BYTES` on hardware.** 64 MiB is a safe over-reserve for
  the measured 20.95 MiB; confirm no other module is dlopened that would grow it.
- [ ] **Build.** Nothing has been compiled. `runtime/` must be linked in (a
  symlink in development; a submodule on GitHub) and the first `./build.sh` run
  in the toolchain container (`devkitARM` + `libnx32` + mesa32 in `portlibs32/`).
- [ ] **`config.ini` table.** No `dcr_config_load()` / `CfgTable`
  (`docs/examples/dcr_config_table.c`) yet; the runtime's own rows
  (`rt_cfg.h`) would do for a first boot.

---

## 7. Layout

```
Makefile              TARGET + PORT_NPDM_PROGRAM_ID + include runtime/runtime.mk
build.sh              exec runtime/tools/docker_build.sh
.gitignore
source/port_config.h  the runtime's settings (macros only; the assembler reads it)
source/config.h       the port's own constants (modules, entry points, OBB)
source/fifa14_main.c  port_load() / port_run()
source/fifa14_storage.c  port_setup_plan (which .so to unpack) + port_apk_help()
tools/imports.cfg     MODULES + the Nimble note
NOTES.md              this file
```

Referenced but not created yet: `source/imports.c`, `tools/imports_needed.txt`,
`source/fifa14_offsets.h`, `launcher/`.
