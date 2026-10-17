# FIFA 14 (Switch, android32) — LAS TECLAS DEL MANDO, SOLO EN EL PARTIDO

Ronda del 2026-10-13 (quinta del día). Objetivo: que las teclas del mando se
entreguen **solo durante el partido**, con una palanca
`config.ini [controls] pad_keys_scope = match | always` (por defecto `match`).

Todo lo de abajo está **MEDIDO** en `libFIFA14.so` (18.187.340 B, el mismo
fichero de siempre: `sha256 cd15bbe4…`, leído del árbol y del OBB) salvo lo que
lleve **SIN CONSOLA**. Los ficheros tocados, los sha256 de los artefactos y la
lista de lo que solo la consola puede confirmar, al final.

---

## 0. RESUMEN EN UNA FRASE

La palanca está puesta y el mecanismo es **fail-open**: con `match` las teclas
del mando (los keycodes del motor) se entregan **solo mientras el motor tiene un
objeto de partida vivo**, que el port lee de **un puntero del propio
`libFIFA14.so`** (`.bss`, VA `0x11e0c84`, leído por el motor en `0x46a564`),
**verificado palabra por palabra antes de usarlo**; si la verificación no cuadra,
el port **no gatea nada** y entrega todas las teclas como siempre. La capa
táctil del port (stick derecho = cursor, StickR/StickL) y `+/-` (Android BACK)
**no** están en el scope: los menús se manejan con eso.

---

## 1. LO QUE SE BUSCÓ Y NO ESTÁ (la evidencia negativa, que es la que manda)

El usuario tenía razón en lo del contexto (`los botones cambian si tengo o no el
balón`), así que lo primero fue intentar leer **el estado contextual del propio
motor**. Y no se puede:

1. **El vocabulario de contextos del motor está en el fichero pero NO LO
   REFERENCIA NADIE.** Las 13 cadenas
   `BUTTON_MODE_NOCHANGE / NONE / WITH_POSSESSION / AIR_BALL / OPPONENT_POSSESSION
   / FREE_BALL / OUTOFPLAY / THROWIN / THROWIN_OPPONENT / PENALTY /
   OOP_OPPONENT_POSSESSION / GOALIE_HOLDING / WITH_POSSESSION_NOT_DRIBBLER`
   están en `0x1025de5`–`0x1025f0c` (una sola copia cada una; el bloque entero
   está entre `aicntrl.cpp`/`aictl.cpp` y `OPTIONS/BUTTON_CONFIG%d` →
   `SoccerAICore/game/control/`). **Barrido del fichero entero: no hay ni una
   palabra de 32 bits que resuelva a ninguna de ellas** por el esquema que el
   propio motor usa para las cadenas (`dirección = 0x10e77d8 + palabra`, con
   0x10e77d8 = DT_PLTGOT), **ni puntero directo, ni entrada en `.rel.dyn`**. Las
   cadenas vecinas (`OPTIONS/BUTTON_CONFIG%d`, `DFK_DRIVEN_POWERUP_TIME`,
   `PENALTY_CONTROLS/*`…) sí tienen referencias medidas (p. ej.
   `OPTIONS/BUTTON_CONFIG%d` en `0x89ea40`). Conclusión: en ESTE build los
   nombres de contexto son cadenas muertas; al motor no se le puede preguntar
   por ellos.
2. **No hay símbolo exportado "de partido".** Barrido del dynsym: los únicos
   `match*` son del `Blaze::GameManager` (online). No hay `IsInMatch`,
   `IsPlaying`, `InGame`…
3. **El estado del HUD / overlays vive en Flash, no en nativo.**
   `OpenOverlay` / `ControllerOverlay` / `CloseAllOverlays` (`0x1001a8f`…)
   se llaman por nombre desde `scriptfuncs.cpp` a través de
   `AptCallFunction @0xa8b594` — es una tabla de funciones ActionScript.
4. **Las claves del esquema de control no son un flag de partido.** Las
   sincroniza el código de ajustes del jugador (`0x7460cc` / `0x7461b4`, y
   `0x746a30` copia `_LOCAL`→`_P0/P1`), que también corre al cambiar ajustes:
   `Has("CONTROL_CONFIG/SCHEME_ID_LOCAL")` puede quedarse a 1 para siempre tras
   el primer partido.

## 2. LA SEÑAL QUE SÍ SE USA: EL OBJETO DE PARTIDA DEL MOTOR

`libFIFA14.so` guarda **un puntero** (a un objeto grande, campos en `+0xc789`,
`+0xc81c4`, `+0xc974` — un objeto de >50 KB) en su propia `.bss`, VA
**`0x11e0c84`**, y su propio código lo lee, una y otra vez, así (MEDIDO, son las
palabras del fichero):

