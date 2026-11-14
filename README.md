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
- V : activer / couper la synchronisation verticale (vsync)
- Glisser un modele sur la fenetre : l'ajouter a la scene (autant que voulu)
- Suppr / Retour arriere : retirer le dernier modele ajoute
- Echap : quitter

## Notes
- Windows : le rendu utilise Direct3D 11 pour l'instant (D3D12 demande des shaders DXIL, a ajouter plus tard).
- Les shaders sont compiles par `shaderc` (construit avec bgfx) et integres au binaire.

## Charger un modele 3D
- Glisser-deposer un fichier sur la fenetre, ou : `DeadZone.exe chemin\vers\modele.fbx`
- Modeles de test fournis : `assets/torus.obj` (gris uni) et `assets/test_cube.obj` (cube texture)
- Le modele est recentre et mis a l'echelle automatiquement.
- STL / certains exports sont "Z vers le haut" : le modele peut apparaitre couche (pas encore corrige).

## Textures
- Textures de couleur (diffuse / base color) : PNG, JPG, TGA, BMP, GIF, PSD, HDR (via stb_image). DDS / KTX : non geres.
- Textures **integrees** au modele (FBX, GLB...) : lues directement dans le fichier.
- Textures **externes** : cherchees a cote du modele (chemin relatif), puis par nom de fichier dans le dossier du modele
  (les FBX contiennent souvent des chemins absolus propres a la machine de l'auteur).
- Mipmaps generees automatiquement. Un probleme (texture introuvable ou illisible) s'affiche en jaune a l'ecran.
- `assets/test_cube.png` sert a verifier l'orientation : sur la face avant, le rouge doit etre en haut a gauche,
  le vert en haut a droite, le bleu en bas a gauche, le jaune en bas a droite, et le "F" doit se lire normalement.
- Pas encore gere : normal maps, metal / rugosite, transparence, couleur de base glTF (le facteur est ignore
  quand une texture est presente).

## Structure du code (src/)
| Fichier | Role |
|---|---|
| `main.cpp` | Point d'entree : cree le moteur et le lance |
| `engine.*` | `Engine` : boucle de jeu, evenements, relie tous les modules |
| `components.h` | Composants ECS (donnees pures) : `Name`, `Transform`, `Spin`, `MeshRenderer`, `CubeRenderer` |
| `scene.*` | `Scene` : registre EnTT, creation d'entites et systemes (rotation, rendu) |
| `window.*` | `Window` : fenetre SDL3, handles natifs, capture de la souris |
| `renderer.*` | `Renderer` : bgfx, shaders integres, dessin (recoit une matrice monde), texte de debug |
| `camera.*` | `Camera` : camera libre (position, regard, matrices vue/projection) |
| `model.*` | `Model` : chargement de modeles 3D (Assimp), buffers GPU et textures |
| `texture.*` | Creation de textures GPU : decodage (stb_image) et mipmaps |
| `../shaders/` | Shaders (.sc), compiles par shaderc pendant le build |

Ordre d'arret important : vider la scene (`Scene::clear`) avant `Renderer::shutdown`, car les modeles liberent leurs buffers GPU.

## ECS (EnTT)
- **Entite** : un simple identifiant. **Composant** : des donnees (`components.h`). **Systeme** : une fonction qui
  parcourt les entites ayant certains composants (`scene.cpp`).
- Systemes actuels : `spinSystem` (Transform + Spin) et `renderSystem` (Transform + MeshRenderer / CubeRenderer).
- Pour ajouter un comportement : creer un composant, l'attacher a des entites, ecrire un systeme et l'appeler dans
  `Scene::update` ou `Scene::render`.
