// DeadZone Engine - le moteur : relie fenetre, rendu, camera et scene (ECS), et fait tourner la boucle de jeu
#pragma once

#include "camera.h"
#include "renderer.h"
#include "scene.h"
#include "window.h"

#include <cstdint>
#include <string>

union SDL_Event;

class Engine {
public:
    Engine() = default;
    Engine(const Engine&)            = delete;
    Engine& operator=(const Engine&) = delete;

    // `argv[1]` (optionnel) : un modele 3D a ajouter a la scene au demarrage.
    bool init(int argc, char** argv);
    void run();
    void shutdown(); // sans danger meme si init() a echoue

private:
    void handleEvent(const SDL_Event& e);
    void update(float dt);
    void render(float dt);
    void spawnModel(const std::string& path); // charge un fichier et cree une entite dans la scene

    Window   m_window;
    Renderer m_renderer;
    Camera   m_camera;
    Scene    m_scene;

    std::string m_error;

    bool     m_running   = false;
    bool     m_mouseLook = false;
    bool     m_spin      = true;

    // Compteur de FPS lisse (moyenne sur ~0,5 s) : l'image par image est trop instable pour etre lisible.
    float    m_fpsTime   = 0.0f;
    int      m_fpsFrames = 0;
    float    m_fps       = 0.0f;
    float    m_frameMs   = 0.0f;
    uint64_t m_lastTicks = 0;
};
