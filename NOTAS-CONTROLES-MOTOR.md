# FIFA 14 (Switch, android32) — EL ESQUEMA DE CONTROL: la via del motor, y donde vive

Ronda del 2026-10-13 (cuarta del dia). Lo **MEDIDO** sale de: `libFIFA14.so`
(18.187.340 B, el mismo fichero de siempre, desensamblado con
`arm-none-eabi-objdump` y un resolvedor de referencias PIC propio), los ficheros
del juego (el OBB: `ini/na/controls.ini`, `ini/na/common.ini`, y la tabla de
1217 entradas) y **la prueba del patcher hecha en este PC** (el codigo del port
compilado para el PC y ejecutado contra copias reales de los dos ini). Lo que no
se ha podido ejecutar va marcado **INFERIDO** / "sin consola".

La pregunta era: *sobre los mandos, no puede ser implementado de alguna manera
modificando el motor?* **SI**, y esta ronda lo deja en el `config.ini` y lo mide.

---

## 0. RESUMEN EN UNA FRASE

Los botones en pantalla **no son una capacidad del dispositivo** (el registro de
los modulos de tacto es incondicional, ya medido): son parte del **esquema de
control** que el motor lee de **sus propios ajustes**, y esos ajustes estan en
**dos ficheros que este port sirve desde el OBB**: `ini/na/controls.ini`
(`[CONTROL_CONFIG]`: SCHEME_ID_P0/P1, LAYOUT_ID, RENDER_DEBUG, USE_SIDE_BUTTONS)
y `ini/na/common.ini` (`[SETTINGS]`: FLOATING_DPAD, BASIC_HAND, BIG_BUTTONS,
BIG_DPAD, ALTERNATE_CONTROLS...). El port ahora los **escribe** desde
`config.ini [controls]` (mismo mecanismo que `[audio] language`) y **mide** los
valores efectivos dos veces: los ficheros que sirve y **la respuesta del propio
motor** (sus lectores del registro, llamados por direccion medida y verificada).

---

## 1. LOS BOTONES: LA CADENA MEDIDA (todo con su direccion)

### 1a. Las claves, en el `.so`

| clave | offset | quien la lee (sitio de la instruccion que calcula la cadena) |
|---|---|---|
| `CONTROL_CONFIG/FLOATING_DPAD` @0x1003036 | | 0x4932a4 (UI de ajustes), 0x49b088/0x49b6f0/0x49bfb0, 0x49c8b8 (**montaje del partido** 0x49c838), 0x55caec (**cargador de ini** 0x55c6c0), 0x74612c/0x746214 (jugador local) |
| `CONTROL_CONFIG/BASIC_HAND` @0x100306a | | idem en 0x4932c4, 0x49b0b4, 0x49b758, 0x49bfcc, 0x49c8d8, 0x55cb0c, 0x74613c, 0x746228 |
| `CONTROL_CONFIG/BIG_BUTTONS` @0x1003098 | | 0x4932e4, 0x49b02c, 0x49b7c0, 0x49bf78, 0x49c918, 0x55cb2c, 0x74610c, 0x7461ec |
| `CONTROL_CONFIG/BIG_DPAD` @0x10030c8 | | 0x493304, 0x49b05c, 0x49b828, 0x49bf94, 0x49c8f8, 0x55cb4c, 0x74611c, 0x746200 |
| `CONTROL_CONFIG/ALTERNATE_CONTROLS` @0x10030f2 | | 0x493324, 0x49b164, 0x49b8f8, 0x49c004, 0x49c978, 0x55cb6c, 0x74614c, 0x74623c |
| `CONTROL_CONFIG/SCHEME_ID_P0` @0x1003c97 | | 0x499cac, 0x49bf44, 0x5c662c, 0x5c66c8, 0x7460ec, 0x7461d4, 0x746334, 0x746a70 |
| `CONTROL_CONFIG/SCHEME_ID_P1` @0x1003cb3 | | 0x499cd0, 0x49bf58, 0x5c65bc, 0x5c663c, 0x5c66e4, 0x7460fc, 0x746a8c |
| `CONTROL_CONFIG/SCHEME_ID_LOCAL` @0x101f553 | | 0x746298, 0x746344, 0x746a40, 0x746a5c |
| `CONTROL_CONFIG/LAYOUT_ID` @0x100419d | | 0x49c8a0 |
| `CONTROL_CONFIG/RENDER_DEBUG` @0x10041b6 | | 0x49c938 |
| `CONTROL_CONFIG/RENDER_DEBUG_SETPIECE` @0x10041d2 | | 0x49c958 |
| `CONTROL_CONFIG/USE_SIDE_BUTTONS` | | **no existe como cadena en el .so** (esta en `controls.ini`, pero el motor no la pregunta por nombre) |

