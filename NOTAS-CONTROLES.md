# FIFA 14 (Switch, android32) — TABLA DE CONTROLES: QUE BOTON HACE QUE

Ronda del 2026-10-13. **Solo lectura**: no se ha compilado nada, no se ha tocado
`source/`, ni los `.sh`, ni la tarjeta (`D:/` solo se ha leido), ni el build en curso.

Fuentes, y solo estas:

| # | Fuente | Ruta |
|---|--------|------|
| E1 | `libFIFA14.so` (ELF32 ARM), leido a mano | `D:/switch/fifa14/libFIFA14.so` (18.187.340 B) |
| E2 | Las cadenas del `.so` (offset de fichero) | idem |
| E3 | Locale del juego (UTF-16LE) | `data/gui/locale/ENG_US.loc` **del OBB**, 844.760 B → extraido a `scratch/fifa_ctrl/data__gui__locale__ENG_US.loc` |
| E4 | Locale de Be A Pro (UTF-16LE) | `D:/switch/fifa14/external/data/gui/locale/beapro/ENG_US.loc`, 311.196 B |
| E5 | Configuracion del juego | `ini/na/controls.ini` (OBB → SD, 2.700 B) |
| E6 | Assets de UI/mando del OBB (BIG4) | `data/gui/...Controller*.big`, `HelpScreens/*`, `GamePad*.big` |
| E7 | Log de la consola (la corrida medida) | `D:/switch/fifa14/debug.log` (774.440 B) |
| E8 | El puerto | `source/fifa14_input.c`, `NOTAS-MANDO-Y-VOCES-2.md` |

`MEDIDO` = la cadena/el byte esta ahi y se cita el fichero + offset.
`INFERIDO` = lo deduzco y lo digo.

---

## 0. RESUMEN EN UNA FRASE (esto es lo importante)

**El juego tiene DOS esquemas de control y NINGUNO es de mando: son de PANTACTIL.**
FIFA 14 Android trae un esquema **TOUCH** (gestos sobre la pantalla) y un esquema
**D-PAD** (mandos virtuales en pantalla). El puerto ya le mete al motor los keycodes
Android correctos y el motor los convierte a sus teclas internas (`0xF026` = A, etc.),
pero **el juego no tiene escrita en ningun sitio una tabla "boton de mando -> accion"**:
lo unico que tiene escrito es "**boton en pantalla -> accion**" y "**gesto -> accion**".
Eso, y no otra cosa, es lo que se puede leer del juego. Lo detallo y lo marco.

---

## 1. LO QUE EL MOTOR HACE CON UN BOTON DE MANDO (MEDIDO, duro)

### 1a. La conversion keycode Android -> tecla del motor

`KeyboardAndroid::RawToStdKey(rawKey, alt)` esta en el `.so` a **offset de fichero
`0x00e0c448`** (codigo **ARM/A32**, no Thumb). Desensamblado a mano, instruccion por
instruccion:

```
e0c448  07 00 41 e2   sub  r0, r1, #7        ; if (raw - 7 <= 9u)
e0c44c  09 00 50 e3   cmp  r0, #9
e0c450  29 00 81 92   addls r0, r1, #0x29    ;     return raw + 41   (0..9 -> '0'..'9')
e0c458  1d 00 41 e2   sub  r0, r1, #0x1D     ; if (raw - 29 <= 25u)
e0c45c  19 00 50 e3   cmp  r0, #0x19
e0c460  44 00 81 92   addls r0, r1, #0x44    ;     return raw + 68   (A..Z -> 'a'..'z')
e0c468  00 00 a0 e3   mov  r0, #0             ; if (raw > 164) return 0
e0c46c  a4 00 51 e3   cmp  r1, #0xA4
e0c474  10 00 9f e5   ldr  r0, [pc, #0x10]    ; ADRL de la tabla
e0c478  10 20 9f e5   ldr  r2, [pc, #0x10]
e0c47c  00 00 8f e0   add  r0, pc, r0
e0c480  00 00 82 e0   add  r0, r0, r2
e0c484  01 01 90 e7   ldr  r1, [r0, r1, lsl #2]  ; kRawToStdKey[raw]  (165 u32)
```

### 1b. La tabla `kRawToStdKey` — **offset de fichero `0x0107ae60`, 165 x u32 LE**

Leida entera. Las entradas que importan (transcritas literalmente del fichero):

| keycode Android | nombre Android | valor en la tabla | = tecla del motor |
|---|---|---|---|
| 96 | `KEYCODE_BUTTON_A` | `0x0000F026` | A |
| 97 | `KEYCODE_BUTTON_B` | `0x0000F027` | B |
| 98 | `KEYCODE_BUTTON_C` | `0x0000F028` | C |
| 99 | `KEYCODE_BUTTON_X` | `0x0000F029` | X |
| 100 | `KEYCODE_BUTTON_Y` | `0x0000F02A` | Y |
| 101 | `KEYCODE_BUTTON_Z` | `0x0000F02B` | Z |
| 102 | `KEYCODE_BUTTON_L1` | `0x0000F022` | L1 |
| 103 | `KEYCODE_BUTTON_R1` | `0x0000F024` | R1 |
| 104 | `KEYCODE_BUTTON_L2` | `0x0000F023` | L2 |
| 105 | `KEYCODE_BUTTON_R2` | `0x0000F025` | R2 |
| 106 | `KEYCODE_BUTTON_THUMBL` | `0x0000F030` | StickL |
| 107 | `KEYCODE_BUTTON_THUMBR` | `0x0000F031` | StickR |
| 108 | `KEYCODE_BUTTON_START` | `0x0000F032` | START |
| 109 | `KEYCODE_BUTTON_SELECT` | `0x0000F033` | SELECT |
| 110 | `KEYCODE_BUTTON_MODE` | `0x0000F034` | MODE |
| 19 | `KEYCODE_DPAD_UP` | `0x00000016` | cruceta arriba |
| 20 | `KEYCODE_DPAD_DOWN` | `0x00000017` | cruceta abajo |
| 21 | `KEYCODE_DPAD_LEFT` | `0x00000018` | cruceta izquierda |
| 22 | `KEYCODE_DPAD_RIGHT` | `0x00000019` | cruceta derecha |
| 23 | `KEYCODE_DPAD_CENTER` | `0x0000001A` | cruceta centro |
| 4 | `KEYCODE_BACK` | `0x00000010` | BACK |
| 3 | `KEYCODE_HOME` | `0x0000000F` | HOME |
| 82 | `KEYCODE_MENU` | `0x0000000E` | MENU |
| 66 | `KEYCODE_ENTER` | `0x0000000D` | ENTER |
| 111 | `KEYCODE_ESCAPE` | `0x0000001B` | ESCAPE |

El log de la consola lo confirma en vivo (E7, linea 4886):

```
[input] KEY (the first one): + -> KEYCODE_BUTTON_START -> 0xF032 -- module 600,
        0x00040008, the id the native itself dispatches, alt 0
```

**EVIDENCIA: E1 (`libFIFA14.so` @0x0107ae60) + E2 + E7. CONFIANZA: MEDIDO.**

