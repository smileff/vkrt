#pragma once

#include <string>

#include "../scene-core/scene.h"

namespace scene_io_pbrt
{
    scene_core::Scene LoadSceneFromPbrt(const std::string& filePath);
}
