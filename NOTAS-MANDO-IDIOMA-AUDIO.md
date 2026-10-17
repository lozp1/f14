# FIFA 14 (Switch, android32) -- el mando, el espanol y el sonido

Medido, no supuesto: la mitad Java en el `classes.dex` del APK (baksmali) y la
mitad nativa en `libFIFA14.so` (desensamblado con `arm-none-eabi-objdump.exe`
desde Python). Continua `NOTAS-ENTRADA-Y-JNI.md` (que cubre el TACTO); aqui va
lo del mando, el idioma y el audio, y lo que quedo sin poder verificar.

---

## 1. LA CADENA DE MANDO (la mitad Java)

El motor de FIFA 14 **no tiene modulo de gamepad**: su "mando" es el modulo de
TECLADO FISICO, y un mando Android entra por `onKeyDown` con sus `KEYCODE_BUTTON_*`.

```
MainActivity.onKeyDown(keyCode, event)      (com/ea/blast/MainActivity.java)
  -> mPhysicalKeyboard.OnKeyDown(keyCode, event)
     (PhysicalKeyboardAndroid extends KeyboardAndroid; mPhysicalKeyboard se crea
      SIEMPRE en MainActivity.onCreate, junto a mMotionEvent)
       IsSystemKey(keyCode)? -> 3(HOME) 5(CALL) 6(ENDCALL) 0x18(VOL_UP)
                                0x19(VOL_DOWN) 0x1a(CAMERA) 0x1b(CLEAR) 0x5b(MUTE)
       getRepeatCount() != 0 -> descarta (sin auto-repeat)
       isAltPressed()        -> el tercer argumento
     -> KeyboardAndroid.NativeOnKeyDown(GetModuleId(), keyCode, alt)
        GetModuleId() = ModuleCatalog.kModuleTypeIdPhysicalKeyboard
  MainActivity.onKeyUp -> mPhysicalKeyboard.OnKeyUp -> NativeOnKeyUp(igual)
  MainActivity.onGenericMotionEvent -> MotionEventAndroid.onGenericMotionEvent
        (<-- LOS EJES NO PASAN POR AQUI: solo un getSource()&2, un packed-switch
             en action 7/8, un Thread.sleep(12) y return true; NUNCA llama a un
             nativo)
```

`IsSystemKey` no es un filtro de mando: las teclas de sistema se entregan al
motor IGUAL y ademas el metodo devuelve `false` (para que Android las procese).
Las que no lo son tambien se entregan y devuelve `true`.

## 1b. EL CONTRATO EXACTO DEL NATIVO (medido instruccion a instruccion)

`Java_com_ea_blast_KeyboardAndroid_NativeOnKeyDown` @0x00e0c498 (220 B) y
`..._NativeOnKeyUp` @0x00e0c574 (220 B), gemelos. La declaracion Java es
`private native NativeOnKeyDown(III)V` -- **TRES parametros para CUATRO valores**,
igual que en el tacto: la llamada es `invoke-direct {p0, moduleId, key, alt}`, asi
que `alt` cae en el slot extra y el nativo lo lee igualmente:

| registro/stack | valor | destino |
|---|---|---|
| r0, r1 | JNIEnv*, jobject | NO se usan |
| r2 | moduleId | `MessageKeyData+12` |
| r3 | androidKey | `MessageKeyData+20` |
| [sp+40] | altPressed | `MessageKeyData+24` |

(Los 40 bytes: `push {r4-r9,fp,lr}` = 32, `sub sp,#8` = 8; el 5.º argumento de
entrada, que estaba en `[sp+0]`, queda en `[sp+40]`.)

El nativo **solo postea**: comprueba `IsMainLoopAlive()` (estado del motor == 2),
pide 28 B al allocator, rellena y despacha con un id CRUDO FIJO en su propio
codigo, no en el argumento:

* `NativeOnKeyDown` -> **0x00040008** (`mov r1,#8; orr r1,r1,#0x40000`)
* `NativeOnKeyUp`   -> **0x00080008** (`mov r1,#8; orr r1,r1,#0x80000`)
* `NativeOnCharacter` @0x00e0c650 -> **0x00000003**, y `NativeOnKeyboardVisibility
  Changed` @0x00e0c720 -> **0x00040009** (y un `MessageToggle` con id 9)

