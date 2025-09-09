#include <iostream>
#include <vector>

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <algorithm>
#include <fstream>
#include <optional>
#include <set>

#include "glm/glm.hpp"
#include "vulkan/vulkan.h"

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const int MAX_FRAMES_IN_FLIGHT = 2;

#define b_qualify_vk(x)                                                          \
  do {                                                                           \
    VkResult ret = x;                                                            \
    if (ret != VK_SUCCESS) {                                                     \
      std::cout << "[QualifyVK] " << #x << " failed with: " << ret << std::endl; \
      return false;                                                              \
    }                                                                            \
  } while (0)

#define v_qualify_vk(x)                                                          \
  do {                                                                           \
    VkResult ret = x;                                                            \
    if (ret != VK_SUCCESS) {                                                     \
      std::cout << "[QualifyVK] " << #x << " failed with: " << ret << std::endl; \
      return;                                                                    \
    }                                                                            \
  } while (0)

#define d_qualify_vk(x)                                                          \
  do {                                                                           \
    VkResult ret = x;                                                            \
    if (ret != VK_SUCCESS) {                                                     \
      std::cout << "[QualifyVK] " << #x << " failed with: " << ret << std::endl; \
      return {};                                                                 \
    }                                                                            \
  } while (0)

#define vk_get_proc(instance, name)                            \
  do {                                                         \
    name = (PFN_##name)vkGetInstanceProcAddr(instance, #name); \
  } while (0)

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                    VkDebugUtilsMessageTypeFlagsEXT type,
                                                    const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data,
                                                    void* p_user_data) {

  std::cerr << "[VkProgram-Val] " << p_callback_data->pMessage << std::endl;

  return VK_FALSE;
}

class Program {
 public:
  Program(GLFWwindow* glfw_window) : m_glfw_window(glfw_window) {};