### 1c. Y AQUI EL HALLAZGO DURO: no hay segunda tabla

Barrido del `.so` **entero** buscando cada tecla del motor (`0xF026`, `0xF027`,
`0xF029`, `0xF02A`, `0xF022`, `0xF024`, `0xF023`, `0xF025`, `0xF032`, `0xF033`,
`0xF034`) como dato de 32 bits, y como inmediato de instruccion ARM (`MOVW`, mascara
`(w & 0xFFF00000) == 0xE3000000`):

* **Inmediatos `MOVW`: 0 coincidencias** para todos ellos.
* Como dato `u32` en todo el fichero, `0xF026..0xF034` **solo** aparecen en
  `0x0107ae60`–`0x0107b018`, o sea **dentro de `kRawToStdKey` y en ningun otro sitio**.

Conclusion MEDIDA: **el `.so` no contiene ninguna tabla "boton de mando -> accion"**.
Contiene la conversion de keycode y nada mas. Todo lo demas que el juego sabe de
botones lo sabe por los botones **en pantalla**.

**EVIDENCIA: E1 (barrido completo de 18.187.340 B). CONFIANZA: MEDIDO.**
Lo que SI se deduce de eso va marcado INFERIDO en la seccion 5.

---

## 2. TABLA DE MENUS

Aviso honesto primero: **el juego no documenta que boton fisico confirma o vuelve**,
porque su UI es de dedo. Lo que hay escrito son **gestos** y **glifos** de una UI de
consola heredada (PS3/Xbox). Separo las dos cosas.

### 2a. Navegacion de menus — lo que el juego SI escribe (MEDIDO)

| Accion | Que dice el juego (literal) | Evidencia | Confianza |
|---|---|---|---|
| Navegar listas largas | "Scroll left and right through the pages of the Tips & Hints book…" | E3 @`0x0148d6` | MEDIDO |
| Navegar paginas de un carrusel | "Swipe horizontally to see all of the match balls available." | E3 @`0x07634a` | MEDIDO |
| Navegar trofeos | "Swipe through your trophies and collect them all!" | E3 @`0x077a04` | MEDIDO |
| Navegar repeticiones | "Swipe through your replay collection and choose which to upload." | E3 @`0x076ade` | MEDIDO |
| Bajar en una lista | "Scroll down for more settings and customize your FIFA experience." | E3 @`0x0769d2` | MEDIDO |
| Bajar en una lista | "Scroll down to see more players." | E3 @`0x076128` | MEDIDO |
| Elegir equipo / estadio / kit | "Scroll through the team logos and tap to choose a desired team." / "Swipe the team shirts to change to secondary kits." | E3 @`0x07659a`, @`0x077258` | MEDIDO |
| Cambiar dificultad | "Swipe here to lower the difficulty setting." / "If you want a bigger challenge, swipe here to change the difficulty." | E3 @`0x078dd0`, @`0x0781f8` | MEDIDO |
| Cambiar el esquema de control | "**Tap here to change\nthe control scheme**" | E3 @`0x09f418` | MEDIDO |
| Aceptar / continuar | "**Press 'X button' to Continue**" / "**PRESS 'X button' TO ACCEPT**" | E3 @`0x0af552`, @`0x0af58e` | MEDIDO |
| Ver ayuda de una pantalla | "Press the '**Select button**' to get tips on any of the Custom Formations screens." | E3 @`0x045088` | MEDIDO |
| Elegir arquero (Be A Keeper) | "**Press the L1 button and the R1 button to toggle through the keepers.**" | E3 @`0x00bf4e` / E4 @`0x005f…` (misma frase; en la copia de la SD esta en `beapro/ENG_US.loc`) | MEDIDO |
| Sacar/cambiar vista en un tiro libre | "**Press the L3 button to lock the camera.**" | E3 @`0x0171b4` | MEDIDO |
| Pausa / menu de partido (glifo) | "Exit to Lobby", "Your teammate has entered the pause menu" | E3 @`0x073e1e`, E3 @`0x0113c8` | MEDIDO |

### 2b. Navegacion de menus — lo que NO esta escrito, y por donde lo busque

**NO ENCONTRADO: "cruceta = navegar, A = confirmar, B = volver" para el build Android.**
No existe esa cadena ni esa tabla. Lo busque en:

* `.so`: barrido de `MOVW` y de datos `u32` de **toda** la tabla de teclas del motor
  (seccion 1c) → 0 resultados fuera de `kRawToStdKey`.
* `.so`: cadenas `KEYCODE_*`, `android/view/KeyEvent`, `BUTTON_L1`… → **0 coincidencias**
  (el `.so` no habla de keycodes Android por nombre en ningun sitio).
* Los `.loc`: la unica mencion a "back"/"volver" es el texto de la UI, no una
  asignacion de boton.
* Los `.big` de UI (E6): el ActionScript de Scaleform esta **fragmentado y con
  fixups intercalados**; no hay zlib (se busco `78 01/5E/9C/DA` en
  `ControllerOverlay.big`, `ControllerHelp.big`, `Controls_Tutorial.big`,
  `GamePad.big`, `ConnectGamePad.big`, `KeyboardOverlay.big`, `HelpScreen.big`,
  `HelpReplays.big`, `newSwapTutorial.big` → **0 streams**), asi que no se puede
  leer la logica de los botones; solo frases sueltas (ver seccion 4).

**Lo unico que si se puede afirmar (INFERIDO, y hay que probarlo en la consola):**
`KEYCODE_BACK` (Android 4) **si** tiene tecla propia en el motor (`kRawToStdKey[4] =
0x00000010`, E1 @`0x0107ae70`). El puerto no mapea ningun boton de Switch a `BACK`
(ver `source/fifa14_input.c:518-533`), asi que hoy **no hay forma de mandar "volver"
al motor**. Los gestos de Android para volver son la tecla BACK del sistema, que el
puerto no emite. **CONFIANZA: INFERIDO** (y es una hipotesis comprobable: mapear `-`
a `KEYCODE_BACK` y mirar si el juego sale de una pantalla).

---

## 3. TABLA DE PARTIDO

El juego trae **dos** esquemas jugables, y los dos estan documentados por el juego
mismo en el locale. La nomenclatura de botones **no es del mando: es del boton en
pantalla** (A/B/C en el esquema por botones; PASS/SHOOT/SPRINT/… en el de gestos).

### 3a. Esquema "CASUAL / ADVANCED" (botones en pantalla A, B, C) — MEDIDO

Pagina de ayuda del juego, E3, zona `0x0c6346`–`0x0c69b0`. Texto literal, en orden:

