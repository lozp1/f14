# FIFA 14 (Switch, android32) -- EL MANDO ARREGLADO y LAS VOCES "INSTALADAS"

Ronda del 2026-10-13 (segunda del dia). Continua `NOTAS-ULTIMO-ESLABON-Y-VOCES.md`.
Lo MEDIDO sale de tres sitios y solo de esos: el desensamblado de `libFIFA14.so`
(`arm-none-eabi-objdump`, el fichero real de `D:/switch/fifa14/`), los ficheros
del juego (APK, OBB, el pack del PC y la copia de la tarjeta) y el `debug.log`
de la consola (la corrida de las 01:13-01:17). Lo que no se ha podido ejecutar
va marcado **INFERIDO**.

---

## 1. EL MANDO: LA TECLA NO LLEGABA PORQUE EL PUERTO RECHAZABA EL MODULO

### 1a. Lo que decia el log (literal, la corrida de la consola)

```
[input] PhysicalKeyboard: ModuleManager::GetModule(600, 0) -> 0x9560a574 (the module is there)
[input] PhysicalKeyboard: 0x9560a574 is NOT a PhysicalKeyboard (its first word is 0x29ba2234,
        the engine's vtable address point is 0x29ba0798): the pad's keys cannot be delivered,
        the engine's message path stays in use
```

El pad SI se leia (`a button was seen: yes`, `keys delivered 53 = ...`) y el
puerto SI entregaba teclas con los keycodes del motor. El objeto SI estaba
(GetModule lo construye a demanda). Lo que fallaba era **la comprobacion de
clase**: `*(uint32_t*)obj == _ZTVN2EA5Blast16PhysicalKeyboardE + 8`.

### 1b. Que era el objeto, MEDIDO en el .so (no inferido)

Las dos tablas del motor, leidas del fichero:

| symbol | vaddr | contenido medido |
|---|---|---|
| `_ZTVN2EA5Blast16PhysicalKeyboardE` | `0x010dc790` | AP en `+8`; `+52` HandleMessage, `+112` OnRawKeyDown, `+116` OnRawKeyUp, `+120` OnRawKeyCancel, `+128` OnLifeCycleFocusLost, `+132` OnLifeCycleFocusGained, `+136` IsEnabled, `+140` SetEnabled, `+144` OnUpdate |
| `_ZTVN2EA5Blast23PhysicalKeyboardAndroidE` | `0x010de190` | **la DERIVADA** (la del telefono); los mismos seis en los mismos offsets; sobreescribe solo D1/D0/Init/IsVisible |

Y lo que cierra el caso: en LAS DOS tablas, el raw slot 39 es `0xfffffffc`
(offset-to-top de un subobjeto a +4), el 40 es el typeinfo y el **41 es el
address point de la tabla SECUNDARIA** de ese subobjeto (`vtable + 0xA4`), con
los thunks `_ZThn4_...`. Entonces

    0x010de190 + 0xA4 - 0x010dc790 - 8 = 0x1A9C

**exactamente el `0x29ba2234 - 0x29ba0798` del log**. O sea: el
`ModuleManager::GetModule()` devuelve el puntero a la **interfaz a +4** del
objeto, no al objeto; el objeto esta 4 bytes antes, y el primer word del objeto
es el address point PRIMARIO de la derivada (`+0x1A08`, que el puerto tampoco
comparaba). La comprobacion vieja no podia pasar **nunca**, ni para la clase
base ni para la derivada. Esto explica las teclas tiradas sin que el modulo
tenga nada malo.

### 1c. Lo que hace ahora el puerto (`source/fifa14_physkbd.c`)

1. Identifica la clase **por sus ENTRADAS**: resuelve por NOMBRE los seis
   metodos del motor (`HandleMessage @+52`, `OnRawKeyDown @+112`,
   `OnRawKeyUp @+116`, `OnLifeCycleFocusGained @+132`, `IsEnabled @+136`,
   `SetEnabled @+140`) y cuenta cuantos estan en la tabla candidata, cada uno en
   su offset medido. **3 o mas = es el modulo**, sea la base o la derivada, sea
   el address point primario o el secundario.
2. Prueba los DOS punteros: el que devuelve GetModule y el de 4 bytes antes; el
   que tenga los entries es el objeto (`this` de las llamadas), y el log dice
   cual era (`el manager devuelve el puntero a la interfaz +4 ...`).
3. Nombra la clase comparando su address point con los dos `_ZTV` exportados
   (base / derivada / otra).
4. Lee el byte +144 (IsEnabled) y lo loguea; con `vtable[34]()` al lado.
5. Si esta a 0: `OnLifeCycleFocusGained` y se vuelve a leer; si sigue a 0,
   `SetEnabled(1)` (`strb r1,[r0,#144]`) y se vuelve a leer. Las tres lecturas
   se loguean.
6. `GetModule(600, 1..4)` si el 0 no diera nada, y si ningun candidato tiene los
   entries, se dice QUE son (y no se sigue el puntero).
7. Todo contra `game_mod.load_virtbase/load_size`: un address point que no este
   en la imagen del motor no se desreferencia.

### 1d. Los DOS caminos, a la vez, con contador cada uno