Las funciones, por lo que son:
* **0x49c838** = el lector de **montaje del partido**: lee LAYOUT_ID,
  RENDER_DEBUG, RENDER_DEBUG_SETPIECE, FLOATING_DPAD, BASIC_HAND, BIG_BUTTONS,
  BIG_DPAD, ALTERNATE_CONTROLS de una vez. Llamado desde 0x49708c, 0x49c06c,
  0x55e130, 0x55e548, 0x5c67c0.
* **0x7460cc / 0x7461b4** = los del **jugador local** (SCHEME_ID_P0/P1/LOCAL,
  FLOATING_DPAD...), llamados desde 0x64045c y 0x64050c (codigo de partido).
* **0x55c6c0** = el **cargador de las 12 ini**: el .so lleva la tabla de nombres
  en 0x10afc50 (`common.ini, ai.ini, sku.ini, locale.ini, emotion.ini,
  career.ini, training.ini, player.ini, controls.ini, fame.ini, easfc.ini,
  fut.ini`) y construye cada ruta con `ini/%s/%s` (0x10085bf, el `REGION` sale
  de la config); su llamador 0x55bdec es el bloque de ini (lee `data/cmn/ini.big`).
  Despues publica los valores en el registro (0x34047c = set, 0x340648 = get).
* **0x493280** = la **UI de ajustes** (cada pareja CONTROL_CONFIG/x con
  SETTINGS/x: la opcion se escribe con los dos nombres).
* **0x49b494** = la pantalla de ajustes: compara el valor del registro con el del
  perfil y refresca las etiquetas ("Floating Joystick", "Large Buttons",
  "Large Joystick", "Left Handed", "Alternate Controls", "Action Feedback"...).

### 1b. Los ficheros, tal y como los sirve el OBB (MEDIDO, leidos del OBB)

`ini/na/controls.ini` (2.700 B), `[CONTROL_CONFIG]`:
`SCHEME_ID_P0 = 0`, `SCHEME_ID_P1 = 0` (el fichero lo dice: 0 = dos botones +
sprint, 1 = tres botones), `LAYOUT_ID = 0`, `RENDER_DEBUG = 0`,
`RENDER_DEBUG_SETPIECE = 0`, `USE_SIDE_BUTTONS = 0`.
Y ademas `[CONTROL_TIMEOUTS]`, `[CONTROL_POWER]`, `[CONTROL_GAMEPLAY]`,
`[CONTROL_GUI]`, `[CONTROLS]` (MANUAL_*), `[PENALTY_CONTROLS]`,
`[FREEKICK_CONTROLS]`.

`ini/na/common.ini` (14.447 B), seccion `[SETTINGS]` (lineas 202-285):
`FLOATING_DPAD = 0`, `BASIC_HAND = 0`, `BIG_BUTTONS = 1`, `BIG_DPAD = 1`,
`ALTERNATE_CONTROLS = 0`, `ACTION_TAKEN_TEXT_ENABLED = 0`,
`ACTION_SKILL_NAME_ENABLED = 1`, `ACTION_SKILL_GRID_ENABLED = 1`,
`ASSISTED_TACKLE = 1`, `FIRST_TOUCH = 1`, `SNAP_2ND_DEF_ON_SWITCH = 0`,
`MANUAL_GROUNDPASS = 0`, `AUTO_SWITCH_LEVEL0/1 = 30`,
`AUTO_SWITCH_TOUCH_LEVEL0/1 = 100`, `TUTORIAL_DPAD = 1`, `TUTORIAL_TOUCH = 1`.
Estos son los nombres `SETTINGS/x` que el .so tambien pregunta.

