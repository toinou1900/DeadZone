# DeadZone Engine

Squelette : SDL3 (fenetre/entrees) + bgfx (rendu). Cible : Windows et Linux.
Contenu actuel : rendu 3D, camera libre, chargement de modeles (Assimp : FBX, OBJ, STL, glTF/GLB, PLY, DAE, 3DS) avec
textures, ECS (EnTT) et physique (Jolt). Shaders dans `shaders/`, compiles au build.

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
Mode **joueur** (par defaut) :
- ZQSD (AZERTY) / WASD (QWERTY) : marcher (memes touches physiques), **Shift** : courir, **Espace** : sauter
- Souris : regarder, avec clic droit maintenu ou **Tab** pour verrouiller la souris (Tab de nouveau pour la liberer)
- **F** : passer en camera libre (on vole : Espace / Ctrl monter / descendre, Shift : vite). Le personnage devient
  visible (boite orange) ; en repassant en mode joueur, il vient se poser sous la camera.

Dans les deux modes :
- **B** : lancer une boite devant la camera
- **P** : mettre la physique en pause / la relancer
- **R** : reinitialiser la scene (sol, cube, joueur)
- Glisser un modele sur la fenetre : l'ajouter a la scene (il tombe sur le sol)
- Suppr / Retour arriere : retirer le dernier objet ajoute (modele ou boite)
- V : activer / couper la synchronisation verticale (vsync)
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

## Physique (Jolt)
- Corps rigides en **boite** : gravite, collisions, frottement, rebond. Les modeles 3D utilisent leur **boite englobante**
  comme forme de collision (pas leur forme exacte).
- Simulation a **pas fixe de 1/60 s** : le resultat ne depend pas du nombre d'images par seconde.
- Le sol est un corps fixe (dessus a y = -2). Gravite : 9,81 m/s2 vers -Y. Une unite = un metre.
- Le composant `RigidBody` decrit le corps ; le corps est cree dans Jolt au premier tour de boucle, puis le moteur
  physique pilote le `Transform` de l'entite. Detruire l'entite detruit son corps.
- Capacite : jusqu'a **10 240 corps** simultanes. Au-dela, B et le glisser-deposer sont refuses avec un message
  (Suppr ou R pour en retirer).
- **Joueur** : capsule de 1,8 m (rayon 0,4 m), vitesses 5 m/s (marche) / 9 m/s (course), saut ~1,2 m, controle reduit en
  l'air. Il pousse les objets et ne traverse ni sol ni murs. S'il tombe du monde (y < -40), il reapparait au depart.
- **Interpolation** : la physique avance par pas de 1/60 s, mais l'affichage est interpole entre les deux derniers pas.
  Sans cela, a 200 FPS le mouvement (et surtout la camera) avancerait par a-coups.
- Pas encore : sphere / forme exacte, escaliers, pentes raides, articulations, raycasts, evenements de collision.

## Structure du code (src/)
| Fichier | Role |
|---|---|
| `main.cpp` | Point d'entree : cree le moteur et le lance |
| `engine.*` | `Engine` : boucle de jeu, evenements, relie tous les modules |
| `components.h` | Composants ECS (donnees pures) : `Name`, `Transform`, `Spin`, `RigidBody`, `CharacterBody`, `Player`, `MeshRenderer`, `CubeRenderer`, `BoxRenderer` |
| `transform_math.h` | Maths : quaternions, matrice monde d'un `Transform` |
| `scene.*` | `Scene` : registre EnTT, creation d'entites et systemes (animation, physique, rendu) |
| `physics.*` | `Physics` : moteur physique Jolt (corps en boite, pas fixe), cache l'API de Jolt |
| `window.*` | `Window` : fenetre SDL3, handles natifs, capture de la souris |
| `renderer.*` | `Renderer` : bgfx, shaders integres, dessin du cube / des boites / des modeles (recoit une matrice monde), texte de debug |
| `camera.*` | `Camera` : camera libre (position, regard, matrices vue/projection) |
| `model.*` | `Model` : chargement de modeles 3D (Assimp), buffers GPU et textures |
| `texture.*` | Creation de textures GPU : decodage (stb_image) et mipmaps |
| `../shaders/` | Shaders (.sc), compiles par shaderc pendant le build |

Ordre d'arret important : vider la scene (`Scene::clear`) avant `Physics::shutdown` et `Renderer::shutdown` (les corps
physiques et les buffers GPU des modeles sont liberes a ce moment).

## ECS (EnTT)
- **Entite** : un simple identifiant. **Composant** : des donnees (`components.h`). **Systeme** : une fonction qui
  parcourt les entites ayant certains composants (`scene.cpp`).
- Systemes actuels : `createBodiesSystem`, `playerSystem`, `syncTransformsSystem` (physique), `spinSystem` (animation des entites
  sans corps physique) et `renderSystem` (Transform + MeshRenderer / CubeRenderer / BoxRenderer).
- `Transform` stocke l'orientation en **quaternion** (x, y, z, w), comme Jolt.
- Pour ajouter un comportement : creer un composant, l'attacher a des entites, ecrire un systeme et l'appeler dans
  `Scene::update` ou `Scene::render`.
