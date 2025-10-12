#ifndef _SCENE_H_
#define _SCENE_H_

#include <vector>
#include "Eigen/Dense"

//const uint32_t VERTEX_ATTRIBUTE_MASK_POSITION = 0x0001;
//const uint32_t VERTEX_ATTRIBUTE_MASK_NORMAL = 0x0002;
//const uint32_t VERTEX_ATTRIBUTE_MASK_TEXCOORD0_U = 0x001;
//const uint32_t VERTEX_ATTRIBUTE_MASK_TEXCOORD0_UV = 0x001;
//const uint32_t VERTEX_ATTRIBUTE_MASK_TEXCOORD0_UVW = 0x001;
//const uint32_t VERTEX_ATTRIBUTE_MASK_TEXCOORD0_UVW = 0x001;
//
//const uint32_t VERTEX_ATTRIBUTE_MASK_TEXCOORD1 = 0x0008;
//const uint32_t VERTEX_ATTRIBUTE_MASK_TEXCOORD2 = 0x0010;
//const uint32_t VERTEX_ATTRIBUTE_MASK_TEXCOORD3 = 0x0020;

struct VertexP3N3
{
	Eigen::Vector3f Position;
	Eigen::Vector3f Normal;
};

struct VertexP3C3
{
	Eigen::Vector3f Position;
	Eigen::Vector3f Color;
};

struct VertexP3N3T2
{
	Eigen::Vector3f Position;
	Eigen::Vector3f Normal;
	Eigen::Vector2f UV0;
};

using PolygonType_Triangle = std::array<uint32_t, 3>;

// Just a container to put vertex and index together.
// Abstract the relation between vertices and indices.
template<class VertexType, uint32_t PolygonVertexNumber>
class Mesh
{
private:
	std::vector<VertexType> mVertices;
	std::vector<uint32_t> mIndices;
public:
	static constexpr uint32_t PolygonVertexNumber = PolygonVertexNumber;
	
	Mesh() {}
	Mesh(const Mesh&) = delete;
	Mesh& operator=(const Mesh&) = delete;
	
	std::vector<VertexType>& Vertices = mVertices;
	std::vector<uint32_t> Indices = mIndices;
};


#endif
