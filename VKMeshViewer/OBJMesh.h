#ifndef _OBJ_MESH_H_
#define _OBJ_MESH_H_

#include "Eigen/Dense"
#include "VKLib/vklib.h"

const uint32_t VERTEX_ATTRIBUTE_POSITION	= 0x0001;
const uint32_t VERTEX_ATTRIBUTE_NORMAL		= 0x0002;
const uint32_t VERTEX_ATTRIBUTE_TEXCOORD0	= 0x0004;
const uint32_t VERTEX_ATTRIBUTE_TEXCOORD1	= 0x0008;
const uint32_t VERTEX_ATTRIBUTE_TEXCOORD2	= 0x0010;
const uint32_t VERTEX_ATTRIBUTE_TEXCOORD3	= 0x0020;



// A container to maintain the vertex buffer and index buffer needed to render a mesh.
class VKRenderMesh
{
public:
	// VKVertexInput(const VKDeviceContext& context, uint32_t vertNum, uint32_t polyVertNum, uint32_t idxNum);
	VKRenderMesh(const VKDeviceContext& context);
	~VKRenderMesh();

	VKRenderMesh(const VKRenderMesh&) = delete;
	VKRenderMesh& operator=(const VKRenderMesh&) = delete;
	
	size_t AppendVertexBuffer(uint32_t vertNum, uint32_t vertStride);
	void HostUpdateVertexBuffer(uint32_t bindingIdx, VkDeviceSize offset, VkDeviceSize size, const void* data);	
	
	// Index part.
	void AppendIndexBuffer(uint32_t idxNum);
	void HostUpdateIndexBuffer(size_t idxOffset, size_t idxCnt, const uint32_t* idxs);
	//VkBuffer GetIndexBuffer() const;

	void TransferToDevice(VkCommandBuffer cmdBuf);
	void ClearStagingBuffers();

	// 
	//uint32_t GetVertexNum() const;

	//uint32_t GetVertexINputBindingNum() const;
	const VkVertexInputBindingDescription* GetVertexInputBindingDescriptions() const;

	uint32_t GetVertexBufferNum() const;
	//const VkBuffer* GetVertexBuffers() const;
	VkBuffer GetVertexBuffer(uint32_t bufIdx) const;
	
	uint32_t GetIndexNum() const;
	VkBuffer GetIndexBuffer() const;

	//uint32_t GetPolyVertNum() const;

private:
	const VKDeviceContext& m_Context;	

	struct VertexData
	{
		uint32_t VertNum = 0;
		VkDeviceSize Stride = 0;
		std::unique_ptr<VMAVertexBufferAlloc> DeviceBufferAlloc;
		std::unique_ptr<VMAStagingBufferAlloc> StagingBufferAlloc;
	};
	std::vector<VertexData> m_VertexData;

	uint32_t m_IdxNum = 0;
	std::unique_ptr<VMAStagingBufferAlloc> m_IndexStagingBufferAlloc;
	std::unique_ptr<VMAIndexBufferAlloc> m_IndexDeviceBufferAlloc;
	
	//std::vector<VkVertexInputBindingDescription> m_VertexInputBindingDescs;

};



#endif