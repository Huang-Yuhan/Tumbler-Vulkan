#include "Core/Engine/Engine.h"
#include "Core/Utils/Log.h"

using namespace Tumbler;

int main() {
    Engine engine;

    if (!engine.Init("engine.json")) {
        LOG_CRITICAL("Engine initialization failed");
        return 1;
    }

    if (!engine.LoadScene("assets/scenes/demo.tscene")) {
        LOG_CRITICAL("Scene loading failed");
        engine.Shutdown();
        return 1;
    }

    LOG_INFO("Entering main loop...");
    engine.Run();
    engine.Shutdown();
    return 0;
}
