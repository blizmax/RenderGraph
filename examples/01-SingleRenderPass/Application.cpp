#include "Application.hpp"

#include <array>
#include <cassert>
#include <iostream>
#include <stdexcept>

void doInitialiseQueueFamilies( Application & application );
VkSwapchainCreateInfoKHR doGetSwapChainCreateInfo( Application & application );
void doCreateRenderingResources( Application & application );
void doCreateFrameBuffers( Application & application );
void doCreateCommandBuffers( Application & application );

void cleanup( Application & app )
{
	// Before destroying any resource, we need to make sure the device is idle.
	vkDeviceWaitIdle( app.device );
	vkFreeCommandBuffers( app.device
		, app.commandPool
		, uint32_t( app.commandBuffers.size() )
		, app.commandBuffers.data() );
	app.commandBuffers.clear();

	for ( auto frameBuffer : app.frameBuffers )
	{
		vkDestroyFramebuffer( app.device
			, frameBuffer
			, nullptr );
	}

	app.frameBuffers.clear();
	vkDestroyRenderPass( app.device
		, app.renderPass
		, nullptr );
	app.renderPass = VK_NULL_HANDLE;

	for ( auto & resources : app.renderingResources )
	{
		vkFreeCommandBuffers( app.device
			, app.commandPool
			, 1u
			, &resources->commandBuffer );
		vkDestroyFence( app.device
			, resources->fence
			, nullptr );
		vkDestroySemaphore( app.device
			, resources->finishedRenderingSemaphore
			, nullptr );
		vkDestroySemaphore( app.device
			, resources->imageAvailableSemaphore
			, nullptr );
	}

	app.renderingResources.clear();
	app.swapChainImages.clear();
	vkDestroySwapchainKHR( app.device
		, app.swapChain
		, nullptr );
	app.swapChain = VK_NULL_HANDLE;
	vkDestroyCommandPool( app.device
		, app.commandPool
		, nullptr );
	app.commandPool = VK_NULL_HANDLE;
	vkDestroyDevice( app.device
		, nullptr );
	app.device = VK_NULL_HANDLE;
	vkDestroySurfaceKHR( app.instance
		, app.surface
		, nullptr );
	app.surface = VK_NULL_HANDLE;
}

VkDevice createDevice( Application & application )
{
	doInitialiseQueueFamilies( application );
	std::vector< float > queuePriorities{ 1.0f };
	std::vector< VkDeviceQueueCreateInfo > queueCreateInfos;

	if ( application.graphicsQueueFamilyIndex != uint32_t( ~( 0u ) ) )
	{
		queueCreateInfos.push_back(
			{
				VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				nullptr,
				0u,
				application.graphicsQueueFamilyIndex,
				uint32_t( queuePriorities.size() ),
				queuePriorities.data(),
			} );
	}

	if ( application.presentQueueFamilyIndex != application.graphicsQueueFamilyIndex )
	{
		queueCreateInfos.push_back(
			{
				VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				nullptr,
				0u,
				application.presentQueueFamilyIndex,
				uint32_t( queuePriorities.size() ),
				queuePriorities.data(),
			} );
	}

	if ( application.computeQueueFamilyIndex != application.graphicsQueueFamilyIndex )
	{
		queueCreateInfos.push_back(
			{
				VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				nullptr,
				0u,
				application.computeQueueFamilyIndex,
				uint32_t( queuePriorities.size() ),
				queuePriorities.data(),
			} );
	}

	std::vector< const char * > enabledExtensions
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	};
	VkPhysicalDeviceFeatures features;
	vkGetPhysicalDeviceFeatures( application.gpu, &features );
	VkDeviceCreateInfo createInfo
	{
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		nullptr,
		0u,
		uint32_t( queueCreateInfos.size() ),
		queueCreateInfos.data(),
		0u,
		nullptr,
		uint32_t( enabledExtensions.size() ),
		enabledExtensions.data(),
		&features,
	};
	return vkCreateObject( application.gpu
		, createInfo
		, vkCreateDevice );
}

