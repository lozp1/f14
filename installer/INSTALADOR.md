# Instalador NSP (DBI) — FIFA 14

Tres `.nsp` empaquetados con **hacBrewPack** (ver `installer/build-installer-nsps.py`).
Los tres llevan el **mismo title id** `055d613a02a6f000` (ver *Title id*).

## Ficheros

| fichero | recipe | exefs (main + main.npdm) | romfs |
|---|---|---|---|
| `installer/fifa14_nx-installer.nsp` | *self* (receta literal) | el **lanzador de 64 bits** (NSO del `.elf` del launcher) | el romfs del lanzador: `fifa14_nx.nsp` (el programa de 32 bits) + `fifa14_nx.build` |
| `installer/fifa14_nx-forwarder.nsp` | *forwarder* | un **loader estilo HBL** de 64 bits | `/nextNroPath` + `/nextArgv` -> `sdmc:/switch/fifa14/fifa14_nx.nro` |
| `installer/fifa14-direct.nsp` | *direct* | **el propio programa de 32 bits** (`main` + `main.npdm` del override validado) | `/nextNroPath` + `/nextArgv` -> `sdmc:/switch/fifa14/fifa14_nx.nro` |

Control NCA (los tres): `control.nacp` (nombre **FIFA 14**, title id `055d613a02a6f000`) +
`icon_AmericanEnglish.dat` (el `launcher/icon.jpg`, 256x256).

## Title id `055d613a02a6f000`, y por qué ése

El lanzador sólo instala el programa de 32 bits si el título **en el que corre**
tiene un title id con el byte alto `0x05` (`exefs_is_forwarder_tid(tid)`:
`(tid >> 56) == 0x05`). Por eso el id **tiene** que ser `0x05xx…`; un id
`0x0100…` normal no valdría. Se deriva como lo hace sphaira:

```
tid = 0x0500_0000_0000_0000 | (sha256(nro_path + nro_path)[0:8] & 0x00FF_FFFF_FFFF_F000)
    = 055d613a02a6f000     (nro_path = "sdmc:/switch/fifa14/fifa14_nx.nro")
```

es decir: determinista, dentro del rango de *forwarders* (`0x05`), y sin colisión
con los demás `0x05…` instalados en esta consola.

## Cómo se instala con DBI

1. Copia el `.nsp` a la SD (cualquier carpeta; p.ej. `sdmc:/installer/`).
2. Abre **DBI** en la consola -> **Install from SD card** -> elige el `.nsp`.
   (DBI también lo instala por MTP/USB si lo prefieres.)
3. DBI crea el **icono "FIFA 14" en el menÚ HOME**. Ya no hace falta el
   "Install Forwarder" de sphaira.

## Qué pasa en el primer arranque

Se lanza **desde el icono del HOME** (nunca desde el Homebrew Menu: el programa
de 32 bits no se puede cargar ahí). El lanzador:

1. mira el title id del título en el que corre,
2. escribe `sdmc:/atmosphere/contents/055d613a02a6f000/exefs.nsp` con el programa de 32
   bits (retargeteando su NPDM a ese title id),
3. reinicia el título -> **arranca el juego**. A partir de ahí ese icono lanza
   directamente el juego.

## AVISO: cuál de los dos funciona

**`-installer.nsp` (receta literal) NO instala el juego.** El lanzador sólo
instala cuando el título en el que corre tiene `/nextNroPath` en su romfs
igual a `argv[0]` — la puerta `fwd_is_mine()` (`runtime/launcher/source/main.c`
línea ~225, `fwd_mine.h`). Eso sólo lo cumple un *forwarder*: un loader HBL lee
`/nextNroPath` de la romfs del título y **pasa esa ruta como `argv[0]`** al
lanzar el `.nro` (sphaira/hbl/source/main.c, `EntryType_Argv`). `-installer.nsp`
lleva el lanzador *como programa* y su romfs no tiene `/nextNroPath`, así que
`fwd_is_mine()` devuelve 0 y el lanzador sólo muestra el texto
"Start this from its own HOME-menu icon": **no escribe el override**.

**`-forwarder.nsp` SÍ funciona**: es exactamente lo que sphaira hace al pulsar
"Install Forwarder", pero preconstruido para que DBI lo instale. Requiere el
`.nro` del lanzador en `sdmc:/switch/fifa14/fifa14_nx.nro` (ya está ahí).

