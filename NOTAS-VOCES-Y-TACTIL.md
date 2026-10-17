# FIFA 14 (Switch, android32) — LAS VOCES Y LOS BOTONES TACTILES

Ronda del 2026-10-13 (tercera del día). Lo MEDIDO sale de tres sitios y solo de
esos: el desensamblado de `libFIFA14.so` (ELF32 ARM, el mismo fichero de
18.187.340 B: `C:/Projects/ports/fifa14/libs/libFIFA14.so`, con `nm`, `objdump`
y un resolvedor de referencias PIC propio — ver §4), los ficheros del juego
(`FIFA14.apk`, el OBB de 1,386,236,976 B, el `Profile` del jugador) y los dos
logs de la consola (`D:/switch/fifa14/debug.log` = la corrida nueva, la del
partido; `debug.log.antes` = la de las 01:13-01:17). Lo que no se ha podido
ejecutar va marcado **INFERIDO**.

**AVISO DE ESTA RONDA**: a mitad del trabajo la tarjeta D: se expulsó sola (el
lector). Todo lo de la tarjeta se leyó ANTES y está citado aquí; el despliegue
del build nuevo y cualquier comprobación posterior en `D:` **no se han podido
hacer**. Lo que se pudo hacer (payload + lanzador + verificación de artefactos)
está en §5.

---

## PARTE A — LAS VOCES

### A.1 QUÉ FICHEROS DE VOZ ABRE EL MOTOR (MEDIDO, literal del log)

**`debug.log.antes` (corrida de las 01:13-01:17)** — el motor SÍ abrió el pack
entero, y lo hizo en el momento en que la pantalla de descarga estaba delante
(la línea del popup es 4 líneas antes de los primeros abiertos):

```
5049  [fs] obb asked="data/gui/components/controls/popups/SpeechDownloadPopup.big" ... served=1
5086  [assets] open("contentmanagerSpeech.cmconfig"): found in the index -- 901 B (deflated in the APK; open() inflates it)
5088  [fs] obb asked="Speech/HiRes.cmconfig"                  ... the player's own file ... served=1
5092  [fs] obb asked="Speech/HiRes.cmconfig.cmserver"         ... served=1
5096  [fs] obb asked="Speech/dat_ENG_US.bh"                   ... served=1
5098  [fs] obb asked="Speech/hdr_ENG_US.big"                  ... served=1
5100  [fs] obb asked="Speech/pbp_ENG_US.evt"                  ... served=1
5102  [fs] obb asked="Speech/sth_ENG_US.big"                  ... served=1
5104  [fs] obb asked="Speech/dat_ENG_US.big"  (los 497 MB)    ... served=1
5106  [fs] obb asked="Speech/timestamp.cmconfig"  <-- NO EXISTE: status=NOT in the OBB (the open failed) served=0
5109  [fs] obb asked="Speech/HiRes.cmconfig.cmserver.cmdownload" -> NOT in the OBB
5113  [fs] open  asked=".../Speech/HiRes.cmconfig.cmserver.cmdownload" flags=0x242 rc=11
            (0x242 = O_RDWR|O_CREAT|O_TRUNC: EA::ContentManager::FileDownloader EMPEZANDO UNA DESCARGA)
5118/5121/5124/5126/5130/5134/... el mismo juego de ficheros otra vez (el CM los re-verifica)
5147-5173  Speech/HiRes.cmconfig, .cmserver, dat_ENG_US.bh, hdr, pbp, sth, dat.big  (segunda pasada)
```

**`debug.log` (la corrida nueva, la del partido)** — el motor **no abrió NI UN
fichero de voz**. De las 3.763 líneas `[fs]` del log, las únicas 12 que dicen
"Speech" son las del arranque (`[obb] ==== the commentary pack (Speech/) ====`,
los 7 ficheros con su tamaño y el borrado del marcador) y las 3 del índice del
APK. En esa corrida el motor:
* NUNCA abrió `contentmanagerSpeech.cmconfig` (no hay línea `[assets] open(...)`):
  la pantalla que lo lee no se visitó.