  bool Init() {
    {  //Vulkan instance initialization
      std::vector<const char*> v_enabled_layers{};

      {  //Vulkan validation layers
        const char* s_validation_layer_name = []() -> const char* {
          uint32_t un_layer_count;
          d_qualify_vk(vkEnumerateInstanceLayerProperties(&un_layer_count, nullptr));

          std::vector<VkLayerProperties> v_available_layers(un_layer_count);
          d_qualify_vk(vkEnumerateInstanceLayerProperties(&un_layer_count, v_available_layers.data()));

          std::vector<const char*> v_validation_layer_names = {"VK_LAYER_KHRONOS_validation",
                                                               "VK_LAYER_LUNARG_standard_validation"};

          for (const auto& s_validation_layer_name : v_validation_layer_names) {
            for (const auto& layer_properties : v_available_layers) {
              if (strcmp(s_validation_layer_name, layer_properties.layerName) == 0) {
                return s_validation_layer_name;
              }
            }
          }

          std::cout << "[Program] Could not find a validation layer!" << std::endl;
          return nullptr;
        }();

        if (s_validation_layer_name) {
          v_enabled_layers.push_back(s_validation_layer_name);
        }
      }

      {  //Create vulkan instance
        VkApplicationInfo application_info = {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName = "Hello Vulkan",
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName = "danwillm",
            .engineVersion = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion = VK_API_VERSION_1_0,
        };

        uint32_t un_glfw_extension_count = 0;
        const char** pp_glfw_extensions = glfwGetRequiredInstanceExtensions(&un_glfw_extension_count);

        std::vector<const char*> v_extensions(pp_glfw_extensions, pp_glfw_extensions + un_glfw_extension_count);
        v_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        VkInstanceCreateInfo instance_create_info = {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .flags = 0,
            .pApplicationInfo = &application_info,
            .enabledLayerCount = (uint32_t)v_enabled_layers.size(),
            .ppEnabledLayerNames = v_enabled_layers.data(),
            .enabledExtensionCount = (uint32_t)v_extensions.size(),
            .ppEnabledExtensionNames = v_extensions.data(),
        };
        b_qualify_vk(vkCreateInstance(&instance_create_info, nullptr, &m_vkinstance));
      }

      {  //extension proc address registration
        vk_get_proc(m_vkinstance, vkCreateDebugUtilsMessengerEXT);
        vk_get_proc(m_vkinstance, vkDestroyDebugUtilsMessengerEXT);
      }

      {  //debug utils
        VkDebugUtilsMessengerCreateInfoEXT debug_utils_messenger_create_info = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .flags = 0,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = DebugCallback,
            .pUserData = nullptr,
        };
        b_qualify_vk(vkCreateDebugUtilsMessengerEXT(m_vkinstance, &debug_utils_messenger_create_info, nullptr,
                                                    &m_vkdebug_utils_messenger));
      }

      //create surface
      b_qualify_vk(glfwCreateWindowSurface(m_vkinstance, m_glfw_window, nullptr, &m_vksurface));

      {  //pick physical device
        uint32_t un_device_count = 0;
        b_qualify_vk(vkEnumeratePhysicalDevices(m_vkinstance, &un_device_count, nullptr));

        if (un_device_count == 0) {
          std::cerr << "No vulkan physical devices!" << std::endl;
          return false;
        }

        std::vector<VkPhysicalDevice> v_physical_devices(un_device_count);
        b_qualify_vk(vkEnumeratePhysicalDevices(m_vkinstance, &un_device_count, v_physical_devices.data()));

        m_vkphysical_device = v_physical_devices.front();
      }

      struct QueueFamilyIndices {
        std::optional<uint32_t> opt_graphics_family;
        std::optional<uint32_t> opt_present_family;

        bool isComplete() { return opt_graphics_family.has_value() && opt_present_family.has_value(); }
      };
      QueueFamilyIndices queue_family_indices{};

      {  // logical device creation

        uint32_t un_queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(m_vkphysical_device, &un_queue_family_count, nullptr);

        std::vector<VkQueueFamilyProperties> v_queue_family_properties(un_queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(m_vkphysical_device, &un_queue_family_count,
                                                 v_queue_family_properties.data());

        for (uint32_t i = 0; i < v_queue_family_properties.size(); i++) {
          if (v_queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            queue_family_indices.opt_graphics_family = i;
          }

          VkBool32 presentSupport = false;
          vkGetPhysicalDeviceSurfaceSupportKHR(m_vkphysical_device, i, m_vksurface, &presentSupport);
          if (presentSupport) {
            queue_family_indices.opt_present_family = i;
          }

          if (queue_family_indices.isComplete()) {
            break;
          }
        }

        if (!queue_family_indices.isComplete()) {
          std::cerr << "Could not find complete queue family!" << std::endl;
          return false;
        }

        std::set<uint32_t> set_unique_queue_families = {queue_family_indices.opt_graphics_family.value(),
                                                        queue_family_indices.opt_present_family.value()};

        float f_queue_priorities = 1.f;
        std::vector<VkDeviceQueueCreateInfo> v_queue_create_infos{};
        for (uint32_t queue_family : set_unique_queue_families) {
          VkDeviceQueueCreateInfo queue_create_info = {
              .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
              .flags = 0,
              .queueFamilyIndex = queue_family,
              .queueCount = 1,
              .pQueuePriorities = &f_queue_priorities,
          };
          v_queue_create_infos.push_back(queue_create_info);
        }

        const std::vector<const char*> v_device_extensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        };

        VkPhysicalDeviceFeatures deviceFeatures{};
        VkDeviceCreateInfo device_create_info = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueCreateInfoCount = (uint32_t)v_queue_create_infos.size(),
            .pQueueCreateInfos = v_queue_create_infos.data(),
            .enabledLayerCount = 0,
            .ppEnabledLayerNames = nullptr,
            .enabledExtensionCount = (uint32_t)v_device_extensions.size(),
            .ppEnabledExtensionNames = v_device_extensions.data(),
            .pEnabledFeatures = &deviceFeatures,
        };
        b_qualify_vk(vkCreateDevice(m_vkphysical_device, &device_create_info, nullptr, &m_vkdevice));

        vkGetDeviceQueue(m_vkdevice, queue_family_indices.opt_graphics_family.value(), 0, &m_vkgraphics_queue);
        vkGetDeviceQueue(m_vkdevice, queue_family_indices.opt_present_family.value(), 0, &m_vkpresent_queue);
      }

