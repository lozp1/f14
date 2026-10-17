# FIFA 14 — Nintendo Switch

Port no oficial de **FIFA 14 (versión Android)** para Nintendo Switch con CFW.

Es un **wrapper**: ejecuta las librerías del propio juego (armeabi-v7a) sobre un
runtime Android de 32 bits para libnx. No es un emulador y **no incluye el juego**:
necesitas tu propia copia (APK 1.3.6 + OBB).

Autor: **Franco López (FZ) — f.paolo** · [github.com/lozp1](https://github.com/lozp1)

## Qué tienes

- **Icono y nombre en el HOME** (forwarder NSP): el runtime no se ve, arranca el juego.
- **Mando**: los botones del pad pulsan los botones táctiles del propio motor
  (es el mismo camino que el dedo en pantalla, no una emulación de teclado).
- **Comentarios**: el hueco `ENG_US` del habla es tuyo (`switch/fifa14/external/Speech/`).
- **Tu texto en "Acerca de"**: `switch/fifa14/external/data/cmn/fe/about_ES.txt`.
- Ajustes en `switch/fifa14/config.ini` (se crea solo en el primer arranque).

## Requisitos

- Switch con **Atmosphère** (CFW) y sigpatches.
- **FIFA 14 Android `com.ea.game.fifa14_row` 1.3.6 (versionCode 136)**, `armeabi-v7a`:
  - `FIFA14.apk`
  - `main.136.com.ea.game.fifa14_row.obb`

## Instalación

1. Descarga **`f14-v1.0-sd.zip`** de [Releases](../../releases) y descomprime `switch/`
   en la raíz de la microSD. Queda `switch/fifa14/`.
2. Copia tu juego dentro de esa carpeta:
   - `FIFA14.apk` → `switch/fifa14/`
   - `main.136.com.ea.game.fifa14_row.obb` → `switch/fifa14/obb/`
3. Instala **`instalador/fifa14-direct.nsp`** con **Sphaira** o **DBI**
   (NSP directo; no uses "Install Forwarder").
4. Abre el juego desde el **HOME**. El primer arranque descomprime las librerías
   del APK y tarda un poco.

### Estructura en la microSD

```text
switch/fifa14/
├── FIFA14.apk
├── config.ini                              # se crea solo
├── obb/
│   └── main.136.com.ea.game.fifa14_row.obb # tu OBB
├── external/                               # tuyo, el port nunca lo sobrescribe
│   ├── data/cmn/fe/about_ES.txt            # "Acerca de"
│   ├── Speech/                             # pack de comentarios (si tienes)
│   └── ini/na/locale.ini                   # idioma del audio
└── instalador/
    ├── fifa14-direct.nsp
    └── LEEME.txt
```

## Ajustes (`config.ini`)

| Clave | Para qué |
|---|---|
| `[audio] language = keep \| ENG_US` | Idioma del audio. `ENG_US` sirve el hueco del pack de comentarios. |
| `[audio] force_speech = 0 \| 1` | Arranca el sistema de habla del motor una vez. |
| `[controls] pad_keys_scope = match \| always` | Las teclas del pad solo en el partido, o siempre. |
| `[controls] pad_touch_buttons = on \| off` | Los botones del pad pulsan los botones táctiles del motor. |
| `[controls] pad_touch_map = "a:0,b:1,x:2,y:3"` | Qué botón del pad pulsa qué rectángulo. |

## Limitaciones

- **Un jugador**: la versión móvil no tiene multijugador local ni pantalla partida.
- **Sin online**: los servidores de FIFA 14 móvil están cerrados. El port es offline.
- **Comentarios**: solo suenan si pones un pack con el formato y los nombres del
  motor (`Speech/hdr_ENG_US.big`, `sth_ENG_US.big`, `dat_ENG_US.bh`, `pbp_ENG_US.evt`).

## Créditos

Runtime [android32](https://github.com/aks796/android32) (aks796) sobre **libnx** y
**Mesa**. FEXTendo / Autorun para el lanzamiento en el HOME. Proyecto no oficial,
sin relación con Electronic Arts. El juego no se incluye ni se distribuye.

## Aviso

Este repositorio contiene solo el código del wrapper y las herramientas. No
incluye APK, OBB, librerías del juego ni datos con copyright. Usa una copia
legalmente obtenida.
