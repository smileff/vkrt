#include "vklib/vklib.h"
#include "Eigen/Dense"
#include "scene.h"


// Class to abstract the rendering using the test shader.
class TestMeshRender
{
public:
	struct
	{
		glm::vec4 WorldViewMatRow0;
		glm::vec4 WorldViewMatRow1;
		glm::vec4 WorldViewMatRow2;
		glm::vec4 ProjectionFactors;
		glm::vec4 UniformColor2;
	} PushConstants = {};

private:
	const VKDeviceContext& mCtx;
	VkPipeline mPipeline = VK_NULL_HANDLE;
	VkPipelineLayout mPipelineLayout = VK_NULL_HANDLE;

	std::unique_ptr<VMAVertexBufferAlloc> m_VertBufAlloc;
	std::unique_ptr<VMAVertexBufferAlloc> m_VertInstAttrBufAlloc;
	std::unique_ptr<VMAIndexBufferAlloc> m_VertIdxBufAlloc;

public:
	TestMeshRender(const VKDeviceContext& ctx);
	~TestMeshRender();	// Release all resources.

	TestMeshRender(const TestMeshRender&) = delete;
	TestMeshRender& operator=(const TestMeshRender&) = delete;

	bool InitPipeline(VkRenderPass pass, uint32_t subpass);

	// Allocate the vertex buffer and index buffer.
	bool InitResource(const Mesh<VertexP3N3T2, 3>& mesh, VkCommandBuffer cmdBuf);


	//void SetWorldTransform(const Eigen::Vector3f& translate, const Eigen::Vector3f& rotate, const Eigen::Vector3f& scale = { 1.0f, 1.0f, 1.0f });

	void CmdRender(VkCommandBuffer cmdBuf);
};

TestMeshRender::TestMeshRender(const VKDeviceContext& ctx)
	: mCtx(ctx)
{
}

TestMeshRender::~TestMeshRender()
{
	if (mPipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(mCtx.GetDevice(), mPipeline, nullptr);
		mPipeline = VK_NULL_HANDLE;
	}
	if (mPipelineLayout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(mCtx.GetDevice(), mPipelineLayout, nullptr);
		mPipelineLayout = VK_NULL_HANDLE;
	}
	m_VertBufAlloc.reset();
	m_VertInstAttrBufAlloc.reset();
	m_VertIdxBufAlloc.reset();
}

