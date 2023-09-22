// Copyright Epic Games, Inc. All Rights Reserved..

/*=============================================================================
	VulkanPipelineCache.h: Private Vulkan RHI definitions.
=============================================================================*/

#pragma once
#include "VulkanRHIPrivate.h"
#include "VulkanResources.h"
#include "VulkanShaderResources.h"
#include "VulkanDescriptorSets.h"
#include "ShaderPipelineCache.h"
#include "Templates/UniquePtr.h"

class FVulkanChunkedPipelineCacheManager
{
	TUniquePtr<class FVulkanChunkedPipelineCacheManagerImpl> VulkanPipelineCacheManagerImpl;
	friend class FVulkanPipelineCacheEntry;
public:
	static void Init();
	static void Shutdown();
	static bool IsEnabled();
	static FVulkanChunkedPipelineCacheManager& Get();

	enum class EPSOOperation
	{
		CreateIfPresent,	// checks the PSO is already present in the cache, VK api requires it creates the PSO too.
		CreateAndStorePSO,	// Create and store the PSO in the cache.
	};

	template<class TPipelineState>
	VkResult CreatePSO(TPipelineState* GraphicsPipelineState, bool bIsPrecompileJob, TUniqueFunction<VkResult(TPipelineState*, VkPipelineCache, EPSOOperation)> PSOCreateFunc);
	void Tick();
};