* NUNCA abrió `Speech/dat_ENG_US.big` (497.643.440 B), ni `.bh`, ni `hdr`, ni
  `sth`, ni `pbp`.
* NO hay línea `[speech]` **en ningún log**: el parche del CM de la ronda
  anterior (servir `updateCheckPeriodDays = -1`) **todavía no se ha ejercitado**
  en la consola, porque el motor no ha vuelto a leer ese fichero.

Sí que se jugó un partido en esa corrida (el motor pide los sonidos del campo):

```
5004-5011  asked="07_Load_General_01.viv" / "07_Onto_Pitch_HG_01.viv" / "07_Pitch_Start_B.viv" ... served=0
5074       asked="10_Foul_Neutral_05.viv"   served=0
5083       asked="07_FreeKick_High_01.viv"  served=0
```

o sea: **el sistema de comentarios no arranca en el partido** (no intenta ni
abrir sus ficheros), mientras que el audio de efectos/chants sí funciona
(`data/audio/chants22K.big` se abre al arrancar, `[I/wrap_printf] init audio
paths data/audio/aemsbank22K.big data/audio/chants22K.big`).

### A.2 QUÉ CONDICIÓN FALTA (MEDIDO en el .so, con direcciones)

1. **El sistema de habla es `EA::Audio` + `SPCH_*`**, y su arranque está en la
   función `libFIFA14.so+0x7e6f1c`, llamada UNA vez desde el init de audio
   (`0x7ea6c0`). Lo que hace, instrucción por instrucción:

```
7e6f1c  push {r4-r9,sl,fp,lr} ; sub sp,#0x4cc
7e6f44  si el puntero global del sistema de audio es NULL -> 0x7e7c7c (salir)
7e6f54  bl 0x63f544 ; cmp r0,#0 ; beq ... ; bl 0x63f544 ; ldr r0,[r0,#0x35a8]
7e6f70  cmp r0,#0 ; blt 0x7e7c7c        <-- un campo con signo NEGATIVO aborta el arranque del habla
7e6f78  bl 0x60b5f4
7e6f80  ldr r8,[pc]  -> el valor que hace (GOT(r8) = "AUDIO_LANGUAGE" @0x10196a4)*
7e6f88  add r0, r8, r0 ; bl 0x340910   <-- LEE LA CLAVE "AUDIO_LANGUAGE"
7e6f90-7e6fe0  copia esa cadena (64 caracteres máx.) a un descriptor en la pila
7e6f70f8  la vuelve a leer y la guarda en el objeto del habla (+0x38)
7e71f0  bl 0x49ddd4 ; cmp r0,#1 ; bne 0x7e7c7c   <-- otra puerta: si != 1, no sigue
7e7234  EA::IO::FileStream::FileStream(path)  ; SetPath(path) ; Open(1,6,1,0)
7e72c8  GetSize() ; allocador (vtable+8) ; SetPosition(0) ; Read(buf,size) ; Close()
7e732c  bl SPCH_AddEventDB(buf, 0)        <-- la BASE DE EVENTOS de voz
7e738c  bl SPCH_GetBankPtrMemSize(2800)
7e73a0  allocador ; 7e73c8 bl SPCH_InitBankMem(2800, mem)   <-- LA MEMORIA DE BANCOS
7e73cc  allocador 0x222E0 bytes (los punteros de banco)
```

   \* la referencia se resuelve con el patrón PIC del motor (`ldr rX,[pc,#imm]` +
   `add rX,pc,rX` = base GOT, `add r0,rY,r0` = base+desplazamiento); el
   resolvedor que lo hace está descrito en §4. En `0x7e6f88` y en `0x7e70f8` el
   destino es **`AUDIO_LANGUAGE`** (cadena en `0x10196a4`).

2. **La puerta que hay DELANTE de la llamada** (init de audio, `0x7ea5ac`):

