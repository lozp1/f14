# FIFA 14 -- puntos de entrada nativos y superficie JNI

Sacado de las propias libs del juego (`libFIFA14.so`, `libnimble.so`), no de suposiciones.
Complementa a `NOTES.md`. Datos completos en `C:\Projects\ports\fifa14\notas\`.

## 0. LO IMPLEMENTADO (leer esto primero)

`source/fifa14_boot.c` (nuevo) hace el arranque de verdad, en el orden de la
seccion 2, con una linea de log por paso. `source/fifa14_boot.h` y la cabecera
del `.c` tienen el orden y el porque de cada paso. `port_run()`
(source/fifa14_main.c) ahora es: constructores de gnustl/nimble ->
`fifa14_boot_run()`. `port_load()` ademas comprueba los datos
(`fifa14_obb_init()` = source/fifa14_obb.c, y `fifa14_assets_init()`). Las
tablas JNI se inicializan (`fifa14_java_init()`), los 16 simbolos se atan
(`so_try_find_addr_rx`, log `[bind]`) y el bucle bombea `NativeOnDrawFrame`.

El log de `[jni] unhandled ...` (y el `[java] <cls>.<metodo> -> no-op` de los
comodines) SIGUE siendo la lista de tareas: la ronda siguiente se lee en
`debug.log` del primer arranque en consola.

## 0b. EL CONTEXTO GL, Y EL CRASH DEL GESTOR DE MODULOS (ronda siguiente)

`source/fifa14_gl.c` (nuevo) + el paso **7b** de `fifa14_boot.c`: EGL
display/config/contexto GLES2/superficie de ventana sobre la ventana del
runtime y `eglMakeCurrent`, **antes del frame de init (paso 8)**, con los
entry points de EGL pedidos por `dcr_gl_lookup` (sirve con los dos renderers).

Por que: el ultimo `crash.log` es un data abort en `libFIFA14.so+0xe5c8b8`
(`ldrb r3,[r0]`, r0 = 0) dentro de `EA::StdC::Strcmp`, con lr `+0x56619c` (su
unico `bl Strcmp`, el de `+0x566198`), r2 = 0x4d = 'M' y r1 = 0x394086ef. Con
la base real **0x383ff000** (pc 0x3925b8b8 - 0xe5c8b8) r1 cae en
`.rodata+0x010096ef` = **"Mali-T604"**, la tabla de GPU del motor ("Adreno (TM)
200/205/220", "PowerVR SGX 530/535/540/543/544", "NVIDIA Tegra", "GT-I8190N").
El NULL comparado es el retorno de la llamada virtual de `+0x566184`: slot
`0x124` de la vtable del objeto de graficos con argumento **0x1F01 =
GL_RENDERER**, o sea `EA::Graphics::OpenGLES20Managed::glGetString` (`+0xe1f9f0`
-> thunk +8 -> `OpenGLES20::glGetString` `+0xe1821c` -> `glGetString@plt`).
Cadena completa en la cabecera de `source/fifa14_gl.c`; la re-deriva sola
`tools/fifa14-crash-evidencia.py <libFIFA14.so>`.

glGetString devolvio 0 porque **no habia contexto EGL current**: Mesa hace
`if (!ctx) return NULL`. El port no creaba ninguno y el juego tampoco puede:
importa solo `eglGetCurrentDisplay`, `eglGetCurrentSurface` y `eglSwapBuffers`
-- no hay `eglCreateContext`/`eglMakeCurrent` ni como import ni como cadena
para un dlsym. En Android el contexto es de la `GLSurfaceView` de Java; sin
Java, lo tiene que crear el port.

Ademas: `port_gl_wrap("glGetString")` devuelve un envoltorio que ya no puede
dar NULL al motor (avisa una vez por pname, con el motivo: sin contexto o sin
entry point, y contesta una cadena propia cuyo nombre no esta en ninguna tabla
del juego), y `DisplayAndroidDelegate.GetGLView()` -- que estaba "unhandled" y
daba NULL, `DisplayAndroid::CreateView @0xe0bc70` lo mete en un global ref para
`ViewAndroid` -- ahora contesta un objeto `android/view/SurfaceView` propio.

## 1. Los 118 simbolos `Java_*` de libFIFA14.so

El juego expone 118 funciones nativas al lado Java. **El wrapper NO necesita las 118**:
solo las que GOBIERNAN el ciclo de vida. Estas, con su RVA dentro de `libFIFA14.so`:

| Firma | RVA | Bytes | Que es |
|---|---|---|---|
| `Java_com_ea_FIFA14_FIFA14_runEntryPoint` | `0x00338fd8` | 48 | **el "main" de EA** (region baja, junto a initJNI) |
| `Java_com_ea_game_fifa14_Fifa14Activity_initJNI` | `0x0033eb1c` | 188 | **init del JNI: probablemente el primer paso** |
| `Java_com_ea_game_fifa14_Fifa14Activity_shutdownJNI` | `0x0033ebd8` | 68 | cierre del JNI |
| `Java_com_ea_blast_MainActivity_NativeOnCreate` | `0x00dffc8c` | 168 | actividad creada (era el que asumiamos) |
| `Java_com_ea_blast_MainActivity_NativeOnPause` | `0x00dffeac` | 236 | pausa |
| `Java_com_ea_blast_MainActivity_NativeOnResume` | `0x00dfff98` | 236 | reanudar |
| `Java_com_ea_blast_MainActivity_NativeOnBackPressed` | `0x00e00378` | 116 | boton atras |
| `Java_com_ea_blast_AndroidRenderer_NativeOnDrawFrame` | `0x00e003ec` | 124 | **el tick del bucle** |
| `Java_com_ea_blast_AndroidRenderer_NativeOnSurfaceChanged` | `0x00e00468` | 224 | cambio de tamano |
| `Java_com_ea_blast_AndroidRenderer_NativeOnSurfaceCreated` | `0x00e00548` | 72 | **superficie EGL lista** |
| `Java_com_ea_blast_AndroidRenderer_NativeOnSurfaceDestroyed` | `0x00e00590` | 72 | superficie destruida |
| `Java_com_ea_blast_KeyboardAndroid_NativeOnKeyDown` | `0x00e0c498` | 220 | tecla abajo |
| `Java_com_ea_blast_KeyboardAndroid_NativeOnKeyUp` | `0x00e0c574` | 220 | tecla arriba |
| `Java_com_ea_blast_KeyboardAndroid_NativeOnKeyboardVisibilityChanged` | `0x00e0c720` | 384 | visibilidad del teclado |

**CORRECCION IMPORTANTE** frente a lo que se creia: ademas de `NativeOnCreate` existe
`Fifa14Activity_initJNI` y `FIFA14_runEntryPoint`. Los dos viven en la region baja
(`0x33xxxx`, la capa "Java glue" del juego), y `initJNI`/`runEntryPoint`/`shutdownJNI`
estan seguidos en el direccionamiento -> es una misma unidad: la secuencia de arranque
de EA. Orden **VERIFICADO EN EL DESENSAMBLADO** (no "probable") -- el que implementa
`source/fifa14_boot.c`, con el porque de cada paso en su cabecera:

```
1. fifa14_java_init()                             JNIEnv + tablas + actividad
2. los 16 simbolos por so_try_find_addr_rx         [bind] una linea por simbolo
3. so_execute_init_array(libFIFA14.so)             197 constructores (Android: dlopen)
4. JNI_OnLoad(g_jni_vm, NULL)  @0x00e005d8         -> 0x00010004; cachea el vm
5. initJNI(actividad)          @0x0033eb1c         GetJavaVM -> global
6. NativeOnCreate(actividad)   @0x00dffc8c         SystemAndroid; estado = 1
7. runEntryPoint()             @0x00338fd8         -> main(0,NULL) = STUB: devuelve 0
8. NativeOnDrawFrame  (EL FRAME DE INIT) @0x00e003ec   estado 1 -> 2 + Init del motor
9. NativeOnSurfaceCreated      @0x00e00548         ahora SI (exige estado == 2)
10.NativeOnSurfaceChanged(w,h) @0x00e00468         el tamano de la ventana
   bucle: NativeOnDrawFrame por frame  (+ [loop] cada 300 frames)
   salida: NativeOnSurfaceDestroyed, NativeOnPause, shutdownJNI