El puerto ya sirve esos dos ficheros desde el OBB a
`<game>/external/ini/na/...`, asi que **escribirlos en la tarjeta es escribir
lo que el motor carga** (el orden es el del arranque: el port parchea antes de
que el motor haga su init).

---

## 2. LA PALANCA NUEVA (lo que el usuario pone en `config.ini`)

```ini
[controls]
# ... (touch_screen sigue igual, arriba)
layout_id = keep          # keep | 0 | 1 | 2 | 3   -> controls.ini  [CONTROL_CONFIG] LAYOUT_ID
scheme_id = keep          # keep | 0 | 1           -> controls.ini  SCHEME_ID_P0 y SCHEME_ID_P1
big_buttons = keep        # keep | 0 | 1           -> common.ini    [SETTINGS] BIG_BUTTONS
big_dpad = keep           # keep | 0 | 1           -> common.ini    BIG_DPAD
floating_dpad = keep      # keep | 0 | 1           -> common.ini    FLOATING_DPAD
alternate_controls = keep # keep | 0 | 1           -> common.ini    ALTERNATE_CONTROLS
basic_hand = keep         # keep | 0 | 1           -> common.ini    BASIC_HAND
render_debug = keep       # keep | 0 | 1           -> controls.ini  RENDER_DEBUG
log_engine_config = true  # la linea [ctlcfg] engine (los lectores del motor)
```

* `keep` (el default de todas) **no escribe nada**: el fichero se queda byte por
  byte como el juego lo trae.
* El mecanismo es el de `[audio] language`: se parchea **la linea** en el
  fichero de la tarjeta (si no esta, se saca del OBB primero), se escribe el
  fichero entero solo si algo cambia, y los dos ficheros estan en
  `is_user_override()` para que la copia del OBB no vuelva a pisar el parche.
* **Probado en este PC** (el `.c` del port compilado para el PC, contra copias
  reales de los dos ini, en `scratch/ctlcfg_host/`): con
  `layout_id=2 scheme_id=1 big_buttons=0 big_dpad=0 floating_dpad=1
  alternate_controls=1 basic_hand=1` cambia **exactamente 8 lineas** (3+5) y
  nada mas; la segunda pasada **no escribe** (idempotente); con todo `keep` no
  escribe. Los comentarios y los CRLF quedan intactos.

---

## 3. LA INSTRUMENTACION (que mirar en el log)

**Al arrancar** (despues del parche), una vez:

```
[ctlcfg] the game's control scheme, as served (boot: what the engine will be served):
[ctlcfg]   ini/na/controls.ini [CONTROL_CONFIG] SCHEME_ID_P0 = 0
[ctlcfg]   ini/na/controls.ini [CONTROL_CONFIG] LAYOUT_ID = 0
[ctlcfg]   ...
[ctlcfg]   ini/na/common.ini [SETTINGS] BIG_BUTTONS = 1
[ctlcfg]   ...
```

**A los ~600 frames y luego cada 1800** (y antes, si alguno de los dos ficheros
cambia de tamano o de fecha -- el juego los reescribe cuando el jugador toca sus
ajustes), la respuesta del **propio motor**:

```
[ctlcfg] the engine's OWN registry (libFIFA14.so's reader, this run):
[ctlcfg]   CONTROL_CONFIG/LAYOUT_ID            has 1  value 0
[ctlcfg]   CONTROL_CONFIG/SCHEME_ID_P0         has 1  value 0
[ctlcfg]   ...
[ctlcfg]   PRACTICE_MODE                       has 0  value 0
```

Esos numeros salen de `libFIFA14.so` con **su** lector: `has` = 0x340acc
(`bool Has(key)`) y `value` = 0x340648 (`int GetInt(key)`), los dos alcanzados
desde el simbolo exportado
`Java_com_ea_game_fifa14_Fifa14Activity_getMusicOptions` (0x33ec1c) por el
desplazamiento medido (+0x1eb0 / +0x1a2c) y **verificados antes de llamarlos**
(sus dos primeras palabras de instruccion se leen de la imagen y se comparan con
las del fichero: `e92d4830 e1a04000` y `e92d4070 e1a04000`). Si el `.so` fuera
otro, el log lo dice y **no se llama a nada**.
`has` distingue "la clave no esta en el registro" de "esta y vale 0", que es
justo lo que hace falta para saber si el valor efectivo es el que se sirvio.
Se puede apagar con `log_engine_config = false` (es la unica llamada al motor
que este instrumento hace).

