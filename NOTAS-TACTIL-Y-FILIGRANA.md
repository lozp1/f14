# FIFA 14 (Switch, android32) — EL ESQUEMA TÁCTIL DEL JUEGO, y LA FILIGRANA

Ronda del 2026-10-13 (quinta del día). Dos cosas, pedidas por el usuario tras
ver en consola que **los botones táctiles cambian según tenga o no el balón**:

1. **documentar el esquema táctil** (qué botones/gestos hay, qué hace cada uno y
   cómo cambian por contexto), y
2. **investigar la filigrana** (los trucos de habilidad) y si el **stick derecho**
   se puede mapear a eso "como en los FIFA de consola".

`MEDIDO` = el dato está en el fichero citado, con su offset. Lo que no se ha
podido ejecutar va marcado **SIN CONSOLA**. Fuentes:

| # | Fuente | Dónde |
|---|---|---|
| E1 | Locale del juego (UTF-16LE) | `data/gui/locale/ENG_US.loc` del OBB, 844.760 B (extraído a `fifa_ctrl/data__gui__locale__ENG_US.loc`) |
| E2 | `libFIFA14.so` (ELF32 ARM) | 18.187.340 B, `sha256 cd15bbe4…` |
| E3 | Los ini del juego | `ini/na/controls.ini` (2.700 B), `ini/na/common.ini` (14.447 B), del OBB |
| E4 | Notas previas de esta misma carpeta | `NOTAS-CONTROLES.md` (la tabla de keycodes), `NOTAS-CONTROLES-MOTOR.md` |
| E5 | El puerto | `source/fifa14_input.c` (la inyección de punteros que YA funciona) |

---

# PARTE A — EL ESQUEMA TÁCTIL, DOCUMENTADO

## A.0. Son dos esquemas, y el táctil es el que corre

El propio juego lo dice: `FIFA14 HAS 2 CONTROL SCHEMES` (E1 @0x737b8),
`TOUCH CONTROLS` / `CLASSIC CONTROLS` (E1 @0x737f4, @0x8f0ac), y
`ini/na/controls.ini` (E3, [CONTROL_CONFIG], comentarios literales del editor):
`SCHEME_ID_P0 = 0` → **0 = dos botones + sprint (basic)**, **1 = tres botones
(advanced)**. En este port, `config.ini [controls] scheme_id` mueve esa línea.

## A.1. El inventario táctil (los botones y gestos que el juego pinta)

Nombres de botón que el juego dibuja, en un solo bloque del locale
(E1 @0x0c5966–0x0c5a78, literal): `PASS`, `SLIDE`, `THROUGH`, `TACKLE`,
`CROSS`, `CLEARANCE`, `SHOT`, `THROUGH BALL`, `LOBBED PASS`, `CHIP SHOT`,
`FINESSE SHOT`, `SLIDING TACKLE`, `ONE-TWO`, `ACTION FEEDBACK`.

Y las reglas generales, también del juego (E1):

| Regla (literal) | Offset |
|---|---|
| `SWIPE UP ON ANY BUTTON TO PERFORM A SPECIAL ACTION` | 0x06f962 |
| `TAP OR HOLD SPRINT FOR DIFFRERENT MOVES.` | 0x06f9ca |
| `TAP, DOUBLE TAP & SWIPE A PLAYER FOR DIFFERENT MOVES` | 0x06fa1e |
| `THE BUTTONS HAVE DIFFERENT FUNCTIONALITY WHEN EITHER ATTACKING OR DEFENDING` | 0x06fa8a |
| `Use the floating joystick to move the player on the field.` | 0x0c6a8e |
| `Sprinting is triggered when the joystick is tilted enough so that it lights up.` | 0x0ca276 |
| `SWIPING UP ON ANY OF THE TWO OUTER BUTTONS WILL PERFORM ACTIONS CORRESPONDING TO THE SECTION OF THE PITCH YOU ARE IN: GROUND CROSSES, FINESSE & CHIP SHOTS, LOBBED THROUGH BALLS` | 0x0707a8 |
| `SWIPE UP ON ANY BUTTON TO PERFORM A SPECIAL ACTION` (la misma idea, en la ayuda por pantalla) | 0x06f962 |

## A.2. Qué hace cada gesto — y **cómo cambia por contexto**

