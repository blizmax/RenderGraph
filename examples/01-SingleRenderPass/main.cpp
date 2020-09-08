#include "Application.hpp"

#include <GLFW/glfw3.h>

#if _WIN32
#	define GLFW_EXPOSE_NATIVE_WIN32
#else
#	define GLFW_EXPOSE_NATIVE_X11
#endif

#include <GLFW/glfw3native.h>

#include <iostream>
#include <string>

struct LayerExtensionList
{
	VkLayerProperties layerProperties;
	std::vector< VkExtensionProperties > extensionProperties;
};
using LayerExtensionListArray = std::vector< LayerExtensionList >;

std::vector< VkLayerProperties > enumerateLayerProperties( PFN_vkEnumerateInstanceLayerProperties enumLayerProperties );
std::vector< VkExtensionProperties > enumerateExtensionProperties( PFN_vkEnumerateInstanceExtensionProperties enumInstanceExtensionProperties
	, std::string const & layerName );

void onWindowResized( GLFWwindow * window, int width, int height );
static VkBool32 VKAPI_PTR DbgCallback( VkDebugReportFlagsEXT msgFlags
	, VkDebugReportObjectTypeEXT objType
	, uint64_t srcObject
	, size_t location
	, int32_t msgCode
	, const char *pLayerPrefix
	, const char * pMsg
	, void * pUserData );

int main( int argc, char * argv[] )
{
	Application app;

	try
	{
		PFN_vkEnumerateInstanceLayerProperties enumLayerProperties;
		enumLayerProperties = ( PFN_vkEnumerateInstanceLayerProperties )vkGetInstanceProcAddr( VK_NULL_HANDLE,
			"vkEnumerateInstanceLayerProperties" );
		PFN_vkEnumerateInstanceExtensionProperties enumInstanceExtensionProperties;
		enumInstanceExtensionProperties = ( PFN_vkEnumerateInstanceExtensionProperties )vkGetInstanceProcAddr( VK_NULL_HANDLE,
			"vkEnumerateInstanceExtensionProperties" );

		std::vector< const char * > instanceLayers;
		auto globalLayerProperties = enumerateLayerProperties( enumLayerProperties );
		std::vector< VkExtensionProperties > globalExtensions;
		LayerExtensionListArray globalLayers( globalLayerProperties.size() );

		for ( uint32_t i = 0; i < globalLayerProperties.size(); ++i )
		{
			VkLayerProperties & srcInfo = globalLayerProperties[i];
			instanceLayers.push_back( srcInfo.layerName );
			auto & dstInfo = globalLayers[i];
			dstInfo.layerProperties = srcInfo;

			// Gets layer extensions, if first parameter is not NULL
			dstInfo.extensionProperties = enumerateExtensionProperties( enumInstanceExtensionProperties
				, srcInfo.layerName );
		}

		// Gets instance extensions, if no layer was specified in the first paramteter
		globalExtensions = enumerateExtensionProperties( enumInstanceExtensionProperties
			, std::string{} );

		VkApplicationInfo appInfo
		{
			VK_STRUCTURE_TYPE_APPLICATION_INFO,
			nullptr,
			"Single Render Pass test",
			VK_MAKE_VERSION( 1, 0, 0 ),
			"Debug",
			VK_MAKE_VERSION( 1, 0, 0 ),
			VK_API_VERSION_1_0,
		};

		// Get all supported Instance extensions (excl. layer-provided ones)
		std::vector< const char * > instanceExtensions;

		for ( auto & ext : globalExtensions )
		{
			instanceExtensions.push_back( ext.extensionName );
		}

		VkInstanceCreateInfo instInfo
		{
			VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			nullptr,
			0u,
			&appInfo,
			0u,
			nullptr,
			uint32_t( instanceExtensions.size() ),
			instanceExtensions.data(),
		};

		// With that informations, we can now create the instance.
		auto res = vkCreateInstance( &instInfo
			, nullptr
			, &app.instance );
		checkError( res, "Instance creation" );

		VkDebugReportCallbackCreateInfoEXT dbgInfo
		{
			VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
			nullptr,
			VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT,
			DbgCallback,
			nullptr,
		};

		std::vector< VkPhysicalDevice > gpus = vkGetArray( app.instance
			, vkEnumeratePhysicalDevices );

		if ( gpus.empty() )
		{
			throw std::runtime_error{ "No available physical device" };
		}

		app.gpu = gpus[0];

		// Now we need a window.
		static constexpr uint32_t width = 800u;
		static constexpr uint32_t height = 600u;
		glfwInit();
		glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );
		GLFWwindow * window = glfwCreateWindow( int( width )
			, int( height )
			, "01 - SingleRenderPass"
			, nullptr
			, nullptr );
		glfwSetWindowUserPointer( window, &app );
		glfwSetWindowSizeCallback( window, onWindowResized );

		// We retrieve this window's native handle, and create the surface from it.
#if _WIN32
		VkWin32SurfaceCreateInfoKHR createInfo
		{
			VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
			nullptr,
			0u,
			nullptr,
			glfwGetWin32Window( window )
		};
		res = vkCreateWin32SurfaceKHR( app.instance
			, &createInfo
			, nullptr
			, &app.surface );

		if ( res != VK_SUCCESS )
		{
			vkDestroyInstance( app.instance, nullptr );
			checkError( res, "Surface creation" );
		}
#else
		auto display = glfwGetX11Display();
		auto drawable = glfwGetX11Window( window );
		auto handle = VkWindowHandle{ std::make_unique< VkIXWindowHandle >( drawable, display ) };
#endif

		// We now create the logical device, using this surface
		app.device = createDevice( app );

		if ( res != VK_SUCCESS )
		{
			vkDestroySurfaceKHR( app.instance, app.surface, nullptr );
			vkDestroyInstance( app.instance, nullptr );
			checkError( res, "Device creation" );
		}

		vkGetDeviceQueue( app.device, app.graphicsQueueFamilyIndex, 0u, &app.graphicsQueue );
		vkGetDeviceQueue( app.device, app.presentQueueFamilyIndex, 0u, &app.presentQueue );
		VkCommandPoolCreateInfo poolInfo
		{
			VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			nullptr,
			VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			app.graphicsQueueFamilyIndex,
		};
		vkCreateCommandPool( app.device, &poolInfo, nullptr, &app.commandPool );

		// Create the swapchain and set it up.
		app.dimensions.width = width;
		app.dimensions.height = height;
		createSwapChain( app );

		// We retrieve the render pass that we'll be using to do our stuff on the swapchain surface.
		app.renderPass = createRenderPass( app );

		// From all those things, we can now prepare our frames (one per framebuffer).
		prepareFrames( app );

		// And let's roll !!
		while ( !glfwWindowShouldClose( window ) )
		{
			glfwPollEvents();

			// Acquire the next frame to present.
			auto resources = getResources( app );

			if ( resources )
			{
				// Submit the command buffer to the graphics queue.
				VkPipelineStageFlags flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				VkSubmitInfo submit
				{
					VK_STRUCTURE_TYPE_SUBMIT_INFO,
					nullptr,
					1u,
					&resources->imageAvailableSemaphore,
					&flags,
					1u,
					&app.commandBuffers[resources->imageIndex],
					1u,
					&resources->finishedRenderingSemaphore,
				};
				vkQueueSubmit( app.graphicsQueue
					, 1u
					, &submit
					, VK_NULL_HANDLE );

				// And we present the frame to the swap chain surface.
				VkPresentInfoKHR present
				{
					VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
					nullptr,
					1u,
					&resources->finishedRenderingSemaphore,
					1u,
					&app.swapChain,
					&resources->imageIndex,
					nullptr
				};
				res = vkQueuePresentKHR( app.presentQueue
					, &present );

				if ( res != VK_SUCCESS )
				{
					// Swapchain reset management.
					checkNeedReset( app
						, res
						, false
						, "Image presentation" );
				}

				resources->imageIndex = ~0u;
			}
		}
	}
	catch ( std::exception & exc )
	{
		std::cerr << "Initialisation failed during the following step: " << exc.what() << std::endl;
	}

	cleanup( app );
	glfwTerminate();
	vkDestroyInstance( app.instance
		, nullptr );
	app.instance = VK_NULL_HANDLE;
}