```

Las tres pruebas que fijan este orden:

- `NativeOnCreate` termina con `str r1=1` sobre el word de estado (0x00dffd14).
- `NativeOnDrawFrame` (0x00e003ec) llama `EA::Blast::LoopLocked` (0x00dffa98) la
  primera vez; `LoopLocked` con estado == 1 llama al slot 0x6c de la vtable del
  sistema (el Init del motor) y pone `estado = 2`. **Es la unica via al estado 2.**
- `NativeOnSurfaceCreated` (72 B) empieza con `if (estado != 2) return;`: llamado
  antes del primer `NativeOnDrawFrame` NO HACE NADA.

Y `Java_com_ea_FIFA14_FIFA14_runEntryPoint` -> `main` @0x00dff844, que en este
build es `mov r0, #0; bx lr`: el "main de EA" es un stub y el titulo lo arranca el
renderer con `NativeOnDrawFrame`, no `main`.

## 2. Superficie de clases Java que el wrapper debe suplir (VM falsa)

Los nombres salen de las cadenas que el codigo nativo pasa a `FindClass`. Todo lo que
NO sea `java/*`, `javax/*`, `android/*` o `dalvik/*` hay que declararlo en las tablas
del port (`jni_class_supers` / clases ausentes).

**LibFIFA14.so: 26 clases, 22 propias.** Las que importan:

```
com/ea/blast/MainActivity                          com/ea/blast/AndroidRenderer
com/ea/blast/TouchSurfaceAndroid                   com/ea/blast/VirtualKeyboardAndroidDelegate
com/ea/blast/PhysicalKeyboardAndroidDelegate       com/ea/blast/ViewAndroidDelegate
com/ea/blast/DisplayAndroidDelegate                com/ea/blast/SystemAndroidDelegate
com/ea/blast/AccelerometerAndroidDelegate          com/ea/blast/BatteryAndroidDelegate
com/ea/blast/VibratorAndroidDelegate               com/ea/blast/PowerManagerAndroid
com/ea/blast/NotificationAndroid                   com/ea/blast/NearFieldCommunicationAndroid
com/ea/blast/DeviceOrientationHandlerAndroidDelegate  com/ea/blast/TouchPadAndroidXperiaPlay
com/ea/blast/C2DMReceiver
com/ea/EAAudioCore/AndroidEAAudioCore              com/ea/EAMIO/StorageDirectory
com/ea/EAMAudio/EAMAudioCoreWrapper                com/ea/EAIO/EAIO
com/ea/EAThread/EAThread                           com/ea/InAppWebBrowser/BrowserAndroid
com/ea/VideoPlayer/PlayerAndroid                   com/ea/game/fifa14/Fifa14Activity
com/ea/game/fifa14/GooglePlusHandler               com/ea/game/fifa14/YouTubeHandler
```

(El framework se llama **EA blast**; la mayoria son *delegates* de servicios Android ->
el wrapper los no-opea pero debe devolver `jclass`/`jmethodID` validos.)

**libnimble.so: 101 clases, 74 propias** -- la capa Java del SDK Nimble
(`com/ea/nimble/*`): `Network`, `HttpRequest`, `IHttpRequest`, `IHttpResponse`,
`Persistence`, `PersistenceService`, `SynergyNetwork`, `SynergyRequest`, `Origin`,
`NimbleConfiguration`, `Facebook`, `friends/*`, `identity/*`, `mtx/NimbleCatalogItem`,
`bridge/*NativeCallback`, `ILog`/`Log`, `Error`, `Utility`...
Y 6 simbolos `Java_*`, todos de `bridge/BaseNativeCallback_nativeCallback__I...`:

```
Java_com_ea_nimble_bridge_BaseNativeCallback_nativeCallback__ILjava_lang_Object_2
                                                      ..._2Ljava_lang_Object_2
                                                      ..._2Ljava_lang_Object_2Ljava_lang_Object_2
                                                      ..._2Ljava_lang_Object_2Ljava_lang_Object_2Ljava_lang_Object_2
Java_com_ea_nimble_bridge_BaseNativeCallback_nativeFinalize
Java_com_ea_nimble_bridge_BaseNativeCallback_nativeObjectCallback
```

Nota: Nimble hace **callbacks hacia el lado Java** con esas 6; en el wrapper no hay lado
Java, asi que si el motor las espera hay que decidir entre no-opearlas o darles un
camino de vuelta. Los servicios de red (Origin/Synergy/MTX/Facebook) **no pueden
funcionar** (EA cerro los servidores): hay que stubbearlos o emulador "offline".

## 3. Como re-generar estos datos

```bash
python "C:\Projects\ports\fifa14\notas\extraer_jni.py"
#  -> fifa14-superficie-jni.md  (clases + firmas JNI + Java_* + GL de fabricante)
# y el listado de direcciones de los puntos de entrada esta en
#    C:\Projects\ports\fifa14\notas\puntos-entrada-libFIFA14.txt
```

Los `.so` son ELF32 ARM: `objdump`/`readelf` de devkit devuelven vacio -> se leen con
Python (`struct`): cabecera ELF32, `.dynsym` (`Elf32_Sym`=16 B, `st_shndx`=0 = UND),
`.dynstr`, `.dynamic` (`DT_NEEDED`=1), `.rel.dyn`/`.rel.plt` (`Elf32_Rel`=8 B,
`r_info>>8` = indice de simbolo).

## 3b. El OBB (1.39 GB) y los datos del juego

`main.136.com.ea.game.fifa14_row.obb`, 1,386,236,976 B. El runtime (dcr_path.c)
traduce la ruta que pide el juego --
`/storage/emulated/0/Android/obb/com.ea.game.fifa14_row/<name>` -- a
`<carpeta del juego>/obb/<name>` (`/sdcard/<name>` va a `<root>/sdcard/<name>`).
`source/fifa14_obb.c` la busca en `obb/`, `sdcard/`, junto al APK y en
`Android/obb/<pkg>/`, la abre, loguea el tamano (comparado con
`FIFA14_OBB_BYTES`), lee 64 KiB a traves de su cache LRU de bloques de 64 KiB
(el patron de `tasm2_obb.c`) y dice si el fichero es un ZIP. **El motor abre el
OBB por su cuenta** (EA::IO / AssetManager -> `EAIO.Startup` ->
`AssetManagerJNI::Startup`), asi que el port NO tiene que servirle los bytes:
lo que aporta es la linea de log que dice si los datos estan y de donde, y
`fifa14_obb_read()` para la ronda siguiente (enganchar o servir las lecturas
del motor). El APK va aparte: `fifa14_assets_init()` (ya llamado en
`port_load`) indexa su `assets/` y loguea cuantos ficheros hay.

Herramientas para volver a mirar el binario (SI funciona con estos ELF32 ARM,
al contrario de lo que decia la nota vieja):

```bash
A=/c/devkitPro/devkitARM/bin/arm-none-eabi-objdump
$A -d -r --start-address=0x00dffa98 --stop-address=0x00dffc8c \
   C:/Projects/ports/fifa14/libs/libFIFA14.so      # LoopLocked, el bucle del motor
/c/devkitPro/devkitARM/bin/arm-none-eabi-readelf -sW \
   C:/Projects/ports/fifa14/libs/libFIFA14.so      # las .dynsym: los 118 Java_*
```