Esta es la tabla que el usuario pedía. Todo es texto del propio juego (E1), y
está agrupado por el contexto que el juego nombra en cada frase.

### Con el balón (ataque)

| Gesto | Acción | Contexto | E1 |
|---|---|---|---|
| Tap **PASS** | pase raso | en posesión | @0x78e2a, @0x0c5e5c |
| Mantener **PASS** | pase bombeado / centro | en posesión | @0x78ca8, @0x0704e2, @0x07050a |
| Tap **PASS** + swipe arriba | pase raso en profundidad | **en posesión** | @0x06ff00 |
| Tap **PASS** + swipe arriba | finesse shot | **dentro del área** | @0x06fdd0, @0x0788e4, @0x0c5db4 |
| Tap **PASS** + swipe arriba | centro raso | **a los lados del área** | @0x070042, @0x078722 |
| Tap **PASS** + swipe arriba | ground through ball | **zona central** | @0x079f66 |
| Tap **SHOOT** | tiro (mantener = más potencia) | en posesión | @0x07906c, @0x0c5d00 |
| Tap **SHOOT** + swipe arriba | chip shot | **dentro del área** | @0x06fe6a, @0x078284, @0x0c5d62 |
| Tap **SHOOT** + swipe arriba | lobbed through ball | **en posesión** | @0x06ffa0 |
| Tap **SHOOT** + swipe arriba | centro raso potente | **a los lados del área** | @0x0700f8, @0x078a10 |
| Tap **SHOOT** + swipe arriba | lofted through ball | **zona central** | @0x078d04 |
| Tap **SHOOT** + swipe abajo | fake shot | mitad contraria | @0x07887a, @0x07054c |
| Doble tap **SPRINT** | knock forward (tirar el balón largo y correr) | corriendo con el balón | @0x0705ce |
| Mantener **SPRINT** | correr con el balón | en posesión | @0x07028a |
| Mantener **SPRINT** al recibir | **first touch** (jugar el balón al espacio con el joystick) | al recibir un pase | @0x0701aa |
| Tap **THROW** | sacar el balón lanzado | **portero con el balón** | @0x078b42 |
| Tap **SHOOT** | despeje | **portero con el balón** | @0x078322 |
| Swipe abajo sobre el portero | que suelte el balón | **portero con el balón** | @0x070674, @0x0787e0 |
| Tap un compañero | pase raso a él / sacar directo | en posesión / saques | @0x070432, @0x070708, @0x0792ee |
| Swipe a un compañero | hacerle correr en esa dirección | en posesión | @0x070358, @0x078f56 |
| **DOBLE TAP** | **SKILL MOVE (la filigrana)** | en posesión | @0x0702fa, @0x07906c, @0x0c5e0a |

### Sin el balón (defensa)

| Gesto | Acción | Contexto | E1 |
|---|---|---|---|
| Tap **SWITCH** | cambiar de jugador | defendiendo | @0x07090c, @0x0c6124 |
| Tap **SWITCH** + swipe arriba y mantener | portero sale de la línea | defendiendo | @0x078aa4, @0x070bb2 |
| Mantener **SPRINT & TACKLE** | acercarse y robar | defendiendo | @0x0709ce |
| Mantener **TACKLE** / **A** | presión y entrada conservadora | defendiendo | @0x0c5f16, @0x0c660c |
| Tap **TACKLE** + swipe arriba | llamar a un segundo defensor | defendiendo | @0x0c6250 |
| Tap **SLIDE** | entrada deslizante | defendiendo | @0x070b14, @0x0c5fe6, @0x0790ca |
| Tap un compañero | cambiar a él | defendiendo | @0x070c88, @0x0c620a |
| Tap **SHOOT** | despeje (dentro de tu mitad) | defendiendo en tu mitad | @0x070a86, @0x0c6036 |
| Tap **SHOOT** + swipe arriba | smart clearance | defendiendo en tu mitad | @0x07914e |
| Tap **JUMP** / tocar la barrera y swipe arriba | saltar la barrera | falta | @0x078c14 |
| Swipe la pantalla / tap jugadores | acciones de jugada a balón parado | set pieces | @0x071f74 |
| Swipe derecha/izquierda | girar la cámara | repeticiones/tiros | @0x0791ec |

