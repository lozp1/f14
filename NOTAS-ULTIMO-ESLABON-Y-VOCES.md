# FIFA 14 (Switch, android32) -- EL ULTIMO ESLABON DEL MANDO y LAS VOCES

Ronda del 2026-10-13. Continua `NOTAS-MANDO-IDIOMA-AUDIO.md` (que cubre la
cadena Java del mando, la tabla `RawToStdKey`, el espanol del TEXTO y el audio).
Todo lo de este documento esta **medido** sobre tres cosas: el desensamblado de
`libFIFA14.so` (fichero real, `arm-none-eabi-objdump`), los ficheros del juego
(el APK, el OBB y su tabla de 1217 entradas, leidos con Python) y **el
`debug.log` de la consola** (`D:/switch/fifa14/debug.log`), que tiene la corrida
entera. Lo que no se pudo ejecutar va marcado como INFERIDO.

---

## 1. POR QUE EL MOTOR DESCARTA LAS TECLAS (el mando)

### 1a. La puerta, instruccion a instruccion

`PhysicalKeyboard::OnRawKeyDown` @0x00df0ba8, entera (0xbc = 188 B):

```
df0ba8  push {r4,r5,r6,lr}; sub sp,#24; mov r4,r0   ; r4 = this (el modulo)
df0bb4  ldr r0,[r4,#36]                             ; this+36 = su MAPPER
df0bb8  ldr r3,[r0]; ldr r3,[r3,#32]; blx r3        ; mapper->vtable[8]()
df0bc4  mov r5,r0                                   ; r5 = RawToStdKey(raw, alt)
df0bc8  cmp r5,#0; beq df0c5c                       ; 0 = tecla que no conoce -> fuera
df0bd0  ldr r0,[r4]; ldr r1,[r0,#136]; blx r1       ; vtable[34]() = IsEnabled()
df0be0  cmp r0,#1; bne df0c5c                       ; <> 1 -> LA TECLA SE TIRA
df0be8  (inserta la tecla en el conjunto de pulsadas: this+68)
df0c30  ... Keyboard::NotifyKey(mapper, 0x40108, key)
```

y `vtable[34]` es `PhysicalKeyboard::IsEnabled` @0x00df17c4, que es una linea:

```
df17c4  ldrb r0,[r0,#144]; bx lr     ; el byte +0x90 del objeto
df17cc  strb r1,[r0,#144]; bx lr     ; SetEnabled(vtable[35]) es su unico escritor
```

El constructor lo escribe **0** (`PhysicalKeyboard::PhysicalKeyboard` @0x00df0018)
y el unico que lo pone a 1 es `OnLifeCycleFocusGained` @0x00df12ec
(`mov r1,#1; ldr r2,[r0,#140]; blx r2` = `SetEnabled(true)`). Es decir: **un
modulo que no oiga el ciclo de vida traga todas las teclas en silencio**, con
los contadores del puerto corriendo -- exactamente lo que decian los logs
(`key 238 = 86 down + 85 up`).

### 1b. La causa mas probable, MEDIDA en la tabla del motor

`RegisterPhysicalKeyboardModule` @0x00df143c construye un `ModuleRegistryEntry`
con `mov r2,#600` (typeId), `mov r3,#0` (index) y **`str r1,[sp,#8]` con r1 = 0
= el argumento `InstantiationMode`**, y

```
ModuleManager::InitAutoInstantiatedModules @0x00e08268
  e08298  ldr r0,[sl,#48]        ; el modo de la entrada
  e0829c  cmp r0,#1
  e082a0  bne  <siguiente>        ; SOLO se instancian las de modo 1
```

Asi que el PhysicalKeyboard **no es un modulo automatico**: es un modulo "a
demanda" (`ModuleManager::GetModule(typeId, index)` @0x00e084d0 lo busca en el
registro y lo crea con `ModuleManager::GetModule(const IModuleInfo*)`
@0x00e08558). El puerto lo unico que habia hecho era contestar
`GetPhysicalKeyboardCount = 1`; **nadie pidio nunca el modulo**, asi que el
mensaje `0x00040008` con module id 600 que el nativo del motor postea
(`NativeOnKeyDown` @0x00e0c498) no tenia a donde ir.

INFERIDO (no comprobado en consola): esa es la causa. El puerto ahora lo mide y
lo arregla (1c), y la linea del log lo dira en la proxima corrida.

### 1c. Lo que el puerto hace ahora (`source/fifa14_physkbd.c`, nuevo)

