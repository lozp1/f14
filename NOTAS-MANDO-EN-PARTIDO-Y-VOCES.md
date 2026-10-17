# FIFA 14 (Switch, android32) -- EL MANDO EN EL PARTIDO (la capa táctil) y LAS VOCES

Ronda del 2026-10-14. Tres cosas, con la corrida de consola del 13/14 (el
`debug.log` de la tarjeta) como evidencia nueva:

1. **OBJETIVO 1 -- el mando en el partido.** El hallazgo del usuario (los
   botones táctiles del partido son contextuales: con balón FILIGRANA / TIRAR /
   PASE / SPRINT) ya decía que el partido lo maneja la capa táctil. Esta ronda lo
   convierte en una medida: **el partido NO escucha el teclado (nadie registra
   un `IKeyListener`: 0 llamadas), y las teclas del mando llegan al motor y no
   tienen a quién notificar.** La solución implementada: el pad **pulsa los
   botones táctiles del propio motor** (el port inyecta el toque igual que el
   panel), leyendo las **rectángulos de botón del objeto de partida del motor**.
2. **EL SCOPE `[pad]`.** La verificación falló en consola (check 2) por un error
   **de la ronda anterior**, no del loader: se leyó la palabra del `.so` en
   *offset de fichero* en vez de en su *dirección virtual* (el segundo PT_LOAD
   desplaza +0x1000). Corregido con la aritmética de la consola como prueba.
3. **LAS VOCES.** El `[ctlcfg] PRACTICE_MODE has 1 value 0` de consola cierra el
   caso de la puerta: **estaba abierta**; el problema no es esa condición. Se
   deja la palanca `[audio] force_speech = 0|1` (llama al arranque del habla del
   motor, una vez, con verificación previa de instrucciones) y la ruta de idioma
   documentada.

`MEDIDO` = leído del fichero `libFIFA14.so` (18.187.340 B, `sha256 cd15bbe4…`)
o del `debug.log` de la consola, con la dirección exacta. Lo que solo puede
decidir una corrida va marcado **SIN CONSOLA**.

---

# PARTE A -- EL MANDO EN EL PARTIDO (OBJETIVO 1)

## A.1 La causa raíz, con la medida que la cierra

El partido no recibe teclas. La evidencia, toda en `libFIFA14.so`:

| Qué | Dónde |
|---|---|
| El módulo de teclado entrega las teclas a sus "listeners" (`EA::Blast::Keyboard::NotifyKey` @0xe06b00 recorre el vector de listeners a +0x24/+0x28 y llama por vtable +8/+12/+16/+20) | 0xe06b00 |
| `EA::Blast::Keyboard::AddKeyListener` @0xe0699c -- **0 llamadas en todo el `.so`** | 0xe0699c |
| `EA::Blast::PhysicalKeyboard::AddKeyListener` @0xdf1054 -- **0 llamadas** | 0xdf1054 |
| `PhysicalKeyboard::AddPhysicalKeyboardListener` @0xdf10b4 -- **0 llamadas** (solo se llama a sí misma desde su thunk) | 0xdf10b4 |

O sea: `OnRawKeyDown` mete la tecla en el conjunto de pulsadas y llama a
`NotifyKey`, y **este build no registra ni un solo listener**. Por eso "las
teclas llegan" (los contadores del puerto lo dicen) y "el jugador no reacciona":
no hay camino.

Comprobación cruzada del log de consola: `direct calls 136 = 68 down + 68 up`,
`routed calls 136`, `PhysicalKeyboard enabled field = 1` y ninguna reacción.

## A.2 El partido escucha TOQUES, y los botones están en el objeto de partida

El motor tiene su propio **HudController** de la capa de entrada
(`cmn/input/HudController.cpp`, la ruta está en el binario @0x1018acb) con el
manejador de toques y el hit-test de los botones. Las direcciones medidas:

```
HANDLER DE TOQUE  0x6e75bc  (objeto, acción, ?, ?, x, y)
  primero comprueba que el punto está en la superficie:
    6e75e4  r0 = 0x114|0xC800 = 0xC914      ; Game[0xC914] = el ANCHO
    6e75f0  cmp x, [G+0xC914] ; bgt fuera
    6e7600  r0 = 0x118|0xC800 = 0xC918      ; Game[0xC918] = el ALTO
  acción 0 (DOWN)  -> bl 0x6e846c            ; el hit-test de botones
  acción 1 (UP/MOVE)-> bl 0x6e98f8           ; el mismo juego de rectángulos
```