Fíjese en la forma de la tabla: **el mismo botón hace cosas distintas según
"attacking or defending"** (E1 @0x06fa8a) **y** según la **zona del campo**
(área, lados del área, central, tu mitad — E1 @0x0707a8 + los textos de
@0x078284…@0x07914e). Eso es exactamente lo que el usuario vio en la consola:
los botones en pantalla cambian con la posesión. La posesión es el primer eje
del contexto; la zona, el segundo.

## A.3. Dónde vive ese contexto dentro del motor (E2)

El juego no tiene una tabla "botón del mando → acción" (medido en la ronda
anterior: `NOTAS-CONTROLES.md` §1c y §3d; el barrido de la tabla de keycodes
`kRawToStdKey` @0x0107ae60 no encontró ni un inmediato `MOVW` fuera de ella).
Lo que sí tiene, medido **esta** ronda:

1. **Los nombres de los contextos de posesión**, en el propio `.so`
   (`SoccerAICore/game/control/`, entre `aicntrl.cpp` y `aictl.cpp`):
   `BUTTON_MODE_NOCHANGE`, `BUTTON_MODE_NONE`, `BUTTON_MODE_WITH_POSSESSION`,
   `BUTTON_MODE_AIR_BALL`, `BUTTON_MODE_OPPONENT_POSSESSION`,
   `BUTTON_MODE_FREE_BALL`, `BUTTON_MODE_OUTOFPLAY`, `BUTTON_MODE_THROWIN`,
   `BUTTON_MODE_THROWIN_OPPONENT`, `BUTTON_MODE_PENALTY`,
   `BUTTON_MODE_OOP_OPPONENT_POSSESSION`, `BUTTON_MODE_GOALIE_HOLDING`,
   `BUTTON_MODE_WITH_POSSESSION_NOT_DRIBBLER` (E2 @0x1025de5–0x1025f0c). **El
   vocabulario del motor para "cómo está el balón" es, literalmente, el que el
   usuario dedujo.** (Estas cadenas no las referencia nadie en este build — ver
   `NOTAS-MANDO-SOLO-EN-PARTIDO.md` §1 — así que no se pueden leer en vivo; lo
   que se puede es *nombrar* el contexto con ellas.)
2. **Las zonas del campo**, en el código del HudController
   (`cmn/input/HudController.cpp`, E2 @0x10189e4–0x1018a3b):
   `FA_UNKNOWN`, `FA_ATT_PENALTY`, `FA_ATT_LEFT`, `FA_ATT_RIGHT`, `FA_ATT_CENTER`,
   `FA_MID_ATT`, `FA_MID_DEF`, `FA_DEF` — el "SECTION OF THE PITCH" de la ayuda
   (@0x0707a8) es un enum de verdad.
3. **Los tiempos del táctil**, en `ini/na/controls.ini` (E3):
   `[CONTROL_TIMEOUTS] SCREEN_TAP = 300` — comentario del editor: *"time (msec)
   to wait for double tap or drag; when this expires, the tap screen action
   starts"* — y `BUTTON_HOLD = 300` (*"time (msec) to press & hold a button
   before the hold action starts"*). El motor lee `CONTROL_TIMEOUTS/SCREEN_TAP`
   (E2, referencia medida en `0x6e17b4`, banda del HudController).
4. **La banda del HudController** (E2, `0x6e0000`–`0x704fff`) es donde se
   convierte el gesto en acción: ahí está `m_swipeSpeed:`/`m_shotPower:`/
   `m_shootingSpin:` (E2 @0x1018b87–0x1018ba4), `Trigger gesture action ->
   hudTargetPosX = 0, hudTargetPosZ = 0` (@0x1018c24), los ajustes de penalti
   `PENALTY_CONTROLS/*` (@0x1018c85+) y `ENABLE_DEBUG_AI_BUTTONS`
   (@0x1018b22, leído en `0x6e97f0`).
5. **La posesión, en el AI** (`SoccerAICore/game/ai/aiballpossessor.cpp`,
   `aiballpossesactions.cpp`, `evalball.cpp` — los nombres de fichero están en
   el binario, E2).

