<img align="right" src="https://visitor-badge.laobi.icu/badge?page_id=lozp1.f14" />

<h1 align="center">
    <img src="https://readme-typing-svg.herokuapp.com/?font=Righteous&size=40&center=true&vCenter=true&width=720&height=80&duration=4000&color=C8161E&lines=f14;FIFA+14+en+tu+Nintendo+Switch;Proyecto+NO+oficial" />
</h1>

<h3 align="center">⚽ Juega a tu <b>FIFA 14</b> de Android en Nintendo Switch, con icono en el HOME y el pad en la mano 🎮</h3>

<div align="center">

![Platform](https://img.shields.io/badge/Platform-Nintendo%20Switch-E60012?style=for-the-badge&logo=nintendoswitch&logoColor=white)
![Version](https://img.shields.io/badge/Versi%C3%B3n-v1.0-C8161E?style=for-the-badge)
![Runtime](https://img.shields.io/badge/Runtime-android32-3B82F6?style=for-the-badge)
![Mando](https://img.shields.io/badge/Mando-S%C3%AD-10B981?style=for-the-badge)
![License](https://img.shields.io/badge/Licencia-MIT-10E761?style=for-the-badge)

</div>

<hr/>

## Qué es f14

**Icono propio en el HOME y el juego arranca directo. El runtime no se ve nunca.**
Usas **tu propia copia** de FIFA 14 de Android; f14 pone lo demás (lanzador, instalador y runtime).

> ⚠️ Proyecto **personal y no oficial**, sin relación con EA. **Aquí no se distribuye el juego**:
> solo el envoltorio. «FIFA» se usa de forma descriptiva para indicar con qué juego es compatible.

Es la **versión móvil**: un jugador, sin online (sus servidores están cerrados).

<hr/>

## 📥 Descargar

**[⬇️ Releases](../../releases/latest)** — un solo fichero:

- **`f14-v1.0-sd.zip`** → descomprime su carpeta `switch/`

Dentro van el lanzador, el forwarder con icono y el NSP directo.

## 🔧 Instalar (4 pasos)

**1.** Descomprime **`switch/`** en la raíz de tu SD. Queda `switch/fifa14/`.

**2.** Copia tu juego ahí dentro: el **APK** en `switch/fifa14/` y el **OBB** en `switch/fifa14/obb/`.

**3.** Instala **`instalador/fifa14-direct.nsp`** con Sphaira o DBI (NSP directo, no "Install Forwarder").

**4.** Abre **f14** desde el icono del HOME. El primer arranque descomprime las librerías y tarda.

```
sdmc:/
└── switch/
    └── fifa14/
        ├── FIFA14.apk
        ├── obb/main.136.com.ea.game.fifa14_row.obb
        ├── external/          <- tuyo: Acerca de, comentarios, idioma
        └── instalador/fifa14-direct.nsp
```

> ⚠️ Necesitas **FIFA 14 Android 1.3.6** (`com.ea.game.fifa14_row`, versionCode 136, armv7).
> Si es otra versión, el port no la reconoce.

<hr/>

## ⚙️ Ajustes

`switch/fifa14/config.ini` se crea solo. Lo que se toca:

| Clave | Para qué |
|---|---|
| `[audio] language` | `keep` o `ENG_US` (el hueco del pack de comentarios) |
| `[audio] force_speech` | `1` arranca el sistema de habla del motor |
| `[controls] pad_touch_buttons` | los botones del pad pulsan los táctiles del motor |
| `[controls] pad_keys_scope` | `match` (teclas solo en el partido) o `always` |

<hr/>

## 🧩 Tu "Acerca de"

`switch/fifa14/external/data/cmn/fe/about_ES.txt` es tuyo: el port nunca lo sobrescribe.

## Créditos

Runtime [android32](https://github.com/aks796/android32) de aks796 sobre libnx y Mesa.
Proyecto no oficial, sin relación con Electronic Arts. El juego no se incluye.

<!-- compilar: runtime/ = android32 · libnx32/ + portlibs32/ (Mesa) · payload con
     ghcr.io/vita2hos/devcontainer/vita2hos + ./build.sh · lanzador con devkitA64 ·
     NSP con python installer/build-installer-nsps.py all -->