```
LOS OCHO RECTÁNGULOS DE BOTÓN, dentro del objeto de partida (G):
  rect i = G + 0xAF58 + 0x18*i, campos {x, y, w, h} en +0/+4/+8/+0xC

  (del hit-test, instrucción a instrucción; el primero, 0x6e84e8:)
    6e84e8  r0 = 0x358|0xAC00 = 0xAF58   ; x0
    6e84f4  cmp x0, x  ; bgt miss
    6e84fc  r1 = 0xF60|0xA000 = 0xAF60   ; w0
    6e8508  add r0, w0, x0 ; cmp r0, x ; ble miss
    6e8514  (y en 0xAF5C, h en 0xAF64, el mismo par)
    6e8540  b 0x6e8a50                   ; HIT -> su manejador

  los ocho, en el orden en que el motor los prueba:
    0: 0xAF58 1: 0xAF70 2: 0xAF88 3: 0xAFA0
    4: 0xAFB8 5: 0xAFD0 6: 0xAFE8 7: 0xB000
```

Y los manejadores por botón (por si hace falta más adelante): el del rect 0 es
0x6e8a50 (que a su vez prueba los rect 1 y 6), el del rect 2 llama
`0x77cc10(G+0xbc34)` @0x6e85c8, el del rect 3 `0x781df0(G+0xbc34)` @0x6e864c, y
el del rect 5 pasa por el camino del temporizador 0x6e871c (evento 26 ->
`bl 0x7f8a50`). Es decir: **el «botón» es un rectángulo de G y su pulsación la
ejecuta el propio motor**; el port no necesita saber más.

## A.3 Lo que hace el puerto ahora (`source/fifa14_padact.c`, nuevo)

Con `[controls] pad_touch_buttons = on` (por defecto):

1. Cada frame, si hay partida (`fifa14_padscope_game()` -- la cadena
   verificada), lee del objeto de partida la superficie (G+0xC914/0xC918) y los
   ocho rectángulos, **verificando cada uno** (dentro de la superficie, tamaño
   positivo y razonable). Si algo no cuadra, ese botón queda fuera y se registra.
2. La primera vez escribe en el log **la tabla entera** con geometría y centros
   (`[padact] rect i @Game+0x…: x, y, w, h, centre x,y`).
3. Los botones del pad mapeados (por defecto `a:0,b:1,x:2,y:3`) se pulsan como
   **toques del propio motor**: `DOWN` en el centro del rectángulo mientras el
   botón está pulsado y `UP` al soltarlo, por el mismo nativo que usa el panel
   (`NativeOnPointerEvent`), con ids propios 120+botón (no chocan con los dedos
   del panel ni con el cursor 100/101). Un toque corto = tap; mantener =
   mantener (el motor decide hold/power).
4. Nada se escribe en el motor: solo se le dan los mismos eventos que da un dedo.
   Al perder el foco o al volver a los menús, los toques que estén pulsados se
   sueltan (`fifa14_padact_release_all()`).

`[controls] pad_touch_map` decide qué botón del pad pulsa qué rectángulo:
`"a:0,b:1,x:2,y:3"` por defecto, con los nombres `a,b,x,y,l,r,zl,zr` y
rectángulos `0..7`. **El mapa por defecto es lo único que no se puede medir del
fichero** (la posición de cada botón en pantalla la define el layout del juego,
no una constante del `.so`); el log de la primera partida imprime los ocho con su
geometría, así que el mapa se corrige con una línea de `config.ini` y los números
de ese log delante.

## A.4 Lo que hay que mirar en la próxima corrida (OBJETIVO 1)

1. `[padact] ARMADED/OFF … el mapa …` al arrancar: dice qué va a hacer.
2. En el partido: `[padact] the engine's touch buttons, out of the game object
   0x… (surface 1280x720):` y las ocho líneas `rect i @Game+0x…: x, y, w, h,
   centre cx,cy -- usable/NOT usable`. **Esta es la tabla que hay que leer para
   el mapa.** Comparadla con lo que veis en pantalla (los botones de la derecha
   y el joystick de la izquierda) y ajustad `pad_touch_map`.
