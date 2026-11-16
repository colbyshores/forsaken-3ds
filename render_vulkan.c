/*
 * render_vulkan.c - Vulkan renderer for the Linux PC build.
 *
 * Replaces the old OpenGL backends (render_gl1/2/3 + shared) behind
 * the same FS interface that render_c3d.c implements on 3DS. One
 * vertex/fragment shader pair mirrors the old GL shaders (BGRA vertex
 * colours, colour-key discard, textureGrad), with a Y-flip in the
 * vertex shader to bridge GL-style bottom-up NDC to Vulkan's
 * top-down framebuffer. Engine projection matrices are already
 * D3D-style [0,1] depth, which matches Vulkan.
 *
 * Textures: opaque masters default to ETC2 - a build-time-encoded
 * .ktx2 sibling (tools/texture/etc2enc.c, run by the PC Makefile)
 * uploaded as VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK when the device
 * exposes textureCompressionETC2. Alpha-bearing textures never get
 * a .ktx2 (encoder skips them) and load RGBA from PNG - the
 * designated RGBA exception class. If ETC2 is unavailable, or no
 * .ktx2 exists, the PNG/RGBA path is used.
 *
 * Frame model deliberately simple: FSBeginScene acquires and opens
 * the single command buffer, render_flip submits and presents;
 * buffer/texture uploads use one-time command buffers with a queue
 * wait. vkQueueWaitIdle per frame keeps ownership trivially correct.
 */
#include "main.h"
#include "util.h"
#include "render.h"
#include "texture.h"
#include "file.h"
#include "main_sdl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>
#include <SDL.h>
#include <SDL_vulkan.h>

#include "vulkan_shaders.h" /* generated: vulkan3d_vert_spv, vulkan2d_vert_spv, vulkan_frag_spv */

extern render_info_t render_info;
bool bSquareOnly = true;

typedef struct vkbuf {
    VkBuffer buf;
    VkDeviceMemory mem;
    void *mapped;
    VkDeviceSize size;
} vkbuf_t;

typedef struct vktex {
    VkImage image;
    VkDeviceMemory mem;
    VkImageView view;
    VkDescriptorSet set;
    int w, h;
    bool colorkey;
} vktex_t;

static VkInstance       s_inst;
static VkSurfaceKHR     s_surface;
static VkPhysicalDevice s_phys;
static VkDevice         s_dev;
static VkQueue          s_queue;
static uint32_t         s_qfam;
static VkSwapchainKHR   s_swap;
static VkFormat         s_swap_fmt;
static VkExtent2D       s_extent;
static VkImage         *s_swap_imgs;
static VkImageView     *s_swap_views;
static VkFramebuffer   *s_fbs;
static uint32_t         s_swap_count;
static VkImage          s_depth_img;
static VkDeviceMemory   s_depth_mem;
static VkImageView      s_depth_view;
static VkRenderPass     s_pass;
static VkCommandPool    s_pool;
static VkCommandBuffer  s_cmd;
static VkSemaphore      s_img_avail, s_done;
static VkFence          s_fence;
static VkPipelineLayout s_layout;
static VkDescriptorSetLayout s_dsl;
static VkDescriptorPool s_dpool;
static VkSampler        s_sampler;
static VkShaderModule   s_vs3d, s_vs2d, s_fs;
static vktex_t         *s_default_tex;
static bool             s_etc2;
static bool             s_frame_active;
static uint32_t         s_img_idx;
static bool             s_init_done;

enum { BLEND_OFF, BLEND_ADD, BLEND_WHITEOUT };
enum { CULL_OFF, CULL_BACK, CULL_FRONT };
static int  s_blend = BLEND_OFF;
static bool s_depth_test = true;
static bool s_depth_write = true;
static int  s_cull = CULL_BACK;

static MATRIX proj_matrix, view_matrix, world_matrix;
static bool mvp_needs_update = true;
static render_viewport_t s_viewport;

static uint32_t find_mem_type(uint32_t filter, VkMemoryPropertyFlags props)
{
    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(s_phys, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; i++)
        if ((filter & (1u << i)) && (mp.memoryTypes[i].propertyFlags & props) == props)
            return i;
    return 0;
}

