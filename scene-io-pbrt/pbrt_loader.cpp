#include "pbrt_loader.h"

namespace scene_io_pbrt
{
    scene_core::Scene LoadSceneFromPbrt(const std::string& filePath)
    {
        scene_core::Scene scene;
        scene.name = filePath;
        scene.sourcePath = filePath;
        return scene;
    }
}