3. `[padact] pad a -> rect 0 @…, centre … -- live` (una línea por botón mapeado).
4. Al pulsar A/B/X/Y en el partido: `[input] the pad's ACTION touch (pointer
   120): DOWN at …` y `[padact] the pad's touches: N down + M up so far …`.
   **Si aparecen y el jugador reacciona** (tira/pasa/etc.), el camino está
   cerrado; si aparecen y no reacciona, el mapa está en otro rectángulo.
5. Si el rectángulo elegido es el equivocado, se cambia `pad_touch_map` y se
   vuelve a probar sin recompilar.

**SIN CONSOLA**: si el motor acepta los toques inyectados como si fueran del
dedo (el táctil real funciona, así que el camino es el mismo), y qué rectángulo
es cada botón.

## A.5 La vía que NO se pudo usar (y por qué, medido)

* **Llamar a los manejadores internos** (como hace el physkbd con el módulo de
  teclado): se han identificado (0x6e8a50, 0x77cc10, 0x781df0, 0x6e871c), pero
  cada uno espera estado que prepara el hit-test (el botón pulsado, el
  temporizador del gesto...), así que llamarlos en frío es la clase de salto "a
  ciegas" que este port prohíbe. La inyección de toques usa el camino COMPLETO
  del motor (hit-test + máquina de gestos + acción), que es exactamente lo que
  hace un dedo.
* **Hooks de instrumentación** en el código del motor: este runtime ejecuta el
  ELF de 32 bits del juego y no tiene (en este port) ninguna facilidad de hook
  de código invitado; no se ha inventado una en esta ronda.

## A.6 Lo que queda para la próxima ronda: MOVER al jugador

El puente de esta ronda hace **las acciones** (los cuatro botones; L/R/ZL/ZR se
pueden mapear con la misma tabla). **Mover al jugador es otra cosa** y se puede
hacer con la misma maquinaria, ya medida:

* el movimiento del partido es el **joystick flotante** de la pantalla (la zona
  táctil de la izquierda): el motor lo trata como un **dedo que baja, se MUEVE y
  se levanta**, con la dirección = (punto - centro) -- el mismo camino que el
  cursor del pad ya usa (`poll_pad_touch`, MOVE con el puntero pulsado).
* Falta **una** cosa: saber cuál de los ocho rectángulos es esa zona (o si el
  joystick es una zona aparte, fuera de los ocho: los ocho son los del
  hit-test de botones y el joystick se maneja en el mismo handler con otra
  condición). El log de la primera partida imprime los ocho con su geometría:
  **el rectángulo grande de la esquina inferior izquierda es el candidato**.
* Implementación propuesta (una ronda): `[controls] pad_stick_move = off |
  rect N` con (a) DOWN en el centro del rect N, (b) MOVE a centro + dirección *
  radio mientras el stick esté desviado (misma histéresis que la cruceta), (c)
  UP al soltar. Nada nuevo: la función de inyección ya está (`?` falta el MOVE,
  que es una línea).
* Por qué NO se ha hecho ya: sin consola no se puede confirmar cuál es el
  rectángulo del joystick, y un DOWN+MOVE "a ciegas" en el rectángulo
  equivocado sería peor que no hacerlo (el jugador se movería o pasaría el
  balón sin querer). El mapa de acciones ya tiene su tabla en el log; el del
  joystick llegará con la misma tabla.

---

# PARTE B -- EL SCOPE DEL PAD: LA ARITMÉTICA QUE FALLÓ (corregido)

La consola del 14/10 trajo el número exacto:

```
[pad] the GOT slot 0x010e1a20 = 0x3a9c7938 (the file's addend is 0x011e0c84 ->
      the loader should have put 0x3aa26c84 there)
[pad] it does NOT point at the game pointer: the signal is UNKNOWN and the gate stays OPEN
```

El loader no tenía nada malo: **el .so no es un espejo 1:1 entre offset de
fichero y VA**. Su segundo PT_LOAD está así:

```
ph 2  offset 0x10ac390  vaddr 0x10ad390  filesz 0xabbd8  memsz 0x32ad08
```