static VkCommandBuffer one_time_cmd(void)
{
    VkCommandBufferAllocateInfo ai = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    ai.commandPool = s_pool; ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; ai.commandBufferCount = 1;
    VkCommandBuffer cb;
    vkAllocateCommandBuffers(s_dev, &ai, &cb);
    VkCommandBufferBeginInfo bi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cb, &bi);
    return cb;
}
static void end_one_time(VkCommandBuffer cb)
{
    vkEndCommandBuffer(cb);
    VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    si.commandBufferCount = 1; si.pCommandBuffers = &cb;
    vkQueueSubmit(s_queue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(s_queue);
    vkFreeCommandBuffers(s_dev, s_pool, 1, &cb);
}

static vkbuf_t *vkbuf_create(VkDeviceSize size, VkBufferUsageFlags usage)
{
    vkbuf_t *b = calloc(1, sizeof(vkbuf_t));
    b->size = size;
    VkBufferCreateInfo ci = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    ci.size = size; ci.usage = usage; ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCreateBuffer(s_dev, &ci, NULL, &b->buf);
    VkMemoryRequirements mr;
    vkGetBufferMemoryRequirements(s_dev, b->buf, &mr);
    VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    ai.allocationSize = mr.size;
    ai.memoryTypeIndex = find_mem_type(mr.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkAllocateMemory(s_dev, &ai, NULL, &b->mem);
    vkBindBufferMemory(s_dev, b->buf, b->mem, 0);
    vkMapMemory(s_dev, b->mem, 0, size, 0, &b->mapped);
    return b;
}
static void vkbuf_destroy(vkbuf_t *b)
{
    if (!b) return;
    if (b->mapped) vkUnmapMemory(s_dev, b->mem);
    vkDestroyBuffer(s_dev, b->buf, NULL);
    vkFreeMemory(s_dev, b->mem, NULL);
    free(b);
}

static void destroy_swapchain(void)
{
    if (s_fbs) { for (uint32_t i = 0; i < s_swap_count; i++) vkDestroyFramebuffer(s_dev, s_fbs[i], NULL); free(s_fbs); s_fbs = NULL; }
    if (s_swap_views) { for (uint32_t i = 0; i < s_swap_count; i++) vkDestroyImageView(s_dev, s_swap_views[i], NULL); free(s_swap_views); s_swap_views = NULL; }
    if (s_depth_view) { vkDestroyImageView(s_dev, s_depth_view, NULL); s_depth_view = VK_NULL_HANDLE; }
    if (s_depth_img) { vkDestroyImage(s_dev, s_depth_img, NULL); s_depth_img = VK_NULL_HANDLE; }
    if (s_depth_mem) { vkFreeMemory(s_dev, s_depth_mem, NULL); s_depth_mem = VK_NULL_HANDLE; }
    if (s_swap_imgs) { free(s_swap_imgs); s_swap_imgs = NULL; }
    if (s_swap) { vkDestroySwapchainKHR(s_dev, s_swap, NULL); s_swap = VK_NULL_HANDLE; }
}

static bool create_swapchain(void)
{
    int dw, dh;
    SDL_Vulkan_GetDrawableSize(render_info.window, &dw, &dh);
    if (dw <= 0 || dh <= 0) { dw = render_info.ThisMode.w; dh = render_info.ThisMode.h; }
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(s_phys, s_surface, &caps);
    uint32_t nfmt = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(s_phys, s_surface, &nfmt, NULL);
    VkSurfaceFormatKHR fmts[16];
    if (nfmt > 16) nfmt = 16;
    vkGetPhysicalDeviceSurfaceFormatsKHR(s_phys, s_surface, &nfmt, fmts);
    VkSurfaceFormatKHR chosen = fmts[0];
    for (uint32_t i = 0; i < nfmt; i++)
        if (fmts[i].format == VK_FORMAT_B8G8R8A8_SRGB) { chosen = fmts[i]; break; }
    s_swap_fmt = chosen.format;
    s_extent.width = (uint32_t)dw; s_extent.height = (uint32_t)dh;
    if (caps.currentExtent.width != 0xFFFFFFFFu) s_extent = caps.currentExtent;

    VkSwapchainCreateInfoKHR ci = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    ci.surface = s_surface;
    ci.minImageCount = caps.minImageCount + 1;
    if (caps.maxImageCount && ci.minImageCount > caps.maxImageCount) ci.minImageCount = caps.maxImageCount;
    ci.imageFormat = chosen.format; ci.imageColorSpace = chosen.colorSpace;
    ci.imageExtent = s_extent;
    ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.preTransform = caps.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    ci.clipped = VK_TRUE;
    if (vkCreateSwapchainKHR(s_dev, &ci, NULL, &s_swap) != VK_SUCCESS) return false;

    vkGetSwapchainImagesKHR(s_dev, s_swap, &s_swap_count, NULL);
    s_swap_imgs = malloc(sizeof(VkImage) * s_swap_count);
    vkGetSwapchainImagesKHR(s_dev, s_swap, &s_swap_count, s_swap_imgs);
    s_swap_views = malloc(sizeof(VkImageView) * s_swap_count);
    for (uint32_t i = 0; i < s_swap_count; i++) {
        VkImageViewCreateInfo vi = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        vi.image = s_swap_imgs[i]; vi.viewType = VK_IMAGE_VIEW_TYPE_2D; vi.format = s_swap_fmt;
        vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vi.subresourceRange.levelCount = 1; vi.subresourceRange.layerCount = 1;
        vkCreateImageView(s_dev, &vi, NULL, &s_swap_views[i]);
    }

    VkImageCreateInfo di = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    di.imageType = VK_IMAGE_TYPE_2D; di.format = VK_FORMAT_D32_SFLOAT;
    di.extent.width = s_extent.width; di.extent.height = s_extent.height; di.extent.depth = 1;
    di.mipLevels = 1; di.arrayLayers = 1; di.samples = VK_SAMPLE_COUNT_1_BIT;
    di.tiling = VK_IMAGE_TILING_OPTIMAL; di.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    vkCreateImage(s_dev, &di, NULL, &s_depth_img);
    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(s_dev, s_depth_img, &mr);
    VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    ai.allocationSize = mr.size;
    ai.memoryTypeIndex = find_mem_type(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(s_dev, &ai, NULL, &s_depth_mem);
    vkBindImageMemory(s_dev, s_depth_img, s_depth_mem, 0);
    VkImageViewCreateInfo dvi = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    dvi.image = s_depth_img; dvi.viewType = VK_IMAGE_VIEW_TYPE_2D; dvi.format = VK_FORMAT_D32_SFLOAT;
    dvi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    dvi.subresourceRange.levelCount = 1; dvi.subresourceRange.layerCount = 1;
    vkCreateImageView(s_dev, &dvi, NULL, &s_depth_view);

    s_fbs = malloc(sizeof(VkFramebuffer) * s_swap_count);
    for (uint32_t i = 0; i < s_swap_count; i++) {
        VkImageView atts[2] = { s_swap_views[i], s_depth_view };
        VkFramebufferCreateInfo fi = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        fi.renderPass = s_pass; fi.attachmentCount = 2; fi.pAttachments = atts;
        fi.width = s_extent.width; fi.height = s_extent.height; fi.layers = 1;
        vkCreateFramebuffer(s_dev, &fi, NULL, &s_fbs[i]);
    }
    render_info.ThisMode.w = (int)s_extent.width;
    render_info.ThisMode.h = (int)s_extent.height;
    return true;
}

typedef struct {
    int ortho, topo, blend, dtest, dwrite, cull;
    VkPipeline pipe;
} pipe_entry_t;
static pipe_entry_t s_pipes[64];
static int s_npipes;

static VkPipeline get_pipeline(int ortho, int topo)
{
    for (int i = 0; i < s_npipes; i++) {
        pipe_entry_t *e = &s_pipes[i];
        if (e->ortho == ortho && e->topo == topo && e->blend == s_blend &&
            e->dtest == (int)s_depth_test && e->dwrite == (int)s_depth_write && e->cull == s_cull)
            return e->pipe;
    }
    if (s_npipes >= 64) return s_pipes[0].pipe;

    VkPipelineShaderStageCreateInfo stages[2] = {
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO }
    };
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = ortho ? s_vs2d : s_vs3d;
    stages[0].pName = "main";
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = s_fs;
    stages[1].pName = "main";

    VkVertexInputBindingDescription binds[1] = {{ 0, ortho ? sizeof(TLVERTEX) : sizeof(LVERTEX), VK_VERTEX_INPUT_RATE_VERTEX }};
    VkVertexInputAttributeDescription attrs[3];
    if (ortho) {
        attrs[0] = (VkVertexInputAttributeDescription){ 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0 };
        attrs[1] = (VkVertexInputAttributeDescription){ 1, 0, VK_FORMAT_R8G8B8A8_UNORM, 16 };
        attrs[2] = (VkVertexInputAttributeDescription){ 2, 0, VK_FORMAT_R32G32_SFLOAT, 20 };
    } else {
        attrs[0] = (VkVertexInputAttributeDescription){ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 };
        attrs[1] = (VkVertexInputAttributeDescription){ 1, 0, VK_FORMAT_R8G8B8A8_UNORM, 12 };
        attrs[2] = (VkVertexInputAttributeDescription){ 2, 0, VK_FORMAT_R32G32_SFLOAT, 16 };
    }
    VkPipelineVertexInputStateCreateInfo vin = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    vin.vertexBindingDescriptionCount = 1; vin.pVertexBindingDescriptions = binds;
    vin.vertexAttributeDescriptionCount = 3; vin.pVertexAttributeDescriptions = attrs;

    VkPipelineInputAssemblyStateCreateInfo ia = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    ia.topology = topo ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vp = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    vp.viewportCount = 1; vp.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rs = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = s_cull == CULL_OFF ? VK_CULL_MODE_NONE :
                  s_cull == CULL_FRONT ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_BACK_BIT;
    rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rs.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    ds.depthTestEnable = s_depth_test ? VK_TRUE : VK_FALSE;
    ds.depthWriteEnable = s_depth_write ? VK_TRUE : VK_FALSE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState ba = { 0 };
    ba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    if (s_blend != BLEND_OFF) {
        ba.blendEnable = VK_TRUE;
        ba.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        ba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        ba.colorBlendOp = VK_BLEND_OP_ADD;
        ba.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        ba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        ba.alphaBlendOp = VK_BLEND_OP_ADD;
    }
    VkPipelineColorBlendStateCreateInfo cb = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    cb.attachmentCount = 1; cb.pAttachments = &ba;

    VkDynamicState dyn[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dyns = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dyns.dynamicStateCount = 2; dyns.pDynamicStates = dyn;

    VkGraphicsPipelineCreateInfo gi = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    gi.stageCount = 2; gi.pStages = stages;
    gi.pVertexInputState = &vin; gi.pInputAssemblyState = &ia;
    gi.pViewportState = &vp; gi.pRasterizationState = &rs;
    gi.pMultisampleState = &ms; gi.pDepthStencilState = &ds;
    gi.pColorBlendState = &cb; gi.pDynamicState = &dyns;
    gi.layout = s_layout; gi.renderPass = s_pass; gi.subpass = 0;

    VkPipeline pipe;
    if (vkCreateGraphicsPipelines(s_dev, VK_NULL_HANDLE, 1, &gi, NULL, &pipe) != VK_SUCCESS)
        return VK_NULL_HANDLE;
    pipe_entry_t *e = &s_pipes[s_npipes++];
    e->ortho = ortho; e->topo = topo; e->blend = s_blend;
    e->dtest = (int)s_depth_test; e->dwrite = (int)s_depth_write; e->cull = s_cull;
    e->pipe = pipe;
    return pipe;
}

static VkShaderModule make_module(const uint32_t *spv, size_t bytes)
{
    VkShaderModuleCreateInfo ci = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    ci.codeSize = bytes; ci.pCode = spv;
    VkShaderModule m;
    vkCreateShaderModule(s_dev, &ci, NULL, &m);
    return m;
}

bool render_init(render_info_t *info)
{
    if (s_init_done) return true;
    unsigned int n_ext = 0;
    SDL_Vulkan_GetInstanceExtensions(info->window, &n_ext, NULL);
    const char **exts = malloc(sizeof(char *) * (n_ext + 1));
    SDL_Vulkan_GetInstanceExtensions(info->window, &n_ext, exts);

    VkApplicationInfo app = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
    app.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo ici = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    ici.pApplicationInfo = &app;
    ici.enabledExtensionCount = n_ext; ici.ppEnabledExtensionNames = exts;
    /* Opt-in validation: FORSAKEN_VK_VALIDATION=1 (development only). */
    const char *val_layer = "VK_LAYER_KHRONOS_validation";
    if (getenv("FORSAKEN_VK_VALIDATION")) {
        ici.enabledLayerCount = 1; ici.ppEnabledLayerNames = &val_layer;
    }
    if (vkCreateInstance(&ici, NULL, &s_inst) != VK_SUCCESS) { DebugPrintf("vulkan: instance failed\n"); return false; }
    free(exts);
    if (!SDL_Vulkan_CreateSurface(info->window, s_inst, &s_surface)) { DebugPrintf("vulkan: surface failed\n"); return false; }

    uint32_t ndev = 0;
    vkEnumeratePhysicalDevices(s_inst, &ndev, NULL);
    VkPhysicalDevice devs[8];
    if (ndev > 8) ndev = 8;
    vkEnumeratePhysicalDevices(s_inst, &ndev, devs);
    s_phys = devs[0];

    uint32_t nqf = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(s_phys, &nqf, NULL);
    VkQueueFamilyProperties qfs[16];
    if (nqf > 16) nqf = 16;
    vkGetPhysicalDeviceQueueFamilyProperties(s_phys, &nqf, qfs);
    s_qfam = 0;
    for (uint32_t i = 0; i < nqf; i++) {
        VkBool32 present = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(s_phys, i, s_surface, &present);
        if ((qfs[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present) { s_qfam = i; break; }
    }

    VkPhysicalDeviceFeatures feats;
    vkGetPhysicalDeviceFeatures(s_phys, &feats);
    s_etc2 = feats.textureCompressionETC2 == VK_TRUE;
    DebugPrintf("vulkan: ETC2 texture compression %s\n", s_etc2 ? "available" : "NOT available (RGBA fallback)");

    float prio = 1.0f;
    VkDeviceQueueCreateInfo qci = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
    qci.queueFamilyIndex = s_qfam; qci.queueCount = 1; qci.pQueuePriorities = &prio;
    const char *dext = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    VkPhysicalDeviceFeatures enabled = { 0 };
    enabled.samplerAnisotropy = feats.samplerAnisotropy;
    VkDeviceCreateInfo dci = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    dci.queueCreateInfoCount = 1; dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = 1; dci.ppEnabledExtensionNames = &dext;
    dci.pEnabledFeatures = &enabled;
    if (vkCreateDevice(s_phys, &dci, NULL, &s_dev) != VK_SUCCESS) { DebugPrintf("vulkan: device failed\n"); return false; }
    vkGetDeviceQueue(s_dev, s_qfam, 0, &s_queue);

    VkAttachmentDescription atts[2] = {
        { 0, VK_FORMAT_UNDEFINED, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
          VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR },
        { 0, VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE,
          VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL }
    };
    /* swap format patched in after swapchain creation below via recreation;
     * renderpass needs the format, so create swapchain-format probe first */
    {
        uint32_t nfmt = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(s_phys, s_surface, &nfmt, NULL);
        VkSurfaceFormatKHR fmts[16];
        if (nfmt > 16) nfmt = 16;
        vkGetPhysicalDeviceSurfaceFormatsKHR(s_phys, s_surface, &nfmt, fmts);
        atts[0].format = fmts[0].format;
        for (uint32_t i = 0; i < nfmt; i++)
            if (fmts[i].format == VK_FORMAT_B8G8R8A8_SRGB) { atts[0].format = fmts[i].format; break; }
    }
    VkAttachmentReference cref = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkAttachmentReference dref = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
    VkSubpassDescription sub = { 0 };
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1; sub.pColorAttachments = &cref; sub.pDepthStencilAttachment = &dref;
    VkRenderPassCreateInfo rci = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    rci.attachmentCount = 2; rci.pAttachments = atts;
    rci.subpassCount = 1; rci.pSubpasses = &sub;
    if (vkCreateRenderPass(s_dev, &rci, NULL, &s_pass) != VK_SUCCESS) { DebugPrintf("vulkan: renderpass failed\n"); return false; }

    VkCommandPoolCreateInfo pci = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    pci.queueFamilyIndex = s_qfam;
    pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    vkCreateCommandPool(s_dev, &pci, NULL, &s_pool);
    VkCommandBufferAllocateInfo cai = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    cai.commandPool = s_pool; cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cai.commandBufferCount = 1;
    vkAllocateCommandBuffers(s_dev, &cai, &s_cmd);

    VkSemaphoreCreateInfo sci = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    vkCreateSemaphore(s_dev, &sci, NULL, &s_img_avail);
    vkCreateSemaphore(s_dev, &sci, NULL, &s_done);
    VkFenceCreateInfo fci = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    vkCreateFence(s_dev, &fci, NULL, &s_fence);

    VkDescriptorSetLayoutBinding db = { 0 };
    db.binding = 0; db.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    db.descriptorCount = 1; db.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo dli = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    dli.bindingCount = 1; dli.pBindings = &db;
    vkCreateDescriptorSetLayout(s_dev, &dli, NULL, &s_dsl);

    VkDescriptorPoolSize dps = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 512 };
    VkDescriptorPoolCreateInfo dpi = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    dpi.maxSets = 512; dpi.poolSizeCount = 1; dpi.pPoolSizes = &dps;
    dpi.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    vkCreateDescriptorPool(s_dev, &dpi, NULL, &s_dpool);

    VkPushConstantRange ranges[2] = {
        { VK_SHADER_STAGE_VERTEX_BIT, 0, 64 },
        { VK_SHADER_STAGE_FRAGMENT_BIT, 64, 8 }
    };
    VkPipelineLayoutCreateInfo pli = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pli.setLayoutCount = 1; pli.pSetLayouts = &s_dsl;
    pli.pushConstantRangeCount = 2; pli.pPushConstantRanges = ranges;
    vkCreatePipelineLayout(s_dev, &pli, NULL, &s_layout);

    VkSamplerCreateInfo samp = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    samp.magFilter = VK_FILTER_LINEAR; samp.minFilter = VK_FILTER_LINEAR;
    samp.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samp.addressModeU = samp.addressModeV = samp.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samp.maxLod = 16.0f;
    if (feats.samplerAnisotropy) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(s_phys, &props);
        samp.anisotropyEnable = VK_TRUE;
        samp.maxAnisotropy = props.limits.maxSamplerAnisotropy > 4.0f ? 4.0f : props.limits.maxSamplerAnisotropy;
    }
    vkCreateSampler(s_dev, &samp, NULL, &s_sampler);

    s_vs3d = make_module(vulkan3d_vert_spv, vulkan3d_vert_spv_len);
    s_vs2d = make_module(vulkan2d_vert_spv, vulkan2d_vert_spv_len);
    s_fs = make_module(vulkan_frag_spv, vulkan_frag_spv_len);

    if (!create_swapchain()) { DebugPrintf("vulkan: swapchain failed\n"); return false; }
    info->ok_to_render = true;
    build_gamma_table(1.0);
    s_init_done = true;
    DebugPrintf("vulkan: init ok %ux%u\n", s_extent.width, s_extent.height);
    return true;
}

void render_cleanup(render_info_t *info)
{
    (void)info;
    if (!s_init_done) return;
    vkDeviceWaitIdle(s_dev);
    if (s_default_tex) { /* destroyed with pool reset below */ s_default_tex = NULL; }
    for (int i = 0; i < s_npipes; i++) vkDestroyPipeline(s_dev, s_pipes[i].pipe, NULL);
    s_npipes = 0;
    destroy_swapchain();
    vkDestroyShaderModule(s_dev, s_vs3d, NULL);
    vkDestroyShaderModule(s_dev, s_vs2d, NULL);
    vkDestroyShaderModule(s_dev, s_fs, NULL);
    vkDestroySampler(s_dev, s_sampler, NULL);
    vkDestroyDescriptorPool(s_dev, s_dpool, NULL);
    vkDestroyPipelineLayout(s_dev, s_layout, NULL);
    vkDestroyDescriptorSetLayout(s_dev, s_dsl, NULL);
    vkDestroySemaphore(s_dev, s_img_avail, NULL);
    vkDestroySemaphore(s_dev, s_done, NULL);
    vkDestroyFence(s_dev, s_fence, NULL);
    vkDestroyCommandPool(s_dev, s_pool, NULL);
    vkDestroyRenderPass(s_dev, s_pass, NULL);
    vkDestroyDevice(s_dev, NULL);
    vkDestroySurfaceKHR(s_inst, s_surface, NULL);
    vkDestroyInstance(s_inst, NULL);
    s_init_done = false;
}

bool render_mode_select(render_info_t *info);
bool render_reset(render_info_t *info)
{
    /* Engine calls this from RenderModeReset expecting the 3DS
     * contract (cleanup is a no-op there, init idempotent, engine
     * buffers survive). Same here: swapchain-only recreation. Full
     * teardown is render_cleanup, called at shutdown. */
    return render_mode_select(info);
}
bool render_mode_select(render_info_t *info)
{
    /* Engine semantics: a video-mode change. The device, buffers and
     * textures all survive (the GL backend just recreated its window
     * surface); only the swapchain is sized to the new mode. The old
     * full-device reset crashed here: InitView releases engine
     * buffers created on the previous device. */
    (void)info;
    if (!s_init_done) return render_init(info);
    vkDeviceWaitIdle(s_dev);
    destroy_swapchain();
    return create_swapchain();
}
void render_set_filter(bool r, bool g, bool b) { (void)r; (void)g; (void)b; }
const char *render_error_description(int e) { (void)e; return NULL; }
void render_mode_wireframe(void) {}
void render_mode_points(void) {}
void render_mode_fill(void) {}

static u_int8_t gamma_table[256];
void build_gamma_table(double gamma)
{
    double k = 255.0 / pow(255.0, 1.0 / gamma);
    for (int i = 0; i <= 255; i++) {
        gamma_table[i] = (u_int8_t)(k * pow((double)i, 1.0 / gamma));
        if (i && !gamma_table[i]) gamma_table[i] = 1;
    }
}

/* ---- render state (mirrors the old GL state functions) ---- */
void reset_trans(void) { s_blend = BLEND_OFF; }
void set_alpha_states(void) { s_depth_write = false; s_blend = BLEND_ADD; }
void set_whiteout_state(void) { s_blend = BLEND_WHITEOUT; s_depth_write = false; }
void set_normal_states(void) { s_depth_test = true; s_depth_write = true; s_blend = BLEND_OFF; }
void reset_zbuff(void) { s_depth_test = true; s_depth_write = true; }
void disable_zbuff_write(void) { s_depth_write = false; }
void disable_zbuff(void) { s_depth_test = false; }
void cull_none(void) { s_cull = CULL_OFF; }
void cull_cw(void) { s_cull = CULL_FRONT; }
void reset_cull(void) { s_cull = CULL_BACK; }

/* ---- matrices ---- */
bool FSSetView(RENDERMATRIX *matrix) { memmove(&view_matrix, &matrix->m, sizeof(view_matrix)); mvp_needs_update = true; return true; }
bool FSSetWorld(RENDERMATRIX *matrix) { memmove(&world_matrix, &matrix->m, sizeof(world_matrix)); mvp_needs_update = true; return true; }
bool FSGetWorld(RENDERMATRIX *matrix) { memmove(&matrix->m, &world_matrix, sizeof(matrix->m)); return true; }
bool FSSetProjection(RENDERMATRIX *matrix) { memmove(&proj_matrix, &matrix->m, sizeof(proj_matrix)); mvp_needs_update = true; return true; }

bool FSSetViewPort(render_viewport_t *view)
{
    s_viewport = *view;
    if (s_frame_active) {
        VkViewport vp = { (float)view->X, (float)view->Y, (float)view->Width, (float)view->Height, view->MinZ, view->MaxZ };
        VkRect2D sc = { { view->X, view->Y }, { (uint32_t)view->Width, (uint32_t)view->Height } };
        vkCmdSetViewport(s_cmd, 0, 1, &vp);
        vkCmdSetScissor(s_cmd, 0, 1, &sc);
    }
    return true;
}
bool FSGetViewPort(render_viewport_t *view) { *view = s_viewport; return true; }
void FSSetNextViewPortScissorMode(bool b) { (void)b; }

bool FSBeginScene(void)
{
    if (!s_init_done || s_frame_active) return true;
    vkWaitForFences(s_dev, 1, &s_fence, VK_TRUE, UINT64_MAX);
    VkResult r = vkAcquireNextImageKHR(s_dev, s_swap, UINT64_MAX, s_img_avail, VK_NULL_HANDLE, &s_img_idx);
    if (r == VK_ERROR_OUT_OF_DATE_KHR) {
        vkDeviceWaitIdle(s_dev);
        destroy_swapchain();
        create_swapchain();
        return true;
    }
    if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) return false;
    vkResetFences(s_dev, 1, &s_fence);
    vkResetCommandBuffer(s_cmd, 0);
    VkCommandBufferBeginInfo bi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    vkBeginCommandBuffer(s_cmd, &bi);
    VkClearValue clears[2];
    memset(clears, 0, sizeof clears);
    clears[1].depthStencil.depth = 1.0f;
    VkRenderPassBeginInfo rp = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    rp.renderPass = s_pass; rp.framebuffer = s_fbs[s_img_idx];
    rp.renderArea.extent = s_extent;
    rp.clearValueCount = 2; rp.pClearValues = clears;
    vkCmdBeginRenderPass(s_cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
    VkViewport vp = { 0, 0, (float)s_extent.width, (float)s_extent.height, 0.0f, 1.0f };
    VkRect2D sc = { { 0, 0 }, s_extent };
    vkCmdSetViewport(s_cmd, 0, 1, &vp);
    vkCmdSetScissor(s_cmd, 0, 1, &sc);
    s_frame_active = true;
    return true;
}
bool FSEndScene(void) { return true; }

bool render_flip(render_info_t *info)
{
    (void)info;
    if (!s_frame_active) return true;
    vkCmdEndRenderPass(s_cmd);
    vkEndCommandBuffer(s_cmd);
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    si.waitSemaphoreCount = 1; si.pWaitSemaphores = &s_img_avail; si.pWaitDstStageMask = &wait_stage;
    si.commandBufferCount = 1; si.pCommandBuffers = &s_cmd;
    si.signalSemaphoreCount = 1; si.pSignalSemaphores = &s_done;
    vkQueueSubmit(s_queue, 1, &si, s_fence);
    VkPresentInfoKHR pi = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    pi.waitSemaphoreCount = 1; pi.pWaitSemaphores = &s_done;
    pi.swapchainCount = 1; pi.pSwapchains = &s_swap; pi.pImageIndices = &s_img_idx;
    VkResult r = vkQueuePresentKHR(s_queue, &pi);
    if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) {
        vkDeviceWaitIdle(s_dev);
        destroy_swapchain();
        create_swapchain();
    }
    s_frame_active = false;
    return true;
}

bool FSClear(XYRECT *rect)
{
    if (!s_frame_active) return true;
    VkClearAttachment atts[2] = {
        { VK_IMAGE_ASPECT_COLOR_BIT, 0, { { 0, 0, 0, 1 } } },
        { VK_IMAGE_ASPECT_DEPTH_BIT, 0, { { 0, 0, 0, 0 } } }
    };
    atts[1].clearValue.depthStencil.depth = 1.0f;
    int y = render_info.ThisMode.h - rect->y1 - (rect->y2 - rect->y1);
    VkClearRect cr = { { { rect->x1, y }, { (uint32_t)(rect->x2 - rect->x1), (uint32_t)(rect->y2 - rect->y1) } }, 0, 1 };
    vkCmdClearAttachments(s_cmd, 2, atts, 1, &cr);
    return true;
}
bool FSClearBlack(void)
{
    if (!s_frame_active) return true;
    VkClearAttachment att = { VK_IMAGE_ASPECT_COLOR_BIT, 0, { { 0, 0, 0, 1 } } };
    VkClearRect cr = { { { 0, 0 }, s_extent }, 0, 1 };
    vkCmdClearAttachments(s_cmd, 1, &att, 1, &cr);
    return true;
}
bool FSClearDepth(XYRECT *rect)
{
    if (!s_frame_active) return true;
    VkClearAttachment att = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, { { 0, 0, 0, 0 } } };
    att.clearValue.depthStencil.depth = 1.0f;
    int y = render_info.ThisMode.h - rect->y1 - (rect->y2 - rect->y1);
    VkClearRect cr = { { { rect->x1, y }, { (uint32_t)(rect->x2 - rect->x1), (uint32_t)(rect->y2 - rect->y1) } }, 0, 1 };
    vkCmdClearAttachments(s_cmd, 1, &att, 1, &cr);
    return true;
}

/* ---- vertex/index/normal buffers (host-visible, persistently mapped) ---- */
bool FSCreateVertexBuffer(RENDEROBJECT *ro, int n) { ro->lpVertexBuffer = (LPVERTEXBUFFER)vkbuf_create((VkDeviceSize)n * sizeof(LVERTEX), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT); return true; }
bool FSCreateDynamicVertexBuffer(RENDEROBJECT *ro, int n) { return FSCreateVertexBuffer(ro, n); }
bool FSCreateDynamic2dVertexBuffer(RENDEROBJECT *ro, int n) { ro->lpVertexBuffer = (LPVERTEXBUFFER)vkbuf_create((VkDeviceSize)n * sizeof(TLVERTEX), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT); return true; }
bool FSCreateNormalBuffer(RENDEROBJECT *ro, int n) { ro->lpNormalBuffer = (LPVERTEXBUFFER)vkbuf_create((VkDeviceSize)n * sizeof(NORMAL), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT); return true; }
bool FSCreateDynamicNormalBuffer(RENDEROBJECT *ro, int n) { return FSCreateNormalBuffer(ro, n); }
bool FSCreateIndexBuffer(RENDEROBJECT *ro, int n) { ro->lpIndexBuffer = (LPVERTEXBUFFER)vkbuf_create((VkDeviceSize)n * 3 * sizeof(WORD), VK_BUFFER_USAGE_INDEX_BUFFER_BIT); return true; }
bool FSCreateDynamicIndexBuffer(RENDEROBJECT *ro, int n) { return FSCreateIndexBuffer(ro, n); }

bool FSLockVertexBuffer(RENDEROBJECT *ro, LVERTEX **verts) { *verts = (LVERTEX *)((vkbuf_t *)ro->lpVertexBuffer)->mapped; return *verts != NULL; }
bool FSUnlockVertexBuffer(RENDEROBJECT *ro) { (void)ro; return true; }
bool FSLockNormalBuffer(RENDEROBJECT *ro, NORMAL **normals) { *normals = (NORMAL *)((vkbuf_t *)ro->lpNormalBuffer)->mapped; return *normals != NULL; }
bool FSUnlockNormalBuffer(RENDEROBJECT *ro) { (void)ro; return true; }
bool FSLockIndexBuffer(RENDEROBJECT *ro, WORD **indices) { *indices = (WORD *)((vkbuf_t *)ro->lpIndexBuffer)->mapped; return *indices != NULL; }
bool FSUnlockIndexBuffer(RENDEROBJECT *ro) { (void)ro; return true; }
bool FSLockPretransformedVertexBuffer(RENDEROBJECT *ro, TLVERTEX **verts) { return FSLockVertexBuffer(ro, (LVERTEX **)verts); }

void FSReleaseRenderObject(RENDEROBJECT *ro)
{
    /* Buffers may be referenced by the in-flight frame. */
    if (s_init_done) vkDeviceWaitIdle(s_dev);
    if (ro->lpVertexBuffer) { vkbuf_destroy((vkbuf_t *)ro->lpVertexBuffer); ro->lpVertexBuffer = NULL; }
    if (ro->lpNormalBuffer) { vkbuf_destroy((vkbuf_t *)ro->lpNormalBuffer); ro->lpNormalBuffer = NULL; }
    if (ro->lpIndexBuffer) { vkbuf_destroy((vkbuf_t *)ro->lpIndexBuffer); ro->lpIndexBuffer = NULL; }
    for (int i = 0; i < ro->numTextureGroups; i++) {
        ro->textureGroups[i].numVerts = 0;
        ro->textureGroups[i].startVert = 0;
        ro->textureGroups[i].texture = NULL;
    }
    ro->numTextureGroups = 0;
}

/* ---- textures ---- */
static vktex_t *vktex_alloc(void)
{
    vktex_t *t = calloc(1, sizeof(vktex_t));
    VkDescriptorSetAllocateInfo ai = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    ai.descriptorPool = s_dpool; ai.descriptorSetCount = 1; ai.pSetLayouts = &s_dsl;
    vkAllocateDescriptorSets(s_dev, &ai, &t->set);
    return t;
}
static void vktex_finish(vktex_t *t, VkFormat fmt, int levels)
{
    VkImageViewCreateInfo vi = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    vi.image = t->image; vi.viewType = VK_IMAGE_VIEW_TYPE_2D; vi.format = fmt;
    vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vi.subresourceRange.levelCount = (uint32_t)levels; vi.subresourceRange.layerCount = 1;
    vkCreateImageView(s_dev, &vi, NULL, &t->view);
    VkDescriptorImageInfo di = { s_sampler, t->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    VkWriteDescriptorSet w = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    w.dstSet = t->set; w.dstBinding = 0; w.descriptorCount = 1;
    w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; w.pImageInfo = &di;
    vkUpdateDescriptorSets(s_dev, 1, &w, 0, NULL);
}
static VkImage vktex_image(int w, int h, int levels, VkFormat fmt)
{
    VkImageCreateInfo ci = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    ci.imageType = VK_IMAGE_TYPE_2D; ci.format = fmt;
    ci.extent.width = (uint32_t)w; ci.extent.height = (uint32_t)h; ci.extent.depth = 1;
    ci.mipLevels = (uint32_t)levels; ci.arrayLayers = 1; ci.samples = VK_SAMPLE_COUNT_1_BIT;
    ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    ci.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    VkImage img;
    vkCreateImage(s_dev, &ci, NULL, &img);
    return img;
}
static void vktex_bind_mem(vktex_t *t)
{
    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(s_dev, t->image, &mr);
    VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    ai.allocationSize = mr.size;
    ai.memoryTypeIndex = find_mem_type(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(s_dev, &ai, NULL, &t->mem);
    vkBindImageMemory(s_dev, t->image, t->mem, 0);
}
static vkbuf_t *staging(const void *data, VkDeviceSize size)
{
    vkbuf_t *st = vkbuf_create(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    memcpy(st->mapped, data, (size_t)size);
    return st;
}
static void transition(VkCommandBuffer cb, VkImage img, int base_level, int levels, VkImageLayout old_l, VkImageLayout new_l, VkAccessFlags src, VkAccessFlags dst)
{
    VkImageMemoryBarrier b = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    b.oldLayout = old_l; b.newLayout = new_l;
    b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = img;
    b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    b.subresourceRange.baseMipLevel = (uint32_t)base_level;
    b.subresourceRange.levelCount = (uint32_t)levels;
    b.subresourceRange.layerCount = 1;
    b.srcAccessMask = src; b.dstAccessMask = dst;
    VkPipelineStageFlags dst_stage = (new_l == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_TRANSFER_BIT;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, dst_stage, 0, 0, NULL, 0, NULL, 1, &b);
}

static bool load_ktx2(vktex_t *t, const char *path, u_int16_t *width, u_int16_t *height, bool *colorkey)
{
    FILE *f = file_open((char *)path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    u_int8_t *data = malloc((size_t)sz);
    if (fread(data, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); free(data); return false; }
    fclose(f);
    static const u_int8_t ktx2id[12] = {0xAB,0x4B,0x54,0x58,0x20,0x32,0x30,0xBB,0x0D,0x0A,0x1A,0x0A};
    if (sz < 88 || memcmp(data, ktx2id, 12) != 0) { free(data); return false; }
    u_int32_t *hdr = (u_int32_t *)(data + 12);
    if (hdr[0] != 144) { free(data); return false; }   /* ETC2_R8G8B8_SRGB_BLOCK */
    int w = (int)hdr[2], h = (int)hdr[3], levels = (int)hdr[7];
    if (w <= 0 || h <= 0 || levels <= 0 || levels > 16) { free(data); return false; }
    /* Level index follows the 88-byte header. */
    struct { u_int64_t off, len, ulen; } *lv = (void *)(data + 88);

    t->image = vktex_image(w, h, levels, VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK);
    t->w = w; t->h = h;
    vktex_bind_mem(t);

    VkDeviceSize total = 0;
    for (int i = 0; i < levels; i++) total += lv[i].len;
    vkbuf_t *st = vkbuf_create(total, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    VkBufferImageCopy regions[16];
    VkDeviceSize dst = 0;
    for (int i = 0; i < levels; i++) {
        memcpy((u_int8_t *)st->mapped + dst, data + lv[i].off, (size_t)lv[i].len);
        int mw = w >> i; if (mw < 1) mw = 1;
        int mh = h >> i; if (mh < 1) mh = 1;
        regions[i].bufferOffset = dst;
        regions[i].bufferRowLength = 0; regions[i].bufferImageHeight = 0;
        regions[i].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        regions[i].imageSubresource.mipLevel = (uint32_t)i;
        regions[i].imageSubresource.baseArrayLayer = 0; regions[i].imageSubresource.layerCount = 1;
        regions[i].imageOffset = (VkOffset3D){ 0, 0, 0 };
        regions[i].imageExtent = (VkExtent3D){ (uint32_t)mw, (uint32_t)mh, 1 };
        dst += lv[i].len;
    }
    VkCommandBuffer cb = one_time_cmd();
    transition(cb, t->image, 0, levels, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT);
    vkCmdCopyBufferToImage(cb, st->buf, t->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (uint32_t)levels, regions);
    transition(cb, t->image, 0, levels, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
    end_one_time(cb);
    vkbuf_destroy(st);
    vktex_finish(t, VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK, levels);
    *width = (u_int16_t)w; *height = (u_int16_t)h; *colorkey = false;
    free(data);
    DebugPrintf("vulkan: ETC2 texture %s %dx%d levels=%d\n", path, w, h, levels);
    return true;
}

static bool create_texture(LPTEXTURE *t, const char *path, u_int16_t *width, u_int16_t *height, int numMips, bool *colorkey)
{
    vktex_t *texdata;
    texture_image_t image;

    if (!*t) {
        /* ETC2 fast path: build-time-encoded sibling pack. */
        char ktxpath[256];
        Change_Ext(path, ktxpath, ".KTX2");
        if (s_etc2 && File_Exists(ktxpath)) {
            texdata = vktex_alloc();
            if (load_ktx2(texdata, ktxpath, width, height, colorkey)) {
                *t = (LPTEXTURE)texdata;
                return true;
            }
            free(texdata);
        }
    }

    Change_Ext(path, image.path, ".PNG");
    if (!File_Exists((char *)image.path)) {
        DebugPrintf("Could not find texture file: %s\n", path);
        return true;
    }
    if (load_image(&image, numMips) != 0) {
        DebugPrintf("create_texture: load_image failed for %s\n", path);
        return false;
    }
    *width = (u_int16_t)image.w;
    *height = (u_int16_t)image.h;
    *colorkey = (bool)image.colorkey;

    /* gamma + colour key, identical processing to the old GL path */
    {
        int pitch = 4 * image.w;
        for (int y = 0; y < image.h; y++)
            for (int x = 0; x < image.w; x++) {
                DWORD index = (DWORD)(y * pitch) + (DWORD)(x * 4);
                image.data[index]     = (char)gamma_table[(u_int8_t)image.data[index]];
                image.data[index + 1] = (char)gamma_table[(u_int8_t)image.data[index + 1]];
                image.data[index + 2] = (char)gamma_table[(u_int8_t)image.data[index + 2]];
                image.data[index + 3] = (char)gamma_table[(u_int8_t)image.data[index + 3]];
                if ((image.data[index] + image.data[index + 1] + image.data[index + 2]) == 0)
                    image.data[index + 3] = 0;
            }
    }

    int levels = 1;
    for (int t2 = image.w > image.h ? image.w : image.h; t2 > 1; t2 >>= 1) levels++;

    if (!*t) {
        texdata = vktex_alloc();
        texdata->image = vktex_image(image.w, image.h, levels, VK_FORMAT_R8G8B8A8_SRGB);
        texdata->w = image.w; texdata->h = image.h;
        vktex_bind_mem(texdata);
        texdata->colorkey = (bool)image.colorkey;
    } else {
        texdata = (vktex_t *)*t;
        levels = 1; /* update path refreshes level 0 only */
    }

    vkbuf_t *st = staging(image.data, (VkDeviceSize)image.w * image.h * 4);
    VkCommandBuffer cb = one_time_cmd();
    transition(cb, texdata->image, 0, 1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT);
    VkBufferImageCopy region = { 0 };
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = (VkExtent3D){ (uint32_t)image.w, (uint32_t)image.h, 1 };
    vkCmdCopyBufferToImage(cb, st->buf, texdata->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    if (!*t || texdata->image) {
        /* generate the mip chain by blitting down (fresh textures) */
        if (*t == NULL || levels > 1) { /* handled below via flag */ }
    }
    transition(cb, texdata->image, 0, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
    end_one_time(cb);
    vkbuf_destroy(st);

    if (!*t) {
        /* mip generation for fresh textures */
        cb = one_time_cmd();
        int mw = image.w, mh = image.h;
        for (int i = 1; i < levels; i++) {
            int nw = mw > 1 ? mw / 2 : 1, nh = mh > 1 ? mh / 2 : 1;
            VkImageMemoryBarrier b = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
            b.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.image = texdata->image;
            b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            b.subresourceRange.baseMipLevel = (uint32_t)(i - 1); b.subresourceRange.levelCount = 1; b.subresourceRange.layerCount = 1;
            b.srcAccessMask = VK_ACCESS_SHADER_READ_BIT; b.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            if (i == 1) { b.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; }
            vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &b);
            VkImageMemoryBarrier bd = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
            bd.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; bd.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            bd.srcQueueFamilyIndex = bd.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            bd.image = texdata->image;
            bd.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            bd.subresourceRange.baseMipLevel = (uint32_t)i; bd.subresourceRange.levelCount = 1; bd.subresourceRange.layerCount = 1;
            bd.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &bd);
            VkImageBlit blit = { 0 };
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = (uint32_t)(i - 1); blit.srcSubresource.layerCount = 1;
            blit.srcOffsets[1] = (VkOffset3D){ mw, mh, 1 };
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel = (uint32_t)i; blit.dstSubresource.layerCount = 1;
            blit.dstOffsets[1] = (VkOffset3D){ nw, nh, 1 };
            vkCmdBlitImage(cb, texdata->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           texdata->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
            VkImageMemoryBarrier ba = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
            ba.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL; ba.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            ba.srcQueueFamilyIndex = ba.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            ba.image = texdata->image;
            ba.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            ba.subresourceRange.baseMipLevel = (uint32_t)(i - 1); ba.subresourceRange.levelCount = 1; ba.subresourceRange.layerCount = 1;
            ba.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT; ba.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &ba);
            VkImageMemoryBarrier bb = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
            bb.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; bb.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            bb.srcQueueFamilyIndex = bb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            bb.image = texdata->image;
            bb.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            bb.subresourceRange.baseMipLevel = (uint32_t)i; bb.subresourceRange.levelCount = 1; bb.subresourceRange.layerCount = 1;
            bb.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; bb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &bb);
            mw = nw; mh = nh;
        }
        end_one_time(cb);
        vktex_finish(texdata, VK_FORMAT_R8G8B8A8_SRGB, levels);
        *t = (LPTEXTURE)texdata;
    }

    destroy_image(&image);
    return true;
}

bool FSCreateTexture(LPTEXTURE *texture, const char *fileName, u_int16_t *width, u_int16_t *height, int numMips, bool *colourkey)
{
    return create_texture(texture, fileName, width, height, numMips, colourkey);
}
bool update_texture_from_file(LPTEXTURE dstTexture, const char *fileName, u_int16_t *width, u_int16_t *height, int numMips, bool *colorkey)
{
    create_texture(&dstTexture, fileName, width, height, numMips, colorkey);
    return true;
}
void release_texture(LPTEXTURE texture)
{
    if (!texture) return;
    vktex_t *t = (vktex_t *)texture;
    vkDeviceWaitIdle(s_dev);
    if (t->set) vkFreeDescriptorSets(s_dev, s_dpool, 1, &t->set);
    if (t->view) vkDestroyImageView(s_dev, t->view, NULL);
    if (t->image) vkDestroyImage(s_dev, t->image, NULL);
    if (t->mem) vkFreeMemory(s_dev, t->mem, NULL);
    free(t);
}

/* ---- drawing ---- */
static vktex_t *default_tex(void)
{
    if (s_default_tex) return s_default_tex;
    vktex_t *t = vktex_alloc();
    u_int32_t white = 0xFFFFFFFFu;
    t->image = vktex_image(1, 1, 1, VK_FORMAT_R8G8B8A8_SRGB);
    t->w = t->h = 1;
    vktex_bind_mem(t);
    vkbuf_t *st = staging(&white, 4);
    VkCommandBuffer cb = one_time_cmd();
    transition(cb, t->image, 0, 1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT);
    VkBufferImageCopy region = { 0 };
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = (VkExtent3D){ 1, 1, 1 };
    vkCmdCopyBufferToImage(cb, st->buf, t->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    transition(cb, t->image, 0, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
    end_one_time(cb);
    vkbuf_destroy(st);
    vktex_finish(t, VK_FORMAT_R8G8B8A8_SRGB, 1);
    s_default_tex = t;
    return t;
}

static void push_mvp(int ortho)
{
    float m[16];
    if (ortho) {
        float left = 0, right = (float)render_info.ThisMode.w;
        float bottom = 0, top = (float)render_info.ThisMode.h;
        memset(m, 0, sizeof m);
        m[0] = 2.0f / (right - left);
        m[5] = (2.0f / (top - bottom)) * -1.0f;
        m[10] = -1.0f;
        m[12] = -(right + left) / (right - left);
        m[13] = -(top + bottom) / (top - bottom) + 2.0f;
        m[15] = 1.0f;
        /* MATRIX in this engine is row-major _11.._44 stored linearly;
         * the array above is written in the same linear order the GL
         * backend uploaded with GL_TRUE transpose. */
        float t[16];
        memcpy(t, m, sizeof t);
        /* transpose into column-major for GLSL mat4 */
        for (int r = 0; r < 4; r++)
            for (int c = 0; c < 4; c++)
                m[c * 4 + r] = t[r * 4 + c];
    } else {
        MATRIX mvp;
        MatrixMultiply(&world_matrix, &view_matrix, &mvp);
        MatrixMultiply(&mvp, &proj_matrix, &mvp);
        memcpy(m, &mvp, sizeof m);
    }
    vkCmdPushConstants(s_cmd, s_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, 64, m);
}

bool draw_render_object(RENDEROBJECT *renderObject, int primitive_type, bool orthographic)
{
    if (!s_frame_active || !renderObject->lpVertexBuffer) return true;
    VkPipeline pipe = get_pipeline(orthographic ? 1 : 0, primitive_type == 1 ? 1 : 0);
    if (!pipe) return false;
    vkCmdBindPipeline(s_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
    VkBuffer vbuf = ((vkbuf_t *)renderObject->lpVertexBuffer)->buf;
    VkDeviceSize off = 0;
    vkCmdBindVertexBuffers(s_cmd, 0, 1, &vbuf, &off);
    if (renderObject->lpIndexBuffer)
        vkCmdBindIndexBuffer(s_cmd, ((vkbuf_t *)renderObject->lpIndexBuffer)->buf, 0, VK_INDEX_TYPE_UINT16);
    push_mvp(orthographic ? 1 : 0);

    for (int i = 0; i < renderObject->numTextureGroups; i++) {
        TEXTUREGROUP *group = &renderObject->textureGroups[i];
        int frag[2] = { group->colourkey ? 1 : 0, group->texture ? 1 : 0 };
        vkCmdPushConstants(s_cmd, s_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 64, 8, frag);
        vktex_t *tex = group->texture ? (vktex_t *)group->texture : default_tex();
        vkCmdBindDescriptorSets(s_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, s_layout, 0, 1, &tex->set, 0, NULL);
        if (renderObject->lpIndexBuffer)
            vkCmdDrawIndexed(s_cmd, (uint32_t)group->numTriangles * 3, 1, (uint32_t)group->startIndex, (int32_t)group->startVert, 0);
        else
            vkCmdDraw(s_cmd, (uint32_t)group->numVerts, 1, (uint32_t)group->startVert, 0);
    }
    return true;
}
bool draw_object(RENDEROBJECT *ro) { return draw_render_object(ro, 0, false); }
bool draw_billboard_object(RENDEROBJECT *ro) { return draw_render_object(ro, 0, false); }
bool draw_2d_object(RENDEROBJECT *ro) { return draw_render_object(ro, 0, true); }
bool draw_line_object(RENDEROBJECT *ro) { return draw_render_object(ro, 1, false); }
