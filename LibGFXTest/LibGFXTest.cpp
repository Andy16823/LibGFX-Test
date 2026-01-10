// LibGFXTest.cpp: Definiert den Einstiegspunkt für die Anwendung.
//
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <memory>
#include "LibGFXTest.h"
#include "LibGFX.h"
#include "VkContext.h"
#include "DepthBuffer.h"
#include "DefaultRenderPass.h"
#include "DescriptorSetLayoutBuilder.h"
#include "DefaultPipeline.h"
#include "DescriptorPoolBuilder.h"
#include "Vertex.h"
#include "DescriptorSetWriter.h"
#include <array>

using namespace std;

struct UniformBufferObject {
	glm::mat4 view;
	glm::mat4 proj;
};

LibGFX::Buffer createVertexBuffer(LibGFX::VkContext* context) {

	auto vertices = std::vector<Vertex3D>{
		{{0.0f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
		{{0.5f, 0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
		{{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}}
	};

	VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

	LibGFX::Buffer vertexBuffer = context->createBuffer(
		bufferSize,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	);

	context->updateBuffer(vertexBuffer, vertices.data(), bufferSize);

	return vertexBuffer;
}

LibGFX::Buffer createIndexBuffer(LibGFX::VkContext* context) {
	auto indices = std::vector<uint16_t>{
		0, 1, 2
	};
	VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();
	LibGFX::Buffer indexBuffer = context->createBuffer(
		bufferSize,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	);
	context->updateBuffer(indexBuffer, indices.data(), bufferSize);
	return indexBuffer;
}

LibGFX::Buffer createUniformBuffer(LibGFX::VkContext* context) {
	VkDeviceSize bufferSize = sizeof(UniformBufferObject);
	LibGFX::Buffer uniformBuffer = context->createBuffer(
		bufferSize,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	);
	return uniformBuffer;
}

void updateUniformBuffer(LibGFX::VkContext* context, LibGFX::Buffer& uniformBuffer) {
	UniformBufferObject ubo = {};
	ubo.view = glm::mat4(1.0f);
	ubo.proj = glm::mat4(1.0f);
	context->updateBuffer(uniformBuffer, &ubo, sizeof(ubo));
}

int main()
{
	// 1. Create window
	auto window = LibGFX::GFX::createWindow(800, 600, "LibGFX Test Window");

	// 2. Create Vulkan context and initialize it. It will select physical device, create logical device, queues, surface, instance
	auto context = LibGFX::GFX::createContext(window);
	context->initialize(LibGFX::VkContext::defaultAppInfo());

	// 3. Create the swapchain with desired present mode
	auto swapchainInfo = context->createSwapChain(VK_PRESENT_MODE_MAILBOX_KHR);

	// 4. Create an depth buffer with the best suitable format
	VkFormat bestDepthFormat = context->findSuitableDepthFormat();
	auto depthBuffer = context->createDepthBuffer(swapchainInfo.extent, bestDepthFormat);

	// 5. Create default render pass. You can also create your own render pass by inheriting from LibGFX::RenderPass but this requires
	// more code and understanding of Vulkan render passes.
	auto renderPass = std::make_unique<LibGFX::Presets::DefaultRenderPass>();
	if (!renderPass->create(context->getDevice(), swapchainInfo.surfaceFormat.format, depthBuffer.format)) {
		cerr << "Failed to create default render pass!" << endl;
		return -1;
	}

	// 6. Create the pipeline. This requires a bit more code from you since there is no default pipeline preset. You need to create your own pipeline
	// inherit from LibGFX::Pipeline. Here we use a DefaultPipeline class defined in this test project. Its nessesare because Pipelines are very 
	// specific to your application and how you want to render things.
	auto pipeline = std::make_unique<DefaultPipeline>();
	auto viewport = context->createViewport(0.0f, 0.0f, swapchainInfo.extent);
	auto scissor = context->createScissorRect(0, 0, swapchainInfo.extent);
	pipeline->create(context.get(), renderPass->getRenderPass(), viewport, scissor);

	// 7. Now we need framebuffers for each swapchain image. We can use the context function here.
	auto framebuffers = context->createFramebuffers(*renderPass, swapchainInfo, depthBuffer);

	// 8. Next we need to create the command pools for the command buffers. For this we need the graphics queue family index and define
	// a flag that allows us to reset command buffers individually.
	auto queueFamilyIndices = context->getQueueFamilyIndices(context->getPhysicalDevice());
	auto commandPool = context->createCommandPool(queueFamilyIndices.graphicsFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	// 9. allocate command buffers for each framebuffer // TODO: Change to allocCommandBuffers in VkContext since we are not creating we are allocating them
	// from the command pool.
	auto commandBuffers = context->allocateCommandBuffers(commandPool, static_cast<uint32_t>(framebuffers.size()));

	// 10. In order to use textures we need to create samplers. Here we create a default texture sampler with anisotropic filtering enabled.
	// You can also create your own sampler by using the createSampler function in VkContext with a custom VkSamplerCreateInfo structure.
	auto textureSampler = context->createTextureSampler(true, 16.0f);

	// 11. For our uniform buffers we need to create descriptor pools and sets.
	// We need an descriptor pool for our 'UniformBufferObject' uniform buffers. We want that every frame inflight has its own uniform buffer.
	// This means we need an pool size for each framebuffer in the swapchain. and we need to set the max sets to the number of framebuffers as well.
	LibGFX::DescriptorPoolBuilder descriptorPoolBuilder;
	descriptorPoolBuilder.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, static_cast<uint32_t>(framebuffers.size()));
	descriptorPoolBuilder.setMaxSets(static_cast<uint32_t>(framebuffers.size()));
	auto descriptorPool = descriptorPoolBuilder.build(*context);
	descriptorPoolBuilder.clear();

	// 12. Create semaphores and fences for rendering synchronization
	// In order to synchronize the rendering process we need sets of semaphores and fences.
	// we need one set for each framebuffer in the swapchain.
	int currentFrame = 0;
	int maxFraomesInFlight = static_cast<int>(framebuffers.size());
	std::vector<VkFence> imagesInFlight(maxFraomesInFlight, VK_NULL_HANDLE);
	auto imageAvailableSemaphores = context->createSemaphores(static_cast<uint32_t>(framebuffers.size()));
	auto renderFinishedSemaphores = context->createSemaphores(static_cast<uint32_t>(framebuffers.size()));
	auto inFlightFences = context->createFences(static_cast<uint32_t>(framebuffers.size()), VK_FENCE_CREATE_SIGNALED_BIT);

	// Test buffer
	auto vertexBuffer = createVertexBuffer(context.get());
	auto indexBuffer = createIndexBuffer(context.get());
	std::vector<LibGFX::Buffer> uniformBuffers;
	for (size_t i = 0; i < framebuffers.size(); i++) {
		uniformBuffers.push_back(createUniformBuffer(context.get()));
	}

	LibGFX::DescriptorSetWriter descriptorSetWriter;
	std::vector<VkDescriptorSet> descriptorSets;
	for (size_t i = 0; i < framebuffers.size(); i++) {
		auto uniformBuffer = uniformBuffers[i];
		auto descriptorSet = context->allocateDescriptorSet(descriptorPool, pipeline->getUniformsLayout());
		
		descriptorSetWriter.addBufferInfo(uniformBuffer.buffer, 0, uniformBuffer.size)
			.setDstBinding(0)
			.setDstArrayElement(0)
			.setDescriptorType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
			.write(*context, descriptorSet);

		descriptorSetWriter.clear();
		descriptorSets.push_back(descriptorSet);
	}


	// 13. Initialize is done. We can now enter the main loop.
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		// Update uniform buffer for this frame
		updateUniformBuffer(context.get(), uniformBuffers[currentFrame]);

		// Wait for the fence to be signaled from the last frame
		context->waitForFence(inFlightFences[currentFrame]);

		uint32_t imageIndex; 
		context->acquireNextImage(swapchainInfo, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, imageIndex);

		if(imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
			context->waitForFence(imagesInFlight[imageIndex]);
		}
		imagesInFlight[imageIndex] = inFlightFences[currentFrame];
		context->resetFence(inFlightFences[currentFrame]);

		// Record command buffer
		context->beginCommandBuffer(commandBuffers[imageIndex]);
		context->beginRenderPass(commandBuffers[imageIndex], *renderPass.get(), framebuffers[imageIndex], swapchainInfo.extent);
		context->bindPipeline(commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline.get());

		// Draw call
		VkDescriptorSet descriptorSet = descriptorSets[imageIndex];
		VkCommandBuffer commandBuffer = commandBuffers[imageIndex];

		std::array<VkDescriptorSet, 1> descriptorSetsToBind = { descriptorSet };
		vkCmdBindDescriptorSets(commandBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipeline->getPipelineLayout(),
			0,
			static_cast<uint32_t>(descriptorSetsToBind.size()),
			descriptorSetsToBind.data(),
			0,
			nullptr
		);

		VkBuffer vertexBuffers[] = { vertexBuffer.buffer };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, std::array<VkDeviceSize, 1>{0}.data());
		vkCmdDraw(commandBuffer, 3, 1, 0, 0);
		// End draw call


		context->endRenderPass(commandBuffers[imageIndex]);
		context->endCommandBuffer(commandBuffers[imageIndex]);

		// Submit command buffer
		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffers[imageIndex];
		VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		context->submitCommandBuffer(submitInfo, inFlightFences[currentFrame]);

		// Present swapchain image
		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapchains[] = { swapchainInfo.swapchain };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapchains;
		presentInfo.pImageIndices = &imageIndex;

		context->queuePresent(presentInfo);
		currentFrame = (currentFrame + 1) % maxFraomesInFlight;
	}

	// Wait for device to be idle before cleanup
	context->waitIdle();

	// Destroy test buffers
	context->destroyBuffer(vertexBuffer);
	context->destroyBuffer(indexBuffer);
	for (auto uniformBuffer : uniformBuffers) {
		context->destroyBuffer(uniformBuffer);
	}

	// Destroy synchronization objects
	context->destroySemaphores(imageAvailableSemaphores);
	context->destroySemaphores(renderFinishedSemaphores);
	context->destroyFences(inFlightFences);

	// Destroy descriptor pool
	context->destroyDescriptorSetPool(descriptorPool);

	// Destroy texture sampler
	context->destroySampler(textureSampler);

	// Destroy command buffers
	for (auto commandBuffer : commandBuffers) {
		context->freeCommandBuffer(commandPool, commandBuffer);
	}
	context->destroyCommandPool(commandPool);

	// Cleanup
	for (auto framebuffer : framebuffers) {
		context->destroyFramebuffer(framebuffer);
	}
	pipeline->destroy(context.get());
	renderPass->destroy(context->getDevice());
	context->destroyDepthBuffer(depthBuffer);
	context->destroySwapChain(swapchainInfo);
	context->dispose();

	std::cout << "Hello CMake." << endl;
	return 0;
}