```
7ea690  bl 0x340acc ; cmp r0,#1 ; bne 0x7ea6bc   ; solo si == 1 hace el test
7ea6a0  ... carga la clave (objeto de datos, no literal: no se ha podido
        resolver su nombre) y bl 0x340648 = el lector de opciones, default 1
7ea6b4  cmp r0,#0 ; bne 0x7ea6f4                 ; OPCION != 0  ->  SE SALTA
7ea6bc  bl 0x7e69f0                              ;   el arranque del habla
7ea6c0  bl 0x7e6f1c                              ;   entero
7ea6f4  ...
```

   Es decir: **hay una opción (leída con default 1) que, si vale distinto de 0,
   salta el arranque del habla**. La clave exacta NO se ha podido nombrar (se
   referencia a través de un objeto de datos, no de un literal, y el
   resolvedor no la alcanza); es lo único que queda por nombrar de la cadena.

3. **El estado del ContentManager es lo que decide si el pack está "puesto".**
   Medido en la ronda anterior y confirmado aquí: el fichero de configuración
   que el motor lee (`contentmanagerSpeech.cmconfig`, 901 B DEFLATE en el APK)
   trae `"updateCheckPeriodDays": 0`, y su propio comentario dice
   `-1 = unlimited, 0 = always suggest an update check`; con 0, cada arranque
   pregunta a `serverUrl` (que en este build es `http://127.0.0.1`: no hay
   nadie) → falla la descarga de la descripción
   (`Speech/HiRes.cmconfig.cmserver.cmdownload`, `flags=0x242`, **literal en el
   log**) → `SyncSpeechFailure`. El port ya sirve esa config con `-1`.
   Y el sello que el CM escribe cuando la comprobación sale bien,
   **`Speech/timestamp.cmconfig`**, **no existe** (el motor lo pide y le
   contesta `served=0`): lo escribe `WriteTimeStamp @0xde17cc` (8 bytes) y lo
   lee `IsTimeStampExpired @0xde1710`; el pack no lo traía.

4. **El OBB NO trae el pack** (medido, tabla de 1217 entradas):

```
IN  data/audio/speech/SPA_ES.mp3   0x2edf820    335.232 B   <- el mp3 pequeño SI existe
IN  data/audio/speech/ENG_US.mp3   0x2d79320    420.480 B
OUT Speech/dat_ENG_US.big  Speech/hdr_ENG_US.big  Speech/pbp_ENG_US.evt   <- el pack NO
```

   o sea que el pack de 476 MB es contenido de descarga por diseño y el port
   hace lo correcto sirviéndolo desde la tarjeta.

### A.3 LOS VOLÚMENES DE AUDIO (MEDIDO)

Claves que existen en el `.so` (barrido de cadenas completo, 18.187.340 B):

| clave en el .so | dónde | qué es |
|---|---|---|
| `OPTIONS/COMMENTARY` | `0x1002e53` | opción del Profile; la lee `0x7df7f4` |
| `OPTIONS/COMMENTARY ` (con espacio) | `0x10085dd` | la variante que aparece junto a `MUSICOVERCOMMENTARY` y `data/audio/mix_onp.ini` |
| `OPTIONS/GAMESFX`, `OPTIONS/MUSIC`, `OPTIONS/MUSICGAME`, `OPTIONS/CHANTS` | `0x1002e66`, `0xff08a1`, `0xff088f`, `0x100359e` | las demás opciones de audio |
| `CommentaryVolume`, `CommentaryVolumeFR`, `GameSFXVolume`, `MenuMusicVolume`, `GameMusicVolume` | `0x1003985`-`0x10039d7` | los nombres de la pantalla de ajustes |
| `COMMENTARYVOLUME`, `CHANTSVOLUME`, `GAMESFXVOLUME`, `MUSICVOLUME`, `iCommentaryVolume`, `iChantsVolume`, `iGameSFXVolume`, `iGameMusicVolume`, `iMenuMusicVolume` | `0x1002e76`-`0x1002f9f`, `0x1002ede` | etiquetas internas / nombres de evento JNI |

**Valores en el `Profile` del jugador** (`external/Profile`, 14.400 B: la cadena
de 1.744 B con los 68 pares `OPTIONS/...=...`):