| Boton | Accion (literal del juego) | Evidencia | Confianza |
|---|---|---|---|
| **A** | "**Press A to shoot.**" | E3 @`0x0c63ba` | MEDIDO |
| **A** + swipe arriba | "Press A and swipe UP for a **chip shot**." | E3 @`0x0c63ba` | MEDIDO |
| **A** + slide izquierda | "Press A and slide left for a **finesse shot**." | E3 @`0x0c63ba` | MEDIDO |
| **A** mantener | "Press and hold down A to **pressure** the opposing player. Your player will move close to the opposing player and attempt a **conservative tackle**." | E3 @`0x0c658a` | MEDIDO |
| **A** (en defensa, sin balon) | "**Press A to clear the ball.** Your player will attempt to pass to a more advanced team-mate or clear the ball upfield." | E3 @`0x0c6728` | MEDIDO |
| **A** + slide izq./arriba (defensa) | "Press A and slide left for a **sliding tackle**." / "Press A and swipe UP for a sliding tackle." | E3 @`0x0c6862`, @`0x0c68be` | MEDIDO |
| **B** | "**Press B to pass. Hold down for increased power.**" | E3 @`0x0c646e` | MEDIDO |
| **B** + swipe arriba | "Press B and swipe UP for a **lobbed pass/cross**." | E3 @`0x0c64d0` | MEDIDO |
| **B** (defensa) | "**Press B to change the selected player.**" | E3 @`0x0c6812` | MEDIDO |
| **C** (modificador) | "Press and **hold down C to protect the ball**" (tras "Release DPAD") | E3 @`0x0c6930`/@`0x0c69b0` | MEDIDO |
| **C** + **A** | "Press C and slide into A for a **finesse shot**." | E3 @`0x0c6412` | MEDIDO |
| **C** + **B** | "Press C and slide into B for a **through ball**." | E3 @`0x0c652e` | MEDIDO |
| **C** + **B** + arriba | "Press C and slide into B and then up for a **lobbed through ball**." | E3 @`0x0c6546` | MEDIDO |
| **C** + **B** mantener | "Press C then slide into B and hold for the **goalkeeper to rush off his line**." | E3 @`0x0c6916` | MEDIDO |
| **C** mantener | "Press C and hold to **call for a second defender**." | E3 @`0x0c69b0` | MEDIDO |
| **DPAD / joystick** | "**Use the floating joystick to move the player on the field.**" / "**Use the DPAD to move the player on the field.**" | E3 @`0x0c6a8e`, @`0x0c5a9a` | MEDIDO |

Cabecera de esa pagina, literal: "…t with two defenders. **CASUAL ADVANCED** Press A
to shoot. …" → el juego dice explicitamente que la pagina es de los dos esquemas.
**EVIDENCIA: E3 @`0x0c6346`. CONFIANZA: MEDIDO.**

Y en `controls.ini` (E5), que es del juego y no del puerto:

```
[CONTROL_CONFIG]
// 0 = two buttons + sprint (basic) ; 1 = three buttons (advanced);
SCHEME_ID_P0 = 0
// 0 = two buttons + sprint (basic) ; 1 = three buttons (advanced);
SCHEME_ID_P1 = 0
// 0 = layout 0 [...] 3 = layout 3
LAYOUT_ID = 0
// 0 = don't use side buttons
USE_SIDE_BUTTONS = 0
...
[CONTROLS]
MANUAL_THROUGH_PASS0 = 0
MANUAL_CROSS0 = 0
MANUAL_THROUGH_PASS1 = 0
MANUAL_CROSS1 = 0
```

O sea: el esquema basico son **2 botones + sprint**; el avanzado, **3 botones**.
Eso cuadra con "Press A / Press B" (basico) y "A / B / C" (avanzado).
**EVIDENCIA: E5 `ini/na/controls.ini` (2.700 B, extraido del OBB). CONFIANZA: MEDIDO.**

### 3b. Esquema "TOUCH" (gestos + botones en pantalla) — MEDIDO

Pagina de ayuda del juego, E3. Nombres de boton en pantalla: **PASS, SHOOT, SPRINT,
SWITCH, SLIDE, TACKLE, THROUGH, CROSS, CLEARANCE, THROW, JUMP**.

| Gesto | Accion (literal del juego) | Evidencia | Confianza |
|---|---|---|---|
| Tap PASS | "**Tap the PASS button for a ground pass**" / "Tap a player of your team for a ground pass" | E3 @`0x078e2a`, @`0x078bba` | MEDIDO |
| Mantener PASS | "**Hold the PASS button for a lob pass or cross**" / "WHEN IN POSSESSION, HOLD PASS TO PLAY A LOBBED PASS" | E3 @`0x078ca8`, @`0x0704e2` | MEDIDO |
| Tap SHOOT | "Tap the SHOOT button to shoot. **Hold longer for a more powerful shot**" | E3 @`0x07906c` (zona) | MEDIDO |
| Tap SHOOT + swipe arriba (area) | "When attacking in the penalty box area, **tap SHOOT and swipe UP for a chipshot**" | E3 @`0x078284` | MEDIDO |
| Tap PASS + swipe arriba (area) | "When attacking in the penalty box area, **tap PASS and swipe UP for a finesse shot**" | E3 @`0x0788e4` | MEDIDO |
| Tap PASS + swipe arriba (banda) | "When attacking on wings, **tap PASS and swipe UP for a ground cross**" | E3 @`0x078722` | MEDIDO |
| Tap SHOOT + swipe arriba (banda) | "When attacking on wings, **tap SHOOT and swipe UP for a powerful low cross**" | E3 @`0x078a10` | MEDIDO |
| Tap PASS + swipe arriba (centro) | "When attacking in the central area, **tap PASS and swipe UP for a through ball**" | E3 @`0x079f66` | MEDIDO |
| Tap SHOOT + swipe arriba (centro) | "When attacking in the central area, **tap SHOOT and swipe UP for a lofted through ball**" | E3 @`0x078d04` | MEDIDO |
| Tap SHOOT + swipe abajo | "**Tap the SHOOT button and slide DOWN for a fake shot**" / "WHEN IN THE OPPOSING HALF, SWIPE DOWN "SHOOT" TO FAKE THE MOVE." | E3 @`0x07887a`, @`0x07054c` | MEDIDO |
| Doble tap SPRINT | "WHEN RUNNING WITH THE BALL, **DOUBLE TAP "SPRINT" TO KNOCK FORWARD** AND RUN AFTER IT" | E3 @`0x0705ce` | MEDIDO |
| Doble tap pantalla | "**DOUBLE TAP THE SCREEN TO PERFORM A SKILL MOVE**" | E3 @`0x0702fa` | MEDIDO |
| Swipe a un companero | "**Swipe a player of your team to make him run in the desired direction**" | E3 @`0x078f56` | MEDIDO |
| Tap a un companero (defensa) | "**WHEN DEFENDING, TAP A TEAMMATE TO SWITCH TO HIM.**" | E3 @`0x070c88` | MEDIDO |
| Tap SWITCH | "Tap SWITCH to change the selected player." / "When defending, tap the SWITCH button for an auto-switch…" | E3 @`0x0c6124`, @`0x079cb2` | MEDIDO |
| Tap SWITCH + swipe arriba y mantener | "**When defending, tap SWITCH, then swipe UP and hold to force a goalkeeper rush**" | E3 @`0x078aa4` | MEDIDO |
| Tap SLIDE | "**Tap SLIDE to perform a sliding tackle.**" | E3 @`0x0c5fe6` | MEDIDO |
| Tap SHOOT + swipe arriba (tu campo) | "**When defending in your own half, tap SHOOT and swipe UP for a smart clearance**" | E3 @`0x07914e` | MEDIDO |
| Tap THROW (portero con balon) | "**When the goalie has the ball, tap THROW to launch the ball**" | E3 @`0x078b42` | MEDIDO |
| Tap SHOOT (portero con balon) | "When the goalie has the ball, tap SHOOT to clear the ball" | E3 @`0x078322` | MEDIDO |
| Swipe abajo al portero | "WHEN YOUR GOALKEEPER HAS THE BALL, **SWIPE DOWN ON HIM TO MAKE HIM DROP IT**" | E3 @`0x070674` | MEDIDO |
| Tap JUMP / swipe a la barrera | "**Tap the JUMP button** or touch the wall and swipe UP to make the wall jump" | E3 @`0x078c14` | MEDIDO |
| Set pieces | "**WHEN PLAYING FROM OR DEFENDING A SET PIECE, SWIPE THE SCREEN OR TAP THE PLAYERS** FOR SITUATION SPECIFIC ACTIONS" | E3 @`0x071f74` | MEDIDO |
| Regla general | "**SWIPE UP ON ANY BUTTON TO PERFORM A SPECIAL ACTION**" | E3 @`0x06f962` | MEDIDO |
| Regla general | "**TAP, DOUBLE TAP & SWIPE A PLAYER FOR DIFFERENT MOVES**" | E3 @`0x06fa1e` | MEDIDO |
| Sprint | "**HOLD "SPRINT" WHEN IN POSSESSION TO RUN WITH THE BALL.**" / "TAP OR HOLD SPRINT FOR DIFFRERENT MOVES." | E3 @`0x07028a`, @`0x06f9ca` | MEDIDO |
| Sprint | "**Hold the SPRINT button for a higher speed, but less control**" | E3 @`0x079bee` | MEDIDO |
| First touch | "**PERFORM A FIRST TOUCH BY HOLDING "SPRINT" WHEN RECEIVING A PASS.** USE THE JOYSTICK TO PLAY THE BALL INTO SPACE." | E3 @`0x0701aa` | MEDIDO |
| Movimiento | "**Use the floating joystick to move the player on the field.**" | E3 @`0x0c6a8e` | MEDIDO |
| Sprint con el joystick | "Sprinting is triggered when the joystick is tilted enough so that it lights up." | E3 @`0x0ca276` | MEDIDO |
| Tiro con efecto | "To add spin to a shot, tap the SHOOT button and move the pad while the power bar is charging" | E3 @`0x079b30` | MEDIDO |
| Tiro deslizando | "**Slide from the ball towards the target and release to SHOOT**" / "A higher slide speed increases the SHOT power" | E3 @`0x07953a`, @`0x0795b4` | MEDIDO |