void createSwapChain( Application & application )
{
	auto createInfo = doGetSwapChainCreateInfo( application );
	createInfo.oldSwapchain = application.swapChain;
	application.imageFormat = createInfo.imageFormat;
	auto res = vkCreateSwapchainKHR( application.device
		, &createInfo
		, nullptr
		, &application.swapChain );
	checkError( res, "Swapchain creation" );

	application.swapChainImages = vkGetArray( application.device
		, application.swapChain
		, vkGetSwapchainImagesKHR );

	application.clearColour = VkClearColorValue{ 1.0f, 0.8f, 0.4f, 0.0f };
	doCreateRenderingResources( application );
}

VkRenderPass createRenderPass( Application & application )
{
	// We'll have only one colour attachment for the render pass.
	VkAttachmentDescription attach
	{
		0u,
		// The format is the swapchain's pixel format.
		application.imageFormat,
		// Multisampling is disabled for this attach.
		VK_SAMPLE_COUNT_1_BIT,
		// We want to clear the attach at the beginning of the render pass.
		VK_ATTACHMENT_LOAD_OP_CLEAR,
		// And we want its result to be stored at the end of the render pass.
		VK_ATTACHMENT_STORE_OP_STORE,
		// We don't care about stencil attachment.
		VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		// We don't care about stencil attachment.
		VK_ATTACHMENT_STORE_OP_DONT_CARE,
		// The initial layout is the layout expected for the attachment at the beginning of the render pass.
		// We expect the attach to have been presented to the surface, so it should be either a present source or undefined.
		VK_IMAGE_LAYOUT_UNDEFINED,
		// The final layout is the layouts into which the attachment is transitioned at the end of the render pass.
		// We want the attach to be presented to the surface, so we make it a present source.
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
	};
	// A render pass always has at least one subpass.
	// In our case, this subpass is also the only one,
	// its only attachment is the render pass' one.
	// We want this attachment to be transitioned to colour attachment, so we can write into it.
	// We now create the subpasses.
	// The subpass state is used to setup the needed states at the beginning of the subpass.
	VkAttachmentReference colour
	{
		0u,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	};
	VkSubpassDescription subpass
	{
		0u,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		0u,
		nullptr,
		1u,
		&colour,
		nullptr,
		nullptr,
		0u,
		nullptr,
	};

	std::array< VkSubpassDependency, 2u > dependencies;
	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0u;
	dependencies[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[1].srcSubpass = 0u;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	VkRenderPassCreateInfo createInfo
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		nullptr,
		0u,
		1u,
		&attach,
		1u,
		&subpass,
		uint32_t( dependencies.size() ),
		dependencies.data(),
	};
	// Eventually, we create the render pass, using all previously built informations.
	return vkCreateObject( application.device, createInfo, vkCreateRenderPass );
}

void prepareFrames( Application & application )
{
	// We retrieve the framebuffers and command buffers for each backbuffer of the swapchain.
	doCreateFrameBuffers( application );
	doCreateCommandBuffers( application );

	VkCommandBufferBeginInfo beginInfo
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		nullptr,
		VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
		nullptr,
	};
	VkClearValue clearValue{ application.clearColour };
	VkRenderPassBeginInfo beginRPInfo
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		nullptr,
		application.renderPass,
		VK_NULL_HANDLE,
		VkRect2D{ { 0, 0 }, application.dimensions },
		1u,
		&clearValue
	};

	// We'll simply clear the swap chain, using its colour defined previously.
	for ( size_t i = 0u; i < application.commandBuffers.size(); ++i )
	{
		auto commandBuffer = application.commandBuffers[i];
		beginRPInfo.framebuffer = application.frameBuffers[i];

		vkBeginCommandBuffer( commandBuffer, &beginInfo );
		vkCmdBeginRenderPass( commandBuffer, &beginRPInfo, VK_SUBPASS_CONTENTS_INLINE );
		vkCmdEndRenderPass( commandBuffer );
		vkEndCommandBuffer( commandBuffer );
	}
}