**El port**, hoy: manda los punteros al motor por su propio native
(`NativeOnPointerEvent`, módulo 1000) y el panel/pad del puerto producen DOWN /
MOVE / UP. Los "botones" y los gestos de A.2 los consume el motor; el puerto no
los reimplementa (y por eso el táctil funciona).

---

# PARTE B — LA FILIGRANA (los trucos) Y EL STICK DERECHO

## B.1. Cómo se dispara la filigrana en la versión Android

**Doble tap.** El juego lo dice tres veces, en los tres sitios donde lo
explicaría a un jugador (E1, literal):

| Texto | Offset |
|---|---|
| `DOUBLE TAP THE SCREEN TO PERFORM A SKILL MOVE` | 0x0702fa |
| `Double tap on the screen to make a skill move` | 0x07906c |
| `Double tap the player for a skill move.` | 0x0c5e0a |

Y el diálogo de doble tap es lo que el `SCREEN_TAP = 300` ms de controls.ini
gobierna (E3 + E2 @0x6e17b4). **No hay más disparadores táctiles de la
filigrana**: no hay botón "SKILL" ni gesto propio; el truco es el doble tap,
contextual (con balón).

## B.2. Del lado del motor

* `TriggerSkillMoveAction [%s] _%s_` (E2 @0x10190ac) — el print de depuración
  del disparador de la filigrana; **una sola referencia en todo el binario**:
  `0x7017c0`, en la banda del HudController / montaje del partido (el `0x70xxxx`
  que también monta el HUD del partido, E2).
* La acción **`SKILL MOVE`** está en el catálogo de acciones del motor
  (E2 @0x0101843a–0x010185ac, la tabla que abre `SMART CLEARANCE, SKILL MOVE,
  FIRST TOUCH, …`) con su clave de L10N
  `wwiFifa11_5610_action_skillMove` (E2 @0x10185f6).
* Configuración de la filigrana en `common.ini` (E3, [SETTINGS], literales):
  `ACTION_SKILL_NAME_ENABLED = 1` (mostrar el NOMBRE del truco) y
  `ACTION_SKILL_GRID_ENABLED = 1`; sus etiquetas de UI son `Skill Move Name` /
  `Skill Move Grid` (E2 @0x1004107/@0x10040f7) y sus claves internas
  `CONTROL_GUI/ACTION_SKILL_NAME_ENABLED` (@0x10031c2) y
  `CONTROL_GUI/ACTION_SKILL_GRID_ENABLED` (@0x1003179). O sea: **el motor sabe
  nombrar el truco y dibujar su rejilla**; el ámbito de esa UI es el partido.
* Telemetría/estadísticas: `&skillMove=%d` (E2 @0xff1b48, referenciado en
  `0x34a15c`), `SKILL_MOVES` (@0x10136cf), `skillmoves` (@0x10100b0),
  `f13_skill_NoSkillMove` / `f13_skill_RandomSkillMove` (@0x102ae60/76, sin
  referencias medidas), `GP_Tut_SkillMove` (@0x1021aa2, el tutorial).
* El AI tiene su propio camino: `SoccerAICore/game/ai/aitrickcheck.cpp` (el
  fichero está en el binario) y el **emulador de pad del AI**:
  `AI::mPadEmulater` (E2 @0x10246f0, nombre de depuración de un miembro de
  `AIBallPossessor`). Es decir: **dentro del motor hay un objeto que "pulsa
  botones" programáticamente** — el AI lo usa para sus trucos. Ese es el camino
  más limpio que existe para una filigrana programada, si algún día se localiza
  esa instancia (ver B.4).

## B.3. El stick derecho: por qué "como en los FIFA de consola" no se puede copiar tal cual

* **En esta build no hay ejes.** Medido en la ronda del mando (E4 =
  `source/fifa14_input.c`, cabecera): el `.so` no tiene `NativeOnMotion` ni
  ningún native de ejes; `MotionEventAndroid.onGenericMotionEvent()` no llama a
  nada; `ModuleCatalog` no tiene módulo de joystick analógico; y en
  `kRawToStdKey` los únicos "thumbs" son **botones**
  (`KEYCODE_BUTTON_THUMBL/THUMBR` → `0xF030/0xF031`). Un mando Android le llega
  al motor como **teclado físico**, nunca como palanca. No existe la vía
  "right stick = truco" que sí existe en el motor de consola.