## 4. Pendiente que estos datos NO resuelven

- ~~Confirmar el ORDEN de arranque real~~ HECHO: ver la seccion 2 (verificado en
  el desensamblado e implementado en source/fifa14_boot.c).
- ~~La entrada: por donde espera el motor los toques~~ HECHO: seccion 5.
- Que pide el motor al lado Java que no este en las tablas: lo dice el log del
  primer arranque (`[jni] unhandled ...` y `[java] <cls>.<metodo> -> no-op`).
- `ModuleCatalog.NativeGetModuleTypeId*` (12 simbolos de 8-12 B que devuelven una
  constante): el catalogo de modulos de entrada, patron de `ds_loader.c`.
- Audio: `AndroidEAAudioCore.Init(AudioTrack, IIII)` recibe un `AudioTrack` que
  crea el Java del APK (que aqui no existe); decidir si el port lo crea (los
  `fifa14_h_track_*` ya escriben en rt_audout) o se stubea.
- Tablas JNI completas (campo + metodo, con firmas) -- aqui van los NOMBRES, no el contrato entero.
- Offsets de parches ARM (VSync/temporizador/GC): hay que derivarlos del binario del usuario.
- Los 179 simbolos GLES/EGL y los 104 de libc/bionic de 32 bits (ver NOTES.md).

---

## 5. LA ENTRADA (HECHO): la cadena, y lo que hace el port

Medido, no supuesto: la mitad Java esta en `classes.dex` del APK (bajada con
`baksmali` y leida), la mitad nativa en `libFIFA14.so`.

### La cadena en Android

```
View (com/ea/blast/MotionEventAndroid).onTouchEvent(MotionEvent)
  action         = event.getAction() & 0xff   (0 down, 1 up, 2 move, 3 cancel,
                                               5 pointer-down, 6 pointer-up)
  blastModuleId  = GetBlastModuleId(event)    (getSource() 0x1002/0x5002 ->
                                               TouchScreen; 0x100008 -> TouchPad;
                                               0x2002/0x6002/0x4002 -> Mouse)
  blastRawMsgId  = ActionToBlastRawMsgId(blastModuleId, action)
  x, y           = event.getX(i), event.getY(i)      <-- PIXELES DE LA VIEW
  -> SendMotionEventInternal:
     TouchSurfaceAndroid.NativeOnPointerEvent(blastRawMsgId, blastModuleId,
                                              event.getPointerId(i), x, y)
```
La declaracion Java es `native NativeOnPointerEvent(IIIFF)V`: CUATRO parametros
para los CINCO valores que pasa la llamada, asi que la `y` cae en el slot de
argumento extra. El nativo (0x00e0620c) la lee igualmente:

| registro/stack | valor | destino |
|---|---|---|
| r0, r1 | JNIEnv*, jclass | NO se usan (el metodo es estatico) |
| r2 | blastRawMsgId | argumento del dispatch |
| r3 | blastModuleId | `MessagePointer+12` |
| [sp+48] | pointerId | `MessagePointer+28` |
| [sp+52] | x | `MessagePointer+20` |
| [sp+56] | y | `MessagePointer+24` |

El nativo solo POSTEA un mensaje: primero `IsMainLoopAlive()` (la palabra de
estado del motor == 2, o sea despues del primer `NativeOnDrawFrame`; si no,
descarta el evento), luego pide 32 B al allocator del motor, rellena y despacha.
El motor lo consume en su bucle, que es el hilo del port -> un toque leido en
`fifa14_input_poll()` lo sirve el frame siguiente.

Los codigos son del propio motor (`NativeGetIdRawPointer*`, 8/12 B cada uno):
**down 0x0006000E, up 0x0008000E, move 0x0004000E, cancel 0x0000000E**, y el id
del modulo tactil (`NativeGetModuleTypeIdTouchScreen`): **1000**.