Y la lista de nombres de boton que el juego pinta en pantalla, en un solo bloque
(E3 `0x0c5966`–`0x0c5a78`), literal y en orden:
`PASS`, `SLIDE`, `THROUGH`, `TACKLE`, `CROSS`, `CLEARANCE`, `SHOT`, `THROUGH BALL`,
`PASS`, `LOBBED PASS`, `CHIP SHOT`, `FINESSE SHOT`, `SLIDING TACKLE`, `ONE-TWO`,
`ACTION FEEDBACK`. **MEDIDO.**

### 3c. El catalogo de ACCIONES del motor (lo que el juego sabe nombrar)

El `.so` tiene la tabla de nombres de accion, en ASCII, a **offset de fichero
`0x0101843a`–`0x010185ac`** (E1/E2), literal y en orden:

```
SMART CLEARANCE, SKILL MOVE, FIRST TOUCH, SWITCH TO SELECTED, PASS TO TARGET,
TRIGGER PLAYER RUN, PASS, LOB, THROUGH PASS, THROUGH LOB, SHOOT, CHIP SHOT,
FINESSE SHOT, PRESSURE TACKLE, SLIDE TACKLE, CALL 2nd DEFENDER, SWITCH, JOCKEYING,
GK CHARGE, THROW, JUMP, DIVE, DROP THE BALL, ONE-TWO, SPRINT, FAKE SHOT,
KNOCK FORWARD, GROUND CROSS, LOW CROSS, WALL MOVE, THROW TO TARGET, PASS AND RUN,
MENTALITY
```

Justo detras, a `0x010185ac`–`0x01018923`, van las claves de L10N de cada accion
(`wwiFifa11_5590_action_smartClearance`, `…_5610_action_skillMove`,
`…_5650_action_groundPass`, `…_5700_action_chipshot`, `…_5760_action_switch`,
`f12_action_knockfw`, `f13_passandrun`, `TM_Mentality`, …). Y esas claves apuntan a
una tabla de punteros en **offset de fichero `0x010ee36c`** (64 punteros seguidos,
uno por accion; p.ej. `0x010ee36c -> 0x010185ac`). **EVIDENCIA: E1/E2. CONFIANZA: MEDIDO.**
El locale las resuelve a texto legible en E3 `0x0c6b1e`–`0x0c6d60`
(`SMART CLEARANCE`, `SKILL MOVE`, `SWITCH TO SELECTED`, `PASS TO TARGET`,
`TRIGGER PLAYER RUN`, `PASS`, `LOB`, `THROUGH PASS`, `THROUGH LOB`, `SHOT`,
`CHIP SHOT`, `FINESSE SHOT`, `FIRST TOUCH`, `PRESSURE TACKLE`, `SLIDE TACKLE`,
`CALL 2nd DEFENDER`, `SWITCH`, `JOCKEYING`, `GK CHARGE`, `THROW`, `JUMP`, `SAVE`).

### 3d. Y AQUI LO QUE FALTA, DICHO CLARO

**NO ENCONTRADO: la fila "boton del mando -> accion" para el partido.** El juego no
la tiene escrita. Lo busque en:

* `.so`: barrido completo de inmediatos `MOVW` y de los `u32` `0xF022..0xF034`
  → **solo** la tabla de conversion (seccion 1c). Verificado dos veces.
* `.so`: clusteres de nombres de accion (`0x1008800`, `0x1018400`, `0x102b300`,
  `0x102d100`, `0x1025d40`) → son **nombres de animacion** (`0SP_LONG_LOB_PASS_RFOOT_45L`,
  `6SP_SLIDETACKLE_0`), **modos de contexto de pulsacion** (`BUTTON_MODE_AIR_BALL`,
  `BUTTON_MODE_PENALTY`, `BUTTON_MODE_GOALIE_HOLDING`…) y **claves de config**, no un
  reparto de botones.
* `.so`: `BUTTON_CONFIG0` @`0x1001f0a` y `OPTIONS/BUTTON_CONFIG%d` @`0x1025d8f`,
  `iControllerConfig` @`0x1001ef8` → es un **campo del perfil** (`OPTIONS/BUTTON_CONFIG0`,
  el "controller config" del jugador), no una tabla.