`FIFA14_INPUT_KEY_DIRECT` (directo al modulo) y `FIFA14_INPUT_KEY_ROUTED`
(nativos de KeyboardAndroid -> mensaje 0x40008 del motor -> ModuleManager -> el
modulo, que ahora existe y esta habilitado) estan los dos a 1: cada tecla va por
los dos y cada camino cuenta lo suyo, para que **una corrida diga cual responde**
(`[input] key paths: direct calls N = ... , routed calls N = ...`).
**Y es seguro, medido**: el `OnRawKeyDown` del motor @0x00df0ba8 mete la tecla en
el conjunto de pulsadas (`bl <hashtable::DoInsertValue>` @0xdf0bf8) y **sale sin
notificar si ya estaba**:

```
df0bfc  ldrb r0,[sp,#16]         ; el flag "inserted" de DoInsertValue
df0c00  cmp r0,#0; beq <return>  ; ya pulsada -> ninguna segunda notificacion
```

o sea que una tecla entregada dos veces (una por camino) avisa al juego UNA vez
(no hay doble paso en los menus). Los dos caminos son **#define de compilacion**
en `source/fifa14_physkbd.h` (`config.ini` no tiene fila para ellos): poner uno a
0 ahi y recompilar.

**INFERIDO (no hay consola aqui)**: que el juego responda al pad con el modulo
identificado y habilitado. Lo dira la pantalla y las lineas
`[input] the module object 0x...: its vtable slots match OnRawKeyDown/HandleMessage/IsEnabled = YES`,
`[input] PhysicalKeyboard enabled field = 1`, `direct calls N = ...`, `routed calls N = ...`.

---

## 2. LAS VOCES: EL ESTADO NO ESTA EN EL Profile, ESTA EN LA CONFIG DEL CM

### 2a. Los dos `Profile` de 14.400 B: NO son estado de instalacion

Volcados enteros y comparados (`D:\switch\fifa14\external\Profile` -- el de la
tarjeta, escrito por el motor a las 01:16 -- contra el del pack):

* Cabecera identica: `0x383c 0x00000000` = 14396 (= 14400 - 12) y 0.
* Byte 0x110: `01 00 00 00 00 00 00 00` y despues **una sola cadena larga**
  `OPTIONS/FAMILY_PLAY0=0&OPTIONS/FAMILY_PLAY1=0&...` (76 pares en el del pack,
  68 en el de la tarjeta) + los nombres de las partidas guardadas
  (`SAVETOURNA.13.10.2026.0.39.57` en el de la tarjeta; `SAVECAREER.19.7.2022.
  15.24.7` y `SAVETOURNA.5.7.2022.22.42.14` en el del pack).
* Todo lo anterior al byte 334 es identico; **desde el 334 cambian 1972 bytes en
  145 tramos** que son, todos, valores de OPCCIONES del autor del pack
  (`CAMERA_HEIGHT 0 vs 50`, `HANDBALLS 1 vs 0`, `CAMERA 5 vs 10`,
  `DIFFICULTY_LEVEL 4 vs 0`, `HALF_LENGTH 6 vs 2`, `KEEPER_DIFFICULTY_LEVEL -1
  vs 1`, `MUSIC/MUSICGAME 0 vs 1`, `RADAR 1 vs 0`, `REFEREE`, `TITLE_MODE`,
  `ISO_TYPE=na`, `DEFAULT_TEXT_LANGUAGE=ENG_US`...) y la LISTA DE PARTIDAS
  GUARDADAS del autor.

Conclusion: **el `Profile` es el perfil del jugador (opciones + indice de
partidas), no el estado del ContentManager**. El del pack lleva **el progreso y
las opciones de otra persona**: no se copia (pisaria las del jugador). Y no hay
ni una clave de `Speech`/`cm`/packs en ninguno de los dos: el "instalado" no
vive aqui.

### 2b. Donde vive el estado, MEDIDO: `updateCheckPeriodDays`

`EA::ContentManager::FlowInitialization::GetRecommendedSyncAction` @0x00dd3a94,
instruccion a instruccion:

```
dd3aa8  GetContentDescFile(0); IsContentDescFileUsable(desc)
dd3ac0  if (!usable)                          -> 1   (sync)
dd3ac8  if (!SyncList::IsSyncListEmpty(...))   -> 1   (sync: faltan ficheros)
dd3adc  r7 = Configuration+20                  <- "updateCheckPeriodDays"
dd3ae0  if (r7 < 0)                            -> 0   ** NO SYNC **
dd3b9c  periodo = r7 * 86400 * 1e9 (constante 0x4E94914F0000, umull/mla)
dd3bbc  TimeStampUtils::IsTimeStampExpired(ruta, periodo)
dd3bf8  return expired ? 2 : 0                 (2 = toca comprobar)
```

`IsTimeStampExpired` @0xde1710: abre la ruta, lee 8 bytes (`GetTime()`, lo que
escribe `WriteTimeStamp` @0xde17cc); si no son 8 -> expirado; **si el periodo es
0 -> expirado**; si no, `now > stamp + periodo` -> expirado.

