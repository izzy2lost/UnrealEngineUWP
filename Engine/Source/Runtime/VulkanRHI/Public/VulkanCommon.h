// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanCommon.h: Common definitions used for both runtime and compiling shaders.
=============================================================================*/

#pragma once

#include "RHIDefinitions.h"
#include "Logging/LogMacros.h"

#ifndef VULKAN_SUPPORTS_GEOMETRY_SHADERS
	#define VULKAN_SUPPORTS_GEOMETRY_SHADERS					PLATFORM_SUPPORTS_GEOMETRY_SHADERS
#endif

// This defines controls shader generation (so will cause a format rebuild)
// be careful wrt cooker/target platform not matching define-wise!!!
// ONLY used for debugging binding table/descriptor set bugs/mismatches.
#define VULKAN_ENABLE_BINDING_DEBUG_NAMES						0

namespace ShaderStage
{
	enum EStage
	{
		// Adjusting these requires a full shader rebuild (ie modify the guid on VulkanCommon.usf)
		// Keep the values in sync with EShaderFrequency
		Vertex = 0,
		Pixel = 1,
		Geometry = 2,

		RayGen = 3,
		RayMiss = 4,
		RayHitGroup = 5,
		RayCallable = 6,

		NumGraphicsStages = 3,
		NumRayTracingStages = 4,

		NumStages = (NumGraphicsStages + NumRayTracingStages),

		// Compute is its own pipeline, so it can all live as set 0
		Compute = 0,

		MaxNumSets = 8,

		Invalid = -1,
	};

	inline EStage GetStageForFrequency(EShaderFrequency Stage)
	{
		switch (Stage)
		{
		case SF_Vertex:		return Vertex;
		case SF_Pixel:		return Pixel;
		case SF_Geometry:	return Geometry;
		case SF_RayGen:			return RayGen;
		case SF_RayMiss:		return RayMiss;
		case SF_RayHitGroup:	return RayHitGroup;
		case SF_RayCallable:	return RayCallable;
		case SF_Compute:	return Compute;
		default:
			checkf(0, TEXT("Invalid shader Stage %d"), (int32)Stage);
			break;
		}

		return Invalid;
	}

	inline EShaderFrequency GetFrequencyForGfxStage(EStage Stage)
	{
		switch (Stage)
		{
		case EStage::Vertex:	return SF_Vertex;
		case EStage::Pixel:		return SF_Pixel;
		case EStage::Geometry:	return SF_Geometry;
		case EStage::RayGen:		return SF_RayGen;
		case EStage::RayMiss:		return SF_RayMiss;
		case EStage::RayHitGroup:	return SF_RayHitGroup;
		case EStage::RayCallable:	return SF_RayCallable;
		default:
			checkf(0, TEXT("Invalid shader Stage %d"), (int32)Stage);
			break;
		}

		return SF_NumFrequencies;
	}
};

namespace VulkanBindless
{
	static constexpr uint32 MaxUniformBuffersPerStage = 16;

	enum EDescriptorSets
	{
		BindlessSamplerSet = 0,

		BindlessStorageBufferSet,
		BindlessUniformBufferSet,

		BindlessStorageImageSet,
		BindlessSampledImageSet,

		BindlessStorageTexelBufferSet,
		BindlessUniformTexelBufferSet,

		BindlessAccelerationStructureSet,

		BindlessSingleUseUniformBufferSet,  // Keep last
		NumBindlessSets,
		MaxNumSets = NumBindlessSets
	};
};

DECLARE_LOG_CATEGORY_EXTERN(LogVulkan, Display, All);

template< class T >
static FORCEINLINE void ZeroVulkanStruct(T& Struct, int32 VkStructureType)
{
	static_assert(!TIsPointer<T>::Value, "Don't use a pointer!");
	static_assert(STRUCT_OFFSET(T, sType) == 0, "Assumes sType is the first member in the Vulkan type!");
	static_assert(sizeof(T::sType) == sizeof(int32), "Assumed sType is compatible with int32!");
	// Horrible way to coerce the compiler to not have to know what T::sType is so we can have this header not have to include vulkan.h
	(int32&)Struct.sType = VkStructureType;
	FMemory::Memzero(((uint8*)&Struct) + sizeof(VkStructureType), sizeof(T) - sizeof(VkStructureType));
}