```
46a564  ldr  r0, [pc, #36]   @ 0x46a590 = 0x00c7d264   -> r0 = 0x10e77d8 (la GOT del motor)
46a568  ldr  r1, [pc, #36]   @ 0x46a594 = 0xffffa248   -> el índice del slot (GOT-24008)
46a56c  add  r0, pc, r0      ; r0 = GOT
46a570  ldr  r0, [r1, r0]    ; r0 = &(el objeto)    [slot 0x10e77d8-0x5db8 = 0x10e1a20]
46a574  ldr  r0, [r0]        ; r0 = el objeto de partida (o 0)
46a578  cmp  r0, #0
46a57c  movne/strne …        ; if (G) { …juego… }   (aquí: G->[0xc81c4] = 1)
```

* **343 sitios** usan ese puntero con la MISMA forma `if (G) { … }`, medidos
  por todo el binario: el camino de "family play aplica el esquema"
  (`0x497054`: `esquema = G->[0xc974]` y se monta con `0x49c838`), el código de
  partido/HUD (`0x64784c`, `0x77b95c`, `0x781dc0`…), toda la banda de
  `SoccerAICore` (`0x88e18c`–`0x8a039c`) y la banda del HudController
  (`0x6e7dc0`–`0x7046bc`).
* Lo que **NO** está probado y no hace falta que lo esté: que el puntero sea
  **NULL en los menús**. Eso lo decide la primera corrida (las líneas `[pad]`
  del log lo imprimen en cada cambio). Este es el motivo de que el gate sea
  fail-open y de que exista `always`.

### Cómo lo lee y cómo lo VERIFICA el port (antes de usarlo)

`source/fifa14_padscope.c`, en `fifa14_padscope_init()` (una vez, cuando el
motor ya está mapeado) — **tres comprobaciones sobre la imagen mapeada**, y si
alguna falla la señal es UNKNOWN y **el gate queda abierto**:

1. las **cinco palabras de instrucción** del motor en `0x46a564` son las del
   fichero (`e59f0024 e59f1024 e08f0000 e7910000 e5900000`) y las **dos palabras
   del pool** (`0x00c7d264`, `0xffffa248`);
2. el **slot de la GOT** `0x10e1a20` contiene exactamente
   `base + 0x11e0c84` (el `R_ARM_RELATIVE` con addend `0x11e0c84` que aplica el
   propio loader del port: `so_util.c` líneas 278/295, `bias = load_virtbase`);
3. todas las direcciones que toca están **dentro de la imagen cargada**
   (`load_virtbase .. + load_size`), como en `fifa14_physkbd.c`.

A partir de ahí, la consulta es **una lectura de 32 bits por frame** (a lo sumo
2-3 con la cuenta), sin llamar al motor y sin escribir nada. El log:

```
[pad] ==== the pad's key scope: is there a game to give a key to? ====
[pad] the signal: the engine's own game object pointer, VA 0x011e0c84 in its .bss,
     read by the engine at 0x0046a564 through the GOT slot 0x010e1a20 …
[pad] the engine's own read at 0x0046a564: 0xe59f0024 … (las del fichero), pool …
[pad] the GOT slot 0x010e1a20 = 0x… (el loader debía poner 0x…)
[pad] VERIFIED: the engine's game object 0x011e0c84 -> 0x… (there IS a game … / there is NO game …)
[pad] the engine's game object 0x011e0c84 = 0x… -> a game is live / no game  (en cada cambio)
[input] the pad's keys: GO IN to the engine / are HELD BACK by the scope …
```

### Qué se gatea exactamente

| Camino | ¿Scopeado? |
|---|---|
| A/B/X/Y, L/R, ZL/ZR, cruceta, **stick izquierdo como cruceta** (los keycodes del motor) | **sí** (`match`: solo con partida viva) |
| `+` / `-` como teclas del motor (START/SELECT) | **sí** (van por el mismo `poll_pad_keys`) |
| Stick derecho = cursor táctil, StickR = click, StickL = segundo dedo | **no** (los menús se manejan ahí) |
| `+`/`-` → Android BACK (`NativeOnBackPressed`) | **no** (es "volver" de los menús) |

Cuando el gate está cerrado: cada borde de tecla que se habría entregado se
**cuenta** (`n_withheld`), y cualquier tecla que estuviera pulsada se **suelta**
(el mesmo key-up que Android da al perder la ventana), y `keys_held` se mantiene
sincronizado para que una tecla ya pulsada no dispare un borde tardío al abrir.