**Sin consola no se puede decir que valores saldran** (la tarjeta D: no esta en
este PC): lo que si esta medido es lo que el OBB trae (seccion 1b) y que el port
se lo sirve. La primera corrida lo dira.

---

## 4. QUE HARIA FALTA PARA QUE DESAPAREZCAN DE VERDAD

Lo medido, sin adornos:

1. **Los valores del esquema son la via limpia y estan ya en la palanca.** Si
   `big_buttons=0 big_dpad=0 floating_dpad=0` los encoge/mueve; si el juego
   ademas los quita con algun esquema (LAYOUT_ID / SCHEME_ID), se vera en la
   pantalla de la proxima corrida con la palanca, sin recompilar. **Esto es lo
   primero que hay que agotar** y es lo que esta ronda deja listo.
2. **La tabla de glifos por dispositivo** esta en el `.so` a
   0x1059400-0x1059dfb (`ps2_button_*`, `ps3_button_*`, `xb_button_*`,
   `xe_button_*`, `win32_button_*`, y la lista de dispositivos `XBox`, `Xenon`,
   `ps2`, `PS3`, `GameCube`, `Windows`, `Wii`). **Quien la consume esta medido**:
   `EA::Input::TriggerConfigParser::Parse(const char*, const char*)`
   (simbolo exportado, codigo alrededor de 0xf09214; las referencias caen en
   0xf08bec/0xf08c0c/0xf09330/0xf09648/0xf09964/0xf09e18..0xf09ef8). Es un
   **parser** de la configuracion de botones (traduce "ps3_button_cross",
   "xe_button_a", "numpad", "all", "define"... a sus ids internos): el
   vocabulario de los botones en pantalla, **no** el que los dibuja.
3. **El dibujo propiamente dicho no se ha localizado en esta ronda** (no hay
   ningun sitio `if (scheme == X) draw` medido). Lo que si esta localizado es
   (a) quien **decide** el esquema al montar el partido (0x49c838, 0x7460cc,
   0x7461b4, con los sitios de la tabla de la seccion 1a) y (b) quien resuelve
   los nombres de boton (TriggerConfigParser). El overlay de mando de la UI se
   pide por nombre: la pelicula `ControllerOverlay` (0x1001a9b, al lado de
   `OpenOverlay`/`CloseAllOverlays`) y el `.big` que el juego tiene en el OBB
   (`data/gui/components/controls/overlays/ControllerOverlay.big`).
4. **Riesgo de parchear a ciegas**: las funciones de (a) no solo dicen "dibuja":
   montan el **mapeo de entrada** del esquema (0x49c838 es el que fija LAYOUT_ID
   y RENDER_DEBUG para el partido). Parchear una de ellas sin ver la pantalla es
   la clase de cambio que deja al jugador sin controles y con un log que dice
   que todo fue bien. **El siguiente paso correcto** (con consola) es
   **instrumentar, no parchear**: leer (o contar) las llamadas a 0x49c838 y a
   0x64045c/0x64050c y ver que valores llegan y cuando, y buscar el consumidor
   del overlay (`ControllerOverlay`) dentro de la UI.