```
OPTIONS/COMMENTARY  = 0          <-- el único que está a 0
OPTIONS/COMMENTARY  = 0          (la variante con espacio, también 0)
OPTIONS/MUSIC       = 1
OPTIONS/GAMESFX     = 1
OPTIONS/MUSICGAME   = 1
(no hay ninguna clave *VOLUME en el Profile: los volúmenes por defecto del
 motor son los que valen)
```

**Y qué hace el motor con ellas (medido, no supuesto)** — el despachador de
efectos `0x7df794`:

```
7df7b0-7df7dc  si el reproductor global es NULL -> salir ; lee "OPTIONS/GAMESFX" (default 1)
7df7e4         si == 0 -> SALIR (no suena NINGÚN efecto)      <-- esta SÍ es una puerta global
7df7f4         lee "OPTIONS/COMMENTARY" (default 1)
7df804-7df81c  if (opcion != 0 && evento == 27) r4 = 0        <-- y esto es TODO lo que hace
7df828-7df834  salto por tabla sobre (evento-1), 64 entradas
```

Conclusión honesta: **`OPTIONS/COMMENTARY = 0` NO es un interruptor global de
los comentarios**: el único sitio donde el motor lo mira es ese despachador y
solo cambia un parámetro del evento 27. El interruptor global de audio que sí
está medido es `OPTIONS/GAMESFX` (= 1 aquí, o sea que el audio de efectos está
encendido). **Ningún volumen está a 0 de forma que explique por sí solo el
silencio**: los ficheros del pack no se abren porque el sistema de habla no
llega a arrancar, no porque su volumen sea 0.

### A.4 LO QUE SE HA ARREGLADO Y LO QUE NO (honesto)

* **Lo que el port ya hacía** (ronda anterior, sigue en pie): sirve
  `contentmanagerSpeech.cmconfig` con `updateCheckPeriodDays = -1` (1 byte sobre
  1, los 901 B intactos) y borra el marcador `.cmdownload` de 0 B si el pack
  está completo. **No se ha podido verificar en consola**: en la corrida nueva
  el motor no leyó esa config (no hay línea `[speech]`). Para verlo hay que
  volver a entrar en la pantalla de idioma/comentarios.
* **Lo que se ha añadido ahora** — una palanca de `config.ini`
  (`[audio] language`, ver §5): el motor **LEE `AUDIO_LANGUAGE` cuando
  inicializa sus bancos de voz** (medido arriba) y los cuatro nombres de
  fichero de voz que pide están **hardcoded en ENG_US**; el port, en cambio,
  fuerza `AUDIO_LANGUAGE = SPA_ES` (por el `data/audio/speech/SPA_ES.mp3` del
  OBB, que es OTRA cosa: el mp3 pequeño del menú, no el pack). Con la palanca se
  puede poner `ENG_US` en una línea y ver en la consola si los bancos cargan,
  sin recompilar. Por defecto se deja **lo de ahora** (SPA_ES) para no cambiar
  el comportamiento sin probarlo: INFERIDO que ENG_US sea la causa, MEDIDO que
  el motor lee esa clave y que sus ficheros son ENG_US.
* **Lo que NO se ha tocado**: `Speech/timestamp.cmconfig` (el fichero de estado
  que falta). No se inventa: con `updateCheckPeriodDays = -1` el motor ni lo lee
  (la salida por "no sync" es ANTES de `IsTimeStampExpired`), y escribir 8 bytes
  con un formato de tiempo que no se ha podido verificar es exactamente la clase
  de "arreglo" que se convierte en otro fallo. Se deja medido y citado.
* **El veredicto**: con lo medido, el pack se sirve bien; lo que falta es que el
  motor ARRANQUE el sistema de habla (función `0x7e6f1c`) y las dos puertas que
  se conocen son (i) el estado del CM (que el port ya empuja con `-1`, sin
  verificar todavía) y (ii) una opción de arranque de audio con default 1 cuyo
  nombre no se ha podido resolver. Las líneas que lo dirán en la próxima
  corrida están en §6.

---