std::vector< VkLayerProperties > enumerateLayerProperties( PFN_vkEnumerateInstanceLayerProperties enumLayerProperties )
{
	if ( !enumLayerProperties )
	{
		return {};
	}

	uint32_t count;
	std::vector< VkLayerProperties > result;
	VkResult res;

	do
	{
		res = enumLayerProperties( &count, nullptr );

		if ( count )
		{
			result.resize( count );
			res = enumLayerProperties( &count, result.data() );
		}
	}
	while ( res == VK_INCOMPLETE );

	checkError( res, "Instance layers retrieval" );
	return result;
}

std::vector< VkExtensionProperties > enumerateExtensionProperties( PFN_vkEnumerateInstanceExtensionProperties enumInstanceExtensionProperties
	, std::string const & layerName )
{
	uint32_t count;
	std::vector< VkExtensionProperties > result;
	VkResult res;

	do
	{
		res = enumInstanceExtensionProperties( layerName.empty() ? nullptr : layerName.c_str()
			, &count
			, nullptr );

		if ( count )
		{
			result.resize( count );
			res = enumInstanceExtensionProperties( layerName.empty() ? nullptr : layerName.c_str()
				, &count
				, result.data() );
		}
	}
	while ( res == VK_INCOMPLETE );

	checkError( res, "Instance layer extensions retrieval" );
	return result;
}

void onWindowResized( GLFWwindow * window, int width, int height )
{
	auto * app = reinterpret_cast< Application * >( glfwGetWindowUserPointer( window ) );
	app->dimensions.width = width;
	app->dimensions.height = height;
	resetSwapChain( *app );
}

static VkBool32 VKAPI_PTR DbgCallback( VkDebugReportFlagsEXT msgFlags
	, VkDebugReportObjectTypeEXT objType
	, uint64_t srcObject
	, size_t location
	, int32_t msgCode
	, const char * pLayerPrefix
	, const char *pMsg
	, void *pUserData )
{
	std::cerr << msgFlags << ": [" << pLayerPrefix << "] Code " << msgCode << " : " << pMsg << std::endl;

	// True is reserved for layer developers, and MAY mean calls are not distributed down the layer chain after validation error.
	// False SHOULD always be returned by apps:
	return false;
}