bool TestMeshRender::InitPipeline(VkRenderPass pass, uint32_t subpass)
{
	if (mPipeline != VK_NULL_HANDLE) {
		std::cerr << "Input pipeline is not null." << std::endl;
		return false;
	}

	VkDevice device = mCtx.GetDevice();

	// Vertex stage.
	std::string vertShaderPath = "./shaders/test.vert.spv";
	VkShaderModule vertShaderModule = VK_NULL_HANDLE;
	if (!VKCreateShaderModuleFromSPV(device, vertShaderPath, vertShaderModule)) {
		return false;
	}
	OnScopeExit deleteVertShaderModule([=] { vkDestroyShaderModule(device, vertShaderModule, nullptr); });

	// Fragment stage.
	std::string fragShaderPath = "./shaders/test.frag.spv";
	VkShaderModule fragShaderModule = VK_NULL_HANDLE;
	if (!VKCreateShaderModuleFromSPV(device, fragShaderPath, fragShaderModule)) {
		return false;
	}
	OnScopeExit deleteFragShaderModule([=] { vkDestroyShaderModule(device, fragShaderModule, nullptr); });

	// First configure shader stages.
	VkPipelineShaderStageCreateInfo shaderStages[] = {
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = vertShaderModule,
			.pName = "main",
		},
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = fragShaderModule,
			.pName = "main",
		},
	};

	VkVertexInputBindingDescription vertInputBindingDesc[] = {
		{
			.binding = 0,
			.stride = sizeof(float) * 6,
			.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
		},
		{
			.binding = 1,
			.stride = sizeof(float) * 3,
			.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE,
		}
	};
	VkVertexInputAttributeDescription vertInputAttrDesc[] = {
		{
			.location = 0,
			.binding = 0,
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = 0,
		},
		{
			.location = 1,
			.binding = 0,
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = sizeof(float) * 3,
		},
		{
			.location = 2,
			.binding = 1,
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = 0,
		}
	};

	VkPipelineVertexInputStateCreateInfo vertInputStateCInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = std::size(vertInputBindingDesc),
		.pVertexBindingDescriptions = vertInputBindingDesc,
		.vertexAttributeDescriptionCount = std::size(vertInputAttrDesc),
		.pVertexAttributeDescriptions = vertInputAttrDesc,
	};

	// Create pipeline layout.
	VkPushConstantRange pushConstantRange[] =
	{
		{
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,  // Each one should be assigned a different stageFlags.
			.offset = 0,
			.size = sizeof(glm::vec4) * 4,
		},
		{
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			.offset = sizeof(glm::vec4) * 4,
			.size = sizeof(glm::vec4),
		},
	};
	VkPipelineLayoutCreateInfo pplCInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pushConstantRangeCount = std::size(pushConstantRange),
		.pPushConstantRanges = pushConstantRange,
	};
	VKCall(vkCreatePipelineLayout(mCtx.GetDevice(), &pplCInfo, nullptr, &mPipelineLayout));

	// Create graphics pipeline.
	VkGraphicsPipelineCreateInfo pipelineCInfo = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = (uint32_t)std::size(shaderStages),
		.pStages = shaderStages,
		.pVertexInputState = &vertInputStateCInfo,
		.pInputAssemblyState = &g_InputAssemblyStateCreateInfo_TriangleList,
		.pViewportState = &g_ViewportStateCreateInfo_DynamicViewportScissor,
		.pRasterizationState = &g_RasterizationStateCreateInfo_Default,
		.pMultisampleState = &g_MultisamplingStateCreateInfo_DisableMultisample,
		.pColorBlendState = &g_ColorBlendStateCreateInfo_RGB_DisableBlend,
		.pDynamicState = &g_DynamicStateCreateInfo_DynamicViewportScissor,
		.layout = mPipelineLayout,
		.renderPass = pass,
		.subpass = subpass,
	};
	if (!VKSucceed(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCInfo, nullptr, &mPipeline))) {
		return false;
	}

	return true;
}


bool TestMeshRender::InitResource(const Mesh<VertexP3N3T2, 3>& mesh, VkCommandBuffer cmdBuf)
{
	// Initialize vertex buffer
	VkDeviceSize vertBufSize = mesh.Vertices.size() * sizeof(VertexP3N3T2);
	m_VertBufAlloc = std::make_unique<VMAVertexBufferAlloc>(mCtx, vertBufSize);

	// Vertex staging buffer.
	VMAStagingBufferAlloc vertStagingBuf(mCtx, vertBufSize);
	vertStagingBuf.HostUpdate(0, vertBufSize, mesh.Vertices.data());

	// Vertex device buffer.
	m_VertBufAlloc = std::make_unique<VMAVertexBufferAlloc>(mCtx, vertBufSize);
	VKCmdCopyBuffer(cmdBuf, vertStagingBuf, (VkDeviceSize)0, vertStagingBuf.GetSize(), *m_VertBufAlloc, (VkDeviceSize)0);

	// Index buffer.
	VkDeviceSize indexBufSize = sizeof(mesh.Indices.size() * sizeof(uint32_t));
	m_VertIdxBufAlloc = std::make_unique<VMAIndexBufferAlloc>(mCtx, indexBufSize);
	vkCmdUpdateBuffer(cmdBuf, m_VertIdxBufAlloc->GetBuffer(), 0, indexBufSize, mesh.Indices.data());
}