* En **este puerto**, el stick derecho **ya tiene dueño**: es el **cursor
  táctil** (mueve el puntero del motor, que es como se manejan los menús) y
  StickR/StickL son click/segundo dedo. Mover eso a "filigrana" dejaría los
  menús y las pantallas que solo responden al tacto (el diálogo de términos,
  p. ej.) sin navegación por mando.
* La equivalencia táctil del truco (B.1) es un **gesto**: doble tap. El port
  **sí sabe** inyectar punteros (es lo que hace con el panel y con el pad:
  `NativeOnPointerEvent`, DOWN/MOVE/UP con ids 100/101 que no chocan con los
  dedos del panel).

## B.4. ¿Se puede prototipar? Sí, con un "pero" que hay que decir claro

**Lo que se puede hacer hoy, barato (horas, no días):** un doble tap inyectado.
El port ya tiene todo menos el disparador: dos parejas DOWN/UP separadas <300 ms
(el `SCREEN_TAP`), en un punto del viewport, por el mismo camino que ya usa
(`deliver("DOWN"/"UP", id_down/id_up, raw 100, x, y)`).

**El pero:** el doble tap táctil del juego es **contextual y sobre el jugador**
(`Double tap the player for a skill move.` @0x0c5e0a). El port **no sabe dónde
está el jugador controlado** en pantalla: eso lo calcula el motor dentro del
partido (la banda del HudController) y no lo publica en ninguna variable que
hayamos podido medir. Un doble tap "a ciegas" solo tiene una probabilidad
razonable de acertar porque la cámara sigue al jugador (suele caer cerca del
centro/cursor). Es decir: **el prototipo es honesto como EXPERIMENTO** (palanca
de config, doble tap en el cursor o en el centro, mirar si sale el truco en
pantalla y el log), pero **no se puede prometer** que funcione hasta la corrida
en consola.

**Lo que sería la solución "de verdad" (y su coste):** localizar el objeto de
estado de entrada del partido y **escribir la acción como la escribe el AI** —
por el `mPadEmulater` de `AIBallPossessor` (B.2) o por la estructura del
`HudController` que consume el gesto (`0x7017c0` es su puerta). Eso es un
trabajo de RE de verdad: encontrar el objeto de partida (el puntero de
`NOTAS-MANDO-SOLO-EN-PARTIDO.md` §2 es la entrada natural a ese grafo: el
esquema del partido sale de `G->[0xc974]`, así que la partida entera está ahí
dentro) y su mapa de acciones; y luego **escribirlo sin romper nada**, que es
justo la clase de cambio que en este port se prohíbe a ciegas. Es *el* camino
para "stick derecho = filigrana de consola", pero no cabe en una ronda de
timebox y **no se ha implementado**: esta ronda entrega la investigación y el
plan, no el truco.

## B.5. Recomendación (el orden honesto)

1. **Primero** cerrar la ronda del scope (`NOTAS-MANDO-SOLO-EN-PARTIDO.md`) y
   leer la corrida: ahí se ve si el puntero del motor discrimina menú/partido
   (y si no, se pone `always` y no se pierde nada).
2. **Si se quiere el truco ya**, el prototipo barato es la palanca
   `[controls] right_stick_skill = off | tap` (nombre a decidir): con `tap`, un
   flick del stick derecho inyecta el doble tap en el cursor. Se prueba en
   consola en una corrida: si sale la filigrana, se refina el punto; si no,
   queda documentado por qué.
3. **La solución de consola** (acción programada por el objeto del partido /
   el emulador de pad del AI) solo después de eso, y con instrumentación antes
   de escribir — la misma regla que las rondas anteriores (`AVISO CRÍTICO`:
   en los sitios del esquema se monta el mapeo de entrada; parchear a ciegas
   deja al jugador sin controles con un log que parece correcto).

---

## Ficheros tocados por esta ronda (A y B)

Ninguno: esta parte es **solo lectura** (OBB extraído a
`scratch/fifa_ctrl/`, `.so` leído del árbol). Lo que sí se tocó y se compiló es
el scope de las teclas — `NOTAS-MANDO-SOLO-EN-PARTIDO.md` §4 y §5 tiene los
ficheros, los sha256 y la verificación.