o sea VA = offset + **0x1000** para todo ese segmento. La palabra que el slot
`0x10e1a20` contiene en tiempo de ejecución es la que está en el offset de
fichero `0x10e0a20` (su VA), y vale **0x1181938** -- no la de offset `0x10e1a20`
(que pertenece al VA `0x10e2a20`) y vale 0x11e0c84. La ronda anterior leyó esa.

La prueba con los números de la consola:

```
0x3a9c7938 - 0x1181938 = 0x39846000 = base de carga de esa corrida
   (OnRawKeyDown @0xdf0ba8 se registró en 0x3a636ba8; 0x3a636ba8-0xdf0ba8 = 0x39846000)
0x39846000 + 0x1181938 = 0x3a9c7938  == lo que la consola leyó en el slot  ✔
```

Y la esperada de la ronda anterior (base + 0x11e0c84 = 0x3aa26c84) falla por
0x5F34C. Con el addend correcto la comprobación 2 **pasa** y `pad_keys_scope =
match` gatea de verdad. Cambios en `source/fifa14_padscope.c`:

* `PADS_GAME` = **0x1181938** (con la explicación del offset vs VA en la
  cabecera y en el `#define`);
* la línea de la comprobación 2 imprime ahora **la base de carga, lo esperado y
  lo leído** (`the load base 0x… ; the GOT slot 0x… = 0x… ; … this run's
  arithmetic is 0x… + 0x… = 0x…`), y el fallo dice el desvío exacto;
* el contador de "qué comprobación falló" (`missing`) ya se asigna en cada
  rama, así que la línea `check N failed` dice la verdad;
* `fifa14_padscope_game(&G)` (nuevo): da la **dirección del objeto de partida**
  al código que la necesita (el puente del pad).

---

# PARTE C -- LAS VOCES (OBJETIVO 2)

## C.1 La puerta, re-resuelta byte a byte: PRACTICE_MODE, y estaba ABIERTA

La ronda anterior dijo "la puerta es una opción con default 1 sin nombre". Esta
ronda la ha resuelto **por aritmética del propio patrón PIC** y la consola la
confirmó:

```
0x7ea5ac (el init de audio del motor)
  7ea690  add r0, r4, r5   ; r4 = 0xfff18dca (del pool 0x7ea714), r5 = GOT
                           ;   GOT 0x10e77d8 + 0xfff18dca = 0x10005a2
                           ;   0x10005a2 = "PRACTICE_MODE"
  7ea694  bl 0x340acc      ; Has("PRACTICE_MODE")
  7ea698  cmp r0,#1 ; bne 0x7ea6bc     ; no existe -> AL ARRANQUE DEL HABLA
  7ea6b0  bl 0x340648      ; GetInt("PRACTICE_MODE", 1)
  7ea6b4  cmp r0,#0 ; bne 0x7ea6f4     ; != 0 -> SE SALTA EL HABLA
  7ea6bc  bl 0x7e69f0      ; r0 = el objeto del habla
  7ea6c0  bl 0x7e6f1c      ; EL ARRANQUE DEL HABLA (0xe78 bytes), r0 = ese objeto
```

Y el `debug.log` de la consola dice `[ctlcfg] PRACTICE_MODE has 1 value 0`: la
condición de la puerta **es falsa**, el motor CAE al arranque del habla. Así
que PRACTICE_MODE **no es la causa** de que los comentaristas callen, y el
`force_speech` de esta ronda no evita ninguna puerta: **es otra cosa** (abajo).

## C.2 El arranque del habla y sus condiciones (lo que SÍ puede estar cortándolo)

`0x7e6f1c` (arranque del habla) sale por su cuenta en estos puntos, medidos:

```
7e6f28  mov sl, r0                          ; el objeto que le pasa el init
7e6f44  r0 = *(base + 0x11e5604)            ; el sistema de audio; NULL -> return
7e6f54  bl 0x63f544 ; (obj+0x35a8) < 0 -> return
7e6f80  r8 = "AUDIO_LANGUAGE" (0x10196a4)
7e6f88  bl 0x340910                          ; LEE la clave (lo que sirve [audio] language)
7e71f0  bl 0x49ddd4 ; != 1 -> return
7e7234  FileStream(path)… SPCH_AddEventDB, SPCH_InitBankMem  ; aquí ya carga el pack
```