Y el `contentmanagerSpeech.cmconfig` del **APK** (901 B, DEFLATE; el puerto lo
sirve por su AssetManager) trae:

```json
"comment": "UpdateCheckPeriod is in Days. -1 = unlimited, 0 = always suggest an update check.",
"updateCheckPeriodDays":     0,
"serverUrl": "http://127.0.0.1",
```

Con `0` TODAS las corridas empiezan "toca comprobar" -> el CM va al servidor
(`http://127.0.0.1`: no hay ninguno) y baja la descripcion
(`Speech/HiRes.cmconfig.cmserver.cmdownload`, `flags=0x242` = O_RDWR|O_CREAT|
O_TRUNC, **el log lo tiene literal**) -> falla -> `SyncSpeechFailure` -> la
pantalla sigue ofreciendo el pack como DESCARGA aunque los 476 MB esten en la
tarjeta y se sirvan (`served=1`). Y el fichero `Speech/timestamp.cmconfig` que el
motor pide y no existe es justo el que sella "ya comprobe"; el pack no lo traia.

### 2c. Lo que hace ahora el puerto

* Sirve **esa sola config** con `updateCheckPeriodDays` = **-1** ("unlimited",
  el valor que documenta el propio comentario del juego), porque este port no
  tiene servidor de EA y el pack ya esta en la tarjeta. El cambio es **1 byte por
  1 byte** (`"-1"` sobre un espacio y el `0`, que la config tiene de sobra):
  el fichero sigue midiendo los 901 B que el motor mide con
  `openFd().getLength()`, no se inventa ningun tamano ni ningun hash, y el APK
  de la tarjeta **no se toca** (se parchea la copia que el puerto sirve, en el
  `InputStream` que el motor lee, y la copia extraida en `<game>/assets/`).
* Borra el marcador `.cmdownload` de 0 B **solo si el pack esta completo** (7 de
  7 ficheros): es el fichero temporal de una descripcion que nunca termino -- lo
  creo el motor en una corrida, y el propio pack del autor lo traia -- y el CM
  lo mira al preparar una reanudacion. Lo mismo que hace
  `TemporaryFilesUtils::RemoveIncompleteDownloads`. Nada mas bajo `Speech/` se
  toca.

**INFERIDO (no hay consola aqui)**: que con "NO SYNC" la interfaz deje de
ofrecer el pack. La linea que lo dirá es la del CM (`[speech] the ContentManager's
config ... : "updateCheckPeriodDays" ... is now -1 ...`) y la pantalla.

---

## 3. LO QUE SE HA TOCADO

```
source/fifa14_physkbd.c   la identificacion por ENTRADAS, el +4, el byte +144,
                          SetEnabled, los contadores del camino directo
source/fifa14_physkbd.h   FIFA14_INPUT_KEY_ROUTED (1) y los accesores nuevos
source/fifa14_input.c     los DOS caminos con contador propio + la linea
                          "[input] key paths: direct calls ... routed calls ..."
source/fifa14_assets.c    fifa14_cm_config_patch() + el parcheo de la copia
                          extraida; el .cmdownload
source/fifa14_assets.h    la declaracion de la anterior
source/fifa14_blast_stubs.c  el gancho en el open del AssetManager
source/fifa14_external.c  borrado (logueado) del marcador .cmdownload
```

No se ha tocado: el render/swap, el tacto, `rt_pad.c`, el locale del TEXTO, el
share, el `About` del usuario, ni un byte de `external/Speech/` salvo el
marcador temporal de 0 B.

## 4. LO QUE NO SE PUDO VERIFICAR (no hay consola ni emulador aqui)

* Que el juego responda al pad: lo dira la pantalla + las lineas del log.
* Que con `-1` la interfaz diga "instalado": lo dira la pantalla.
* Que `GetModule` devuelva el puntero a +4 **en la consola**: medido en el .so y
  consistente con los dos numeros del log (`0x1A9C`), pero la corrida nueva lo
  confirmara con la linea `the module object ... = YES` y
  `the manager hands back the +4 INTERFACE pointer`.

## 5. LOS ARTEFACTOS DE ESTA RONDA (medidos)

```
payload   fifa14_nx.nsp                3833941 B  sha256 c8e1df55e828ce8f13882ca3ab126881518757d99c4498d9839442bf3b204d7e
romfs     launcher/romfs/fifa14_nx.nsp 3833941 B  sha256 c8e1df55e828ce8f13882ca3ab126881518757d99c4498d9839442bf3b204d7e   (== payload)
lanzador  launcher/fifa14_nx.nro       4103101 B  sha256 9b556a85804a2e301dbf49a19a1302bbf0abdf29ebd6b20f04b979c39e23c282
desplegado D:/switch/fifa14/fifa14_nx.nro  == el lanzador (mismo sha256)
build     ./build-fixes.sh: payload rc=0, launcher rc=0, 0 lineas "error:" en los dos logs
          payload VERBATIM dentro del .nro (offset 269013)
```

Los logs de la build: `build-fixes-payload.log` / `build-fixes-launcher.log` y
`build-fixes-*-exit.txt`. El diff de la ronda: `fifa14-mando-voces.diff`.