RenderingResources * getResources( Application & application )
{
	auto & resources = *application.renderingResources[application.resourceIndex];
	application.resourceIndex = ( application.resourceIndex + 1 ) % application.renderingResources.size();

	uint32_t imageIndex{ 0u };
	auto res = vkAcquireNextImageKHR( application.device
		, application.swapChain
		, std::numeric_limits< uint64_t >::max()
		, resources.imageAvailableSemaphore
		, resources.fence
		, &imageIndex );
	auto needsReset = res;
	res = vkWaitForFences( application.device
		, 1u
		, &resources.fence
		, VK_TRUE
		, std::numeric_limits< uint64_t >::max() );

	if ( res == VK_SUCCESS )
	{
		vkResetFences( application.device, 1u, &resources.fence );

		if ( checkNeedReset( application
			, needsReset
			, true
			, "Swap chain image acquisition" ) )
		{
			resources.imageIndex = imageIndex;
			return &resources;
		}

		return nullptr;
	}

	std::cerr << "Couldn't retrieve rendering resources";
	return nullptr;
}

bool checkNeedReset( Application & application
	, VkResult errCode
	, bool acquisition
	, char const * const action )
{
	bool result{ false };

	switch ( errCode )
	{
	case VK_SUCCESS:
		result = true;
		break;

	case VK_ERROR_OUT_OF_DATE_KHR:
		if ( !acquisition )
		{
			resetSwapChain( application );
		}
		else
		{
			result = true;
		}
		break;

	case VK_SUBOPTIMAL_KHR:
		resetSwapChain( application );
		break;

	default:
		checkError( errCode, action );
		break;
	}

	return result;
}

void resetSwapChain( Application & application )
{
	vkDeviceWaitIdle( application.device );
	createSwapChain( application );
	prepareFrames( application );
}

void checkError( VkResult errCode
	, char const * const text )
{
	bool result = errCode == VK_SUCCESS;

	if ( !result )
	{
		throw std::runtime_error{ text };
	}
}

void doInitialiseQueueFamilies( Application & application )
{
	// Parcours des propriétés des files, pour vérifier leur support de la présentation.
	auto queueProps = vkGetArray( application.gpu
		, vkGetPhysicalDeviceQueueFamilyProperties );
	std::vector< uint32_t > supportsPresent( static_cast< uint32_t >( queueProps.size() ) );
	uint32_t i{ 0u };
	application.graphicsQueueFamilyIndex = std::numeric_limits< uint32_t >::max();
	application.presentQueueFamilyIndex = std::numeric_limits< uint32_t >::max();
	application.computeQueueFamilyIndex = std::numeric_limits< uint32_t >::max();

	for ( auto & present : supportsPresent )
	{
		VkBool32 present;
		auto res = vkGetPhysicalDeviceSurfaceSupportKHR( application.gpu
			, i
			, application.surface
			, &present );
		checkError( res, "Presentation surface support check" );

		if ( queueProps[i].queueCount > 0 )
		{
			if ( queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT )
			{
				// Tout d'abord on choisit une file graphique
				if ( application.graphicsQueueFamilyIndex == std::numeric_limits< uint32_t >::max() )
				{
					application.graphicsQueueFamilyIndex = i;
				}

				// Si la file supporte aussi les calculs, on la choisit en compute queue
				if ( queueProps[i].queueFlags & VK_QUEUE_COMPUTE_BIT
					&& application.computeQueueFamilyIndex == std::numeric_limits< uint32_t >::max() )
				{
					application.computeQueueFamilyIndex = i;
				}

				// Si une file supporte les graphismes et la présentation, on la préfère.
				if ( present )
				{
					application.graphicsQueueFamilyIndex = i;
					application.presentQueueFamilyIndex = i;
					break;
				}
			}
			else if ( queueProps[i].queueFlags & VK_QUEUE_COMPUTE_BIT
				&& application.computeQueueFamilyIndex == std::numeric_limits< uint32_t >::max() )
			{
				application.computeQueueFamilyIndex = i;
			}

		}

		++i;
	}

	if ( application.presentQueueFamilyIndex == std::numeric_limits< uint32_t >::max() )
	{
		// Pas de file supportant les deux, on a donc 2 files distinctes.
		for ( size_t i = 0; i < queueProps.size(); ++i )
		{
			if ( supportsPresent[i] )
			{
				application.presentQueueFamilyIndex = static_cast< uint32_t >( i );
				break;
			}
		}
	}

	// Si on n'en a pas trouvé, on génère une erreur.
	if ( application.graphicsQueueFamilyIndex == std::numeric_limits< uint32_t >::max()
		|| application.presentQueueFamilyIndex == std::numeric_limits< uint32_t >::max()
		|| application.computeQueueFamilyIndex == std::numeric_limits< uint32_t >::max() )
	{
		throw std::exception{ "Queue families retrieval" };
	}
}