* `.ini` del juego: `ini/na/controls.ini` (tiempos y potencias del táctil),
  `data/cmn/cfg_def.ini` y `data/cmn/cfg_psp.ini` (**render**: `RMRAIN_*`,
  `PLAYER_LOD_*`, `GODRAYS_ENABLED`…). `ini/na/config.dat` = 34.614 B, tambien de
  configuracion global. **Ninguno reparte botones.**
* Los `.big` de UI: el ActionScript esta fragmentado (sin zlib, con fixups)
  → no se puede leer la logica.

---

## 4. QUE BOTONES PUEDE USAR EL JUGADOR: LOS ASSETS DE MANDO

### 4a. Los iconos de boton que el juego trae (MEDIDO)

En el `.so`, a **offset de fichero `0x01059800`–`0x01059d80`**, hay una tabla de
nombres de boton **por dispositivo** (E1/E2). Transcripcion de lo que hay:

| Prefijo | Que es | Ejemplos literales |
|---|---|---|
| `ps3_*` | mando PS3 | `ps3_button_cross`, `ps3_stick_ry`, `ps3_stick_lx`… |
| `xb_button_*` | mando Xbox (Xbox clasico / "XBox") | `xb_button_a`, `xb_button_b`, `xb_button_x`, `xb_button_y`, `xb_button_back`, `xb_button_start`, `xb_button_dpadup/down/left/right`, `xb_button_leftbutton` (=L), `xb_button_rightbutton` (=R), `xb_button_lefttrigger`, `xb_button_righttrigger`, `xb_button_leftthumb`, `xb_button_rightthumb` |
| `xb_thumb_*` | ejes del Xbox | `xb_thumb_lx`, `xb_thumb_ly`, `xb_thumb_rx`, `xb_thumb_ry` |
| `xe_button_*` | mando Xbox 360 ("Xenon") | `xe_button_a/b/x/y`, `xe_button_start`, `xe_button_back`, `xe_button_leftshoulder`, `xe_button_rightshoulder`, `xe_button_lefttrigger`, `xe_button_righttrigger`, `xe_button_dpad*`, `xe_button_leftthumb`, `xe_button_rightthumb` |
| `win32_button_*` | teclado/raton PC | `win32_button_0..8`, `win32_button_l1`, `win32_button_r1`, `win32_button_l2`, `win32_button_r2`, `win32_button_start`, `win32_button_select`, `win32_button_circle/cross/square/triangle` |

Y justo detras, la lista de **dispositivos** que el motor conoce (E2 @`0x01059dc7`):
`XBox`, `Xenon`, `ps2`, `PS3`, `GameCube`, `Windows`, `win32`, `Wii`.

Ojo: **no hay ninguna entrada `android_*` ni `ios_*` en esa tabla.** El build Android
hereda los nombres de los dispositivos de consola.

### 4b. Que assets de mando trae el OBB (MEDIDO)

| Asset | Tamano | Evidencia |
|---|---|---|
| `data/gui/components/controls/overlays/ControllerOverlay.big` | 239.932 B | tabla de nombres del OBB, entrada 489 |
| `data/gui/screens/HelpAndAbout/ControllerHelp.big` | 523.686 B | OBB entrada 269 (offset 0x228df60, djb2 `3ae58df7`) |
| `data/gui/screens/commonfe/GamePad.big` | 71.398 B | OBB | 
| `data/gui/screens/commonfe/ConnectGamePad.big` | 129.957 B | OBB |
| `data/gui/components/controls/KeyboardOverlay.big` | 52.687 B | OBB |
| `data/gui/components/controls/GamePadStatusAnimation.big` | 240.203 B | OBB |
| `data/gui/screens/artwork/HelpScreens/Controls_Tutorial.big` | 1.773.245 B | OBB (titulo interno: **"FIFA 14 NEW TOUCH CONTROLS"**, E6 @`0x33c`) |
| `data/gui/screens/artwork/HelpScreens/*_DPAD.big` + `*_TOUCH.big` | 44 ficheros | lista completa abajo |
| `data/gui/components/controls/overlays/tutorials/newSwapTutorial.big` | 10.185 B | OBB |

Las dos familias de `HelpScreens` (MEDIDO, de la tabla de nombres del OBB): una por
cada esquema. Las de **DPAD**: `Call_Second_DPAD`, `Chip_Shots_DPAD`, `Clearance_DPAD`,
`Fake_Shot_DPAD`, `Finess_Shot_DPAD`, `First_Touch_DPAD`,
`Forward_Run_DPAD`, `GK_Rush_DPAD`, `Ground_Cross_DPAD`,
`Ground_Through_Ball_DPAD`, `Knock_Forward_DPAD`, `Lobbed_Through_DPAD`,
`One_Two_DPAD`, `Skill_Moves_DPAD`, `Sliding_Tackle_DPAD`, `Sprint_DPAD`,
`Sprint_and_Tackle_DPAD`, `Swipe_To_Drop_DPAD`, `Switch_DPAD`, `Tap_To_Pass_DPAD`.
Las de **TOUCH**: `Clearance_TOUCH`, `Fake_Shot_TOUCH`, `Forward_Run_TOUCH`,
`GK_Rush_TOUCH`, `Sprint_TOUCH`, `The_Lasso_TOUCH` (y las de FUT/CoreX, que no son de
juego). Mas `FIFA_IntroFlow*` y `FUT_*`.

### 4c. Que assets de mando ha PEDIDO el juego en la consola (MEDIDO, E7)

Del `debug.log` de la corrida real:

```
[obb] open(data/gui/screens/artwork/HelpScreens/Lobbed_Through_DPAD.big) -> SERVED 1254722 B
[fs]  open "/sdcard/Android/data/com.ea.game.fifa14_row/data/gui/components/controls/GamePadStatusAnimation.big" rc=9
     (x20 en el log)
     KeyboardOverlay.big (x12 en el log)
     GamePad.big (x1 en el log)
```

O sea: **el juego ha cargado de verdad el tutorial del esquema DPAD** (concretamente
la pagina del *lobbed through ball*), y **`GamePadStatusAnimation.big`** (el aviso de
"Gamepad disconnected", E3 @`0x06dd3c`). Eso prueba que el juego resuelve las
pantallas de ayuda por esquema en tiempo de ejecucion. **CONFIANZA: MEDIDO.**

### 4d. Donde sale la tabla de ayuda de mando DENTRO del juego

MEDIDO:
* **`CUSTOMIZE` → `GAME HELP`** — "Visit the **GAME HELP section in the CUSTOMIZE tab**
  to read extensive tips about both control schemes and other useful general help."
  (E3 @`0x0a0260`; las etiquetas `GAME HELP` @`0x0967ae` y `CUSTOMIZE` @`0x093cea`).
* **`GAME SETTINGS` → `Controls`** — "Go to **Game Settings > Controls** to customize
  the interface: left-handed UI, larger buttons, action feedback and more."
  (E3 @`0x088340`/@`0x0883b8`; `GAME SETTINGS` @`0x0ae9ee`, `Controls` @`0x0ca0ea`).