Para comparar, el tacto usa 0x0000000E/0x0004000E/0x0006000E/0x0008000E: el nibble
bajo identifica la CLASE (0xE = puntero, 8 = tecla, 9 = visibilidad, 3 = caracter)
y los altos la accion.

## 1c. DENTRO DEL MOTOR, Y LA PUERTA QUE HABIA QUE ABRIR

`PhysicalKeyboard::HandleMessage` @0x00df0864 enruta por `msg+12 == 600`:
0x40008 -> `OnRawKeyDown`, 0x80008 -> `OnRawKeyUp`, 0x20008 -> `CancelPressedKeys`.

`OnRawKeyDown` @0x00df0ba8 mapea la tecla (objeto en +36) y **primero exige
`IsEnabled()`** (byte +0x90 del modulo). El constructor escribe ese byte a 0:

    PhysicalKeyboard::PhysicalKeyboard() @0x00df0018
      strb r6(=0),[r4,#0x90]   <- IsEnabled = false
      strb r7(=1),[r4,#0x91]   <- IsVisible = true

¿Quien lo enciende? `OnLifeCycleFocusGained` @0x00df12ec:

    mov r1,#1; ldr r2,[r0,#140]; blx r2      <- this->vtable[140](1) = SetEnabled(true)

y `PhysicalKeyboard::Init` @0x00df035c **termina llamando a esa funcion** cuando
la llamada de ciclo de vida del sistema devuelve 5. **El encendido es automatico
-- pero solo para un modulo que EXISTA**, y

    AllocPhysicalKeyboardModule @0x00df13e4
      system->vtable[24]()  ->  if (count < 1) return 0;     // NO se crea el modulo
                               else PhysicalKeyboard::Create(...)

El puerto contestaba `GetPhysicalKeyboardCount()I -> 0`, asi que **el modulo no
se construia nunca y no habia donde poner una tecla**. Eso esta cambiado a 1.
(`SetEnabled` @0x00df17cc no tiene ni un solo `BL` en todo el .so: solo se llega
a el por la vtable, y ese es el camino que se acaba de describir.)

## 1d. LA TABLA: QUE HACE EL MOTOR CON CADA KEYCODE (esto es la respuesta a "¿sabe de mandos?")

`KeyboardAndroid::RawToStdKey(rawKey, alt)` @0x00e0c448, entero:

```c
if (raw -  7 <=  9u) return raw + 41;   // KEYCODE_0..9  -> '0'..'9'
if (raw - 29 <= 25u) return raw + 68;   // KEYCODE_A..Z  -> 'a'..'z'
if (raw > 164)       return 0;          // no es una tecla que el motor conozca
return kRawToStdKey[raw];               // tabla de 165 u32
```

La tabla, leida del fichero (**165 u32 en el offset 0x0107ae60**; el puntero sale
de los dos literales PC-relativos `pc + 0x002db354 + 0xfff93688`):

| Android | motor | | Android | motor |
|---|---|---|---|---|
| BUTTON_A 96 | **0xF026** | | BUTTON_THUMBL 106 | 0xF030 |
| BUTTON_B 97 | 0xF027 | | BUTTON_THUMBR 107 | 0xF031 |
| BUTTON_C 98 | 0xF028 | | BUTTON_START 108 | 0xF032 |
| BUTTON_X 99 | 0xF029 | | BUTTON_SELECT 109 | 0xF033 |
| BUTTON_Y 100 | 0xF02A | | BUTTON_MODE 110 | 0xF034 |
| BUTTON_Z 101 | 0xF02B | | L1 102 / R1 103 | 0xF022 / 0xF024 |
| L2 104 / R2 105 | 0xF023 / 0xF025 | | DPAD_UP/DOWN/LEFT/RIGHT 19/20/21/22 | 0x16/0x17/0x18/0x19 |
| DPAD_CENTER 23 | 0x1A | | BACK 4 | 0x10 |
| HOME 3 | 0x0F | | MENU 82 | 0x0E |
| ENTER 66 | 0x0D | | DEL 67 | 0x08 |
| ESCAPE 111 | 0x1B | | TAB 61 / SPACE 62 | 0x09 / 0x20 |

**Si, el motor conoce los botones de mando como teclas de primera clase.** La
variante `KeyboardAndroidXperiaPlay::RawToStdKey` @0x00e0c9dc ademas manda
DPAD_CENTER -> 0xF02D, BUTTON_X -> 0xF02E y BUTTON_Y -> 0xF02F (y BACK+alt ->
0xF02C) para ese telefono.

IDs de modulo (`ModuleCatalog`), leidos de sus 8/12 bytes:
`Invalid = -1(0xFFFFFFFF)`, Accelerometer 100, Compass 1400, Display 400,
Gyroscope 1500, Mouse 2200, **PhysicalKeyboard 600**, TouchPad 1100,
**TouchScreen 1000**, VirtualKeyboard 700.

## 1e. LOS EJES: NO HAY

Medido, y es la razon de que el stick vaya como cruceta:

* `MotionEventAndroid.onGenericMotionEvent` no llama a ningun nativo (ver 1).
* NO existe ningun `NativeOnMotion*` / `NativeOnAxis*` / `NativeOn*` con
  Axis/Motion en las 26500+ entradas de `.dynsym` de libFIFA14.so.
* `ModuleCatalog` no tiene modulo de joystick/gamepad.
* Ninguna constante `AXIS_*` aparece en el dex.
* Los simbolos de entrada analogica que SI existen (`EA::Input::Touchpad::
  IsTouchpadPointActive`, `CurrentAnalogX/Y` del FUI, `AptAddToInputAnalogQueue`)
  son de la capa del juego/touch, no una entrada que el puerto pueda empujar.

Conclusion: un stick analogico solo puede entrar como su equivalente DIGITAL en
la cruceta. Es lo que hace el puerto (umbral 0.55 / 0.38 con histeresis). ZL/ZR
igual: entran como `BUTTON_L2/R2` (0xF023/0xF025), que es como el motor los
conoce.

## 1f. ¿OCULTA EL MOTOR SOLO SUS CONTROLES TACTILES AL DETECTAR UN MANDO?

**NO. Medido, y no hay camino para que lo haga:**

* No hay ningun `hasGamepad` / `IsPadConnected` / `onInputDeviceAdded` /
  `input device` en el dex ni en el .so.
* El unico punto donde el motor pregunta por un mando es
  `PhysicalKeyboardAndroid::IsVisible()` @0x00e0ccd4, que llama al Java
  `PhysicalKeyboardAndroidDelegate.IsPhysicalKeyboardVisible()`
  (= `Configuration.hardKeyboardHidden`) y guarda el resultado en el modulo.
  Eso no toca la UI: no hay ni una referencia desde el overlay tactil.
* El `onGenericMotionEvent` que podria delatar un mando no llama a nadie (1).
* Lo que SI existe es el juego describiendo un mando en su UI (assets del OBB):
  `data/gui/screens/commonfe/ConnectGamePad.big`, `.../GamePad.big`,
  `data/gui/components/controls/GamePadStatusAnimation.big`,
  `.../controls/overlays/ControllerOverlay.big`,
  `data/gui/screens/HelpAndAbout/ControllerHelp.big`, y en libFIFA14.so los
  nombres de sprite `xb_button_a`, `xb_button_dpadup`, `xb_thumb_lx`,
  `gc_button_a`, `xe_button_lefttrigger`... O sea: la UI TIENE iconos de mando
  (los del FIFA de consola, heredados) -- pero quien los elige es el juego, y no
  hay ninguna regla medida "hay mando -> oculto el tactil".
* `ini/na/controls.ini` (dentro del OBB, 2700 B) **no tiene ninguna clave para
  ocultar el overlay**: solo `SCHEME_ID_P0/P1`, `LAYOUT_ID`, `RENDER_DEBUG`,
  `USE_SIDE_BUTTONS`, tiempos y potencias. Ni `ini/na/locale.ini` ni el
  `AndroidManifest` traen nada parecido.

Lo que SI se puede hacer, sin inventar un parche de render:

1. La via del propio juego: el panel de opciones de control tactil
   (`Control Scheme`, `Big Buttons`, `Floating Joystick`, `Left Handed`,
   `Alternate Controls` -- claves `iControlSetting`, `iBasicScheme`,
   `iBigButtons`, `iFloatingDpad`, `iLeftHanded`, `iAlternateControls`), que es
   donde el juego deja elegir el esquema.
2. La via de los modulos: el motor solo construye los modulos que el perfil de
   dispositivo declara (`AllocXModule`, patron medido en 1c). Responder
   `GetTouchScreenCount() -> 0` haria que NO existiese modulo TouchScreen y por
   tanto no habria overlay -- pero **rompe el tacto**, y el encargo pide dejarlo.
   No se ha hecho.
3. **NO se ha inventado nada de render.** Cualquier "ocultar el overlay"
   dibujando encima, o saltandose sus draws, seria un parche de render y el
   encargo lo prohibe.

---

## 2. EL ESPANOL

### 2a. El OBB SI trae el espanol (nombres exactos)

Indice del contenedor EA "EB" leido entero desde el fichero (1217 entradas de
16 B en 0x30 ordenadas por hash djb2, nombres en 0x5110, tabla de directorios en
0x010f38 de registros de 66 B con la cadena en +8, datos sin comprimir desde
0x1fd70). **El espanol esta, y con los mismos papeles que el ingles:**

    data/gui/locale/SPA_ES.loc                          (el principal)
    data/gui/locale/beapro/SPA_ES.loc
    data/gui/locale/ball_names/SPA_ES.loc
    data/gui/locale/boot_names/SPA_ES.loc
    data/gui/locale/cgexport/SPA_ES.loc
    data/gui/locale/futconsole/SPA_ES.loc
    data/gui/locale/futios/SPA_ES.loc
    data/gui/locale/futplayerstyles/SPA_ES.loc
    data/gui/locale/league_names/SPA_ES.loc
    data/gui/locale/matchday/SPA_ES.loc
    data/gui/locale/online/SPA_ES.loc
    data/gui/locale/stadium_names/SPA_ES.loc
    data/gui/locale/team_names/SPA_ES.loc
    data/gui/locale/trivia/SPA_ES.loc
    data/gui/locale/trophy_names/SPA_ES.loc
    data/audio/speech/SPA_ES.mp3
    data/cmn/fe/about_ES.txt

16 `SPA_ES.loc` y 16 `ENG_US.loc`, uno por uno en LOS MISMOS 14 subdirectorios
(`beapro, ball_names, boot_names, cgexport, futconsole, futios,
futplayerstyles, league_names, matchday, online, stadium_names, team_names,
trivia, trophy_names`) mas el de `data/gui/locale/` y el `.mp3`. **NO existe
ningun `SPA_MX`** en el OBB (el `locale.ini` lo nombra; el fichero no esta).

### 2b. `ini/na/locale.ini` (862 B, extraido del OBB, = el del SD)

```
ISO_TYPE = na
LANG_OVERRIDE = 1   // 0 = yes override (use the DEFAULT_TEXT_LANGUAGE), 1 = no
                    // override (use the device's language), 2 = lang select screen
// set LANG_OVRERRIDE to 0 in order to test the languages:
// ENG_US FRE_FR SPA_ES GER_DE ITA_IT JPN_JP KOR_KR CHS_CN RUS_RU POR_BR DUT_NL
DEFAULT_TEXT_LANGUAGE = ENG_US
[TEXT_LANGUAGES]
ENGLISH = ENG_US
SPANISH = SPA_MX
AUDIO_LANGUAGE = ENG_US
[AUDIO_LANGUAGES]
ENGLISH = eng
SPANISH = mex
```

Es decir: **`LANG_OVERRIDE = 1` = "usa el idioma del DISPOSITIVO"**, y por eso el
motor le pregunta al lado Java. Y su propio comentario dice que `SPA_ES` es uno
de los idiomas que se pueden probar.

### 2c. Y el motor tiene la lista de locales del sistema

En libFIFA14.so, offset 0x01019501:

    en_US, fr_FR, es_ES, de_DE, it_IT, ja_JP, ko_KR, zh_CN,
    nl_NL -> DUT_NL, ru_RU, pt_BR -> POR_BR, tr_TR -> TUR_TR

y las funciones `Localization::OverrideLanguageFromOS` (con sus trazas
"use default text: %s, audio: %s", "%s = %s", "language select screen enabled"),
`SetGameLocale::DoJobFS: set game locale to %s.` y
`ConvertGameLocaleToNetCoreLang`. `es_ES` esta en esa lista **escrito exactamente
asi**, y `GetLocale()` es lo que esa capa compara: el APK lo devuelve como
`Locale.getDefault().toString()` -- `"en_US"` / `"es_ES"`.

En el log de la consola se ve la cadena entera:
`[I/wrap_printf] LANGUAGE en` / `LOCALE en_US` (el motor imprime lo que le
contesta el puerto) y despues carga `ENG_US.loc` en los 14 directorios. Con
`LOCALE es_ES` le toca `SPA_ES.loc`.

### 2d. Lo que se ha hecho

`source/fifa14_blast_stubs.c`: `DEV_LANGUAGE "en" -> "es"` y
`DEV_LOCALE "en_US" -> "es_ES"`.

**NO se ha podido verificar en consola** que el juego arranque en espanol (no hay
consola ni emulador). Lo que SI es dato: la lista del motor tiene `es_ES`, el
OBB tiene los 17 ficheros, y el `locale.ini` pide que se use el idioma del
dispositivo. Si en la proxima prueba el texto siguiera en ingles, **la segunda
via, documentada por el propio juego**, es editar DOS lineas de

    sdmc:/switch/fifa14/external/ini/na/locale.ini      (el fichero del OBB que el
                                                         puerto ya deja extraido)
        LANG_OVERRIDE = 1        ->  0
        DEFAULT_TEXT_LANGUAGE = ENG_US  ->  SPA_ES

(no se ha tocado: la SD es de solo lectura para esta ronda). El audio de
comentarios es otra clave del mismo fichero: `AUDIO_LANGUAGE = ENG_US`, y para
espanol su tabla dice `SPANISH = mex` -- pero en el OBB solo hay
`data/audio/speech/{ENG_US,SPA_ES}.mp3`, asi que lo coherente ahi es
`AUDIO_LANGUAGE = SPA_ES`.

---

## 3. EL SONIDO

### 3a. La cadena, medida

```
APK: com/ea/EAAudioCore/AndroidEAAudioCore.Startup(channels = 2)
  samplerate    = AudioTrack.getNativeOutputSampleRate(3)   // 3 = STREAM_MUSIC
  channelConfig = 12 (CHANNEL_OUT_STEREO; 4 si channels == 1)
  format        = 2  (ENCODING_PCM_16BIT)
  bufsize       = AudioTrack.getMinBufferSize(samplerate, channelConfig, format)
  sAudioTrack   = new AudioTrack(3, samplerate, channelConfig, format, bufsize, 1)
  frames        = bufsize / (2 * channels)
  Init(sAudioTrack, frames, channels, samplerate, sIsOtherMusicPlaying)   // NATIVO
  sInit = true

NATIVO: Java_com_ea_EAAudioCore_AndroidEAAudioCore_Init @0x00ccef5c (440 B)
  r0=env, r1=jclass, r2=track, r3=frames, [sp+32]=channels, [sp+36]=rate,
  [sp+40]=otherMusic      (el orden de la llamada Java)
  -> guarda en AndroidEAAudioCoreJni: sAudioTrackObj, sNumChans, sSampleRate,
     sBufferSize, sIsOtherMusicPlaying
  -> GetMethodID de AudioTrack: sMethodIdWrite, sMethodIdPlay, sMethodIdStop
  -> NewShortArray(channels << 9)
  El hilo mezclador del motor llama entonces: play(), write([SII) / write([BII),
  stop()/flush()/release().
```

### 3b. Por que no se oia nada (dos causas, las dos medidas)

1. **`AndroidEAAudioCore.Startup()V` caia en el comodin del puerto**:
   `[java] com/ea/EAAudioCore/AndroidEAAudioCore.<init>()V -> no-op (one of the
   port's wildcard classes)` y `...Startup()V -> no-op` en `debug.log`. El nativo
   `Init` no se llamaba NUNCA, o sea que el motor no tenia mezclador: por eso en
   el log no hay NI UNA linea de `AudioTrack`.
   (La causa de fondo: `find_method_def` devuelve la PRIMERA coincidencia y el
   comodin de clase -- nombre NULL -- coincide con cualquier metodo, asi que las
   entradas especificas de `F_AUDIOCORE` tienen que ir ANTES del comodin. Estan
   ahora, como ya hacia `F_EAMIO_STORAGE`. Eso se ha corregido.)
2. **`fifa14_audio_init()` no se llamaba desde ningun sitio** (era solo una
   definicion y una declaracion en la cabecera), asi que `g_ao_ready` seguia a 0
   y `write_samples` tiraba todo con un `svcSleepThread`. El dispositivo SI
   estaba abierto (`[audio] audout open: 48000 Hz, 2 ch; 3 x 1024-frame buffers`
   y el self-test OK, de `rt_audout_selftest` del arranque), pero el camino de
   escritura estaba cerrado.

### 3c. Lo que se ha cableado

* `AndroidEAAudioCore.Startup()V` -> `fifa14_h_audiocore_startup`: abre audout
  (`fifa14_audio_init`), crea el objeto `android/media/AudioTrack`, calcula
  `frames = getMinBufferSize / (2*channels)` con los MISMOS numeros que el puerto
  contesta a `getNativeOutputSampleRate`/`getMinBufferSize` (asi el buffer del
  motor es exactamente el que audout acepta y un `write` es una copia directa) y
  llama al nativo `Java_com_ea_EAAudioCore_AndroidEAAudioCore_Init`
  (contrato medido, ver 3a). `Startup(I)V` y `Shutdown()V` (este con el nativo
  `..._Release`) tambien.
* `AudioTrack.play/stop/flush` pasan de comodin a manejadores con estado (antes
  no se enteraba nadie de que el motor habia arrancado su mezcla).
* El hilo: **no se crea ninguno nuevo**. El que llama a `write()` es el hilo de
  audio del PROPIO motor (EAThread), y `rt_audout_submit` se bloquea mientras
  las tres buffers estan en cola -- ese bloqueo es el que marca el paso de SU
  mezclador. El bucle de frames no toca audout, asi que **el sonido no puede
  costar un frame**.
* Log: una linea al abrir el dispositivo (rate, canales, formato s16, tamano de
  buffer) y **una linea `[audio]` por ventana** en el bucle de frames
  (`fifa14_audio_report()`, justo despues de la linea `[loop]`, o sea cada
  `FIFA14_FPS_LOG_FRAMES` = 300 frames) con **bloques escritos, muestras,
  frames, PICO de la mezcla (y el pico de toda la corrida), underruns y
  dropped**, y si el motor esta `playing`/`paused`/`not started`. Si en tres
  ventanas no ha llegado ni un bloque, lo dice explicitamente.
* El pico se mide sobre las muestras que el mezclador entrega (barato: un
  recorrido lineal del buffer que ya se esta copiando).

---

## 4. LO QUE NO SE PUDO VERIFICAR (no hay consola ni emulador)

* Que el motor acepte las teclas: se vera en el `[input]` del proximo
  `debug.log` (los contadores `key N = D down + U up` y la primera tecla de cada
  tipo con su nombre). El encendido del modulo es AUTOMATICO por lo medido en 1c,
  pero no se ha podido ejecutar.
* Que `GetPhysicalKeyboardCount = 1` baste para que se cree el modulo: la logica
  esta medida (`if (count < 1) return 0`), el efecto no.
* Que el juego arranque en espanol (ver 2d) y que el sonido suene (ver 3).
* Que el motor oculte el overlay tactil: NO lo hace por lo medido en 1f.
* El `.nro` nuevo NO se ha copiado a la SD: esta ronda tiene D: en solo lectura.
  El que hay en `D:/switch/fifa14/fifa14_nx.nro` es el de la build ANTERIOR.
