// DeadZone Engine - le moteur : relie fenetre, rendu, camera et modeles, et fait tourner la boucle de jeu
#pragma once

#include "camera.h"
#include "model.h"
#include "renderer.h"
#include "window.h"

#include <cstdint>
#include <string>

union SDL_Event;

class Engine {
public:
    Engine() = default;
    Engine(const Engine&)            = delete;
    Engine& operator=(const Engine&) = delete;

    // `argv[1]` (optionnel) : un modele 3D a charger au demarrage.
    bool init(int argc, char** argv);
    void run();
    void shutdown(); // sans danger meme si init() a echoue

private:
    void handleEvent(const SDL_Event& e);
    void update(float dt);
    void render(float dt);
    void loadModel(const std::string& path);

    Window   m_window;
    Renderer m_renderer;
    Camera   m_camera;
    Model    m_model;

    std::string m_modelName;
    std::string m_modelError;

    bool     m_running   = false;
    bool     m_mouseLook = false;
    bool     m_spin      = true;
    float    m_time      = 0.0f;
    uint64_t m_lastTicks = 0;
};