uint32_t doGetImageCount( Application & application )
{
	VkSurfaceCapabilitiesKHR surfaceCaps;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR( application.gpu
		, application.surface
		, &surfaceCaps );
	uint32_t desiredNumberOfSwapChainImages{ surfaceCaps.minImageCount + 1 };

	if ( ( surfaceCaps.maxImageCount > 0 ) &&
		( desiredNumberOfSwapChainImages > surfaceCaps.maxImageCount ) )
	{
		// L'application doit s'initialiser avec moins d'images que voulu.
		desiredNumberOfSwapChainImages = surfaceCaps.maxImageCount;
	}

	return desiredNumberOfSwapChainImages;
}

VkSurfaceFormatKHR doSelectFormat( Application & application )
{
	VkSurfaceFormatKHR result;
	auto formats = vkGetArray( application.gpu
		, application.surface
		, vkGetPhysicalDeviceSurfaceFormatsKHR );

	// Si la liste de formats ne contient qu'une entr�e VK_FORMAT_UNDEFINED,
	// la surface n'a pas de format préféré. Sinon, au moins un format supporté
	// sera renvoyé.
	if ( formats.size() == 1u && formats[0].format == VK_FORMAT_UNDEFINED )
	{
		result.format = VK_FORMAT_R8G8B8A8_UNORM;
		result.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	}
	else
	{
		assert( !formats.empty() );
		auto it = std::find_if( formats.begin()
			, formats.end()
			, []( VkSurfaceFormatKHR const & lookup )
			{
				return lookup.format == VK_FORMAT_R8G8B8A8_UNORM;
			} );

		if ( it != formats.end() )
		{
			result = *it;
		}
		else
		{
			result = formats[0];
		}
	}

	return result;
}

VkPresentModeKHR doSelectPresentMode( Application & application )
{
	auto presentModes = vkGetArray( application.gpu
		, application.surface
		, vkGetPhysicalDeviceSurfacePresentModesKHR );
	// Si le mode boîte aux lettres est disponible, on utilise celui-là, car c'est celui avec le
	// minimum de latence dans tearing.
	// Sinon, on essaye le mode IMMEDIATE, qui est normalement disponible, et est le plus rapide
	// (bien qu'il y ait du tearing). Sinon on utilise le mode FIFO qui est toujours disponible.
	VkPresentModeKHR result{ VK_PRESENT_MODE_FIFO_KHR };

	for ( auto mode : presentModes )
	{
		if ( mode == VK_PRESENT_MODE_MAILBOX_KHR )
		{
			result = mode;
			break;
		}

		if ( ( result != VK_PRESENT_MODE_MAILBOX_KHR )
			&& ( mode == VK_PRESENT_MODE_IMMEDIATE_KHR ) )
		{
			result = mode;
		}
	}

	return result;
}

