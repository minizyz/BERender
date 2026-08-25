#include "berender/backend/vulkan_backend.h"
#include "berender/core/logger.h"
#include "berender/core/math.h"
#ifdef BERENDER_HAS_VULKAN
#include <vulkan/vulkan.h>
#include <cstring>
#include <algorithm>
#include <vector>
#include <set>
#include <limits>
namespace berender {
namespace {
constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
const std::vector<const char*> VALIDATION_LAYERS = { "VK_LAYER_KHRONOS_validation" };
const std::vector<const char*> DEVICE_EXTENSIONS = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
};
#ifdef NDEBUG
constexpr bool ENABLE_VALIDATION_LAYERS = false;
#else
constexpr bool ENABLE_VALIDATION_LAYERS = true;
#endif
}
VulkanBackend::VulkanBackend()
    : m_instance(VK_NULL_HANDLE), m_physicalDevice(VK_NULL_HANDLE), m_device(VK_NULL_HANDLE)
    , m_graphicsQueue(VK_NULL_HANDLE), m_graphicsQueueFamily(0), m_commandPool(VK_NULL_HANDLE)
    , m_commandBuffer(VK_NULL_HANDLE), m_surface(VK_NULL_HANDLE), m_swapchain(VK_NULL_HANDLE)
    , m_swapchainFormat(VK_FORMAT_UNDEFINED), m_renderPass(VK_NULL_HANDLE)
    , m_imageAvailableSemaphore(VK_NULL_HANDLE), m_renderFinishedSemaphore(VK_NULL_HANDLE)
    , m_inFlightFence(VK_NULL_HANDLE), m_currentImageIndex(0), m_descriptorPool(VK_NULL_HANDLE)
    , m_descriptorSetLayout(VK_NULL_HANDLE), m_descriptorSet(VK_NULL_HANDLE)
    , m_pipelineCache(VK_NULL_HANDLE), m_useDynamicRendering(false)
    , m_nextBufferId(1), m_nextTextureId(1), m_nextPipelineId(1)
    , m_initialized(false), m_frameStarted(false) {
    std::memset(&m_caps, 0, sizeof(m_caps));
    m_caps.type = BackendType::Vulkan;
    m_swapchainExtent = {0, 0};
}
VulkanBackend::~VulkanBackend() { shutdown(); }
Result VulkanBackend::init(void* nativeWindow) {
    BERENDER_INFO("VulkanBackend::init");
    Result r;
    r = createInstance(); if (r != Result::Success) return r;
    r = pickPhysicalDevice(); if (r != Result::Success) return r;
    r = createLogicalDevice(); if (r != Result::Success) return r;
    r = createCommandPool(); if (r != Result::Success) return r;
    r = createDescriptorSetLayout(); if (r != Result::Success) return r;
    r = createDescriptorPool(); if (r != Result::Success) return r;
    r = allocateDescriptorSet(); if (r != Result::Success) return r;
    r = createSyncObjects(); if (r != Result::Success) return r;
    r = createPipelineCache(); if (r != Result::Success) return r;
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(m_device, &allocInfo, &m_commandBuffer) != VK_SUCCESS) {
        BERENDER_ERROR("Failed to allocate command buffer");
        return Result::ErrorInitializationFailed;
    }
    m_initialized = true;
    BERENDER_INFO("VulkanBackend initialized (device: %s)", m_caps.deviceName);
    return Result::Success;
}
void VulkanBackend::shutdown() {
    if (!m_initialized) return;
    BERENDER_INFO("VulkanBackend::shutdown");
    vkDeviceWaitIdle(m_device);
    for (auto& [id, res] : m_buffers) { vkDestroyBuffer(m_device, res.buffer, nullptr); vkFreeMemory(m_device, res.memory, nullptr); }
    m_buffers.clear();
    for (auto& [id, res] : m_textures) { vkDestroyImageView(m_device, res.view, nullptr); vkDestroyImage(m_device, res.image, nullptr); vkFreeMemory(m_device, res.memory, nullptr); vkDestroySampler(m_device, res.sampler, nullptr); }
    m_textures.clear();
    for (auto& [id, res] : m_pipelines) { vkDestroyPipeline(m_device, res.pipeline, nullptr); vkDestroyPipelineLayout(m_device, res.layout, nullptr); }
    m_pipelines.clear();
    if (m_pipelineCache) vkDestroyPipelineCache(m_device, m_pipelineCache, nullptr);
    if (m_descriptorSet) vkFreeDescriptorSets(m_device, m_descriptorPool, 1, &m_descriptorSet);
    if (m_descriptorPool) vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
    if (m_descriptorSetLayout) vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
    if (m_inFlightFence) vkDestroyFence(m_device, m_inFlightFence, nullptr);
    if (m_renderFinishedSemaphore) vkDestroySemaphore(m_device, m_renderFinishedSemaphore, nullptr);
    if (m_imageAvailableSemaphore) vkDestroySemaphore(m_device, m_imageAvailableSemaphore, nullptr);
    for (auto& fb : m_framebuffers) vkDestroyFramebuffer(m_device, fb, nullptr);
    m_framebuffers.clear();
    if (m_renderPass) vkDestroyRenderPass(m_device, m_renderPass, nullptr);
    for (auto& iv : m_swapchainImageViews) vkDestroyImageView(m_device, iv, nullptr);
    m_swapchainImageViews.clear();
    if (m_swapchain) vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
    if (m_commandPool) vkDestroyCommandPool(m_device, m_commandPool, nullptr);
    if (m_device) vkDestroyDevice(m_device, nullptr);
    if (m_surface) vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    if (m_instance) vkDestroyInstance(m_instance, nullptr);
    m_initialized = false; m_frameStarted = false;
}
Result VulkanBackend::createInstance() {
    if (ENABLE_VALIDATION_LAYERS && !checkValidationLayerSupport()) {
        BERENDER_ERROR("Validation layers not available"); return Result::ErrorUnsupported;
    }
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "BERender"; appInfo.applicationVersion = VK_MAKE_VERSION(0,1,0);
    appInfo.pEngineName = "BERender Engine"; appInfo.engineVersion = VK_MAKE_VERSION(0,1,0);
    appInfo.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    std::vector<const char*> extensions;
#ifdef BERENDER_PLATFORM_ANDROID
    extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME); extensions.push_back("VK_KHR_android_surface");
