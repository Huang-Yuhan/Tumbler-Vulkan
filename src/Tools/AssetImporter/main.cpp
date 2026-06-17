#include "AssetImporter.h"

#include <cstring>
#include <iostream>
#include <string>

using namespace Tumbler;

static void PrintUsage() {
    std::cout << R"(TumblerImporter — Asset Import CLI

Usage:
  TumblerImporter mesh <input.obj> [--output <dir>]
      Import a single OBJ mesh → .tmesh

  TumblerImporter texture <input.png> [--output <dir>]
      Import a single texture → .ttex

  TumblerImporter --input <scene.json> [--output <dir>] [--incremental] [--dry-run]
      Import all assets referenced by a scene JSON, generate asset_map.json

Options:
  --output <dir>      Output directory for cooked assets (default: cooked/)
  --incremental        Skip assets whose sourceHash hasn't changed
  --dry-run            List what would be processed, don't write files
)";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        PrintUsage();
        return 1;
    }

    AssetImporter::Config config;
    config.OutputDir = "cooked/";

    std::string inputPath;
    enum class Mode { Mesh, Texture, Scene } mode = Mode::Scene;

    // 解析参数
    int i = 1;
    if (std::strcmp(argv[1], "mesh") == 0) {
        if (argc < 3) {
            std::cerr << "Error: 'mesh' subcommand requires an input file." << std::endl;
            return 1;
        }
        mode = Mode::Mesh;
        inputPath = argv[2];
        i = 3;
    } else if (std::strcmp(argv[1], "texture") == 0) {
        if (argc < 3) {
            std::cerr << "Error: 'texture' subcommand requires an input file." << std::endl;
            return 1;
        }
        mode = Mode::Texture;
        inputPath = argv[2];
        i = 3;
    } else if (std::strcmp(argv[1], "--input") == 0) {
        if (argc < 3) {
            std::cerr << "Error: --input requires a scene JSON path." << std::endl;
            return 1;
        }
        mode = Mode::Scene;
        inputPath = argv[2];
        i = 3;
    } else if (std::strcmp(argv[1], "--help") == 0 || std::strcmp(argv[1], "-h") == 0) {
        PrintUsage();
        return 0;
    } else {
        std::cerr << "Error: Unknown option '" << argv[1] << "'" << std::endl;
        PrintUsage();
        return 1;
    }

    // 解析剩余选项
    for (; i < argc; i++) {
        if (std::strcmp(argv[i], "--output") == 0 || std::strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) {
                config.OutputDir = argv[++i];
            }
        } else if (std::strcmp(argv[i], "--incremental") == 0) {
            config.bIncremental = true;
        } else if (std::strcmp(argv[i], "--dry-run") == 0) {
            config.bDryRun = true;
        } else {
            std::cerr << "Warning: Unknown option '" << argv[i] << "'" << std::endl;
        }
    }

    // 确保 output 目录以 / 结尾
    if (!config.OutputDir.empty() && config.OutputDir.back() != '/') {
        config.OutputDir += '/';
    }

    // 执行
    AssetImporter importer;
    importer.Init(config);

    bool result = false;
    switch (mode) {
        case Mode::Mesh:
            result = importer.ImportMesh(inputPath);
            break;
        case Mode::Texture:
            result = importer.ImportTexture(inputPath);
            break;
        case Mode::Scene:
            result = importer.ImportScene(inputPath);
            break;
    }

    importer.Shutdown();
    return result ? 0 : 1;
}