## PARTE B — ¿SE PUEDEN HACER DESAPARECER LOS BOTONES TÁCTILES?

### B.1 QUÉ HACEN LOS PORTS ANDROID EXISTENTES CON ESTE RUNTIME (MEDIDO)

El runtime de este port es **android32** (`github.com/aks796/android32`, el
mismo linaje de `Thorhax/android32` y `xerpi/vita2hos`) y está en
`C:/Projects/ports/android32`.

* **No hay NINGUNA emulación de capacidades del dispositivo para el tacto**:
  `grep -rn "TouchScreenCount\|TouchPadCount\|GetTouchScreenCount\|GetTouchPadCount"`
  sobre `source/`, `launcher/` y los `.md` → **0 coincidencias**. Tampoco hay
  nada de `SystemAndroidDelegate` (esas clases son de EA::Blast, o sea del
  juego, no del runtime).
* **Su única superficie de entrada es `rt_pad`** (`source/rt_pad.h`): qué slots
  de jugador tienen mando, leer un slot, un Joy-Con suelto de lado, y su propio
  comentario lo dice: *"What the buttons do, touch, tilt and cursors stay the
  port's"* — o sea que **el tacto es cosa de cada port**, no del runtime.
* **El perfil del dispositivo (modelo, versión de Android, capacidades) lo
  contesta cada port en sus tablas JNI**. En este: `source/fifa14_jni_tables.c`
  (`GetTouchScreenCount`/`GetTouchPadCount` → handlers) y
  `source/fifa14_blast_stubs.c` (los valores: 1 y 0).
* De los otros juegos de este runtime (`dcr 8`, `a8r 8`, `flappy 8`, `lab2 2`,
  `pvz 2`, `sonic 2`, `abs 1` slots de mando, los mismos de los que habla
  `rt_pad.h`) **no hay fuentes en el árbol**: es el runtime y los ports de al
  lado, y ninguno de ellos trae un caso de "mando presente → escondo los
  controles en pantalla". **No existe esa receta en este linaje**, y el que
  mapea el mando a eventos (a teclas, no a toques) es este port.

### B.2 QUÉ HARÍA ESTE MOTOR CON TouchScreen = 0 (MEDIDO)

* **El registro de los módulos de tacto es INCONDICIONAL.** La función de
  registro de módulos de plataforma, desensamblada entera
  (`libFIFA14.so+0x33a328`):

```
33a328  push {fp,lr}
33a32c  bl EA::Blast::RegisterAccelerometerModule()
33a330  bl EA::Blast::RegisterBatteryModule()
33a334  bl EA::Blast::RegisterDeviceModule()
33a338  mov r0,#0 ; bl EA::Blast::RegisterDisplayModule(0)
33a340  mov r0,#1 ; bl EA::Blast::RegisterDisplayModule(1)
33a348  bl EA::Blast::RegisterPhysicalKeyboardModule()
33a34c  bl EA::Blast::RegisterTouchscreenModule()      <-- SIN NINGÚN TEST
33a350  bl EA::Blast::RegisterTouchPadModule()         <-- SIN NINGÚN TEST
33a354  bl EA::Blast::RegisterVibratorModule()
33a358  bl EA::Blast::RegisterNotificationModule()
33a35c  bl EA::Blast::RegisterVirtualKeyboardModule()
33a360  pop {fp,pc}
```

  o sea: **no hay un "si hay pantalla táctil, registro el módulo"**. El valor
  que el port contesta (`GetTouchScreenCount`) NO decide si el módulo de tacto
  existe.