## Receta *direct*: el juego ES el título

`fifa14-direct.nsp` no lleva lanzador: su Program NCA ExeFS es **el propio
programa de 32 bits del juego** (`main` + `main.npdm`), copiado **literalmente**
del override que el lanzador ya escribía en
`sdmc:/atmosphere/contents/055d613a02a6f000/exefs.nsp` (ese `main.npdm` ya lleva
el program id `055d613a02a6f000`). Instalar ese NSP y pulsar el icono =
**arranca el juego**: no hay lanzador, ni ExeFS override, ni nada que instalar en
el primer arranque. El romfs conserva `/nextNroPath` + `/nextArgv`
(`sdmc:/switch/fifa14/fifa14_nx.nro`) para que el juego siga localizando su
`.nro` para auto-actualizarse.

Se empaqueta con `--nosignncasig2` **obligatoriamente**: sin ese flag
hacBrewPack re-firma `main.npdm` (*"Patching ACID public key"*) y reescribe 170
bytes del NPDM (offsets ACI0/ACID del META, más la public key y la firma del
ACID), con lo que el ExeFS dejaría de ser idéntico al override.

`build-installer-nsps.py direct` lo verifica y lo imprime al construir:

```
[2] ExeFS identico override: True  (f268744153b7d1d3f42e3dc9010a279f.nca
    3771570 B sha256=89f49b23a96425edf3ed546bedf5b3b8211a00a9f63cbbe65c2dbc35602af14f
    vs  exefs.nsp 3771570 B sha256=89f49b23a96425edf3ed546bedf5b3b8211a00a9f63cbbe65c2dbc35602af14f)
      main         3770502 B sha256=0761916d3e03a0293220ac978659dcfe84473a9eecdba8a12035b013cfeeefbe igual=True
      main.npdm        972 B sha256=1f81aa570e61d59308fb0f7d9ed366a12f410faf42d238299f95152e00b1ca38 igual=True
```

`nstool -y` sigue avisando `NCA Header Main Signature: FAIL` / `ACID Signature:
FAIL`: igual que en los forwarder y en cualquier NSP homebrew.

## Estructura verificada (nstool)

Cada `.nsp` es un **PFS0 con 3 NCA** (Program + Control + Meta). Verificado con
`nstool` (jakcron) y parseo propio:

- los nombres de los NCA == `sha256(nca)[0:32]` (content id);
- `ProgID` de las 3 NCA == `055d613a02a6f000`, y el `.cnmt` referencia los content id de
  Program y Control;
- Program NCA ExeFS = `main` + `main.npdm` con `aci0.program_id` y el rango ACID
  == `055d613a02a6f000`, `is_64_bit=1`, `address_space_type=1`;
- `installer`: `main` == NSO del lanzador; romfs = `fifa14_nx.nsp`
  (sha256 `57fe02ebdaf9f8d9d687bc3594285e19f59970d44f564b24d1c6f45cc1297139`, idéntico al del lanzador) + `fifa14_nx.build`;
- `forwarder`: `main` == el loader HBL (22292 B); romfs `nextNroPath` =
  `sdmc:/switch/fifa14/fifa14_nx.nro`;
- Control NCA tiene `control.nacp` (nombre `FIFA 14`, title id en 0x3038)
  e `icon_AmericanEnglish.dat` idéntico a `launcher/icon.jpg`.

`nstool -y` avisa `NCA Header Main Signature: FAIL` / `ACID Signature: FAIL`:
**es normal** en NSP homebrew (hacBrewPack firma con la clave "acid" de test, no
con la de Nintendo); Atmosphere/DBI no comprueban esa firma.

## Reconstruir

```sh
python installer/build-installer-nsps.py direct    # -> installer/fifa14-direct.nsp (+ verificacion)
python installer/build-installer-nsps.py all       # self + forwarder + direct
python installer/build-installer-nsps.py           # self + forwarder (legacy)
```

Usa `C:/devkitPro` (elf2nso, npdmtool, nacptool), `hacbrewpack` de NTON, la
keyset `C:/Users/f.paolo/.switch/prod.keys` (sólo `header_key`; nunca se
imprime) y `hbl.nso` (loader compilado de `sphaira/hbl`).
