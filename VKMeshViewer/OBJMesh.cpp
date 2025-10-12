#include "OBJMesh.h"

// VKVertexInput 

static uint32_t CalcPolyNum(uint32_t vertNum, uint32_t polyVertNum, uint32_t idxNum)
{
	if (idxNum == 0) {
		// Don't use index buffer.
		return vertNum / polyVertNum;
	} else {
		// Use index buffer.
		return idxNum / polyVertNum;
	}
}

//VKVertexInput::VKVertexInput(const VKDeviceContext& context, uint32_t vertNum, uint32_t polyVertNum, uint32_t idxNum)
//	: m_Context(context)
//	, m_Allocator(context.GetVmaAllocator())
//	, m_VertNum(vertNum)
//	, m_IdxNum(idxNum)
//	, m_PolyVertNum(polyVertNum)
//	, m_PolyNum(CalcPolyNum(vertNum, polyVertNum, idxNum))
//{
//	assert(vertNum % polyVertNum == 0);
//
//	//if (m_IdxNum > 0) {
//	//	m_IndexBufferAlloc = std::make_unique<VKBufferAlloc>(m_Allocator, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, m_IdxNum * sizeof(uint32_t));
//	//}
//}

VKRenderMesh::VKRenderMesh(const VKDeviceContext& context)
	: m_Context(context)	
{}

VKRenderMesh::~VKRenderMesh()
{}

size_t VKRenderMesh::AppendVertexBuffer(uint32_t vertNum, uint32_t vertStride)
{
	VertexData& vertData = m_VertexData.emplace_back();
	vertData.VertNum = vertNum;
	vertData.Stride = vertStride;

	// VkDeviceSize BufferSize = vertNum * vertStride;
	// vertData.StagingBufferAlloc = std::make_unique<VMAStagingBufferAlloc>(m_Context, vertData.BufferSize);
	// vertData.DeviceBufferAlloc = std::make_unique<VMAVertexBufferAlloc>(m_Context, vertData.BufferSize);
	// m_VertexBuffers.push_back(vertData.DeviceBufferAlloc->GetBuffer());

	return m_VertexData.size() - 1;
}

void VKRenderMesh::HostUpdateVertexBuffer(uint32_t bindingIdx, VkDeviceSize offset, VkDeviceSize size, const void* data)
{
	assert(bindingIdx < m_VertexData.size());

	VertexData& vertData = m_VertexData[bindingIdx];

	VkDeviceSize bufSize = vertData.VertNum * vertData.Stride;	
	assert(offset + size <= bufSize);	

	if (!vertData.StagingBufferAlloc) {
		vertData.StagingBufferAlloc = std::make_unique<VMAStagingBufferAlloc>(m_Context, bufSize);
	}	
	vertData.StagingBufferAlloc->HostUpdate(offset, size, data);
}

void VKRenderMesh::AppendIndexBuffer(uint32_t idxNum)
{
	assert(m_IdxNum == 0 && idxNum > 0);
	m_IdxNum = idxNum;

	//if (!m_IndexBuffer) {
	//	m_IndexBufferAlloc = std::make_unique<VMAIndexBufferAlloc>(m_Context, m_IdxNum * sizeof(uint32_t));
	//}
}

void VKRenderMesh::HostUpdateIndexBuffer(size_t idxOffset, size_t idxCnt, const uint32_t* idxs)
{
	assert(m_IdxNum > 0);

	VkDeviceSize idxBufSize = m_IdxNum * sizeof(uint32_t);

	if (!m_IndexStagingBufferAlloc) {
		m_IndexStagingBufferAlloc = std::make_unique<VMAStagingBufferAlloc>(m_Context, idxBufSize);
	}

	assert(idxOffset + idxCnt <= m_IdxNum);
	m_IndexStagingBufferAlloc->HostUpdate(idxOffset * sizeof(uint32_t), idxCnt * sizeof(uint32_t), (const void*)idxs);

	//if (!m_IndexDeviceBufferAlloc) {
	//	m_IndexDeviceBufferAlloc = std::make_unique<VMAIndexBufferAlloc>(m_Context, m_IdxNum * sizeof(uint32_t));
	//}
}

//uint32_t VKVertexInput::GetVertexNum() const
//{
//	return m_VertNum;
//}

//uint32_t VKVertexInput::GetVertexINputBindingNum() const
//{
//	return (uint32_t)m_VertexInputBindingDescs.size();
//}
//
//const VkVertexInputBindingDescription* VKVertexInput::GetVertexInputBindingDescriptions() const
//{
//	return m_VertexInputBindingDescs.data();
//}

uint32_t VKRenderMesh::GetVertexBufferNum() const
{
	return (uint32_t)m_VertexData.size();
}

//const VkBuffer* VKVertexInput::GetVertexBuffers() const
//{
//	return m_VertexBuffers.data();
//}

uint32_t VKRenderMesh::GetIndexNum() const
{
	return m_IdxNum;
}

VkBuffer VKRenderMesh::GetIndexBuffer() const
{
	assert(m_IndexDeviceBufferAlloc);
	return m_IndexDeviceBufferAlloc->GetBuffer();
}

//uint32_t VKVertexInput::GetPolyVertNum() const
//{
//	return m_PolyVertNum;
//}
