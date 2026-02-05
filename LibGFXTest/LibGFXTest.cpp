// LibGFXTest.cpp: Definiert den Einstiegspunkt für die Anwendung.
//
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define STB_IMAGE_IMPLEMENTATION
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
#include "Imaging.h"
#include "stb_image.h"

using namespace std;

struct UniformBufferObject {
	glm::mat4 view;
	glm::mat4 proj;
};

LibGFX::ImageData createImageData(const std::string& imagePath) {
	int texWidth, texHeight, texChannels;
	stbi_uc* pixels = stbi_load(imagePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	if (!pixels) {
		throw std::runtime_error("Failed to load texture image!");
	}
	LibGFX::ImageData imageData;
	imageData.pixels = pixels;
	imageData.width = static_cast<uint32_t>(texWidth);
	imageData.height = static_cast<uint32_t>(texHeight);
	imageData.format = VK_FORMAT_R8G8B8A8_UNORM;
	return imageData;
}

LibGFX::Buffer createVertexBuffer(LibGFX::VkContext* context) {

	auto vertices = std::vector<Vertex3D>{
		{{-0.5f, -0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}, // Top Left
		{{0.5f, -0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f}}, // Top Right
		{{0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f}}, // Bottom Right
		{{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f}}, // Bottom Left
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
		0, 2, 3, // First Triangle
		0, 1, 2  // Second Triangle
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
	// Create an GLFW window for the application
	auto window = LibGFX::GFX::createWindow(800, 600, "LibGFX Test Window");

	// Create the Vulkan context and initialize it
	auto context = LibGFX::GFX::createContext(window);
	context->initialize(LibGFX::VkContext::defaultAppInfo(), true);

	// Create the swapchain with the desired present mode
	auto swapchainInfo = context->createSwapChain(VK_PRESENT_MODE_MAILBOX_KHR);

	// Create an optimal depth format and depth buffer
	VkFormat bestDepthFormat = context->findSuitableDepthFormat();
	auto depthBuffer = context->createDepthBuffer(swapchainInfo.extent, bestDepthFormat);

	// Create an render pass. Here we use the default render pass preset from LibGFX.
	auto renderPass = std::make_unique<LibGFX::Presets::DefaultRenderPass>();
	if (!renderPass->create(context->getDevice(), swapchainInfo.surfaceFormat.format, depthBuffer.format)) {
		cerr << "Failed to create default render pass!" << endl;
		return -1;
	}

	// Create the graphics pipeline. You need to create the pipeline for yourself.
	auto pipeline = std::make_unique<DefaultPipeline>();
	auto viewport = context->createViewport(0.0f, 0.0f, swapchainInfo.extent);
	auto scissor = context->createScissorRect(0, 0, swapchainInfo.extent);
	pipeline->setViewport(viewport);
	pipeline->setScissor(scissor);
	pipeline->create(context.get(), renderPass->getRenderPass());

	// Create framebuffer for each swapchain image
	auto framebuffers = context->createFramebuffers(*renderPass, swapchainInfo, depthBuffer);

	// Create a command pool for command buffer allocation
	auto queueFamilyIndices = context->getQueueFamilyIndices(context->getPhysicalDevice());
	auto commandPool = context->createCommandPool(queueFamilyIndices.graphicsFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	// Allocate command buffers from the command pool
	auto commandBuffers = context->allocateCommandBuffers(commandPool, static_cast<uint32_t>(framebuffers.size()));

	// Create buffers for rendering
	auto vertexBuffer = createVertexBuffer(context.get());				// Vertex buffer
	auto indexBuffer = createIndexBuffer(context.get());				// Index buffer
	std::vector<LibGFX::Buffer> uniformBuffers;							// Uniform buffers for each frame inflight
	for (size_t i = 0; i < framebuffers.size(); i++) {
		uniformBuffers.push_back(createUniformBuffer(context.get()));	// Create uniform buffer for the model-view-projection matrices
	}

	// Create descriptor pool for the uniform buffers
	LibGFX::DescriptorPoolBuilder descriptorPoolBuilder;
	descriptorPoolBuilder.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, static_cast<uint32_t>(framebuffers.size()));
	descriptorPoolBuilder.setMaxSets(static_cast<uint32_t>(framebuffers.size()));
	auto descriptorPool = descriptorPoolBuilder.build(*context);
	descriptorPoolBuilder.clear();

	// Create the uniform descriptor sets for the matrices for each framebuffer
	LibGFX::DescriptorSetWriter descriptorSetWriter;
	std::vector<VkDescriptorSet> descriptorSets;
	for (size_t i = 0; i < framebuffers.size(); i++) {
		auto buffer = uniformBuffers[i];
		auto descriptorSet = context->allocateDescriptorSet(descriptorPool, pipeline->getUniformsLayout());
		descriptorSetWriter.addBufferInfo(buffer.buffer, 0, buffer.size)
			.write(*context, descriptorSet, 0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
			.clear();

		descriptorSets.push_back(descriptorSet);
	}

	// Create texture image, image view and texture sampler
	auto imageData = createImageData("C:/Users/andy1/Pictures/CF Logo 2.jpg");
	auto textureSampler = context->createTextureSampler(true, 16.0f);
	auto image = context->createImage(imageData, commandPool);
	imageData.pixels = nullptr; // Image data has been copied to GPU, we can free the pixel data now

	// Create descriptor pool for the texture sampler
	LibGFX::DescriptorPoolBuilder textureDescriptorPoolBuilder;
	textureDescriptorPoolBuilder.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 250);
	textureDescriptorPoolBuilder.setMaxSets(250);
	auto textureDescriptorPool = textureDescriptorPoolBuilder.build(*context);

	// Create descriptor set for the texture sampler. Layout is defined in the pipeline.
	VkDescriptorSet textureDescriptorSet = context->allocateDescriptorSet(textureDescriptorPool, pipeline->getTextureLayout());
	LibGFX::DescriptorSetWriter textureDescriptorSetWriter;
	textureDescriptorSetWriter.addImageInfo(image.imageView, textureSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		.write(*context, textureDescriptorSet, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
		.clear();

	// Create synchronization objects
	int currentFrame = 0;
	int maxFramesInFlight = static_cast<int>(framebuffers.size());
	std::vector<VkFence> imagesInFlight(maxFramesInFlight, VK_NULL_HANDLE);
	auto imageAvailableSemaphores = context->createSemaphores(static_cast<uint32_t>(framebuffers.size()));
	auto renderFinishedSemaphores = context->createSemaphores(static_cast<uint32_t>(framebuffers.size()));
	auto inFlightFences = context->createFences(static_cast<uint32_t>(framebuffers.size()), VK_FENCE_CREATE_SIGNALED_BIT);

	// Main loop
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		// Wait for the fence to be signaled from the last frame
		context->waitForFence(inFlightFences[currentFrame]);

		// Acquire next image from the swapchain
		uint32_t imageIndex; 
		context->acquireNextImage(swapchainInfo, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, imageIndex);

		// Check if a previous frame is using this image (i.e. there is its fence to wait on)
		if(imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
			context->waitForFence(imagesInFlight[imageIndex]);
		}
		imagesInFlight[imageIndex] = inFlightFences[currentFrame];
		context->resetFence(inFlightFences[currentFrame]);

		// Update uniform buffer for this frame
		updateUniformBuffer(context.get(), uniformBuffers[imageIndex]);

		// Record command buffer, begin render pass and bind pipeline
		context->beginCommandBuffer(commandBuffers[imageIndex]);
		context->beginRenderPass(commandBuffers[imageIndex], *renderPass.get(), framebuffers[imageIndex], swapchainInfo.extent);
		context->bindPipeline(commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline.get());

		// Begin draw call
		VkDescriptorSet descriptorSet = descriptorSets[imageIndex];
		VkCommandBuffer commandBuffer = commandBuffers[imageIndex];

		// Bind descriptor sets to the pipeline
		std::array<VkDescriptorSet, 2> descriptorSetsToBind = { descriptorSet, textureDescriptorSet };
		vkCmdBindDescriptorSets(commandBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipeline->getPipelineLayout(),
			0,
			static_cast<uint32_t>(descriptorSetsToBind.size()),
			descriptorSetsToBind.data(),
			0,
			nullptr
		);

		// Bind vertex and index buffers and issue draw call
		VkBuffer vertexBuffers[] = { vertexBuffer.buffer };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, std::array<VkDeviceSize, 1>{0}.data());
		vkCmdBindIndexBuffer(commandBuffer, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT16);
		vkCmdDrawIndexed(commandBuffer, 6, 1, 0, 0, 0);

		// End render pass and command buffer recording
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

		// Advance to the next frame
		currentFrame = (currentFrame + 1) % maxFramesInFlight;
	}

	// Wait for device to be idle before cleanup
	context->waitIdle();

	// Destroy synchronization objects
	context->destroySemaphores(imageAvailableSemaphores);
	context->destroySemaphores(renderFinishedSemaphores);
	context->destroyFences(inFlightFences);

	// Destroy texture image
	context->destroySampler(textureSampler);
	context->destroyImage(image);
	context->destroyDescriptorSetPool(textureDescriptorPool);

	// Destroy buffers
	context->destroyBuffer(vertexBuffer);
	context->destroyBuffer(indexBuffer);
	for (auto uniformBuffer : uniformBuffers) {
		context->destroyBuffer(uniformBuffer);
	}

	// Destroy descriptor pool
	context->destroyDescriptorSetPool(descriptorPool);

	// Destroy command buffers
	for (auto commandBuffer : commandBuffers) {
		context->freeCommandBuffer(commandPool, commandBuffer);
	}
	context->destroyCommandPool(commandPool);

	// Destroy framebuffers
	for (auto framebuffer : framebuffers) {
		context->destroyFramebuffer(framebuffer);
	}

	// Destroy pipeline and render pass
	pipeline->destroy(context.get());
	renderPass->destroy(context->getDevice());

	// Destroy depth buffer and swapchain
	context->destroyDepthBuffer(depthBuffer);
	context->destroySwapChain(swapchainInfo);

	// Dispose the Vulkan context
	context->dispose();

	std::cout << "Bye Triangle!" << std::endl;
	return 0;
}