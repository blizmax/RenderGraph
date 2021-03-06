﻿/*
This file belongs to RenderGraph.
See LICENSE file in root folder.
*/
#pragma once

#include "vulkan/vulkan.h"

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <set>
#include <vector>

namespace crg
{
	template< typename DataT >
	struct Id;

	struct Attachment;
	struct AttachmentTransition;
	struct ImageData;
	struct ImageViewData;
	struct GraphNode;
	struct RenderPass;
	struct RenderPassDependencies;

	class GraphVisitor;
	class RenderGraph;

	using ImageId = Id < ImageData >;
	using ImageViewId = Id < ImageViewData >;

	using RenderPassPtr = std::unique_ptr< RenderPass >;
	using GraphNodePtr = std::unique_ptr< GraphNode >;
	using GraphAdjacentNode = GraphNode *;
	using ConstGraphAdjacentNode = GraphNode const *;

	using AttachmentArray = std::vector< Attachment >;
	using AttachmentTransitionArray = std::vector< AttachmentTransition >;
	using RenderPassPtrArray = std::vector< RenderPassPtr >;
	using GraphNodePtrArray = std::vector< GraphNodePtr >;
	using RenderPassDependenciesArray = std::vector< RenderPassDependencies >;
	using GraphAdjacentNodeArray = std::vector< GraphAdjacentNode >;
	using AttachmentsNodeMap = std::map< ConstGraphAdjacentNode, AttachmentTransitionArray >;
}