void TestMeshRender::CmdRender(VkCommandBuffer cmdBuf)
{
	vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline);

	vkCmdPushConstants(cmdBuf, mPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &PushConstants);

	// Bind vertex buffer.	
	VkBuffer vertBufs[] = { m_VertBufAlloc->GetBuffer(), m_VertInstAttrBufAlloc->GetBuffer() };
	VkDeviceSize vertBufOffsets[] = { 0, 0 };
	vkCmdBindVertexBuffers(cmdBuf, 0, std::size(vertBufs), vertBufs, vertBufOffsets);

	vkCmdDraw(cmdBuf, 6, 4, 0, 0);
	//vkCmdDrawInstanced();

	vkCmdBindIndexBuffer(cmdBuf, m_VertIdxBufAlloc->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
	//vkCmdDrawIndexed(cmdBuf, 6, 2, 0, 0, 0);
	//vkCmdDrawIndexedIndirectCountAMD
}

bool VKCreateGraphicsPipelineTest(const VKDeviceContext& ctx, VkPipelineLayout pipelineLayout, VkRenderPass pass, uint32_t subpass, VkPipeline& pipeline)
{
	if (pipeline != VK_NULL_HANDLE) {
		std::cerr << "Input pipeline is not null." << std::endl;
		return false;
	}

	VkDevice device = ctx.GetDevice();

	// Vertex stage.
	std::string vertShaderPath = "./shaders/test.vert.spv";
	VkShaderModule vertShaderModule = VK_NULL_HANDLE;
	if (!VKCreateShaderModuleFromSPV(device, vertShaderPath, vertShaderModule)) {
		return false;
	}
	OnScopeExit deleteVertShaderModule([=] { vkDestroyShaderModule(device, vertShaderModule, nullptr); });

	// Fragment stage.
	std::string fragShaderPath = "./shaders/test.frag.spv";
	VkShaderModule fragShaderModule = VK_NULL_HANDLE;
	if (!VKCreateShaderModuleFromSPV(device, fragShaderPath, fragShaderModule)) {
		return false;
	}
	OnScopeExit deleteFragShaderModule([=] { vkDestroyShaderModule(device, fragShaderModule, nullptr); });

	// First configure shader stages.
	VkPipelineShaderStageCreateInfo shaderStages[] = {
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = vertShaderModule,
			.pName = "main",
		},
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = fragShaderModule,
			.pName = "main",
		},
	};

	VkVertexInputBindingDescription vertInputBindingDesc[] = {
		{
			.binding = 0,
			.stride = sizeof(float) * 6,
			.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
		},
		{
			.binding = 1,
			.stride = sizeof(float) * 3,
			.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE,
		}
	};
	VkVertexInputAttributeDescription vertInputAttrDesc[] = {
		{
			.location = 0,
			.binding = 0,
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = 0,
		},
		{
			.location = 1,
			.binding = 0,
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = sizeof(float) * 3,
		},
		{
			.location = 2,
			.binding = 1,
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = 0,
		}
	};

	VkPipelineVertexInputStateCreateInfo vertInputStateCInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = std::size(vertInputBindingDesc),
		.pVertexBindingDescriptions = vertInputBindingDesc,
		.vertexAttributeDescriptionCount = std::size(vertInputAttrDesc),
		.pVertexAttributeDescriptions = vertInputAttrDesc,
	};

	// Create graphics pipeline.
	VkGraphicsPipelineCreateInfo pipelineCInfo = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = (uint32_t)std::size(shaderStages),
		.pStages = shaderStages,
		.pVertexInputState = &vertInputStateCInfo,
		.pInputAssemblyState = &g_InputAssemblyStateCreateInfo_TriangleList,
		.pViewportState = &g_ViewportStateCreateInfo_DynamicViewportScissor,
		.pRasterizationState = &g_RasterizationStateCreateInfo_Default,
		.pMultisampleState = &g_MultisamplingStateCreateInfo_DisableMultisample,
		.pColorBlendState = &g_ColorBlendStateCreateInfo_RGB_DisableBlend,
		.pDynamicState = &g_DynamicStateCreateInfo_DynamicViewportScissor,
		.layout = pipelineLayout,
		.renderPass = pass,
		.subpass = subpass,
	};
	if (!VKSucceed(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCInfo, nullptr, &pipeline))) {
		return false;
	}

	return true;
}

