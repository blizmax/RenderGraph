/*
This file belongs to RenderGraph.
See LICENSE file in root folder.
*/
#pragma once

#if _WIN32
#	define VK_USE_PLATFORM_WIN32_KHR
#else
#	define VK_USE_PLATFORM_XLIB_KHR
#endif

#include <vulkan/vulkan.h>

#ifdef min
#	undef min
#endif
#ifdef max
#	undef max
#endif

#include <cstdint>
#include <memory>
#include <vector>

struct RenderingResources
{
	RenderingResources( VkSemaphore imageAvailableSemaphore
		, VkSemaphore finishedRenderingSemaphore
		, VkFence fence
		, VkCommandBuffer commandBuffer
		, uint32_t imageIndex )
		: imageAvailableSemaphore{ imageAvailableSemaphore }
		, finishedRenderingSemaphore{ finishedRenderingSemaphore }
		, fence{ fence }
		, commandBuffer{ commandBuffer }
		, imageIndex{ imageIndex }
	{
	}

	VkSemaphore imageAvailableSemaphore;
	VkSemaphore finishedRenderingSemaphore;
	VkFence fence;
	VkCommandBuffer commandBuffer;
	uint32_t imageIndex{ 0u };
};
using RenderingResourcesPtr = std::unique_ptr< RenderingResources >;
using RenderingResourcesArray = std::vector< RenderingResourcesPtr >;

struct Application
{
	VkInstance instance{ VK_NULL_HANDLE };
	VkPhysicalDevice gpu{ VK_NULL_HANDLE };
	uint32_t presentQueueFamilyIndex{};
	uint32_t graphicsQueueFamilyIndex{};
	uint32_t computeQueueFamilyIndex{};
	VkSurfaceKHR surface{ VK_NULL_HANDLE };
	VkDevice device{ VK_NULL_HANDLE };
	VkExtent2D dimensions{};
	VkSwapchainKHR swapChain{ VK_NULL_HANDLE };
	std::vector< VkImage > swapChainImages;
	std::vector< VkImageView > views;
	std::vector< VkFramebuffer > frameBuffers;
	VkCommandPool commandPool{ VK_NULL_HANDLE };
	std::vector< VkCommandBuffer > commandBuffers;
	VkRenderPass renderPass{ VK_NULL_HANDLE };
	VkClearColorValue clearColour{};
	VkQueue graphicsQueue{ VK_NULL_HANDLE };
	VkQueue presentQueue{ VK_NULL_HANDLE };
	RenderingResourcesArray renderingResources;
	uint32_t resourceIndex{};
	VkFormat imageFormat{ VK_FORMAT_UNDEFINED };
};

void cleanup( Application & app );

VkDevice createDevice( Application & application );
void createSwapChain( Application & application );
VkRenderPass createRenderPass( Application & application );
void prepareFrames( Application & application );
RenderingResources * getResources( Application & application );
bool checkNeedReset( Application & application
	, VkResult errCode
	, bool acquisition
	, char const * const action );
void resetSwapChain( Application & application );

void checkError( VkResult errCode
	, char const * const text );

template< typename TypeT, typename DispatchableT >
std::vector< TypeT > vkGetArray( DispatchableT dispatchable
	, VkResult( *getter ) ( DispatchableT, uint32_t *, TypeT * ) )
{
	uint32_t count = 0u;
	auto res = getter( dispatchable, &count, nullptr );
	checkError( res, "Object array retrieval" );
	std::vector< TypeT > result;

	if ( count > 0 )
	{
		result.resize( count );
		res = getter( dispatchable, &count, result.data() );
		checkError( res, "Object array retrieval" );
	}

	return result;
}

template< typename TypeT, typename DispatchableT, typename ObjectT >
std::vector< TypeT > vkGetArray( DispatchableT dispatchable
	, ObjectT object
	, VkResult( *getter ) ( DispatchableT, ObjectT, uint32_t *, TypeT * ) )
{
	uint32_t count = 0u;
	auto res = getter( dispatchable, object, &count, nullptr );
	checkError( res, "Object array retrieval" );

	std::vector< TypeT > result;

	if ( count > 0 )
	{
		result.resize( count );
		res = getter( dispatchable, object, &count, result.data() );
		checkError( res, "Object array retrieval" );
	}

	return result;
}

template< typename TypeT, typename DispatchableT >
std::vector< TypeT > vkGetArray( DispatchableT dispatchable
	, void( *getter ) ( DispatchableT, uint32_t *, TypeT * ) )
{
	uint32_t count = 0u;
	getter( dispatchable, &count, nullptr );
	std::vector< TypeT > result;

	if ( count > 0 )
	{
		result.resize( count );
		getter( dispatchable, &count, result.data() );
	}

	return result;
}

template< typename TypeT, typename DispatchableT, typename ObjectT >
std::vector< TypeT > vkGetArray( DispatchableT dispatchable
	, ObjectT object
	, void( *getter ) ( DispatchableT, ObjectT, uint32_t *, TypeT * ) )
{
	uint32_t count = 0u;
	getter( dispatchable, &count, nullptr );
	std::vector< TypeT > result;

	if ( count > 0 )
	{
		result.resize( count );
		getter( dispatchable, object, &count, result.data() );
	}

	return result;
}

template< typename TypeT, typename DispatchableT, typename CreateInfoT >
TypeT vkCreateObject( DispatchableT dispatchable
	, CreateInfoT const & createInfo
	, VkResult( *creator ) ( DispatchableT, const CreateInfoT *, const VkAllocationCallbacks *, TypeT * ) )
{
	TypeT result;
	auto res = creator( dispatchable
		, &createInfo
		, nullptr
		, &result );
	checkError( res, "Object creation" );
	return result;
}

template< typename TypeT, typename DispatchableT, typename ObjectT, typename CreateInfoT >
TypeT vkCreateObject( DispatchableT dispatchable
	, ObjectT object
	, CreateInfoT const & createInfo
	, VkResult( *creator ) ( DispatchableT, ObjectT, const CreateInfoT *, const VkAllocationCallbacks *, TypeT * ) )
{
	TypeT result;
	auto res = creator( dispatchable
		, object
		, &createInfo
		, nullptr
		, &result );
	checkError( res, "Object creation" );
	return result;
}