---

## 3. LO QUE HAY QUE MIRAR EN LA PRÓXIMA CORRIDA (checklist)

1. `[pad] VERIFIED` en el arranque → la señal existe y el gate puede actuar.
   Si sale "the addresses are NOT all inside the engine's image" o "IS NOT that
   build of the game", el port **no** gatea (entrega todo) y lo dice.
2. En los **menús**: `[pad] … -> no game (pad keys are held back)` y
   `[input] the pad's keys: are HELD BACK by the scope`; los botones del mando
   no deben hacer nada raro en la UI, y el cursor (stick derecho) + click
   (StickR) + BACK (+/-) deben seguir funcionando.
3. En el **partido**: `[pad] … -> a game is live (pad keys go in)` y
   `[input] the pad's keys: GO IN to the engine`; las teclas deben volver.
4. Si en el partido sale "no game" (o el puntero no cambia nunca), poner
   `pad_keys_scope = always` en `config.ini` — un tirón, sin recompilar.

`config.ini` (el runtime escribe la fila con su ayuda; esto es lo que se toca):

```ini
[controls]
pad_keys_scope = match      # match | always
```

---

## 4. FICHEROS TOCADOS

| fichero | qué |
|---|---|
| `source/fifa14_padscope.h` | nuevo: la API de la señal (`init` / `match` / `report`) |
| `source/fifa14_padscope.c` | nuevo: la cadena medida, la verificación y la lectura |
| `source/fifa14_config.c` | fila `[controls] pad_keys_scope` (default `match`) + ayuda |
| `source/fifa14_input.c` | el gate (`key_gate`), la suelta al cerrarse, el log del scope, `fifa14_padscope_init()` en el arranque del input y `fifa14_padscope_report()` en el bucle |
| `source/fifa14_input.h` | nota del scope en la cabecera |
| `tools/syntax-check.sh` | comprueba también `fifa14_padscope.c`, `fifa14_config.c`, `fifa14_ctlcfg.c` |

No se ha tocado nada de: mando (mapeo), tacto, render/swap, locale, share,
audio, `rt_pad.c`, `rt_window.c`, ni los `.sh` de build.

## 5. ARTEFACTOS Y VERIFICACIÓN (hecha en este PC)

```
payload   fifa14_nx.nsp                 3.846.585 B  sha256 bb9b4d5a2d1b1a3dcaeee7e6480fe609554ea80dbe823f18a7610fdd0908df80
romfs     launcher/romfs/fifa14_nx.nsp  3.846.585 B  sha256 bb9b4d5a…  (== payload)
NSO       build/fifa14_nx.nso           3.845.517 B  sha256 e25ed929413c122129d5986821540ed5fa6a73410d8d220fc392be33b4454575
lanzador  launcher/fifa14_nx.nro        4.115.745 B  sha256 49d236a2dde872c237e09e2509b9a7279447c63a7458bdc328a781eee26cdb2d
NSP directo installer/fifa14-direct.nsp 4.072.152 B  sha256 57c0ddd2b181b7019aec01eb541b26a901a7818be8a7355b6509313dfacbdc32
```

* `./build-fixes.sh`: payload rc=0, launcher rc=0, **0 líneas `error:`**, 0
  warnings en los dos logs.
* `payload VERBATIM dentro del .nro`: sí (offset 269013); `.nro` más nuevo que
  el payload (`23:24:05` vs `23:24:03`). Lo comprueba `tools/verify-artifacts.py`
  → `verify-scope.json` (`sha_match`, `payload_verbatim_in_nro`, `error_lines: []`).
* **La cadena de identidad, completa**: el NSO dentro de `fifa14_nx.nsp` es
  **byte a byte** `build/fifa14_nx.nso`; `main` dentro de `fifa14-direct.nsp`
  tiene su mismo sha256 (`e25ed929…`); y el `.elf` de esta build (de donde sale
  ese NSO) contiene ya las cadenas nuevas (`pad_keys_scope`, `[pad] …`). El NSP
  directo se regeneró con `mk_override2.py` + `ATMO_CONTENTS=installer/.local-atmo/contents
  python3 installer/build-installer-nsps.py direct`, y su ExeFS quedó
  **idéntico** al override nuevo (`verificación [2]: ExeFS identico override: True`).
* Nº de build: `202610140523`.

**SIN CONSOLA** (y con la tarjeta D: fuera de este PC): si el puntero del motor
es NULL en los menús y no-NULL en el partido (la checklist de la sección 3 lo
dirá), y si las teclas del mando de verdad dejan de hacer nada en los menús.
La primera corrida lo contesta sin tocar nada más.
