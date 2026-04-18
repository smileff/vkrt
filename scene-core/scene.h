#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace scene_core
{
    inline constexpr std::uint32_t InvalidIndex = std::numeric_limits<std::uint32_t>::max();

    struct Transform
    {
        std::array<float, 3> translation = { 0.0f, 0.0f, 0.0f };
        std::array<float, 4> rotation = { 0.0f, 0.0f, 0.0f, 1.0f };
        std::array<float, 3> scale = { 1.0f, 1.0f, 1.0f };
    };

    struct VertexStreams
    {
        std::vector<float> positions;
        std::vector<float> normals;
        std::vector<float> tangents;
        std::vector<float> texcoords0;
        std::vector<float> texcoords1;
    };

    struct Submesh
    {
        std::uint32_t materialIndex = InvalidIndex;
        std::uint32_t indexOffset = 0;
        std::uint32_t indexCount = 0;
    };

    struct Mesh
    {
        std::string name;
        VertexStreams vertexStreams;
        std::vector<std::uint32_t> indices;
        std::vector<Submesh> submeshes;
    };

    struct Texture
    {
        std::string name;
        std::string uri;
    };

    struct MaterialPBR
    {
        std::string name;
        std::array<float, 4> baseColorFactor = { 1.0f, 1.0f, 1.0f, 1.0f };
        std::array<float, 3> emissiveFactor = { 0.0f, 0.0f, 0.0f };
        float metallicFactor = 1.0f;
        float roughnessFactor = 1.0f;
        std::uint32_t baseColorTextureIndex = InvalidIndex;
        std::uint32_t normalTextureIndex = InvalidIndex;
        std::uint32_t emissiveTextureIndex = InvalidIndex;
        bool alphaMasked = false;
        float alphaCutoff = 0.5f;
    };

    struct Camera
    {
        std::string name;
        float verticalFovRadians = 0.78539816339f;
        float nearPlane = 0.1f;
        float farPlane = 1000.0f;
    };

    enum class LightType
    {
        Directional,
        Point,
        Spot,
    };

    struct Light
    {
        std::string name;
        LightType type = LightType::Directional;
        std::array<float, 3> color = { 1.0f, 1.0f, 1.0f };
        float intensity = 1.0f;
        float range = 0.0f;
        float innerConeAngleRadians = 0.0f;
        float outerConeAngleRadians = 0.0f;
    };

    struct MeshRef
    {
        std::uint32_t meshIndex = InvalidIndex;
        std::uint32_t submeshIndex = InvalidIndex;
    };

    struct Node
    {
        std::string name;
        Transform localTransform;
        std::vector<std::uint32_t> children;
        MeshRef mesh;
        std::uint32_t cameraIndex = InvalidIndex;
        std::uint32_t lightIndex = InvalidIndex;
    };

    struct Scene
    {
        std::string name;
        std::string sourcePath;
        std::vector<Node> nodes;
        std::vector<Mesh> meshes;
        std::vector<MaterialPBR> materials;
        std::vector<Texture> textures;
        std::vector<Camera> cameras;
        std::vector<Light> lights;
        std::uint32_t rootNode = InvalidIndex;
    };
}
