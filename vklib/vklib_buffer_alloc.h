#ifndef _VK_BUFFER_ALLOC_H_
#define _VK_BUFFER_ALLOC_H_

#include "vklib.h"

// VMAAutoBufferAlloc.
// Hide the VMA memory allocation.
template<VkBufferUsageFlags BufferUsageFlags, VmaAllocationCreateFlags AllocationCreateFlags>
class VMAAutoBufferAlloc
{
	const VmaAllocator m_Allocator = VK_NULL_HANDLE;

	VkDeviceSize m_BufferSize = 0;
	VkBuffer m_Buffer = VK_NULL_HANDLE;
	VmaAllocation m_Alloc = VK_NULL_HANDLE;
public:

	static constexpr VkBufferUsageFlags BufferUsageFlags = BufferUsageFlags;
	static constexpr VmaAllocationCreateFlags AllocationCreateFlags = AllocationCreateFlags;

	VMAAutoBufferAlloc(const class VKDeviceContext& context, VkDeviceSize bufSize)
		: m_Allocator(context.GetVmaAllocator())
	{
		m_BufferSize = bufSize;

		VkBufferCreateInfo bufCInfo = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = m_BufferSize,
			.usage = BufferUsageFlags,
		};
		VmaAllocationCreateInfo allocCInfo = {
			.flags = AllocationCreateFlags,
			.usage = VMA_MEMORY_USAGE_AUTO,
		};
		VKCall(vmaCreateBuffer(m_Allocator, &bufCInfo, &allocCInfo, &m_Buffer, &m_Alloc, nullptr));
	}
	
	~VMAAutoBufferAlloc()
	{
		Clear();
	}

	// Forbid copy.
	VMAAutoBufferAlloc(const VMAAutoBufferAlloc&) = delete;
	VMAAutoBufferAlloc& operator=(const VMAAutoBufferAlloc&) = delete;

	void HostUpdate(VkDeviceSize offset, VkDeviceSize size, const void* data)
	{
		assert(m_Buffer != VK_NULL_HANDLE && m_Alloc != VK_NULL_HANDLE);

		assert(AllocationCreateFlags & (VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT));
		assert(offset + size <= m_BufferSize);
		VKCall(vmaCopyMemoryToAllocation(m_Allocator, data, m_Alloc, offset, size));
	}

	VkBuffer GetBuffer() const {
		return m_Buffer;
	}

	VkDeviceSize GetSize() const
	{
		return m_BufferSize;
	}

	void Clear()
	{
		if (m_Buffer != VK_NULL_HANDLE) {
			assert(m_Alloc != VK_NULL_HANDLE);
			vmaDestroyBuffer(m_Allocator, m_Buffer, m_Alloc);

			m_Buffer = VK_NULL_HANDLE;
			m_Alloc = VK_NULL_HANDLE;
		}
	}
};

template<
	VkBufferUsageFlags SrcBufferUsageFlags, VmaAllocationCreateFlags SrcAllocationCreateFlags,
	VkBufferUsageFlags DstBufferUsageFlags, VmaAllocationCreateFlags DstAllocationCreateFlags>
void VKCmdCopyBuffer(VkCommandBuffer cmdBuf,
	const typename VMAAutoBufferAlloc<SrcBufferUsageFlags, SrcAllocationCreateFlags>& srcBuf, VkDeviceSize srcOffset, VkDeviceSize size,
	const typename VMAAutoBufferAlloc<DstBufferUsageFlags, DstAllocationCreateFlags>& dstBuf, VkDeviceSize dstOffset)
{
	assert(srcOffset + size <= srcBuf.GetSize());
	assert(dstOffset + size <= dstBuf.GetSize());

	VkBufferCopy bufCopy = {
		.srcOffset = srcOffset,
		.dstOffset = dstOffset,
		.size = size,
	};
	vkCmdCopyBuffer(cmdBuf, srcBuf.GetBuffer(), dstBuf.GetBuffer(), 1, &bufCopy);
}


using VMAStagingBufferAlloc = VMAAutoBufferAlloc<VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT>;
using VMAVertexBufferAlloc = VMAAutoBufferAlloc<VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 0>;
using VMAIndexBufferAlloc = VMAAutoBufferAlloc<VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 0>;

#endif