y `0x7e69f0` (5 instrucciones) devuelve `*(base + 0x11e5600)` -- el "objeto del
habla" que el arranque exige como argumento.

Los cuatro nombres de fichero del pack son **literales ENG_US** en el `.so`
(`Speech/hdr_ENG_US.big` @0x10042e4, `sth_ENG_US.big` @0x10042fa,
`dat_ENG_US.bh` @0x1004310, `pbp_ENG_US.evt` @0x1004325, y `Speech/dat_ENG_US.big`
@0x10236ee) y el **pack latino del usuario vive en ese hueco**, así que la ruta
de idioma correcta es `[audio] language = ENG_US` (el puerto escribe
`AUDIO_LANGUAGE` en `ini/na/locale.ini`; `keep`, el default, sirve SPA_ES --
el mp3 de muestra del OBB, no el pack).

## C.3 La palanca nueva: `[audio] force_speech = 0 | 1` (`source/fifa14_spch.c`)

Con **1**, el puerto llama al par del motor **una vez por corrida**, en cuanto
existe el global que el propio arranque exige (`*(base+0x11e5600) != 0`) y
después del frame 120:

```c
void *obj = ((void *(*)(void))(base + 0x7e69f0))();   /* el objeto del habla */
((void (*)(void *))(base + 0x7e6f1c))(obj);           /* el arranque         */
```

* **Verificación previa, byte a byte**: las 5 palabras de `0x7e69f0`
  (`e59f000c e59f100c e08f0000 e7910000 e12fff1e`) y las 3 de `0x7e6f1c`
  (`e92d4ff0 e24dd0cc e24ddb01`) se comparan con las del fichero antes de
  llamar; si no cuadran, NO se llama y se registra (fail-open).
* Todas las direcciones se comprueban dentro de la imagen cargada.
* Con **0** (default) no se llama a nada: un `return` por frame y la línea del
  arranque.

**Honesto**: sin un hook no se puede ver si el motor YA llamó al arranque antes;
por eso la prueba de la próxima corrida es la de después: con
`force_speech = 1`, las líneas `[fs] obb asked="Speech/…"` (que el puerto sirve)
dirán si el sistema cargó el pack. Si no aparecen, el arranque salió por uno de
sus returns (el sistema de audio, el campo +0x35a8, o `0x49ddd4 != 1`) y la línea
`[locale] AUDIO_LANGUAGE -> …` de la misma corrida dice con qué idioma se sirvió.

## C.4 Qué mirar (OBJETIVO 2)

1. `[spch] [audio] force_speech = 1 (the port will call that pair itself, once)`.
2. `[spch] force_speech = 1: calling the engine's own speech start now (frame …)`
   + `the engine's getter 0x7e69f0 returned 0x…` + la vuelta.
3. **Después**: `[fs] obb asked="Speech/hdr_ENG_US.big" … served=1` (y los otros
   tres). Si salen, el sistema arrancó y toca mirar el volumen (`OPTIONS/
   COMMENTARY` del perfil) y el partido.
4. `[locale] the audio language served: AUDIO_LANGUAGE -> ENG_US …` con
   `[audio] language = ENG_US`.

---

# PARTE D -- FICHEROS TOCADOS

| fichero | qué |
|---|---|
| `source/fifa14_padact.c` / `.h` | **nuevo**: el puente pad -> botones táctiles del motor (rectángulos, verificación, log, mapa por config) |
| `source/fifa14_spch.c` / `.h` | **nuevo**: `[audio] force_speech` (llamada verificada al arranque del habla) + el informe del arranque |
| `source/fifa14_padscope.c` / `.h` | el addend correcto (0x1181938), la aritmética en el log, `fifa14_padscope_game()` |
| `source/fifa14_input.c` / `.h` | `fifa14_input_inject_touch()` (el toque del puente por el mismo nativo), la llamada al puente en `poll_pad`, `arm`/`release_all`, el include |
| `source/fifa14_config.c` | filas `[controls] pad_touch_buttons`, `[controls] pad_touch_map`, `[audio] force_speech` + textos de ayuda (y el VA corregido en la ayuda del scope) |
| `source/fifa14_boot.c` | `fifa14_spch_frame(frames)` en el bucle |
| `tools/syntax-check.sh` | comprueba también los dos ficheros nuevos |