      {  //swapchain creation
        struct SwapchainSupportDetails {
          VkSurfaceCapabilitiesKHR v_capabilities;
          std::vector<VkSurfaceFormatKHR> v_formats;
          std::vector<VkPresentModeKHR> v_present_modes;
        };

        SwapchainSupportDetails swapchain_support{};
        b_qualify_vk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_vkphysical_device, m_vksurface,
                                                               &swapchain_support.v_capabilities));

        uint32_t un_format_count;
        b_qualify_vk(vkGetPhysicalDeviceSurfaceFormatsKHR(m_vkphysical_device, m_vksurface, &un_format_count, nullptr));

        if (un_format_count != 0) {
          swapchain_support.v_formats.resize(un_format_count);
          b_qualify_vk(vkGetPhysicalDeviceSurfaceFormatsKHR(m_vkphysical_device, m_vksurface, &un_format_count,
                                                            swapchain_support.v_formats.data()));
        }

        uint32_t un_present_mode_count;
        b_qualify_vk(vkGetPhysicalDeviceSurfacePresentModesKHR(m_vkphysical_device, m_vksurface, &un_present_mode_count,
                                                               nullptr));

        if (un_present_mode_count != 0) {
          swapchain_support.v_present_modes.resize(un_present_mode_count);
          b_qualify_vk(vkGetPhysicalDeviceSurfacePresentModesKHR(
              m_vkphysical_device, m_vksurface, &un_present_mode_count, swapchain_support.v_present_modes.data()));
        }

        //choose format
        m_swapchain_format = swapchain_support.v_formats[0];
        for (const auto& available_surface_format : swapchain_support.v_formats) {
          if (available_surface_format.format == VK_FORMAT_B8G8R8A8_SRGB &&
              available_surface_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            m_swapchain_format = available_surface_format;
            break;
          }
        }

        //choose present mode
        VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
        for (const auto& available_present_mode : swapchain_support.v_present_modes) {
          if (available_present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            present_mode = available_present_mode;
          }
        }

        {  //set swapchain extent
          if (swapchain_support.v_capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            m_swapchain_extent = swapchain_support.v_capabilities.currentExtent;
          } else {
            int n_width, n_height;
            glfwGetFramebufferSize(m_glfw_window, &n_width, &n_height);

            m_swapchain_extent = {(uint32_t)n_width, (uint32_t)n_height};

            m_swapchain_extent.width =
                std::clamp(m_swapchain_extent.width, swapchain_support.v_capabilities.minImageExtent.width,
                           swapchain_support.v_capabilities.maxImageExtent.width);
            m_swapchain_extent.height =
                std::clamp(m_swapchain_extent.height, swapchain_support.v_capabilities.minImageExtent.height,
                           swapchain_support.v_capabilities.maxImageExtent.height);
          }
        }

        //set image count
        uint32_t un_image_count = swapchain_support.v_capabilities.minImageCount + 1;
        if (swapchain_support.v_capabilities.maxImageCount > 0 &&
            un_image_count > swapchain_support.v_capabilities.maxImageCount) {
          un_image_count = swapchain_support.v_capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR swapchain_create_info = {
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .pNext = nullptr,
            .flags = 0,
            .surface = m_vksurface,
            .minImageCount = un_image_count,
            .imageFormat = m_swapchain_format.format,
            .imageColorSpace = m_swapchain_format.colorSpace,
            .imageExtent = m_swapchain_extent,
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
            .preTransform = swapchain_support.v_capabilities.currentTransform,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode = present_mode,
            .clipped = VK_TRUE,
            .oldSwapchain = VK_NULL_HANDLE,
        };

        b_qualify_vk(vkCreateSwapchainKHR(m_vkdevice, &swapchain_create_info, nullptr, &m_vkswapchain));

        b_qualify_vk(vkGetSwapchainImagesKHR(m_vkdevice, m_vkswapchain, &un_image_count, nullptr));
        m_swapchain_images.resize(un_image_count);

        b_qualify_vk(vkGetSwapchainImagesKHR(m_vkdevice, m_vkswapchain, &un_image_count, m_swapchain_images.data()));

        m_swapchain_image_views.resize(m_swapchain_images.size());
        for (size_t i = 0; i < m_swapchain_images.size(); i++) {
          VkImageViewCreateInfo image_view_create_info = {
              .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
              .pNext = nullptr,
              .flags = 0,
              .image = m_swapchain_images[i],
              .viewType = VK_IMAGE_VIEW_TYPE_2D,
              .format = m_swapchain_format.format,
              .components =
                  {
                      .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                      .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                      .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                      .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                  },
              .subresourceRange =
                  {
                      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                      .baseMipLevel = 0,
                      .levelCount = 1,
                      .baseArrayLayer = 0,
                      .layerCount = 1,
                  },
          };
          b_qualify_vk(vkCreateImageView(m_vkdevice, &image_view_create_info, nullptr, &m_swapchain_image_views[i]));
        }
      }

      {  //render pass
        VkAttachmentDescription color_attachment_description = {
            .flags = 0,
            .format = m_swapchain_format.format,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        };

        VkAttachmentReference color_attachment_reference = {
            .attachment = 0,
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };
        VkSubpassDescription subpass_description = {
            .flags = 0,
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .inputAttachmentCount = 0,
            .pInputAttachments = nullptr,
            .colorAttachmentCount = 1,
            .pColorAttachments = &color_attachment_reference,
            .pResolveAttachments = nullptr,
            .pDepthStencilAttachment = nullptr,
            .preserveAttachmentCount = 0,
            .pPreserveAttachments = nullptr,
        };

        VkSubpassDependency subpass_dependency = {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = 0,
        };

        VkRenderPassCreateInfo render_pass_create_info = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .attachmentCount = 1,
            .pAttachments = &color_attachment_description,
            .subpassCount = 1,
            .pSubpasses = &subpass_description,
            .dependencyCount = 1,
            .pDependencies = &subpass_dependency,
        };
        b_qualify_vk(vkCreateRenderPass(m_vkdevice, &render_pass_create_info, nullptr, &m_renderpass));
      }

      {  // create pipeline
        auto ReadFile = [&](const std::string& filename) -> std::vector<char> {
          std::ifstream file(filename, std::ios::ate | std::ios::binary);

          if (!file.is_open()) {
            return {};
          }

          size_t n_file_size = static_cast<size_t>(file.tellg());
          std::vector<char> buffer(n_file_size);

          file.seekg(0);
          file.read(buffer.data(), n_file_size);

          return buffer;
        };

        auto vert_shader = ReadFile("shaders/hello.vert.spv");
        auto frag_shader = ReadFile("shaders/hello.frag.spv");

        auto CreateShaderModule = [](VkDevice device, size_t size_buffer, const std::vector<char>& v_buffer,
                                     VkShaderModule& out_vk_shader_module) {
          VkShaderModuleCreateInfo vk_shader_module_create_info = {
              .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
              .pNext = nullptr,
              .flags = 0,
              .codeSize = size_buffer,
              .pCode = reinterpret_cast<const uint32_t*>(v_buffer.data()),
          };

          b_qualify_vk(vkCreateShaderModule(device, &vk_shader_module_create_info, nullptr, &out_vk_shader_module));

          return true;
        };

        if (!CreateShaderModule(m_vkdevice, vert_shader.size(), vert_shader, m_vert_shader)) {
          std::cout << "Failed to create vertex shader module" << std::endl;
          return false;
        }

        if (!CreateShaderModule(m_vkdevice, frag_shader.size(), frag_shader, m_frag_shader)) {
          std::cout << "Failed to create fragment shader module" << std::endl;
          return false;
        }

        VkPipelineShaderStageCreateInfo vert_shader_stage_create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = m_vert_shader,
            .pName = "main",
            .pSpecializationInfo = nullptr,
        };
        VkPipelineShaderStageCreateInfo frag_shader_stage_create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = m_frag_shader,
            .pName = "main",
            .pSpecializationInfo = nullptr,
        };

        VkPipelineShaderStageCreateInfo shader_stage_create_infos[] = {vert_shader_stage_create_info,
                                                                       frag_shader_stage_create_info};

        VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .vertexBindingDescriptionCount = 0,
            .pVertexBindingDescriptions = nullptr,
            .vertexAttributeDescriptionCount = 0,
            .pVertexAttributeDescriptions = nullptr,
        };

        VkPipelineInputAssemblyStateCreateInfo input_assembly_state_create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE,
        };

        VkViewport viewport = {
            .x = 0.f,
            .y = 0.f,
            .width = (float)m_swapchain_extent.width,
            .height = (float)m_swapchain_extent.height,
            .minDepth = 0.f,
            .maxDepth = 1.f,
        };

        VkRect2D scissor = {
            .offset = {0, 0},
            .extent = m_swapchain_extent,
        };

        std::vector<VkDynamicState> v_dynamic_states = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };

        VkPipelineDynamicStateCreateInfo dynamic_state_create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .dynamicStateCount = static_cast<uint32_t>(v_dynamic_states.size()),
            .pDynamicStates = v_dynamic_states.data(),
        };

        VkPipelineViewportStateCreateInfo viewport_state_create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .viewportCount = 1,
            .pViewports = &viewport,
            .scissorCount = 1,
            .pScissors = &scissor,
        };

        VkPipelineRasterizationStateCreateInfo rasterization_state_create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .depthClampEnable = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_POLYGON_MODE_FILL,
            .frontFace = VK_FRONT_FACE_CLOCKWISE,
            .depthBiasEnable = VK_FALSE,
            .depthBiasConstantFactor = 0.f,
            .depthBiasClamp = 0.f,
            .depthBiasSlopeFactor = 0.f,
            .lineWidth = 1.f,
        };

        VkPipelineMultisampleStateCreateInfo multisample_state_create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable = VK_FALSE,
            .minSampleShading = 1.f,
            .pSampleMask = nullptr,
            .alphaToCoverageEnable = VK_FALSE,
            .alphaToOneEnable = VK_FALSE,
        };

        VkPipelineColorBlendAttachmentState color_blend_attachment_state = {
            .blendEnable = VK_FALSE,
            .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
            .colorBlendOp = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
            .alphaBlendOp = VK_BLEND_OP_ADD,
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                              VK_COLOR_COMPONENT_A_BIT,
        };
        VkPipelineColorBlendStateCreateInfo color_blend_state_create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .logicOpEnable = VK_FALSE,
            .logicOp = VK_LOGIC_OP_COPY,
            .attachmentCount = 1,
            .pAttachments = &color_blend_attachment_state,
            .blendConstants = {0.f, 0.f, 0.f, 0.f},
        };

        VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .setLayoutCount = 0,
            .pSetLayouts = nullptr,
            .pushConstantRangeCount = 0,
            .pPushConstantRanges = nullptr,
        };

        b_qualify_vk(vkCreatePipelineLayout(m_vkdevice, &pipeline_layout_create_info, nullptr, &m_pipeline_layout));

        VkGraphicsPipelineCreateInfo pipeline_create_info = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stageCount = 2,
            .pStages = shader_stage_create_infos,
            .pVertexInputState = &vertex_input_state_create_info,
            .pInputAssemblyState = &input_assembly_state_create_info,
            .pTessellationState = nullptr,
            .pViewportState = &viewport_state_create_info,
            .pRasterizationState = &rasterization_state_create_info,
            .pMultisampleState = &multisample_state_create_info,
            .pDepthStencilState = nullptr,
            .pColorBlendState = &color_blend_state_create_info,
            .pDynamicState = &dynamic_state_create_info,
            .layout = m_pipeline_layout,
            .renderPass = m_renderpass,
            .subpass = 0,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = -1,
        };

        b_qualify_vk(
            vkCreateGraphicsPipelines(m_vkdevice, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &m_pipeline));
      }

      {  //framebuffers
        m_swapchain_framebuffers.resize(m_swapchain_image_views.size());

        for (size_t i = 0; i < m_swapchain_image_views.size(); i++) {
          VkImageView attachments[] = {
              m_swapchain_image_views[i],
          };

          VkFramebufferCreateInfo framebuffer_create_info = {
              .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
              .pNext = nullptr,
              .flags = 0,
              .renderPass = m_renderpass,
              .attachmentCount = 1,
              .pAttachments = attachments,
              .width = m_swapchain_extent.width,
              .height = m_swapchain_extent.height,
              .layers = 1,
          };

          b_qualify_vk(
              vkCreateFramebuffer(m_vkdevice, &framebuffer_create_info, nullptr, &m_swapchain_framebuffers[i]));
        }
      }

      {  // command pool
        VkCommandPoolCreateInfo cmd_pool_create_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = queue_family_indices.opt_graphics_family.value(),
        };
        b_qualify_vk(vkCreateCommandPool(m_vkdevice, &cmd_pool_create_info, nullptr, &m_vkcommand_pool));
      }

      {  // create command buffers
        mv_vkcommand_buffers.resize(MAX_FRAMES_IN_FLIGHT);

        VkCommandBufferAllocateInfo cmd_buffer_allocate_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = m_vkcommand_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = static_cast<uint32_t>(mv_vkcommand_buffers.size()),
        };
        b_qualify_vk(vkAllocateCommandBuffers(m_vkdevice, &cmd_buffer_allocate_info, mv_vkcommand_buffers.data()));
      }

      {  //create sync objects
        mv_vksemaphores_image_available.resize(m_swapchain_images.size());
        mv_vksemaphores_render_finished.resize(m_swapchain_images.size());
        mv_vkfences_in_flight.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphore_create_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
        };
        VkFenceCreateInfo fence_create_info = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };

        for (size_t i = 0; i < m_swapchain_images.size(); i++) {
          b_qualify_vk(
              vkCreateSemaphore(m_vkdevice, &semaphore_create_info, nullptr, &mv_vksemaphores_image_available[i]));

          b_qualify_vk(
              vkCreateSemaphore(m_vkdevice, &semaphore_create_info, nullptr, &mv_vksemaphores_render_finished[i]));
        }

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
          b_qualify_vk(vkCreateFence(m_vkdevice, &fence_create_info, nullptr, &mv_vkfences_in_flight[i]));
        }
      }

      return true;
    }
  }

  void Tick() {
    vkWaitForFences(m_vkdevice, 1, &mv_vkfences_in_flight[m_uncurrent_frame], VK_TRUE, UINT64_MAX);
    vkResetFences(m_vkdevice, 1, &mv_vkfences_in_flight[m_uncurrent_frame]);

    //acquire image from swapchain
    uint32_t un_image_index;
    v_qualify_vk(vkAcquireNextImageKHR(m_vkdevice, m_vkswapchain, UINT64_MAX,
                                       mv_vksemaphores_image_available[m_uncurrent_frame], VK_NULL_HANDLE,
                                       &un_image_index));

    {  //record command buffer
      v_qualify_vk(vkResetCommandBuffer(mv_vkcommand_buffers[m_uncurrent_frame], 0));

      VkCommandBufferBeginInfo begin_info = {
          .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
          .pNext = nullptr,
          .flags = 0,
          .pInheritanceInfo = nullptr,
      };
      v_qualify_vk(vkBeginCommandBuffer(mv_vkcommand_buffers[m_uncurrent_frame], &begin_info));

      VkClearValue clear_value = {
          .color =
              {
                  .float32 = {0.f, 0.f, 0.f, 1.f},
              },
      };
      VkRenderPassBeginInfo render_pass_begin_info = {
          .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
          .pNext = nullptr,
          .renderPass = m_renderpass,
          .framebuffer = m_swapchain_framebuffers[un_image_index],
          .renderArea =
              {
                  .offset = {0, 0},
                  .extent = m_swapchain_extent,
              },
          .clearValueCount = 1,
          .pClearValues = &clear_value,
      };
      vkCmdBeginRenderPass(mv_vkcommand_buffers[m_uncurrent_frame], &render_pass_begin_info,
                           VK_SUBPASS_CONTENTS_INLINE);

      vkCmdBindPipeline(mv_vkcommand_buffers[m_uncurrent_frame], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

      VkViewport viewport = {
          .x = 0.f,
          .y = 0.f,
          .width = static_cast<float>(m_swapchain_extent.width),
          .height = static_cast<float>(m_swapchain_extent.height),
          .minDepth = 0.f,
          .maxDepth = 1.f,
      };
      vkCmdSetViewport(mv_vkcommand_buffers[m_uncurrent_frame], 0, 1, &viewport);

      VkRect2D scissor = {
          .offset = {0, 0},
          .extent = m_swapchain_extent,
      };
      vkCmdSetScissor(mv_vkcommand_buffers[m_uncurrent_frame], 0, 1, &scissor);

      vkCmdDraw(mv_vkcommand_buffers[m_uncurrent_frame], 3, 1, 0, 0);

      vkCmdEndRenderPass(mv_vkcommand_buffers[m_uncurrent_frame]);
      v_qualify_vk(vkEndCommandBuffer(mv_vkcommand_buffers[m_uncurrent_frame]));
    }

    VkSemaphore signal_semaphores[] = {mv_vksemaphores_render_finished[un_image_index]};
    {  //submit
      VkSemaphore wait_semaphores[] = {mv_vksemaphores_image_available[m_uncurrent_frame]};
      VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
      VkSubmitInfo submit_info = {
          .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
          .pNext = nullptr,
          .waitSemaphoreCount = 1,
          .pWaitSemaphores = wait_semaphores,
          .pWaitDstStageMask = wait_stages,
          .commandBufferCount = 1,
          .pCommandBuffers = &mv_vkcommand_buffers[m_uncurrent_frame],
          .signalSemaphoreCount = 1,
          .pSignalSemaphores = signal_semaphores,
      };
      v_qualify_vk(vkQueueSubmit(m_vkgraphics_queue, 1, &submit_info, mv_vkfences_in_flight[m_uncurrent_frame]));
    }

    {  //present
      VkSwapchainKHR swapchains[] = {m_vkswapchain};

      VkPresentInfoKHR present_info = {
          .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
          .pNext = nullptr,
          .waitSemaphoreCount = 1,
          .pWaitSemaphores = signal_semaphores,
          .swapchainCount = 1,
          .pSwapchains = swapchains,
          .pImageIndices = &un_image_index,
          .pResults = nullptr,
      };
      v_qualify_vk(vkQueuePresentKHR(m_vkpresent_queue, &present_info));
    }

    m_uncurrent_frame = (m_uncurrent_frame + 1) % MAX_FRAMES_IN_FLIGHT;
  }

  ~Program() {
    vkDeviceWaitIdle(m_vkdevice);

    for (VkSemaphore semaphore : mv_vksemaphores_image_available) {
      vkDestroySemaphore(m_vkdevice, semaphore, nullptr);
    }

    for (VkSemaphore semaphore : mv_vksemaphores_render_finished) {
      vkDestroySemaphore(m_vkdevice, semaphore, nullptr);
    }

    for (VkFence fence : mv_vkfences_in_flight) {
      vkDestroyFence(m_vkdevice, fence, nullptr);
    }

    vkDestroyCommandPool(m_vkdevice, m_vkcommand_pool, nullptr);

    for (auto framebuffer : m_swapchain_framebuffers) {
      vkDestroyFramebuffer(m_vkdevice, framebuffer, nullptr);
    }

    vkDestroyPipeline(m_vkdevice, m_pipeline, nullptr);
    vkDestroyPipelineLayout(m_vkdevice, m_pipeline_layout, nullptr);
    vkDestroyRenderPass(m_vkdevice, m_renderpass, nullptr);

    vkDestroyShaderModule(m_vkdevice, m_vert_shader, nullptr);
    vkDestroyShaderModule(m_vkdevice, m_frag_shader, nullptr);

    for (auto image_view : m_swapchain_image_views) {
      vkDestroyImageView(m_vkdevice, image_view, nullptr);
    }

    vkDestroySwapchainKHR(m_vkdevice, m_vkswapchain, nullptr);
    vkDestroyDevice(m_vkdevice, nullptr);
    vkDestroyDebugUtilsMessengerEXT(m_vkinstance, m_vkdebug_utils_messenger, nullptr);
    vkDestroySurfaceKHR(m_vkinstance, m_vksurface, nullptr);
    vkDestroyInstance(m_vkinstance, nullptr);
  }

 private:
  GLFWwindow* m_glfw_window;

  VkInstance m_vkinstance;
  VkDebugUtilsMessengerEXT m_vkdebug_utils_messenger;

  VkPhysicalDevice m_vkphysical_device;
  VkDevice m_vkdevice;

  VkQueue m_vkgraphics_queue;
  VkQueue m_vkpresent_queue;

  VkSurfaceKHR m_vksurface;

  VkSurfaceFormatKHR m_swapchain_format;
  VkExtent2D m_swapchain_extent{};
  VkSwapchainKHR m_vkswapchain;
  std::vector<VkImage> m_swapchain_images;
  std::vector<VkImageView> m_swapchain_image_views;

  std::vector<VkFramebuffer> m_swapchain_framebuffers;

  VkShaderModule m_vert_shader;
  VkShaderModule m_frag_shader;

  VkRenderPass m_renderpass;
  VkPipelineLayout m_pipeline_layout;

  VkPipeline m_pipeline;

  VkCommandPool m_vkcommand_pool;
  std::vector<VkCommandBuffer> mv_vkcommand_buffers;

  std::vector<VkSemaphore> mv_vksemaphores_image_available;
  std::vector<VkSemaphore> mv_vksemaphores_render_finished;
  std::vector<VkFence> mv_vkfences_in_flight;

  uint32_t m_uncurrent_frame = 0;

  PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT;
  PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT;
};

int main() {
  glfwInit();

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

  GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Hello Vulkan", nullptr, nullptr);

  {
    Program program(window);
    if (!program.Init()) {
      return 1;
    }

    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();

      program.Tick();
    }
  }

  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}