1. `gSystem` (simbolo OBJECT exportado del .so) -> el `SystemAndroid*` del motor
   (`NativeOnCreate` @0x00dffc8c hace `str r4,[r6]` sobre esa palabra).
2. `*(SystemAndroid* + 20)` = su `ModuleManager` (`System::GetModuleManager`
   @0x00e01ad4 es `ldr r0,[r0,#20]`, y `System::Init` @0x00e0757c es el
   `str r5,[r4,#20]` que lo guarda).
3. `ModuleManager::GetModule(600, 0)` -> **el modulo, creado si el motor no lo
   habia construido**.
4. Lee el mapper (+36), el byte +0x90 y llama `vtable[34]()` **por la vtable del
   propio modulo** (la misma puerta que mira `OnRawKeyDown`).
5. Si el byte es 0, llama `OnLifeCycleFocusGained(modulo)` -- lo que hace el
   `onResume` de un telefono -- y vuelve a leerlo.
6. Cada objeto se identifica por su vtable antes de tocarlo: `obj[0] ==
   &_ZTV<clase> + 8` (los dos words anteriores al address point son offset-to-top
   y typeinfo). Si no cuadra, se registra y no se sigue.
7. Cada 30 frames (los primeros 1800) y despues cada 300, la linea pedida:
   `[input] PhysicalKeyboard enabled field = N (...)`.

### 1d. El experimento decisivo, y el interruptor

`FIFA14_INPUT_KEY_DIRECT` (`source/fifa14_physkbd.h`, por defecto **1**): las
teclas del pad se entregan llamando **directamente** a `OnRawKeyDown`
@0x00df0ba8 / `OnRawKeyUp` @0x00df0d10 sobre el modulo, con los mismos keycodes
Android. Con 0, se entregan como siempre (nativos de `KeyboardAndroid`, el motor
postea y enruta el mensaje). Los dos caminos son exclusivos: una tecla nunca va
dos veces.

Que mirar en la tele:
* el pad responde al juego -> **el fallo era el enrutado** (el modulo no existia
  o no estaba habilitado) y esta ronda lo ha arreglado creandolo/habilitandolo;
* no responde -> el fallo esta dentro del modulo o mas alla, y el log dira que
  las llamadas se hicieron (`direct calls N = D down + U up`).

---

## 2. LAS VOCES: SI SE PUEDEN, Y DE DONDE SALEN

### 2a. Lo que pide el motor (del debug.log de la consola, literal)

```
[assets] open("contentmanagerSpeech.cmconfig"): found in the index -- 901 B (deflated)
[obb] open("Speech/HiRes.cmconfig") -> NOT in the OBB ... served=0
[assets] open("Speech/HiRes.cmconfig") -> NOT in the APK's assets/ ... FileNotFoundException
[jni] throw java/io/FileNotFoundException: Speech/HiRes.cmconfig
[obb] open("Speech/HiRes.cmconfig.cmserver") -> NOT in the OBB
[obb] open("Speech/HiRes.cmconfig.cmserver.cmdownload") -> NOT in the OBB
[fs] mkdir .../Speech rc=0
[fs] open  .../Speech/HiRes.cmconfig.cmserver.cmdownload flags=0x242 rc=11
```

`assets/contentmanagerSpeech.cmconfig` (901 B, sacado del APK) dice
`"workingDirectory": "Speech"` y `"serverUrl": "http://127.0.0.1"` (o sea: **no
hay servidor en el APK que se distribuye**). Y los nombres de fichero del pack
estan en el propio `libFIFA14.so` como cuatro cadenas consecutivas:
`Speech/hdr_ENG_US.big`, `Speech/sth_ENG_US.big`, `Speech/dat_ENG_US.bh`,
`Speech/pbp_ENG_US.evt` -- **solo las inglesas**. El motor valida **el tamaño**,
no un hash: los `.cmmetadata` del pack son `{"size": N}` y los dos
`contentmanager*.cmconfig` traen `"checksumValidationEnabled": false` y
`"eTagEnabled": false`. No hay firma que falsificar.

### 2b. El pack de la tarjeta (lo que hay que copiar)

Origen en el PC: `C:\Users\f.paolo\Downloads\crash_reports\otro usuario\NARRADORES EN ESPAÑOL BY FABIX7G`
Destino:        `sdmc:/switch/fifa14/external/Speech/`  (= `/sdcard/Android/data/com.ea.game.fifa14_row/Speech/`)