* **Menu previo al partido (kit selection)** — "You can switch game controls between
  the classic scheme and the new touch controls. Do this from **Game Settings** or the
  **pre-match kit selection menu**." (E3 @`0x09fdc8`). Y "**Tap here to change\nthe
  control scheme**" (E3 @`0x09f418`).
* Las pantallas `HelpAndAbout/ControllerHelp.big`, `HelpAndAbout/HelpScreen.big`,
  `HelpAndAbout/HelpReplays.big`, `HelpAndAbout/HelpAboutManual.big` (E6) son las
  pantallas de la seccion de ayuda; `ControllerHelp.big` esta **en el OBB pero no
  extraido a la SD**, o sea que el juego aun no la ha pedido.

---

## 5. LAS OPCIONES DE CONTROL DEL JUEGO, Y DONDE ESTAN

### 5a. Claves de configuracion, leidas del `.so` (MEDIDO, duro)

Cluster ASCII a **offset de fichero `0x01003036`–`0x010040c8`** (E1/E2). Pares
clave-de-config / etiqueta-de-UI, literal:

| Clave en `config`/perfil | Clave en `SETTINGS/` | Etiqueta de UI (literal, misma zona) |
|---|---|---|
| `CONTROL_CONFIG/FLOATING_DPAD` @`0x1003036` | `SETTINGS/FLOATING_DPAD` @`0x1003053` | "Floating Joystick" @`0x100401e` |
| `CONTROL_CONFIG/BASIC_HAND` @`0x100306a` | `SETTINGS/BASIC_HAND` @`0x1003084` | "Left Handed" @`0x1004030` |
| `CONTROL_CONFIG/BIG_BUTTONS` @`0x1003098` | `SETTINGS/BIG_BUTTONS` @`0x10030b3` | "Large Buttons" @`0x100403c` |
| `CONTROL_CONFIG/BIG_DPAD` @`0x10030c8` | `SETTINGS/BIG_DPAD` @`0x10030e0` | "Large Joystick" @`0x100404a` |
| `CONTROL_CONFIG/ALTERNATE_CONTROLS` @`0x10030f2` | `SETTINGS/ALTERNATE_CONTROLS` @`0x1003114` | "Alternate Controls" @`0x1004069` |
| `CONTROL_GUI/ACTION_TAKEN_TEXT_ENABLED` @`0x1003130` | `SETTINGS/…` | "Action Feedback" @`0x1004059` |
| `CONTROL_GUI/ACTION_SKILL_GRID_ENABLED` @`0x1003179` | `SETTINGS/…` | ( `iSkillGrid` @`0x1003fe3` ) |
| `CONTROL_GUI/ACTION_SKILL_NAME_ENABLED` @`0x10031c2` | `SETTINGS/…` | ( `iSkillName` @`0x1003fee` ) |
| `OPTIONS/ASSISTED_TACKLE` @`0x100320b` | `SETTINGS/ASSISTED_TACKLE` @`0x1003223` | "Auto-Tackle" @`0x100407c` |
| `OPTIONS/FIRST_TOUCH` @`0x100323c` | `SETTINGS/FIRST_TOUCH` @`0x1003250` | "First Touch" @`0x1004088` |
| `OPTIONS/SNAP_2ND_DEF_ON_SWITCH` @`0x1003265` | `SETTINGS/…` | "Reset 2nd Def" @`0x1004094` |
| `OPTIONS/AUTO_SWITCH_LEVEL0/1` @`0x10032a4`/`0x10032db` | `SETTINGS/…` | "Auto Switch" @`0x10040bc` |
| `OPTIONS/AUTO_SWITCH_TOUCH_LEVEL0/1` @`0x1003312`/`0x1003355` | `SETTINGS/…` | "Auto Switch" (variante tactil) |
| `OPTIONS/TUTORIAL_DPAD` @`0x1003398` | `SETTINGS/TUTORIAL_DPAD` @`0x10033ae` | (aviso "in-game Tips") |
| `OPTIONS/TUTORIAL_TOUCH` @`0x10033c5` | `SETTINGS/TUTORIAL_TOUCH` @`0x10033dc` | idem |
| `OPTIONS/MANUAL_GROUNDPASS` @`0x10033f4` | `SETTINGS/MANUAL_GROUNDPASS` @`0x100340e` | "Manual Ground Pass" @`0x10040a2` |
| `CONTROL_CONFIG/SCHEME_ID_P0` @`0x1003c97` | `CONTROL_CONFIG/SCHEME_ID_P1` @`0x1003cb3` | "Control Scheme" @`0x100400f` |
| `CONTROLS/MANUAL_THROUGH_PASS0` @`0x1003f1e` | `CONTROLS/MANUAL_CROSS0` @`0x1003f3c` | "MANUAL THROUGH BALL"/"MANUAL CROSS" (E3 @`0x01f582`/`0x01f5ac`) |

Enteros de la misma zona: `iBasicScheme`, `iBigButtons`, `iBigDpad`, `iFloatingDpad`,
`iLeftHanded`, `iShowAction`, `iSkillGrid`, `iSkillName`, `iAlternateControls`
(todos en `0x01003f9b`–`0x0100400f`). Y `[PENALTY_CONTROLS]`,
`[FREEKICK_CONTROLS]`, `[CONTROL_TIMEOUTS]`, `[CONTROL_GAMEPOWER…]`:
`CONTROL_GAMEPLAY` @`0x1025ba3`, `CONTROL_TIMEOUTS/SCREEN_TAP` @`0x1018a42`,
`CONTROL_GUI/ACTION_TAKEN_DISPLAY_TIMEOUT` @`0x1018a5e`,
`PENALTY_CONTROLS/*` @`0x1018c85+`, `FREEKICK_CONTROLS/*` @`0x1018e93+`,
`CONTROL_POWER/*` @`0x102bdc8+`.

### 5b. Lo mismo visto desde el locale — las mismas opciones con su descripcion

Pagina **GAME SETTINGS**, E3 `0x0c9000`–`0x0ca800` (MEDIDO):

