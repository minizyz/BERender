#pragma once
#include "berender/backend/render_backend.h"
#ifdef BERENDER_HAS_VULKAN
#include <vulkan/vulkan.h>
#endif
#include <vector>
#include <cstdint>
#include <unordered_map>

namespace berender {

class VulkanBackend : public RenderBackend {
public:
    VulkanBackend(); ~VulkanBackend() override;
    Result init(void* nativeWindow) override;
    void shutdown() override;
    const BackendCapabilities& capabilities() const override { return m_caps; }
    Result beginFrame() override;
    Result endFrame() override;
    BufferHandle createBuffer(uint64_t size,BufferUsage usage,MemoryHint hint) override;
    void destroyBuffer(BufferHandle handle) override;
    void* mapBuffer(BufferHandle handle,uint64_t offset,uint64_t size) override;
    void unmapBuffer(BufferHandle handle) override;
    void updateBuffer(BufferHandle handle,uint64_t offset,const void* data,uint64_t size) override;
    TextureHandle createTexture(uint32_t w,uint32_t h,uint32_t ch,const void* data) override;
    void destroyTexture(TextureHandle handle) override;
    PipelineHandle createPipeline(RenderBucket bucket,const Material* material) override;
    void destroyPipeline(PipelineHandle handle) override;
    void bindGlobalBuffers(BufferHandle vb,BufferHandle ib) override;
    void bindCameraUniform(BufferHandle ubo) override;
    void bindMaterialStorage(BufferHandle ssbo) override;
    void bindSectionStorage(BufferHandle ssbo) override;
    void bindPipeline(PipelineHandle pipeline) override;
    void drawIndexedIndirect(BufferHandle indirect,uint32_t offset,uint32_t count,uint32_t stride) override;
    void drawIndexedIndirectFallback(BufferHandle indirect,uint32_t offset,uint32_t count,uint32_t stride) override;
    void drawIndexed(uint32_t ic,uint32_t inst,uint32_t first,int32_t voff,uint32_t finst) override;
    void setViewport(const Viewport& vp) override;
    void setScissor(const ScissorRect& rect) override;
    void bufferMemoryBarrier(BufferHandle handle,uint64_t offset,uint64_t size) override;
    BackendType type() const override { return BackendType::Vulkan; }
private:
#ifdef BERENDER_HAS_VULKAN
    VkInstance m_instance; VkPhysicalDevice m_physicalDevice; VkDevice m_device;
    VkQueue m_graphicsQueue; uint32_t m_graphicsQueueFamily;
    VkCommandPool m_commandPool; VkCommandBuffer m_commandBuffer;
    VkSurfaceKHR m_surface; VkSwapchainKHR m_swapchain; VkFormat m_swapchainFormat;
    VkExtent2D m_swapchainExtent; std::vector<VkImage> m_swapchainImages;
    std::vector<VkImageView> m_swapchainImageViews; std::vector<VkFramebuffer> m_framebuffers;
    VkRenderPass m_renderPass;
    VkSemaphore m_imageAvailableSemaphore, m_renderFinishedSemaphore; VkFence m_inFlightFence;
    uint32_t m_currentImageIndex;
    VkDescriptorPool m_descriptorPool; VkDescriptorSetLayout m_descriptorSetLayout; VkDescriptorSet m_descriptorSet;
    VkPipelineCache m_pipelineCache; bool m_useDynamicRendering;
    struct BufferResource { VkBuffer buffer; VkDeviceMemory memory; uint64_t size; BufferUsage usage; MemoryHint hint; bool mapped; void* mappedPtr; };
    struct TextureResource { VkImage image; VkDeviceMemory memory; VkImageView view; VkSampler sampler; uint32_t w,h; };
    struct PipelineResource { VkPipeline pipeline; VkPipelineLayout layout; RenderBucket bucket; };
    std::unordered_map<uint64_t,BufferResource> m_buffers;
    std::unordered_map<uint64_t,TextureResource> m_textures;
    std::unordered_map<uint64_t,PipelineResource> m_pipelines;
    uint64_t m_nextBufferId, m_nextTextureId, m_nextPipelineId;
    Result createInstance(), pickPhysicalDevice(), createLogicalDevice(), createCommandPool();
    Result createSwapchain(void*), createRenderPass(), createFramebuffers();
    Result createDescriptorSetLayout(), createDescriptorPool(), allocateDescriptorSet();
    Result createSyncObjects(), createPipelineCache();
    uint32_t findMemoryType(uint32_t filter,VkMemoryPropertyFlags props);
    void beginRenderPass(), endRenderPass();
    bool checkValidationLayerSupport(), checkDeviceExtensionSupport(VkPhysicalDevice);
#endif
    BackendCapabilities m_caps; bool m_initialized, m_frameStarted;
};

} // namespace berender