Copiar **el directorio `Speech/` entero**, 12 ficheros (476 MB):

    Speech/HiRes.cmconfig                       5.129 B     (la descripcion de contenido)
    Speech/HiRes.cmconfig.cmserver              4.476 B     (la misma, como descargada)
    Speech/dat_ENG_US.big                     497.643.440 B (los comentarios)
    Speech/dat_ENG_US.big.cmmetadata
    Speech/dat_ENG_US.bh                          127.984 B
    Speech/dat_ENG_US.bh.cmmetadata
    Speech/hdr_ENG_US.big                         332.880 B
    Speech/hdr_ENG_US.big.cmmetadata
    Speech/sth_ENG_US.big                         373.492 B
    Speech/sth_ENG_US.big.cmmetadata
    Speech/pbp_ENG_US.evt                          57.484 B
    Speech/pbp_ENG_US.evt.cmmetadata

**NO copiar**: `Speech/HiRes.cmconfig.cmserver.cmdownload` (0 B) -- es el
fichero TEMPORAL de una descarga incompleta (`FileDownloader::
SetTemporaryFileExtension`; el motor lo creo el solo con `open(...,0x242)` =
`O_RDWR|O_CREAT|O_TRUNC`, y ese `rc=11` del log **es un descriptor de fichero,
no un error**: los `open` que funcionan dan rc=6, 7, 9...). El puerto NO lo crea
nunca; si ya esta, **borrarlo**. Tampoco hay que copiar `Profile`,
`Converte.fsh` ni `unnamed.webp` (no son del pack).

### 2c. Lo que hace el puerto

`source/fifa14_external.c`: `is_user_override()` ahora tiene una lista de
**prefijos** y `"Speech/"` esta en ella, asi que todo lo que haya bajo
`<external>/Speech/` se sirve tal cual y **nunca se pisa**. Ademas
`fifa14_speech_pack_report()` (llamada desde `fifa14_obb.c`, tras el parche de
idioma) escribe al arranque la lista del pack con los tamaños que el motor
espera, y avisa del marcador `.cmdownload`.

---

## 3. EL WATCHDOG: NO ES UN ATASCO DE I/O (medido)

`debug.log` lineas 4856 y 7852: `[watchdog] no frame finished for 10 s`. La
misma linea dice `presented +1304` / `presented +8030`: **los frames SI se
presentaron** en el intervalo (el "ultimo frame" pasa de 1218 a 9146 entre los
dos avisos). Los hilos de fichero estan IDLE, no atascados:

* tid 1003 `/` y tid 1006: `EA::Thread::Condition::Wait` dentro de
  `rw::core::filesys::Device::ThreadEntry` -- su estado de reposo, y **CPU 321 /
  513 ms acumulados en toda la corrida** (0,3-0,5 s: nada).
* tid 1000 (main), aviso 1: dentro de `FileSystem::Load -> AsyncOp::GetStatus`;
  aviso 2: dentro del interprete de scripts del juego (`AptActionInterpreter`).
* tid 1010: `EA::ContentManager::DownloadClientDir...` en `ThreadSleep` (el
  cliente de descargas, dormido).

O sea: el aviso coincide con frames del propio motor largos (el log trae
`[frame] frame 10526 took 357 ms`, 156 ms, 163 ms...), no con un bloqueo de I/O.
Lo que si cuesta segundos de frame es la extraccion **sincrona** de entradas
grandes del OBB por parte del puerto (los `aemsbank22K.big` de 29 MB): es
rendimiento, no entrada. **No se ha tocado** en esta ronda.

---

## 4. LO QUE NO SE PUDO VERIFICAR (no hay consola ni emulador aqui)

* Que el juego reaccione al pad con las llamadas directas: lo dira la pantalla y
  las lineas `direct calls N = D down + U up` + `PhysicalKeyboard enabled field`.
* Que `GetModule(600, 0)` construya el modulo completo (con su Init y su mapper
  a +36): el log imprime el mapper; si saliera 0, `Create` no llamo a `Init`
  (INFERIDO que si: `AllocPhysicalKeyboardModule` -> `PhysicalKeyboard::Create`
  @0x00dff564 es el asignador del registro, y `Init` es lo que hace
  `Keyboard::Create` -> `str r0,[r4,#36]`).
* Que el motor acepte el pack de voces: los nombres, el tamaño y la ausencia de
  checksum estan medidos; la carga de los `.big` no se ha ejecutado.
* El `.nro` NO se ha copiado a la SD (esta ronda no toca la tarjeta): el que hay
  en `D:/switch/fifa14/fifa14_nx.nro` es el de la build anterior.
