#include "Core/EditorApplication.h"
#include "Core/Utils/Log.h"

int main(int argc, char** argv) {
    Tumbler::Log::Init();

    Tumbler::EditorApplication app;
    if (!app.Init(argc, argv)) {
        Tumbler::Log::Shutdown();
        return 1;
    }

    app.Run();
    app.Shutdown();
    Tumbler::Log::Shutdown();
    return 0;
}