VkSwapchainCreateInfoKHR doGetSwapChainCreateInfo( Application & application )
{
	VkExtent2D swapChainExtent{};
	VkSurfaceCapabilitiesKHR surfaceCaps;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR( application.gpu
		, application.surface
		, &surfaceCaps );

	// width et height valent soient tous les deux -1 ou tous les deux autre chose que -1.
	if ( surfaceCaps.currentExtent.width == uint32_t( -1 ) )
	{
		// Si les dimensions de la surface sont indéfinies, elles sont initialisées
		// aux dimensions des images requises.
		swapChainExtent = application.dimensions;
	}
	else
	{
		// Si les dimensions de la surface sont définies, alors les dimensions de la swap chain
		// doivent correspondre.
		swapChainExtent = surfaceCaps.currentExtent;
	}

	// Parfois, les images doivent être transformées avant d'être présentées (lorsque l'orientation
	// du périphérique est différente de l'orientation par défaut, par exemple).
	// Si la transformation spécifiée est différente de la transformation par défaut, le moteur de 
	// présentation va transformer l'image lors de la présentation. Cette opération peut avoir un
	// impact sur les performances sur certaines plateformes.
	// Ici, nous ne voulons aucune transformation, donc si la transformation identité est supportée,
	// nous l'utilisons, sinon nous utiliserons la même transformation que la transformation courante.
	VkSurfaceTransformFlagBitsKHR preTransform{};

	if ( ( surfaceCaps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR ) )
	{
		preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	}
	else
	{
		preTransform = surfaceCaps.currentTransform;
	}

	auto presentMode = doSelectPresentMode( application );
	auto surfaceFormat = doSelectFormat( application );
	return VkSwapchainCreateInfoKHR
	{
		VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		nullptr,
		0u,
		application.surface,
		doGetImageCount( application ),
		surfaceFormat.format,
		surfaceFormat.colorSpace,
		swapChainExtent,
		1u,
		surfaceCaps.supportedUsageFlags,
		VK_SHARING_MODE_EXCLUSIVE,
		0u,
		nullptr,
		preTransform,
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		presentMode,
		VK_TRUE,
		nullptr
	};
}

void doCreateRenderingResources( Application & application )
{
	application.renderingResources.clear();

	for ( uint32_t i = 0u; i < uint32_t( application.swapChainImages.size() ); ++i )
	{
		VkCommandBuffer commandBuffer;
		VkCommandBufferAllocateInfo cbInfo
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			nullptr,
			application.commandPool,
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			1u,
		};
		vkAllocateCommandBuffers( application.device, &cbInfo, &commandBuffer );
		application.renderingResources.emplace_back( std::make_unique< RenderingResources >( 
			vkCreateObject( application.device
				, VkSemaphoreCreateInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, nullptr, 0u }
				, vkCreateSemaphore ),
			vkCreateObject( application.device
				, VkSemaphoreCreateInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, nullptr, 0u }
				, vkCreateSemaphore ),
			vkCreateObject( application.device
				, VkFenceCreateInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, nullptr, VK_FENCE_CREATE_SIGNALED_BIT }
				, vkCreateFence ),
			commandBuffer,
			0u ) );
	}
}

std::vector< VkImageView > doPrepareAttaches( Application const & application
	, uint32_t backBuffer
	, std::vector< VkImageView > & views )
{
	std::vector< VkImageView > attaches;
	auto image = application.swapChainImages[backBuffer];
	VkImageViewCreateInfo createInfo
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		nullptr,
		0u,
		image,
		VK_IMAGE_VIEW_TYPE_2D,
		application.imageFormat,
		{},
		{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u, }
	};
	views.push_back( vkCreateObject( application.device, createInfo, vkCreateImageView ) );

	for ( auto & view : views )
	{
		attaches.emplace_back( view );
	}

	return attaches;
}

void doCreateFrameBuffers( Application & application )
{
	application.frameBuffers.reserve( application.swapChainImages.size() );

	for ( size_t i = 0u; i < application.swapChainImages.size(); ++i )
	{
		auto attaches = doPrepareAttaches( application
			, uint32_t( i )
			, application.views );
		VkFramebufferCreateInfo createInfo
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			nullptr,
			0u,
			application.renderPass,
			uint32_t( attaches.size() ),
			attaches.data(),
			application.dimensions.width,
			application.dimensions.height,
			1u,
		};
		application.frameBuffers.push_back( vkCreateObject( application.device
			, createInfo
			, vkCreateFramebuffer ) );
	}
}

void doCreateCommandBuffers( Application & application )
{
	application.commandBuffers.resize( application.swapChainImages.size() );
	VkCommandBufferAllocateInfo allocInfo
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		nullptr,
		application.commandPool,
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		uint32_t( application.commandBuffers.size() ),
	};
	vkAllocateCommandBuffers( application.device
		, &allocInfo
		, application.commandBuffers.data() );
}
