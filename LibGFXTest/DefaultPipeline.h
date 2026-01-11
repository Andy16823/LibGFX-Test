#pragma once
#include "Pipeline.h"
#include <vulkan/vulkan.h>
#include <vector>
#include "VkContext.h"

class DefaultPipeline : public LibGFX::Pipeline
{
private:
	VkPipeline m_pipeline; 
	VkPipelineLayout m_pipelineLayout;
	VkDescriptorSetLayout m_uniformsLayout;
	VkDescriptorSetLayout m_textureLayout;

public:
	void create(LibGFX::VkContext* context, VkRenderPass renderPass, VkViewport viewport, VkRect2D scissor);
	void destroy(LibGFX::VkContext* context);
	VkPipeline getPipeline() const override;
	VkPipelineLayout getPipelineLayout() const override;
	VkDescriptorSetLayout getUniformsLayout() const { return m_uniformsLayout; }
	VkDescriptorSetLayout getTextureLayout() const { return m_textureLayout; }
};