#elif defined(BERENDER_PLATFORM_WINDOWS)
    extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME); extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#else
    extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME); extensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#endif
    createInfo.enabledExtensionCount = (uint32_t)extensions.size();
    createInfo.ppEnabledExtensionNames = extensions.data();
    if (ENABLE_VALIDATION_LAYERS) {
        createInfo.enabledLayerCount = (uint32_t)VALIDATION_LAYERS.size();
        createInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();
    }
    if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS) {
        BERENDER_ERROR("Failed to create Vulkan instance"); return Result::ErrorInitializationFailed;
    }
    BERENDER_INFO("Vulkan instance created"); return Result::Success;
}
bool VulkanBackend::checkValidationLayerSupport() {
    uint32_t layerCount; vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
    for (const char* layerName : VALIDATION_LAYERS) {
        bool found = false;
        for (const auto& layer : availableLayers) if (std::strcmp(layer.layerName, layerName) == 0) { found = true; break; }
        if (!found) return false;
    }
    return true;
}
bool VulkanBackend::checkDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionCount; vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());
    std::set<std::string> required(DEVICE_EXTENSIONS.begin(), DEVICE_EXTENSIONS.end());
    for (const auto& ext : availableExtensions) required.erase(ext.extensionName);
    return required.empty();
}
Result VulkanBackend::pickPhysicalDevice() {
    uint32_t deviceCount = 0; vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
    if (deviceCount == 0) { BERENDER_ERROR("No Vulkan GPUs"); return Result::ErrorUnsupported; }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());
    for (const auto& device : devices) {
        VkPhysicalDeviceProperties props; vkGetPhysicalDeviceProperties(device, &props);
        VkPhysicalDeviceFeatures features; vkGetPhysicalDeviceFeatures(device, &features);
        if (!features.multiDrawIndirect) BERENDER_WARN("Device '%s' no MDI support", props.deviceName);
        if (!checkDeviceExtensionSupport(device)) continue;
        uint32_t qfCount = 0; vkGetPhysicalDeviceQueueFamilyProperties(device, &qfCount, nullptr);
        std::vector<VkQueueFamilyProperties> qfs(qfCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &qfCount, qfs.data());
        bool found = false;
        for (uint32_t i = 0; i < qfCount; ++i) if (qfs[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) { m_graphicsQueueFamily = i; found = true; break; }
        if (!found) continue;
        m_physicalDevice = device;
        std::strncpy(m_caps.deviceName, props.deviceName, sizeof(m_caps.deviceName)-1);
        m_caps.multiDrawIndirect = features.multiDrawIndirect;
        m_caps.multiDrawIndirectCount = features.multiDrawIndirect;
        m_caps.storageBuffer = true; m_caps.dynamicRendering = true;
        m_caps.deviceGeneratedCommands = false;
        VkPhysicalDeviceLimits limits = props.limits;
        m_caps.maxTextureSize = limits.maxImageDimension2D;
        m_caps.maxUniformBufferRange = limits.maxUniformBufferRange;
        m_caps.maxStorageBufferRange = limits.maxStorageBufferRange;
        m_caps.minIndirectBufferOffsetAlignment = limits.minIndirectBufferOffsetAlignment;
        if (m_caps.minIndirectBufferOffsetAlignment == 0) m_caps.minIndirectBufferOffsetAlignment = 4;
        BERENDER_INFO("Selected: '%s' (MDI:%s API:%d.%d.%d)", props.deviceName,
            features.multiDrawIndirect?"yes":"no(fallback)",
            VK_VERSION_MAJOR(props.apiVersion),VK_VERSION_MINOR(props.apiVersion),VK_VERSION_PATCH(props.apiVersion));
        return Result::Success;
    }
    BERENDER_ERROR("No suitable Vulkan device"); return Result::ErrorUnsupported;
}
Result VulkanBackend::createLogicalDevice() {
    float qp = 1.0f;
    VkDeviceQueueCreateInfo qci = {}; qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = m_graphicsQueueFamily; qci.queueCount = 1; qci.pQueuePriorities = &qp;
    VkPhysicalDeviceFeatures df = {}; df.multiDrawIndirect = VK_TRUE;
    VkDeviceCreateInfo ci = {}; ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    ci.pQueueCreateInfos = &qci; ci.queueCreateInfoCount = 1; ci.pEnabledFeatures = &df;
    ci.enabledExtensionCount = (uint32_t)DEVICE_EXTENSIONS.size(); ci.ppEnabledExtensionNames = DEVICE_EXTENSIONS.data();
    if (ENABLE_VALIDATION_LAYERS) { ci.enabledLayerCount = (uint32_t)VALIDATION_LAYERS.size(); ci.ppEnabledLayerNames = VALIDATION_LAYERS.data(); }
    if (vkCreateDevice(m_physicalDevice, &ci, nullptr, &m_device) != VK_SUCCESS) {
        BERENDER_ERROR("Failed to create logical device"); return Result::ErrorInitializationFailed;
    }
    vkGetDeviceQueue(m_device, m_graphicsQueueFamily, 0, &m_graphicsQueue);
    BERENDER_INFO("Logical device created"); return Result::Success;
}
Result VulkanBackend::createCommandPool() {
    VkCommandPoolCreateInfo pi = {}; pi.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pi.queueFamilyIndex = m_graphicsQueueFamily; pi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(m_device, &pi, nullptr, &m_commandPool) != VK_SUCCESS) {
        BERENDER_ERROR("Failed to create command pool"); return Result::ErrorInitializationFailed;
    }
    return Result::Success;
}
Result VulkanBackend::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding bindings[3] = {};
    bindings[0].binding=0; bindings[0].descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; bindings[0].descriptorCount=1; bindings[0].stageFlags=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[1].binding=1; bindings[1].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; bindings[1].descriptorCount=1; bindings[1].stageFlags=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[2].binding=2; bindings[2].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; bindings[2].descriptorCount=1; bindings[2].stageFlags=VK_SHADER_STAGE_VERTEX_BIT;
    VkDescriptorSetLayoutCreateInfo li = {}; li.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    li.bindingCount=3; li.pBindings=bindings;
    if (vkCreateDescriptorSetLayout(m_device, &li, nullptr, &m_descriptorSetLayout) != VK_SUCCESS) {
        BERENDER_ERROR("Failed to create descriptor set layout"); return Result::ErrorInitializationFailed;
    }
    return Result::Success;
}
Result VulkanBackend::createDescriptorPool() {
    VkDescriptorPoolSize ps[3] = {};
    ps[0].type=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; ps[0].descriptorCount=10;
    ps[1].type=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; ps[1].descriptorCount=20;
    ps[2].type=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; ps[2].descriptorCount=10;
    VkDescriptorPoolCreateInfo pi = {}; pi.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pi.poolSizeCount=3; pi.pPoolSizes=ps; pi.maxSets=10;
    if (vkCreateDescriptorPool(m_device, &pi, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        BERENDER_ERROR("Failed to create descriptor pool"); return Result::ErrorInitializationFailed;
    }
    return Result::Success;
}
Result VulkanBackend::allocateDescriptorSet() {
    VkDescriptorSetAllocateInfo ai = {}; ai.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool=m_descriptorPool; ai.descriptorSetCount=1; ai.pSetLayouts=&m_descriptorSetLayout;
    if (vkAllocateDescriptorSets(m_device, &ai, &m_descriptorSet) != VK_SUCCESS) {
        BERENDER_ERROR("Failed to allocate descriptor set"); return Result::ErrorInitializationFailed;
    }
    return Result::Success;
}
Result VulkanBackend::createSyncObjects() {
    VkSemaphoreCreateInfo si = {}; si.sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fi = {}; fi.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO; fi.flags=VK_FENCE_CREATE_SIGNALED_BIT;
    if (vkCreateSemaphore(m_device,&si,nullptr,&m_imageAvailableSemaphore)!=VK_SUCCESS ||
        vkCreateSemaphore(m_device,&si,nullptr,&m_renderFinishedSemaphore)!=VK_SUCCESS ||
        vkCreateFence(m_device,&fi,nullptr,&m_inFlightFence)!=VK_SUCCESS) {
        BERENDER_ERROR("Failed to create sync objects"); return Result::ErrorInitializationFailed;
    }
    return Result::Success;
}
Result VulkanBackend::createPipelineCache() {
    VkPipelineCacheCreateInfo ci = {}; ci.sType=VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    if (vkCreatePipelineCache(m_device,&ci,nullptr,&m_pipelineCache)!=VK_SUCCESS) {
        BERENDER_ERROR("Failed to create pipeline cache"); return Result::ErrorInitializationFailed;
    }
    return Result::Success;
}
uint32_t VulkanBackend::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties mp; vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
        if ((typeFilter & (1<<i)) && (mp.memoryTypes[i].propertyFlags & properties) == properties) return i;
    BERENDER_ERROR("No suitable memory type"); return 0;
}
Result VulkanBackend::beginFrame() {
    if (!m_initialized) return Result::ErrorInitializationFailed;
    vkWaitForFences(m_device, 1, &m_inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(m_device, 1, &m_inFlightFence);
    vkResetCommandBuffer(m_commandBuffer, 0);
    VkCommandBufferBeginInfo bi = {}; bi.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(m_commandBuffer, &bi) != VK_SUCCESS) {
        BERENDER_ERROR("Failed to begin command buffer"); return Result::ErrorUnknown;
    }
    m_frameStarted = true; return Result::Success;
}
Result VulkanBackend::endFrame() {
    if (!m_initialized || !m_frameStarted) return Result::ErrorUnknown;
    if (vkEndCommandBuffer(m_commandBuffer) != VK_SUCCESS) { BERENDER_ERROR("Failed to record command buffer"); return Result::ErrorUnknown; }
    VkSubmitInfo si = {}; si.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkSemaphore waitSem[] = {m_imageAvailableSemaphore};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    si.waitSemaphoreCount=1; si.pWaitSemaphores=waitSem; si.pWaitDstStageMask=waitStages;
    si.commandBufferCount=1; si.pCommandBuffers=&m_commandBuffer;
    VkSemaphore signalSem[] = {m_renderFinishedSemaphore};
    si.signalSemaphoreCount=1; si.pSignalSemaphores=signalSem;
    if (vkQueueSubmit(m_graphicsQueue, 1, &si, m_inFlightFence) != VK_SUCCESS) {
        BERENDER_ERROR("Failed to submit"); return Result::ErrorDeviceLost;
    }
    m_frameStarted = false; return Result::Success;
}
BufferHandle VulkanBackend::createBuffer(uint64_t size, BufferUsage usage, MemoryHint memoryHint) {
    VkBufferCreateInfo bi = {}; bi.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO; bi.size=size; bi.usage=0;
    if((uint32_t)usage&(uint32_t)BufferUsage::Vertex)bi.usage|=VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if((uint32_t)usage&(uint32_t)BufferUsage::Index)bi.usage|=VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if((uint32_t)usage&(uint32_t)BufferUsage::Uniform)bi.usage|=VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if((uint32_t)usage&(uint32_t)BufferUsage::Storage)bi.usage|=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if((uint32_t)usage&(uint32_t)BufferUsage::Indirect)bi.usage|=VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    if((uint32_t)usage&(uint32_t)BufferUsage::TransferSrc)bi.usage|=VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if((uint32_t)usage&(uint32_t)BufferUsage::TransferDst)bi.usage|=VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bi.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    VkBuffer buffer; if(vkCreateBuffer(m_device,&bi,nullptr,&buffer)!=VK_SUCCESS){BERENDER_ERROR("Create buffer failed");return BufferHandle::invalid();}
    VkMemoryRequirements mr; vkGetBufferMemoryRequirements(m_device,buffer,&mr);
    VkMemoryPropertyFlags mp=0;
    switch(memoryHint){case MemoryHint::DeviceLocal:mp=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;break;case MemoryHint::HostVisible:mp=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;break;case MemoryHint::HostCached:mp=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_CACHED_BIT;break;case MemoryHint::Staging:mp=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;break;}
    VkMemoryAllocateInfo ai={};ai.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;ai.allocationSize=mr.size;ai.memoryTypeIndex=findMemoryType(mr.memoryTypeBits,mp);
    VkDeviceMemory mem; if(vkAllocateMemory(m_device,&ai,nullptr,&mem)!=VK_SUCCESS){vkDestroyBuffer(m_device,buffer,nullptr);return BufferHandle::invalid();}
    vkBindBufferMemory(m_device,buffer,mem,0);
    uint64_t id=m_nextBufferId++; m_buffers[id]={buffer,mem,size,usage,memoryHint,false,nullptr}; return {id};
}
void VulkanBackend::destroyBuffer(BufferHandle handle){auto it=m_buffers.find(handle.id);if(it==m_buffers.end())return;if(it->second.mapped)vkUnmapMemory(m_device,it->second.memory);vkDestroyBuffer(m_device,it->second.buffer,nullptr);vkFreeMemory(m_device,it->second.memory,nullptr);m_buffers.erase(it);}
void* VulkanBackend::mapBuffer(BufferHandle handle,uint64_t offset,uint64_t size){auto it=m_buffers.find(handle.id);if(it==m_buffers.end())return nullptr;if(it->second.mapped)return it->second.mappedPtr;void* data;if(vkMapMemory(m_device,it->second.memory,offset,size,0,&data)!=VK_SUCCESS)return nullptr;it->second.mapped=true;it->second.mappedPtr=data;return data;}
void VulkanBackend::unmapBuffer(BufferHandle handle){auto it=m_buffers.find(handle.id);if(it==m_buffers.end()||!it->second.mapped)return;vkUnmapMemory(m_device,it->second.memory);it->second.mapped=false;it->second.mappedPtr=nullptr;}
void VulkanBackend::updateBuffer(BufferHandle handle,uint64_t offset,const void* data,uint64_t size){void* m=mapBuffer(handle,offset,size);if(m){memcpy(m,data,size);unmapBuffer(handle);}}
TextureHandle VulkanBackend::createTexture(uint32_t w,uint32_t h,uint32_t ch,const void* data){(void)data;(void)ch;
    VkImageCreateInfo ii={};ii.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;ii.imageType=VK_IMAGE_TYPE_2D;ii.extent.width=w;ii.extent.height=h;ii.extent.depth=1;ii.mipLevels=1;ii.arrayLayers=1;ii.format=VK_FORMAT_R8G8B8A8_UNORM;ii.tiling=VK_IMAGE_TILING_OPTIMAL;ii.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;ii.usage=VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;ii.sharingMode=VK_SHARING_MODE_EXCLUSIVE;ii.samples=VK_SAMPLE_COUNT_1_BIT;
    VkImage image;if(vkCreateImage(m_device,&ii,nullptr,&image)!=VK_SUCCESS)return TextureHandle::invalid();
    VkMemoryRequirements mr;vkGetImageMemoryRequirements(m_device,image,&mr);
    VkMemoryAllocateInfo ai={};ai.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;ai.allocationSize=mr.size;ai.memoryTypeIndex=findMemoryType(mr.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VkDeviceMemory mem;if(vkAllocateMemory(m_device,&ai,nullptr,&mem)!=VK_SUCCESS){vkDestroyImage(m_device,image,nullptr);return TextureHandle::invalid();}
    vkBindImageMemory(m_device,image,mem,0);
    VkImageViewCreateInfo vi={};vi.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;vi.image=image;vi.viewType=VK_IMAGE_VIEW_TYPE_2D;vi.format=VK_FORMAT_R8G8B8A8_UNORM;vi.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;vi.subresourceRange.baseMipLevel=0;vi.subresourceRange.levelCount=1;vi.subresourceRange.baseArrayLayer=0;vi.subresourceRange.layerCount=1;
    VkImageView view;if(vkCreateImageView(m_device,&vi,nullptr,&view)!=VK_SUCCESS){vkDestroyImage(m_device,image,nullptr);vkFreeMemory(m_device,mem,nullptr);return TextureHandle::invalid();}
    VkSamplerCreateInfo si={};si.sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;si.magFilter=VK_FILTER_NEAREST;si.minFilter=VK_FILTER_NEAREST;si.addressModeU=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;si.addressModeV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;si.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;si.anisotropyEnable=VK_FALSE;si.borderColor=VK_BORDER_COLOR_INT_OPAQUE_BLACK;si.unnormalizedCoordinates=VK_FALSE;si.compareEnable=VK_FALSE;si.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;
    VkSampler sampler;vkCreateSampler(m_device,&si,nullptr,&sampler);
    uint64_t id=m_nextTextureId++;m_textures[id]={image,mem,view,sampler,w,h};return {id};
}
void VulkanBackend::destroyTexture(TextureHandle handle){auto it=m_textures.find(handle.id);if(it==m_textures.end())return;vkDestroySampler(m_device,it->second.sampler,nullptr);vkDestroyImageView(m_device,it->second.view,nullptr);vkDestroyImage(m_device,it->second.image,nullptr);vkFreeMemory(m_device,it->second.memory,nullptr);m_textures.erase(it);}
PipelineHandle VulkanBackend::createPipeline(RenderBucket bucket,const Material* material){(void)material;
    VkPipelineLayoutCreateInfo li={};li.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;li.setLayoutCount=1;li.pSetLayouts=&m_descriptorSetLayout;
    VkPipelineLayout layout;if(vkCreatePipelineLayout(m_device,&li,nullptr,&layout)!=VK_SUCCESS)return PipelineHandle::invalid();
    VkPipeline pipeline=VK_NULL_HANDLE;
    uint64_t id=m_nextPipelineId++;m_pipelines[id]={pipeline,layout,bucket};return {id};
}
void VulkanBackend::destroyPipeline(PipelineHandle handle){auto it=m_pipelines.find(handle.id);if(it==m_pipelines.end())return;if(it->second.pipeline)vkDestroyPipeline(m_device,it->second.pipeline,nullptr);vkDestroyPipelineLayout(m_device,it->second.layout,nullptr);m_pipelines.erase(it);}
void VulkanBackend::bindGlobalBuffers(BufferHandle vb,BufferHandle ib){auto vit=m_buffers.find(vb.id),iit=m_buffers.find(ib.id);if(vit==m_buffers.end()||iit==m_buffers.end())return;VkBuffer vbs[]={vit->second.buffer};VkDeviceSize offs[]={0};vkCmdBindVertexBuffers(m_commandBuffer,0,1,vbs,offs);vkCmdBindIndexBuffer(m_commandBuffer,iit->second.buffer,0,VK_INDEX_TYPE_UINT16);}
void VulkanBackend::bindCameraUniform(BufferHandle ubo){auto it=m_buffers.find(ubo.id);if(it==m_buffers.end())return;VkDescriptorBufferInfo bi={};bi.buffer=it->second.buffer;bi.offset=0;bi.range=it->second.size;VkWriteDescriptorSet w={};w.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;w.dstSet=m_descriptorSet;w.dstBinding=0;w.descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;w.descriptorCount=1;w.pBufferInfo=&bi;vkUpdateDescriptorSets(m_device,1,&w,0,nullptr);vkCmdBindDescriptorSets(m_commandBuffer,VK_PIPELINE_BIND_POINT_GRAPHICS,m_pipelines.begin()->second.layout,0,1,&m_descriptorSet,0,nullptr);}
void VulkanBackend::bindMaterialStorage(BufferHandle ssbo){auto it=m_buffers.find(ssbo.id);if(it==m_buffers.end())return;VkDescriptorBufferInfo bi={};bi.buffer=it->second.buffer;bi.offset=0;bi.range=it->second.size;VkWriteDescriptorSet w={};w.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;w.dstSet=m_descriptorSet;w.dstBinding=1;w.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;w.descriptorCount=1;w.pBufferInfo=&bi;vkUpdateDescriptorSets(m_device,1,&w,0,nullptr);}
void VulkanBackend::bindSectionStorage(BufferHandle ssbo){auto it=m_buffers.find(ssbo.id);if(it==m_buffers.end())return;VkDescriptorBufferInfo bi={};bi.buffer=it->second.buffer;bi.offset=0;bi.range=it->second.size;VkWriteDescriptorSet w={};w.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;w.dstSet=m_descriptorSet;w.dstBinding=2;w.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;w.descriptorCount=1;w.pBufferInfo=&bi;vkUpdateDescriptorSets(m_device,1,&w,0,nullptr);}
void VulkanBackend::bindPipeline(PipelineHandle pipeline){auto it=m_pipelines.find(pipeline.id);if(it==m_pipelines.end()||!it->second.pipeline)return;vkCmdBindPipeline(m_commandBuffer,VK_PIPELINE_BIND_POINT_GRAPHICS,it->second.pipeline);}
void VulkanBackend::drawIndexedIndirect(BufferHandle indirect,uint32_t offset,uint32_t count,uint32_t stride){
    auto it=m_buffers.find(indirect.id);if(it==m_buffers.end())return;
    vkCmdDrawIndexedIndirect(m_commandBuffer,it->second.buffer,offset,count,stride);
    BERENDER_DEBUG("vkCmdDrawIndexedIndirect: offset=%u count=%u stride=%u",offset,count,stride);
}
void VulkanBackend::drawIndexedIndirectFallback(BufferHandle indirect,uint32_t offset,uint32_t count,uint32_t stride){
    auto it=m_buffers.find(indirect.id);if(it==m_buffers.end())return;
    for(uint32_t i=0;i<count;i++)vkCmdDrawIndexedIndirect(m_commandBuffer,it->second.buffer,offset+i*stride,1,stride);
    BERENDER_DEBUG("Fallback: %u individual indirect draws",count);
}
void VulkanBackend::drawIndexed(uint32_t ic,uint32_t inst,uint32_t first,int32_t voff,uint32_t finst){vkCmdDrawIndexed(m_commandBuffer,ic,inst,first,voff,finst);}
void VulkanBackend::setViewport(const Viewport& vp){VkViewport v={};v.x=vp.x;v.y=vp.y;v.width=vp.width;v.height=vp.height;v.minDepth=vp.minDepth;v.maxDepth=vp.maxDepth;vkCmdSetViewport(m_commandBuffer,0,1,&v);}
void VulkanBackend::setScissor(const ScissorRect& r){VkRect2D s={};s.offset.x=r.x;s.offset.y=r.y;s.extent.width=r.width;s.extent.height=r.height;vkCmdSetScissor(m_commandBuffer,0,1,&s);}
void VulkanBackend::bufferMemoryBarrier(BufferHandle handle,uint64_t offset,uint64_t size){auto it=m_buffers.find(handle.id);if(it==m_buffers.end())return;VkBufferMemoryBarrier b={};b.sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;b.srcAccessMask=VK_ACCESS_INDIRECT_COMMAND_READ_BIT;b.dstAccessMask=VK_ACCESS_INDIRECT_COMMAND_READ_BIT;b.buffer=it->second.buffer;b.offset=offset;b.size=size;vkCmdPipelineBarrier(m_commandBuffer,VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,0,0,nullptr,1,&b,0,nullptr);}
Result VulkanBackend::createSwapchain(void* nw){(void)nw;return Result::Success;}
Result VulkanBackend::createRenderPass(){return Result::Success;}
Result VulkanBackend::createFramebuffers(){return Result::Success;}
void VulkanBackend::beginRenderPass(){}
void VulkanBackend::endRenderPass(){}
VkFormat VulkanBackend::findSupportedFormat(const std::vector<VkFormat>& c,VkImageTiling t,VkFormatFeatureFlags f){for(VkFormat fmt:c){VkFormatProperties p;vkGetPhysicalDeviceFormatProperties(m_physicalDevice,fmt,&p);if(t==VK_IMAGE_TILING_LINEAR&&(p.linearTilingFeatures&f)==f)return fmt;else if(t==VK_IMAGE_TILING_OPTIMAL&&(p.optimalTilingFeatures&f)==f)return fmt;}return VK_FORMAT_UNDEFINED;}
void VulkanBackend::transitionImageLayout(VkImage image,VkFormat format,VkImageLayout old,VkImageLayout newL){(void)format;(void)image;(void)old;(void)newL;}
void VulkanBackend::copyBufferToImage(VkBuffer buffer,VkImage image,uint32_t w,uint32_t h){(void)buffer;(void)image;(void)w;(void)h;}
} // namespace berender
#else
namespace berender {
VulkanBackend::VulkanBackend():m_initialized(false),m_frameStarted(false){memset(&m_caps,0,sizeof(m_caps));m_caps.type=BackendType::Vulkan;}
VulkanBackend::~VulkanBackend()=default;
Result VulkanBackend::init(void*){BERENDER_ERROR("Vulkan not compiled");return Result::ErrorUnsupported;}
void VulkanBackend::shutdown(){}
const BackendCapabilities& VulkanBackend::capabilities() const{return m_caps;}
Result VulkanBackend::beginFrame(){return Result::ErrorUnsupported;}
Result VulkanBackend::endFrame(){return Result::ErrorUnsupported;}
BufferHandle VulkanBackend::createBuffer(uint64_t,BufferUsage,MemoryHint){return BufferHandle::invalid();}
void VulkanBackend::destroyBuffer(BufferHandle){}
void* VulkanBackend::mapBuffer(BufferHandle,uint64_t,uint64_t){return nullptr;}
void VulkanBackend::unmapBuffer(BufferHandle){}
void VulkanBackend::updateBuffer(BufferHandle,uint64_t,const void*,uint64_t){}
TextureHandle VulkanBackend::createTexture(uint32_t,uint32_t,uint32_t,const void*){return TextureHandle::invalid();}
void VulkanBackend::destroyTexture(TextureHandle){}
PipelineHandle VulkanBackend::createPipeline(RenderBucket,const Material*){return PipelineHandle::invalid();}
void VulkanBackend::destroyPipeline(PipelineHandle){}
void VulkanBackend::bindGlobalBuffers(BufferHandle,BufferHandle){}
void VulkanBackend::bindCameraUniform(BufferHandle){}
void VulkanBackend::bindMaterialStorage(BufferHandle){}
void VulkanBackend::bindSectionStorage(BufferHandle){}
void VulkanBackend::bindPipeline(PipelineHandle){}
void VulkanBackend::drawIndexedIndirect(BufferHandle,uint32_t,uint32_t,uint32_t){}
void VulkanBackend::drawIndexedIndirectFallback(BufferHandle,uint32_t,uint32_t,uint32_t){}
void VulkanBackend::drawIndexed(uint32_t,uint32_t,uint32_t,int32_t,uint32_t){}
void VulkanBackend::setViewport(const Viewport&){}
void VulkanBackend::setScissor(const ScissorRect&){}
void VulkanBackend::bufferMemoryBarrier(BufferHandle,uint64_t,uint64_t){}
}
#endif