* **No hay ninguna regla "hay mando → oculto el táctil"**: barrido de cadenas
  completo del `.so` → no existe `hasGamepad`, `IsPadConnected`, `gamepad`,
  `Gamepad` más allá de `isGamepadServer`/`isGamepadClient` (que son de red,
  Blaze), ni `TouchPad`/`touchpad` en minúsculas. Lo que SÍ existe es el juego
  de claves de la capa de controles **de dedo**:
  `CONTROL_CONFIG/LAYOUT_ID`, `CONTROL_CONFIG/SCHEME_ID_P0`, `_P1`, `_LOCAL`,
  `CONTROL_CONFIG/BIG_BUTTONS`, `CONTROL_CONFIG/BIG_DPAD`,
  `CONTROL_CONFIG/FLOATING_DPAD`, `CONTROL_CONFIG/ALTERNATE_CONTROLS`,
  `CONTROL_CONFIG/RENDER_DEBUG`, `CONTROL_CONFIG/BASIC_HAND`, y los nombres de
  glifo por consola `ps2_button_dpad*`, `ps3_...`, `xb_...`, `xe_...`: los
  botones en pantalla son parte del **esquema de control que el juego lee de sus
  ajustes** (`CONTROL_CONFIG/SCHEME_ID_P0/P1`, `OPTIONS/BUTTON_CONFIG0`,
  `SETTINGS/BIG_DPAD`, `SETTINGS/FLOATING_DPAD`), no de una capacidad del
  dispositivo.
* **Consecuencia**: con `TouchScreen = 0` lo más probable es que los botones
  **NO desaparezcan** (el módulo de tacto se registra igual y la UI que los
  dibuja es la del esquema tocable) y que en cambio se pierda el camino que hoy
  hace funcionar los menús: el log nuevo enseña que los menús se navegan con
  **punteros táctiles** (`[input] frame 3000: ... panel up (1 finger), last
  pointer MOVE 0 at 82,517`), y los dos esquemas de este build (TOUCH y
  CLASSIC/DPAD) son los dos táctiles (medido en la ronda de `NOTAS-CONTROLES.md`:
  el `.so` no tiene tabla "botón de mando → acción"). **El riesgo de quedarse
  sin interfaz es real**, así que NO se pone por defecto.

### B.3 LA PROPUESTA (implementada, reversible, y con el default de siempre)

Una clave en el `config.ini` del port:

```ini
[controls]
# (texto literal en el fichero, ver el .diff)
touch_screen = 1        # 1 = lo de siempre (el movil tiene pantalla tactil)
                        # 0 = EL EXPERIMENTO: el juego cree que no hay tactil
```

* La contesta el handler `GetTouchScreenCount` leyéndola del config
  (`fifa14_blast_stubs.c`), y el log imprime en cada arranque qué valor sirve:
  `[dev] SystemAndroidDelegate.GetTouchScreenCount()I -> N: ...`.
* Es **reversible en el sitio**: una línea del fichero, sin recompilar ni
  volver a desplegar, y el default no cambia el comportamiento actual.
* Vías alternativas consideradas y descartadas por el momento (con su riesgo,
  por si se quiere seguir): (a) alterar el perfil del dispositivo *solo* para
  los módulos de UI (no hay superficie para eso: el registro es incondicional,
  y el único valor que el motor pregunta es el que ya está en la palanca);
  (b) tocar los `CONTROL_CONFIG/*` del OBB/servidos para vaciar los botones del
  esquema (es SUPERFICIE DE CONFIGURACIÓN del motor, es la vía limpia de
  verdad — pero hay que saber qué esquema está activo en el partido, y el log
  nuevo no lo dice: `CONTROL_CONFIG/LAYOUT_ID` no aparece en él. Primero hay
  que instrumentarlo, y esta ronda no tenía presupuesto para eso).

---

## 4. LA HERRAMIENTA DE ESTA RONDA (para las siguientes)

El `.so` referencia sus cadenas con el patrón PIC de Android/ARM de TRES
instrucciones, y sin él no se puede decir "quién usa esto":

```
ldr  rX, [pc, #imm]      ; el desplazamiento desde el PC a la base (GOT)
add  rX, pc, rX          ; rX = base        (siempre 0x10e77d8 en este .so)
ldr  rY, [pc, #imm2]     ; el desplazamiento de la cadena respecto a la base
add  r0, rY, rX          ; r0 = base + desplazamiento = la CADENA
```