5. `RENDER_DEBUG` es el **unico flag de debug medido** (`controls.ini`, leido en
   0x49c938; el comentario del propio fichero: "0 = don't render debug; 1 =
   render debug"). Ya esta en la palanca (`render_debug = 1`): si el motor
   dibuja algo suyo, se vera en la pantalla y en el log.

---

## 5. BONUS: EL ESLABON SIN NOMBRE DE LAS VOCES, NOMBRADO

**La clave es `PRACTICE_MODE`** (cadena ASCII en el fichero, offset
**0x10005a2**, entre `SKIP_FE` y `TUTORIALS/GAMEPLAY_TUTORIAL`).

La funcion de arranque de audio (0x7ea5ac) la usa **dos veces**:

```
7ea5e0  r0 = PRACTICE_MODE ; bl 0x340acc        ; Has("PRACTICE_MODE")
7ea5e8  cmp r0,#1 ; bne 7ea614                   ; (y si esta, 7ea600 GetInt)
7ea690  r0 = PRACTICE_MODE ; bl 0x340acc         ; la puerta del habla
7ea698  cmp r0,#1 ; bne 7ea6bc                   ; no esta -> arranca el habla
7ea6b0  bl 0x340648                              ; GetInt("PRACTICE_MODE")
7ea6b4  cmp r0,#0 ; bne 7ea6f4                   ; esta y != 0 -> SE SALTA el habla
7ea6bc  bl 0x7e69f0 ; bl 0x7e6f1c                ; el arranque del habla
```

(las direcciones de las cadenas se comprueban sumando la base PIC: 0x7ea6b0 +
0x8fd128 = 0x10e77d8 (GOT) y 0x10e77d8 + 0xfff18dca = 0x10005a2; el mismo metodo
da `CHANTS` en la puerta de 0x7ea6d8 y `PRACTICE_MODE` en la de 0x7ea5e0 —
comprobado tambien con el resolvedor de referencias del `.so`).

Es decir: **en el modo entrenamiento (PRACTICE_MODE) el motor NO arranca el
sistema de habla** — que es exactamente lo que se veia en el log de la consola
(ningun fichero `Speech/*` abierto durante un partido). Y no es una clave de
ningun `.ini` (se busco en los 12 ini del OBB y en `config.dat`: no aparece):
la pone el propio modo de juego en su registro. **Por eso NO se le pone palanca
en `config.ini`**: no hay fichero que escribir. La linea `[ctlcfg] engine` si la
pregunta (`PRACTICE_MODE has ? value ?`), asi que la proxima corrida dira si
esta en el registro en un partido normal (0 esperado) o en entrenamiento.
El default que la ronda anterior llamaba "1" es el `r1 = 1` que el llamador
pasa a 0x340648: **medido, esa funcion lo ignora** (lo sobreescribe con la
clave y devuelve 0 cuando la clave no esta) — la puerta es
"existe Y vale distinto de 0".

---

## 6. ARTEFACTOS Y LO QUE NO SE PUDO VERIFICAR

```
payload   fifa14_nx.nsp                 3.842.533 B  sha256 a6bb27c545819737e87f26379dec72ca65e2d9a9c9e9efef1f99747acc5df727
romfs     launcher/romfs/fifa14_nx.nsp  3.842.533 B  sha256 a6bb27c545819737e87f26379dec72ca65e2d9a9c9e9efef1f99747acc5df727  (== payload)
lanzador  launcher/fifa14_nx.nro        4.111.693 B  sha256 0b8ffa08bc6505784790fa5ebb82eb88176ec585e7408a7d44cbc56e594947b7
build     ./build-fixes.sh: payload rc=0, launcher rc=0, 0 lineas "error:", 0 warnings
          payload VERBATIM dentro del .nro (offset 269013), .nro mas nuevo que el payload
```

* El **patcher probado en el PC** (no es la consola): `scratch/ctlcfg_host/`
  (compila `source/fifa14_ctlcfg.c` con gcc y lo corre contra copias reales de
  los dos ini; salida en `scratch/ctlcfg_host/run1.log`).
* **Sin consola** (y con la tarjeta D: fuera de este PC) no se puede verificar:
  que el motor tome los valores parcheados (lo dira `[ctlcfg] engine`), si los
  botones se encogen o desaparecen, ni si el juego reescribe los dos ini al
  guardar sus ajustes (el watcher de ficheros lo dira: imprime la linea cuando
  el tamano o la fecha cambian).
* No se ha tocado el mando, ni el render/swap, ni el tacto, ni `rt_pad.c`, ni el
  locale del TEXTO, ni el share, ni el About, ni `[controls] touch_screen` ni
  `[audio] language` (se extienden). Nada de `D:/` (no esta) y nada de
  `Speech/`.