class VKMeshViewerApp : public SDLVulkanApplication
{
public:
	bool Initialize(SDL_Window* sdlWin, const VkInstance& vkInst, const VkSurfaceKHR& vkSurf) override;

	void HandleInput(SDL_Event& event) override;

	void RunOneFrame(double frameSeconds, double FPS) override;

	bool Shutdown() override;

private:
	SDL_Window* m_window = nullptr;
	VkSurfaceKHR m_vkSurface = VK_NULL_HANDLE;
	uint32_t m_rtWidth = 0, m_rtHeight = 0;

	std::unique_ptr<VKDeviceContext> m_ctx;

	uint32_t m_vkQueueFamilyIdx = (uint32_t)-1;
	VkQueue m_vkQueue = VK_NULL_HANDLE;

	std::unique_ptr<VKSwapchainContext> m_swapchainContext;
	std::unique_ptr<VKInflightContext> m_inflightContext;	

	VkCommandPool m_vkCmdPool = VK_NULL_HANDLE;
	std::vector<VkCommandBuffer> m_vkCmdBufs;

	std::unique_ptr<TestMeshRender> mTestPipelineCtx;

	std::unique_ptr<Mesh<VertexP3N3T2, 3>> mTestMesh;
	
	void PrepareTestTriangle();
};

bool VKMeshViewerApp::Initialize(SDL_Window* sdlWin, const VkInstance& vkInst, const VkSurfaceKHR& vkSurf)
{
	m_window = sdlWin;
	m_vkSurface = vkSurf;

	// Get drawable size.
	SDL_Vulkan_GetDrawableSize(m_window, (int*)&m_rtWidth, (int*)&m_rtHeight);

	// Pick physical device and queue family.
	VkPhysicalDevice vkPhysicalDevice;
	uint32_t vkQueueFamilyIdx;
	if (!VKPickSinglePhysicalDeviceAndQueueFamily(vkInst, vkSurf, &vkPhysicalDevice, &vkQueueFamilyIdx)) {
		return false;
	}

	// Create device context.
	m_ctx = std::make_unique<VKDeviceContext>(vkInst, vkPhysicalDevice, "DeviceContext");
	std::vector<VKDeviceQueueCreateRequest> vkQueueCreateRequests = {
		{ vkQueueFamilyIdx, 1, {1.0f} },
	};
	const char* extensionNames[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	if (!m_ctx->InitDeviceContext(vkQueueCreateRequests.size(), vkQueueCreateRequests.data(), 0, nullptr, 1, extensionNames)) {
		return false;
	}

	// Get queue family index and queue.
	m_vkQueueFamilyIdx = m_ctx->GetQueueFamilyIdx(0);
	m_vkQueue = m_ctx->GetQueueFamilyQueue(0, 0);

	// Determine the main surface format.
	VkSurfaceFormatKHR vkSwapchainSurfaceFormat;
	if (!VKPickSurfaceFormat(m_ctx->GetPhysicalDevice(), m_vkSurface, &vkSwapchainSurfaceFormat)) {
		return false;
	}

	// Create swapchain context.
	m_swapchainContext = std::make_unique<VKSwapchainContext>(*m_ctx);
	if (!m_swapchainContext->InitSwapchain(m_vkQueueFamilyIdx, m_vkSurface, vkSwapchainSurfaceFormat, m_rtWidth, m_rtHeight)) {
		return false;
	}

	// Create inflight context.
	m_inflightContext = std::make_unique<VKInflightContext>(*m_ctx);
	if (!m_inflightContext->InitInflight()) {
		return false;
	}

	// Initialize ImGui.
	ImGuiInitialize(
		m_window,
		m_ctx->GetInstance(),
		m_ctx->GetPhysicalDevice(),
		m_ctx->GetDevice(),
		m_vkQueueFamilyIdx,
		m_vkQueue,
		m_swapchainContext->GetSwapchainImageNum(),
		m_swapchainContext->GetSwapchainRenderPass());

	// Create command pool.
	VkCommandPoolCreateInfo vkCmdPoolCInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = m_vkQueueFamilyIdx,
	};
	VKCall(vkCreateCommandPool(m_ctx->GetDevice(), &vkCmdPoolCInfo, nullptr, &m_vkCmdPool));

	// Create command buffer.
	uint32_t inflightFrameNum = m_inflightContext->GetInflightFrameNum();
	m_vkCmdBufs.resize(inflightFrameNum);
	{
		VkCommandBufferAllocateInfo vkCmdBufAllocInfo = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = m_vkCmdPool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = (uint32_t)m_vkCmdBufs.size(),
		};
		VKCall(vkAllocateCommandBuffers(m_ctx->GetDevice(), &vkCmdBufAllocInfo, m_vkCmdBufs.data()));
	}
	
	PrepareTestTriangle();

	return true;
}

