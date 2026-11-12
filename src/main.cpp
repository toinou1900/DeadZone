// DeadZone Engine - point d'entree
#include "engine.h"

int main(int argc, char** argv) {
    Engine engine;
    const bool ok = engine.init(argc, argv);
    if (ok) engine.run();
    engine.shutdown();
    return ok ? 0 : 1;
}