No se ha tocado: el mapeo del pad (teclas), el táctil real, el render/swap, el
locale del TEXTO, el share, el `About`, ni un byte de `external/Speech/`.

---

# PARTE E -- LO QUE SOLO LA CONSOLA PUEDE DECIR

* Que el motor acepte los toques inyectados como un dedo (el camino es idéntico
  al del panel, que funciona; la prueba es la corrida).
* Qué rectángulo es cada botón (el log los imprime; el mapa se ajusta).
* Si con `force_speech = 1` el sistema de habla carga el pack (las líneas
  `[fs] … Speech/…`).
* Si el scope `match` ahora gatea (la comprobación 2 pasa con la aritmética
  corregida; el log trae base, esperado y leído).

---

# PARTE F -- ARTEFACTOS Y VERIFICACIÓN (hecha en este PC)

```
payload    fifa14_nx.nsp                 3.853.658 B  sha256 9a2912cecc6f245c2a2c6edea2026b1d31d6859c917e4d40c47bf412209e3406
romfs      launcher/romfs/fifa14_nx.nsp  3.853.658 B  sha256 9a2912ce…  (== payload)
lanzador   launcher/fifa14_nx.nro        4.122.817 B  sha256 0850f78e9eecbfeb5c68c1a19185e7655f6104811045bff8242fd97b0ea0a96f
NSO        build/fifa14_nx.nso           3.852.590 B  sha256 bee69a29d8ac6fc008f2b9ff36f51198fdfa15fdcd4a83e573d7704d171f9f67
NSP directo install/fifa14-direct.nsp    4.079.320 B  sha256 e6da543af229980b824029fa1d6a9834fec3abf0527580e3fa7b9a3a2529b65d
nº de build 202610140647
```

* `./build-fixes.sh`: payload rc=0, launcher rc=0, **0 líneas `error:` y 0
  `warning:`** en los dos logs (contadas con `grep -ci`).
* `tools/verify-artifacts.py` → `verify-padact.json`:
  `sha_match: true`, `nro_newer_than_payload: true`,
  `payload_verbatim_in_nro: true` (offset **269013**), `error_lines: []`.
* **Cadena de identidad completa**: el `main` dentro de `fifa14_nx.nsp` es
  **byte a byte** `build/fifa14_nx.nso` (`bee69a29…`); el `main` del NSP directo
  tiene **el mismo sha256** (`igual=True` en su verificación); y el `.elf` de
  esta build contiene las cadenas nuevas (`[padact]` ×14, `[spch]` ×9,
  `force_speech` ×4, `the load base` ×1). El NSP directo se regeneró con
  `mk_override2.py fifa14_nx.nsp 055D613A02A6F000 installer/.local-atmo/contents/
  055D613A02A6F000/exefs.nsp` + `ATMO_CONTENTS=installer/.local-atmo/contents
  python3 installer/build-installer-nsps.py direct`, y su ExeFS quedó **idéntico
  al override nuevo** (`[2] ExeFS identico override: True`).
* `tools/syntax-check.sh` (devkitARM en este PC): `exit=0`, sin errores ni
  warnings nuevos (los dos `-Wformat` de `fifa14_external.c` son de antes de
  esta ronda y no se han tocado).
* `tools/verify-round-padact.py` (nuevo, de esta ronda): **28 comprobaciones,
  28 PASS** (`verify-round-padact.out`), re-derivando del `.so` cada constante
  que usan los tres ficheros nuevos -- el slot del objeto de partida y su
  trampa offset/VA, la aritmética de la consola (base 0x39846000, slot
  0x3a9c7938), los ocho rectángulos y su superficie, el despacho de toques
  (0x564a58 lee **el mismo slot** `GOT+0x9a160` = 0x1181938), y el par del
  habla con sus palabras de verificación y su clave `PRACTICE_MODE`.

**El LEEME de la tarjeta** (`D:/switch/fifa14/LEEME.txt`) lleva ya las dos
secciones nuevas (el mando en el partido con `pad_touch_buttons`/`pad_touch_map`
y los comentaristas con `[audio] language = ENG_US` + `force_speech = 1`), con
las líneas del log que hay que mirar en cada caso.