Dentro, el dispatch llega al modulo TouchScreen, que lleva un
`EA::Blast::TouchSurface` en +36 y llama `TouchSurface::OnRawTouchDown/Move/Up/
Cancel` @0x00dfadbc: ahi el raw id pasa a std id (`RawToStdPointerId` @0x00dfad68
es un MAPA: cualquier id estable vale) y ahi esta la transformacion de
orientacion. Las coordenadas se usan como PIXELES (el volteo es
`(size-1) - y` con el size de `SetSize`), y el panel de la consola reporta
1280x720, que es el espacio que el motor ya tiene (el boot le dice 1280x720 en
`NativeOnSurfaceChanged`): sin reescalado.

### Lo que hace el port

`source/fifa14_input.c` (+ `.h`): llama a ESE nativo, con la `Fifa14PointerFn`
del mismo contrato (softfp: dos palabras en r2/r3 y tres en el stack -- el .elf
generado lo confirma, `deliver()` pone msg/module en r2/r3 y pointer/x/y en
[sp]/[sp+4]/[sp+8]).

- **PANTALLA TACTIL (portatil)**: `hid` (activado con la orden 11 a mano, para no
  abortar si la consola no la da), `hidGetTouchScreenStates`, y cada dedo sale
  como DOWN / MOVE / UP con su `finger_id` como raw id. La activacion tambien
  sube si no hay panel (docked): la linea del log lo dice.
- **MANDO**: ahora es el MANDO DE VERDAD del motor (ver
  `NOTAS-MANDO-IDIOMA-AUDIO.md` seccion 1: A/B/X/Y, L/R, ZL/ZR, cruceta y stick
  izquierdo como cruceta, +/- = START/SELECT, por los nativos de teclado del
  propio motor, `KeyboardAndroid.NativeOnKeyDown/Up`). Lo unico que queda de la
  capa tactil en el mando son las partes que el motor no usa: **stick derecho =
  el cursor**, pulsacion del stick derecho = clic, pulsacion del izquierdo =
  segundo dedo (raw id 101). El segundo dedo sigue sin usar el raton del motor
  (`MouseAndroid.NativeOnMouseEvent` @0x00e01bcc, modulo Mouse = 2200) a
  proposito: el perfil de dispositivo del puerto contesta TouchScreen=1 y
  TouchPad=0 y el motor NUNCA pide un raton (no hay ningun `GetMouseCount` en el
  log, y el puerto no tiene handler para el), asi que un evento de raton iria a
  un modulo que el motor no tiene por que haber construido. El segundo dedo cae
  en el modulo que SI existe.
- **Log**: una linea la PRIMERA vez de cada tipo (DOWN, MOVE, UP, CANCEL, KEY) con
  las coordenadas, y un resumen cada 300 frames (entregados, por tipo, punteros
  abajo, dedos del panel, cursor, ultimo puntero). `FIFA14_INPUT_TRACE 1` loguea
  todo (ruidoso).
- **Coste**: dos lecturas de servicio por frame y, solo con evento, una llamada
  al motor. Nada en el camino del render; el swap sigue igual.
- En perder el foco, Android da ACTION_CANCEL: `fifa14_input_release_all()` manda
  el codigo cancel por cada puntero abajo (si no, la UI se queda pulsada).

### Lo que NO se pudo verificar (no hay consola ni emulador)

- Que el motor acepte el evento y la UI responda: se vera en el `[input]` del
  proximo `debug.log` de la consola (los contadores y el resumen cada 300 frames).
- Que el modulo TouchScreen exista de verdad en el motor: el port contesta
  `GetTouchScreenCount()I -> 1` (unico dato que lo sostiene) y el dispatch por
  modulo podria descartar el mensaje en silencio.
- Cursor visible para el mando: NO se dibuja (seria una pasada de GL propia sobre
  el frame del motor, y el encargo incluye no tocar el render). El log da la
  posicion.
- Empujar el evento NO puede romper el render (no toca GL); un crash, si lo
  hubiera, seria del dispatch del motor, y se veria en crash.log.
