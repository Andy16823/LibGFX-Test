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
	VkViewport m_viewport;
	VkRect2D m_scissor;

public:
	void setViewport(VkViewport viewport) { m_viewport = viewport; }
	void setScissor(VkRect2D scissor) { m_scissor = scissor; }
	void create(LibGFX::VkContext* context, VkRenderPass renderPass);
	void destroy(LibGFX::VkContext* context);
	VkPipeline getPipeline() const override;
	VkPipelineLayout getPipelineLayout() const override;
	VkDescriptorSetLayout getUniformsLayout() const { return m_uniformsLayout; }
	VkDescriptorSetLayout getTextureLayout() const { return m_textureLayout; }
};