| Etiqueta | Offset | Descripcion literal del juego |
|---|---|---|
| `Game settings` | @`0x0c9506` | (cabecera) |
| `Auto Switching` | @`0x0c9524` | |
| `Camera` | @`0x0c96e0` | |
| `Player Names` | @`0x0c97c2` | "Disable / enable player name display during match." |
| `Device Music` | @`0x0c9846` | |
| `Casual Control` | @`0x0c98f8` | "…vanced gives more control." (@`0x0c9a00`) |
| `Floating joystick` | @`0x0c9a3a` | "When enabled, the joystick will move in the left area of the screen to accommodate your finger movements. Otherwise, it will be fixed in the lower left corner." (@`0x0c9a60`) |
| `Large Buttons & Joystick` | @`0x0c9ba2` | "Increase the button size to improve readability and ease of use." (@`0x0c9bd6`) |
| `Action Feedback` | @`0x0c9c5a` | "When enabled, the name of the action you have just performed will be displayed on the lower edge of the screen." (@`0x0c9c7c`) |
| `In-game Tips` | @`0x0c9d5e` | "Tips with instructions on actions that can be performed will appear above some of your players during play." (@`0x0c9d7a`) |
| `Instant Replays` | @`0x0c9e54` | "After a goal, a button appears in the upper right corner. Touching it will switch to the instant replay mode… pinch to zoom and swipe to rotate the camera…" |
| `Controls` | @`0x0ca0ea` | — |
| → `Sprinting` | @`0x0ca0fe` | "Move your player using the joystick. The further from the center you drag the knob, the faster your player will run…" (@`0x0ca114`) / "Sprinting is triggered when the joystick is tilted enough so that it lights up…" (@`0x0ca276`) |
| → `Passing` | @`0x0ca4b2` | "On the casual control scheme, touching the "Pass" or "Through" buttons will play that specific pass type. Holding it will increase both power and height…" / "On the advanced scheme, these actions are independent…" (@`0x0ca67e`) |
| → `Shooting` | @`0x0ca77c` | |
| → `Conservative and slide tackles` | @`0x0ca8d2` | |

Interruptores con su etiqueta literal: `LARGE BUTTONS` @`0x0c6d6e`,
`LARGE JOYSTICK` @`0x0c6d8c`, `FLOATING JOYSTICK` @`0x0c6dac`,
`LEFT HANDED` + `ON OFF` @`0x0c6dd2`. **MEDIDO.**

### 5c. Los esquemas, dichos por el juego

| Texto (literal) | Evidencia | Confianza |
|---|---|---|
| "**FIFA14 HAS 2 CONTROL SCHEMES**" | E3 @`0x0737b8` | MEDIDO |
| "**TOUCH CONTROLS**" / "**CLASSIC CONTROLS**" / "**CONTROLS SCHEME**" / "**ADVANCED SCHEME**" | E3 @`0x0737f4`, @`0x08f0ac`, @`0x08fad4`, @`0x07232c` | MEDIDO |
| "Change to the **advanced control scheme**." | E3 @`0x040b32` | MEDIDO |
| "If you feel you need even more control over your game, **switch to the advanced control scheme. There will be no assists, you'll be on your own.**" | E3 @`0x071a74` | MEDIDO |
| "Please take a few moments to get used to the **Classic control scheme** layout." | E3 @`0x078490` | MEDIDO |
| "The new **Touch Football control scheme** allows you to control more than one players at the same time." | E3 @`0x09aff6` | MEDIDO |
| "You can change controls schemes from the **CUSTOMIZE or SETTINGS** menus." | E3 @`0x09f306` | MEDIDO |
| "**GET READY FOR A MATCH WITH THE NEW FOOTBALL TOUCH CONTROLS**" | E3 @`0x09c560` | MEDIDO |
| "Choose the controller scheme that best suits your playing style. Choose between **analog and digital** controls with a variety of button configurations." | E3 @`0x012156` | MEDIDO |
| Dribble layouts: `Classic Analog Dribble`, `Classic Digital Dribble`, `Classic Combined Dribble`, `New Analog Dribble`, `New Digital Dribble`, `New Combined Dribble` | E3 @`0x025cfc`–`0x025de4` | MEDIDO |
| "**AUTO-TACKLE ON**" / "Moving the d-pad has no effect." — "**AUTO-TACKLE OFF**" / "Tackling is still automatic, but the **d-pad changes the player's direction**." | E3 @`0x08842e`/`0x0884e2` y @`0x088524`/`0x0885b8` | MEDIDO |
| "Go to Game Settings > Controls to turn **Auto-Tackle OFF**." | E3 @`0x088546` | MEDIDO |
| "When disabled, any **DPAD input will take precendence** over the automatic direction of the controlled defender." | E3 @`0x0780b2` | MEDIDO |
| "**CONNECT GAMEPAD**" / "Gamepad disconnected" / "You need the **"EA SPORTS Gamepad" application** downloaded on one or two iPhone/iPod Touch devices in order to play." | E3 @`0x06db08`, @`0x06dd3c`, @`0x06dda6` | MEDIDO |

### 5d. La configuracion que el juego trae en disco

`ini/na/controls.ini` (E5, 2.700 B) — es **del juego**, no del puerto. Bloques:
`[CONTROL_CONFIG]` (los esquemas, arriba), `[CONTROL_TIMEOUTS]`
(`SCREEN_TAP = 300` ms para doble tap/drag, `BUTTON_HOLD = 300` ms para mantener),
`[CONTROL_POWER]` (`MIN_LOB_POWER_BASIC`, `MIN_THROUGH_LOB_POWER_BASIC`,
`MIN_LOB_POWER_ADVANCED`, `MIN_THROUGH_LOB_POWER_ADVANCED`),
`[CONTROL_GAMEPLAY]` (`MIN_DISTANCE_FOR_SMART_CLEARANCE = 1000`),
`[CONTROL_GUI]` (`ACTION_TAKEN_DISPLAY_TIMEOUT = 3000`,
`REPLAY_SHORTCUT_DISPLAY_TIMEOUT = 3000`), `[CONTROLS]` (los `MANUAL_*`),
`[PENALTY_CONTROLS]` y `[FREEKICK_CONTROLS]` (potencias y spin).
**Los comentarios del propio fichero son los que dicen "dos/tres botones".** MEDIDO.

---

## 6. LO QUE ESTO SIGNIFICA PARA EL PUERTO (INFERIDO, hay que probarlo)

El puerto (`source/fifa14_input.c:495-533`) manda los keycodes Android correctos:

| Boton de Switch | keycode Android | tecla del motor |
|---|---|---|
| A | 96 | `0xF026` |
| B | 97 | `0xF027` |
| X | 99 | `0xF029` |
| Y | 100 | `0xF02A` |
| L | 102 | `0xF022` |
| R | 103 | `0xF024` |
| ZL | 104 | `0xF023` |
| ZR | 105 | `0xF025` |
| + | 108 | `0xF032` |
| − | 109 | `0xF033` |
| cruceta | 19/20/21/22 | `0x16`/`0x17`/`0x18`/`0x19` |
| stick izq. | (umbral) 19/20/21/22 | idem cruceta |
| stick der. | (cursor tactil) | — |

**INFERIDO** (no esta escrito en el juego; hay que verlo en la consola):

1. Como el juego **no** tiene tabla de mando, sus botones de mando caen en el mismo
   sistema de eventos que los botones en pantalla. Lo mas probable es que `A`
   (`0xF026`) haga de **boton de accion** (el `SHOOT`/`A` de la ayuda) y `B`
   (`0xF027`) el `PASS`/`B`. Pero **eso no lo dice el juego en ningun fichero**, y
   ademas el motor tiene que **componer** gestos (mantener, doble tap, swipe) que un
   mando digital no produce. **Sin probar.**
2. `KEYCODE_BACK` (`4 -> 0x10`) **si** existe en el motor y **no** esta mapeado por el
   puerto: hoy no hay "volver" por boton. Candidato: mapear `−`/`B` a `KEYCODE_BACK`.
   **INFERIDO.**