void VKMeshViewerApp::HandleInput(SDL_Event& event)
{
	ImGui_ImplSDL2_ProcessEvent(&event);
}

void VKMeshViewerApp::RunOneFrame(double frameSeconds, double FPS)
{
	// Update fps to window title.
	std::stringstream ss;
	ss << "FPS: " << std::setprecision(3) << std::fixed << FPS;
	SDL_SetWindowTitle(m_window, ss.str().c_str());

	// Wait for the previous frame to finish.
	if (!m_inflightContext->WaitForInflightFrameFence()) {
		return;
	}
	uint32_t inflightFrameIdx = m_inflightContext->GetInflightFrameIdx();

	// Get the target image from the swapchain.
	m_swapchainContext->AcquireNextSwapchainImage(m_inflightContext->GetImageAvailableSemaphore(), UINT64_MAX);

	// ImGui render.
	ImGuiBeginFrame();

	// ImGui demo window.
	ImGui::ShowDemoWindow();

	// Render this frame.

	VkCommandBuffer cmdBuf = m_vkCmdBufs[inflightFrameIdx];
	VKCall(vkResetCommandBuffer(cmdBuf, 0));

	VkCommandBufferBeginInfo cmdBufBeginInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};
	VKCall(vkBeginCommandBuffer(cmdBuf, &cmdBufBeginInfo));

	// Set viewport and scissor, required by the pipeline.
	VkRect2D swapchainRect = m_swapchainContext->GetSwapchainRect();
	VkViewport viewport = { 0, 0, (float)swapchainRect.extent.width, (float)swapchainRect.extent.height, 0.0f, 1.0f };
	vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
	vkCmdSetScissor(cmdBuf, 0, 1, &swapchainRect);

	// Begin render pass.
	VkClearValue clearValue = { VkClearColorValue{ 0.1f, 0.0f, 0.0f, 1.0f } };
	VkRenderPassBeginInfo renderPassBeginInfo = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = m_swapchainContext->GetSwapchainRenderPass(),
		.framebuffer = m_swapchainContext->GetSwapchainFramebuffer(),
		.renderArea = m_swapchainContext->GetSwapchainRect(),
		.clearValueCount = 1,
		.pClearValues = &clearValue,
	};
	vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);


	//mTestPipelineCtx->CmdBindPipeline(cmdBuf);

	// Set the world view transform uniform.
	static float rotRadian = 0.0f;
	rotRadian += (float)(M_PI / 3.0f * frameSeconds);
	float cosRotRadian = cos(rotRadian);
	float sinRotRadian = sin(rotRadian);
	mTestPipelineCtx->PushConstants.WorldViewMatRow0 = { cosRotRadian, 0.0f, sinRotRadian, 0.0f };
	mTestPipelineCtx->PushConstants.WorldViewMatRow1 = { 0.0, 1.0f, 0.0f, 0.0f };
	mTestPipelineCtx->PushConstants.WorldViewMatRow2 = { -sinRotRadian, 0.0f, cosRotRadian, -2.0f };
	// Projection uniform.
	float fov = 45.0f;
	float aspect = (float)m_rtWidth / (float)m_rtHeight;
	float tanHalfFov = tan(glm::radians(fov) / 2.0f);
	float fNear = 0.01f;
	float fFar = 100.0f;
	glm::vec4 projFactors = { -1.0f / (aspect * tanHalfFov), 1.0 / tanHalfFov, fFar / (fFar - fNear), fNear * fFar / (fFar - fNear) };
	mTestPipelineCtx->PushConstants.ProjectionFactors = projFactors;
	// Color uniform.
	glm::vec4 uniformColor2 = { 0.25f, 0.5f, 1.0f, 0.0f };
	mTestPipelineCtx->PushConstants.UniformColor2 = uniformColor2;
	//mTestPipelineCtx->CmdPushConstants(cmdBuf);

	mTestPipelineCtx->CmdRender(cmdBuf);



	// ImGui render.
	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuf);

	vkCmdEndRenderPass(cmdBuf);

	VKCall(vkEndCommandBuffer(cmdBuf));

	// Render.
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	VkSubmitInfo submitInfo = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &m_inflightContext->GetImageAvailableSemaphore(),
		.pWaitDstStageMask = waitStages,
		.commandBufferCount = (uint32_t)1,
		.pCommandBuffers = &cmdBuf,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &m_inflightContext->GetRenderFinishedSemaphore(),
	};
	VKCall(vkQueueSubmit(m_vkQueue, 1, &submitInfo, m_inflightContext->GetInflightFrameFence()));

	// Present.
	VkPresentInfoKHR presentInfo = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &m_inflightContext->GetRenderFinishedSemaphore(),
		.swapchainCount = 1,
		.pSwapchains = &m_swapchainContext->GetSwapchain(),
		.pImageIndices = &m_swapchainContext->GetSwapchainImageIdx(),
	};
	vkQueuePresentKHR(m_vkQueue, &presentInfo);

	m_inflightContext->NextFrame();
}