`scratch/resolv.py` + `scratch/whatis.py` + `scratch/armhelp.py` (en
`C:/Users/f.paolo/AppData/Local/hermes/cache/scratch`) implementan eso sobre el
`.text` entero: resuelven **207.180** referencias a **8.371** destinos
distintos, y con eso se responde "quién lee esta clave" en un comando:

```
python whatis.py 7e6f60-7e6fa0       -> 0x7e6f88 -> 0x10196a4 'AUDIO_LANGUAGE'
python who.py 1002e53                -> los 4 sitios que usan OPTIONS/COMMENTARY
```

Máscaras que costaron tiempo (y hay que dejar escritas): `ldr rX,[pc,#imm]` se
reconoce con `(w & 0xFFF00000)==0xE5900000` **más** `Rn == 0xF` (el nibble de Rd
está en los bits 12-15, y exigirlo rompe todos los cargados en registros que no
sean r0); `add rZ,rY,rM` se reconoce con `(w>>28)==0xE && (w & 0x0FE00FF0)==0x00800000`.
Y las fronteras de función de verdad salen del **`.ARM.exidx`** con PREL31 de
31 bits con signo (`(v & 0x7FFFFFFF)`, signo en el bit 30), no de la tabla de
símbolos (que en este `.so` tiene huecos enormes).

---

## 5. LOS ARTEFACTOS DE ESTA RONDA (MEDIDOS)

```
payload   fifa14_nx.nsp                3835722 B  sha256 f5de79187d7a9626bb30f57aff743938ff3ff6a20719929dd1d1e414f78f015c
romfs     launcher/romfs/fifa14_nx.nsp 3835722 B  sha256 f5de79187d7a9626bb30f57aff743938ff3ff6a20719929dd1d1e414f78f015c  (== payload)
lanzador  launcher/fifa14_nx.nro       4104881 B  sha256 9aa86e3f03d67e1daf09d9e8708666509b4c49e411eccc03cc7e60dd348c0208
build     ./build-fixes.sh: payload rc=0, launcher rc=0, 0 líneas "error:" en los dos logs
          payload VERBATIM dentro del .nro (offset 269013), .nro mas nuevo que el payload
          (02:56:29 > 02:56:25)
sin desplegar: D: se expulsó antes de poder copiar nada
```

Los ficheros tocados: `source/fifa14_config.c` (las dos filas nuevas),
`source/fifa14_blast_stubs.c` (`GetTouchScreenCount` leído del config + el
include de `rt_cfg.h`), `source/fifa14_external.c` (el idioma de audio leído del
config + el include), `fifa14-voces-y-tactil.diff` (este cambio) y este
`NOTAS-VOCES-Y-TACTIL.md`. No se ha tocado el mando, ni el render/swap, ni el
tacto, ni `rt_pad.c`, ni el locale del TEXTO, ni el share, ni el `About`, ni un
byte de `external/Speech/`.

## 6. LO QUE HAY QUE MIRAR EN LA PRÓXIMA CORRIDA (y lo que no se pudo verificar)

1. `[locale] the audio language this run wants: AUDIO_LANGUAGE = ...` → dice qué
   se sirve (§A.4). Poner `[audio] language = ENG_US` y repetir.
2. `[dev] SystemAndroidDelegate.GetTouchScreenCount()I -> 1` → la palanca está
   viva. Con `[controls] touch_screen = 0`: ¿desaparecen los botones? ¿siguen
   funcionando los menús con el dedo?
3. Entrar en la pantalla de idioma/comentarios y buscar `[speech] ... is now -1`
   (el parche del CM, sin verificar todavía) y si el pack sale como DESCARGA.
4. Jugar un partido y buscar `[fs] obb asked="Speech/dat_ENG_US.big"`: hoy esa
   línea **no aparece**. Si con ENG_US aparece y suena, es eso. Si no aparece,
   el corte está en las puertas de §A.2 (el estado del CM o la opción de
   arranque de audio sin nombre).
5. NO verificado por falta de consola (y por la expulsión de D:): nada de lo
   anterior; el despliegue del build; y si el motor escribe
   `Speech/timestamp.cmconfig` él solo cuando el CM acaba bien.