3. El stick derecho ya es el cursor tactil del puerto, y el juego **si** usa el stick
   derecho en su ayuda heredada ("Right analog stick switching", E3 @`0x0166f6`) —
   pero esa es la ayuda de consola que el build Android no ejecuta. **INFERIDO.**

**Prueba barata que lo cerraria:** arrancar un partido, pulsar A y B por separado y
leer en el `debug.log` si el `[input]` cuenta la tecla y si el juego responde
(el HUD "Action Feedback" pinta el NOMBRE de la accion que acabas de hacer — opcion
`ACTION_TAKEN_TEXT_ENABLED`, E1 @`0x1003130`, y ese nombre sale del catalogo de la
seccion 3c). Es una prueba de un minuto y convierte el INFERIDO en MEDIDO.

---

## 7. LISTA DE "NO ESTA", CON POR DONDE SE BUSCO

Para que no quede duda de lo que **no** se ha encontrado y de que se ha intentado:

1. **Tabla "boton de mando -> accion" en el `.so`.** Buscado: inmediatos `MOVW`
   (`(w & 0xFFF00000) == 0xE3000000`) con inm16 en `{0xF022..0xF034, 0x16..0x1A, 0x10}`;
   datos `u32`/`u16` de esos mismos valores en los 18.187.340 B; cadenas
   `KEYCODE_*`, `android/view/KeyEvent`, `BUTTON_L1`. **Resultado: 0 inmediatos, y los
   `u32` solo dentro de `kRawToStdKey` (@`0x0107ae60`).**
2. **Logica de botones en los `.apt` de Scaleform.** Buscado: zlib (`78 01/5E/9C/DA`)
   en los 9 `.big` de mando → 0 streams; HEXdump de la cabecera BIG4 → el contenido es
   ActionScript con fixups intercalados; se sacaron frases sueltas (la ayuda del
   `ControllerHelp`: "FINESSE … INSIDE THE BOX … A finesse shot trades power for
   accuracy, allowing t…", "WHEN …, PR "PASS" … N SWIPE UP TO PERFORM A … SHO…",
   y las claves `$GS_MovePlay`, `CMN_Sprint`, `wwiFifa11_5…`) pero **no la asignacion
   por pantalla**. La estructura del contenedor **si** quedo resuelta: BIG4 con
   `n` en `+0x08` (BE), fin de directorio en `+0x0c`, inicio de datos en `+0x10`,
   `+0x14` = primer offset; cada entrada = `nombre\0` + `(u32 offset, u32 tamano)` BE;
   cada sub-fichero lleva **6 bytes de prefijo** y luego su contenido (`Apt Data:1:7:4`,
   `Apt constant file`, `Apt PCGL geometry file`).
3. **Un `controls.ini` de mando (estilo PC) en el OBB.** El OBB trae
   `ini/na/controls.ini` (tactil), `ini/na/config.dat`, `ini/disc.ini`,
   `ini/na/{ai,career,common,easfc,emotion,fame,fut,locale,player,sku,training}.ini`,
   `data/cmn/cfg_def.ini`, `data/cmn/cfg_psp.ini`, `ini/juiceconfig.ini`.
   **Ninguno reparte botones.**
4. **Texto explicito de menu "A confirma / B vuelve".** No existe para Android; lo mas
   cercano son los glifos heredados de consola ('X button' = aceptar, L1/R1, L3,
   'Select button' = tips, 'Triangle'/'Circle'/'Square' en el texto viejo de FIFA 11).
   Tabla de glifos completa y literal, E3 @`0x0434e0`–`0x043664`: `'Circle button'`,
   `'DPadD button'`, `'DPadL button'`, `'DPadL button' 'DPadR button'`,
   `'DPadR button'`, `'DPadU button'`, `'L button'`, `'L button''R button'`,
   `'R button'`, `'Square button'`, `'Triangle button'`, `'X button'`.
5. **Captions por pantalla de los `HelpScreens/*_DPAD.big`.** Los nombres de las
   paginas existen en el locale (`KNOCK FORWARD` @`0x06d988`, `FORWARD RUN`
   @`0x0729ce`, `GK RUSH` @`0x072a06`, `GROUND THROUGH BALL` @`0x072a5a`,
   `LOB PASS` @`0x072b78`, `LOBBED THROUGH BALL` @`0x072b8c`, `SWIPE TO DROP`
   @`0x0736fe`, `TAP TO PASS` @`0x07373a`, `CLEARANCE` @`0x02620a`, `FINESSE SHOT`
   @`0x0ae944`, `FIRST TOUCH` @`0x0ae960`, `DROP THE BALL` @`0x0ae8be`,
   `KEEPER CHARGE` @`0x0aeb7a`, `PACE CONTROL` @`0x0aeeea`, `SPRINT` @`0x0af1ea`,
   `SLIDE TACKLE` @`0x0af166`, `SWITCH PLAYER` @`0x0af1fa`, `TACKLE` @`0x0af218`,
   `THROUGH BALL` @`0x0af234`, `THROW` @`0x0af250`, `PASS` @`0x0aef30`,
   `SHOOT` @`0x0af108`), pero **cada pagina es una imagen + un caption**: el caption
   no dice que boton es. La asociacion "pagina -> boton" **no esta escrita**.

---

## 8. COMO SE HIZO (para poder repetirlo)

* OBB: contenedor `EB`, `n = 1217` en `+0x004` (BE), inicio de la tabla de nombres en
  `+0x00c`, entradas de 16 B desde `0x30`, tabla de nombres de 40 B en `0x005110`
  (basenames), datos desde `0x01fd70`, **sin comprimir**. La entrada es
  `(offset/16, 0, tamano, djb2)` — **el tamano es el TERCER u32**, no el segundo (me
  costo un intento: con el segundo salian 0 bytes en todos los ficheros).
  Las rutas completas se recuperaron con el `djb2` de la ruta
  (`H = 0x1505; H = H*33 + c`) contra un diccionario de directorios; el resultado esta
  en `scratch/obb_index.json` (`byname` va en paralelo a la tabla de entradas).
* Scripts usados (todos en `C:/Users/f.paolo/AppData/Local/hermes/cache/scratch/`):
  `obb_get2.py` (saca por ruta completa), `big4.py` / `big4_extract.py` (contenedor
  BIG4), `fifa_elf.py` (segmentos y punteros), `fifa_arm.py` (`MOVW`),
  `fifa_movw.py` (intento Thumb, descartado: **el codigo es ARM/A32**),
  `loc.py` / `locdump.py` (locale UTF-16LE), `apt_text.py` (texto del `.apt`),
  `help_text.py` (barrido de los `.big` de mando).
* Los `.loc` son **UTF-16LE**: `[clave][00 00][texto][00 00]`, con una tabla de
  offsets `u32` al principio. El `string.idx` de al lado son parejas `(u32, u32)` de
  8 B (5.857 entradas).

Nada de esto ha escrito en `source/`, ni en los `.sh`, ni en `D:/`.