bool VKMeshViewerApp::Shutdown()
{
	vkDeviceWaitIdle(m_ctx->GetDevice());

	if (m_vkCmdPool) {
		vkDestroyCommandPool(m_ctx->GetDevice(), m_vkCmdPool, nullptr);
	}

	ImGuiShutdown();

	m_vkQueue = VK_NULL_HANDLE;

	//vkDestroyPipeline(m_ctx->GetDevice(), m_vkGraphicsPipeline, nullptr);
	//vkDestroyPipelineLayout(m_ctx->GetDevice(), m_vkPipelineLayout, nullptr);

	mTestPipelineCtx.reset();
	m_swapchainContext.reset();
	m_inflightContext.reset();

	m_ctx.reset();

	return true;
}

void VKMeshViewerApp::PrepareTestTriangle()
{
	// Define the mesh.
	// Vertex data.
	mTestMesh = std::make_unique<Mesh<VertexP3N3T2, 3>>();
	mTestMesh->Vertices = {
		{ { 0.0f, 0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
		{ {-0.5f, -0.0f, 0.0f}, {0.0f, 1.0f, 0.0f} },
		{ {0.5f, -0.0f, 0.0f}, {0.0f, 0.0f, 1.0f} },
		{ {0.0f, -0.5f, 0.0f}, {0.5f, 0.0f, 0.0f} },
		{ {0.5f, -0.0f, 0.0f}, {0.0f, 0.0f, 0.5f} },
		{ {-0.5f, -0.0f, 0.0f}, {0.0f, 0.5f, 0.0f} },
	};
	VkDeviceSize vertBufSize = mTestMesh->Vertices.size() * sizeof(VertexP3N3T2);
	// Index data.
	mTestMesh->Indices = {
		0, 1, 2,
		2, 1, 3,
	};
	VkDeviceSize indexBufSize = sizeof(mTestMesh->Indices.size() * sizeof(uint32_t));

	mTestPipelineCtx = std::make_unique<TestMeshRender>(*m_ctx);
	mTestPipelineCtx->InitPipeline(m_swapchainContext->GetSwapchainRenderPass(), 0);

	// Create a command buffer to fill the vertex buffer and index buffer.
	VkCommandBuffer initCmdBuf = VK_NULL_HANDLE;
	VkCommandBufferAllocateInfo stCmdBufAllocInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = m_vkCmdPool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1,
	};
	VKCall(vkAllocateCommandBuffers(m_ctx->GetDevice(), &stCmdBufAllocInfo, &initCmdBuf));
	OnScopeExit deleteInitCmdBuf([&] { vkFreeCommandBuffers(m_ctx->GetDevice(), m_vkCmdPool, 1, &initCmdBuf); });

	// Transfer the staging buffer to the vertex buffer.
	VkCommandBufferBeginInfo initCmdBufBeginInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};
	vkBeginCommandBuffer(initCmdBuf, &initCmdBufBeginInfo);

	// Vertex staging buffer.
	VMAStagingBufferAlloc vertStagingBuf(*m_ctx, vertBufSize);
	vertStagingBuf.HostUpdate(0, vertBufSize, mTestMesh->Vertices.data());

	// Vertex device buffer.
	m_VertBufAlloc = std::make_unique<VMAVertexBufferAlloc>(*m_ctx, vertBufSize);
	VKCmdCopyBuffer(initCmdBuf, vertStagingBuf, (VkDeviceSize)0, vertStagingBuf.GetSize(), *m_VertBufAlloc, (VkDeviceSize)0);

	// Index buffer.
	m_VertIdxBufAlloc = std::make_unique<VMAIndexBufferAlloc>(*m_ctx, indexBufSize);
	vkCmdUpdateBuffer(initCmdBuf, m_VertIdxBufAlloc->GetBuffer(), 0, indexBufSize, mTestMesh->Indices.data());

	// Draw instance attributes.
	glm::vec3 instColor[] = {
		glm::vec3(1.0f, 1.0f, 0.0f),
		glm::vec3(0.0f, 1.0f, 1.0f),
		glm::vec3(1.0f, 0.0f, 1.0f),
		glm::vec3(1.0f, 1.0f, 1.0f),
		glm::vec3(1.0f, 1.0f, 1.0f),
		glm::vec3(1.0f, 1.0f, 1.0f),
	};
	VkDeviceSize instColorSize = sizeof(instColor);
	VMAStagingBufferAlloc instColorStagingBuf(*m_ctx, instColorSize);
	instColorStagingBuf.HostUpdate(0, instColorSize, &instColor);
	m_VertInstAttrBufAlloc = std::make_unique<VMAVertexBufferAlloc>(*m_ctx, instColorSize);
	VKCmdCopyBuffer(initCmdBuf, instColorStagingBuf, (VkDeviceSize)0, instColorStagingBuf.GetSize(), *m_VertInstAttrBufAlloc, (VkDeviceSize)0);

	vkEndCommandBuffer(initCmdBuf);

	// Submit the command buffer.
	VkSubmitInfo submitInfo = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &initCmdBuf,
	};
	vkQueueSubmit(m_vkQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(m_vkQueue);
}

int main(int argc, char** argv)
{
	VKMeshViewerApp app;
	app.Run(1280, 800);

	return 0;
}