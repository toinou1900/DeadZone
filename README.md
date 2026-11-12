# DeadZone Engine

Squelette : SDL3 (fenetre/entrees) + bgfx (rendu). Cible : Windows et Linux.
Contenu actuel : un cube 3D, une camera libre et le chargement de modeles 3D via Assimp
(FBX, OBJ, STL, glTF/GLB, PLY, DAE, 3DS). Shaders dans `shaders/`, compiles au build.

## Prerequis
- CMake >= 3.24, Git
- **Windows** : Visual Studio 2022 (charge de travail "Developpement Desktop C++")
- **Linux** (Debian/Ubuntu) :
  `sudo apt install build-essential cmake ninja-build git pkg-config libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxfixes-dev libxss-dev libgl1-mesa-dev libwayland-dev libxkbcommon-dev libasound2-dev libpulse-dev libudev-dev`
  (Arch/CachyOS : `sudo pacman -S base-devel cmake ninja git libx11 libxext libxrandr libxcursor libxi libxfixes libxss mesa wayland libxkbcommon alsa-lib libpulse`)

## Compiler
Windows :
    cmake --preset windows
    cmake --build --preset windows-release
    build\windows\Release\DeadZone.exe

Linux :
    cmake --preset linux
    cmake --build --preset linux
    ./build/linux/DeadZone

La premiere configuration telecharge SDL3 et bgfx (plusieurs minutes, c'est normal).

## Controles
- ZQSD (AZERTY) / WASD (QWERTY) : deplacement (memes touches physiques)
- Espace / Ctrl gauche : monter / descendre, Shift : plus vite
- Clic droit maintenu + souris : regarder autour
- R : arreter / relancer la rotation
- Echap : quitter

## Notes
- Windows : le rendu utilise Direct3D 11 pour l'instant (D3D12 demande des shaders DXIL, a ajouter plus tard).
- Les shaders sont compiles par `shaderc` (construit avec bgfx) et integres au binaire.

## Charger un modele 3D
- Glisser-deposer un fichier sur la fenetre, ou : `DeadZone.exe chemin\vers\modele.fbx`
- Un modele de test est fourni : `assets/torus.obj`
- Le modele est recentre et mis a l'echelle automatiquement. Les couleurs viennent des matieres
  du fichier ; les textures ne sont pas encore gerees.
- STL / certains exports sont "Z vers le haut" : le modele peut apparaitre couche (pas encore corrige).
