// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanResources.h: Vulkan resource RHI definitions.
=============================================================================*/

#pragma once

#include "BoundShaderStateCache.h"
#include "CrossCompilerCommon.h"
#include "VulkanCommon.h"
#include "VulkanThirdParty.h"


// Vulkan ParameterMap:
// Buffer Index = EBufferIndex
// Base Offset = Index into the subtype
// Size = Ignored for non-globals
struct FVulkanShaderHeader
{
	enum EType
	{
		PackedGlobal,
		Global,
		UniformBuffer,

		Count,
	};

	struct FSpirvInfo
	{
		FSpirvInfo() = default;
		FSpirvInfo(uint32 InDescriptorSetOffset, uint32 InBindingIndexOffset)
			: DescriptorSetOffset(InDescriptorSetOffset)
			, BindingIndexOffset(InBindingIndexOffset)
		{
		}

		uint32	DescriptorSetOffset = UINT32_MAX;
		uint32	BindingIndexOffset = UINT32_MAX;
	};



	struct FUniformBufferInfo
	{
		uint32					LayoutHash;
		uint16					ConstantDataOriginalBindingIndex;
		uint8					bOnlyHasResources;
		uint8					bHasResources;
#if VULKAN_ENABLE_BINDING_DEBUG_NAMES
		FString					DebugName;
#endif
	};
	TArray<FUniformBufferInfo>	UniformBuffers;

	struct FGlobalInfo
	{
		uint32							TypeIndex;
#if VULKAN_ENABLE_BINDING_DEBUG_NAMES
		FString							DebugName;
#endif
	};
	TArray<FGlobalInfo>					Globals;

	struct FPackedGlobalInfo
	{
		uint16							ConstantDataSizeInFloats;
		uint8							PackedUBIndex;
		uint8							Pad0 = 0;
#if VULKAN_ENABLE_BINDING_DEBUG_NAMES
		FString							DebugName;
#endif
	};
	TArray<FPackedGlobalInfo>			PackedGlobals;

	struct FPackedUBInfo
	{
		uint32							SizeInBytes;
		uint32							SPIRVDescriptorSetOffset;
		uint32							SPIRVBindingIndexOffset;
	};
	TArray<FPackedUBInfo>					PackedUBs;

	enum class EAttachmentType : uint8
	{
		Color0,
		Color1,
		Color2,
		Color3,
		Color4,
		Color5,
		Color6,
		Color7,
		Depth,

		Count,
	};
	struct FInputAttachment
	{
		uint16			GlobalIndex;
		EAttachmentType	Type;
		uint8			Pad = 0;
	};
	TArray<FInputAttachment>				InputAttachments;

	// Mostly relevant for Vertex Shaders
	uint32									InOutMask;

	// Relevant for Ray Tracing Shaders
	uint32                                  RayTracingPayloadType = 0;
	uint32                                  RayTracingPayloadSize = 0;

	FSHAHash								SourceHash;
	uint32									SpirvCRC = 0;
	uint8									WaveSize = 0;

	// For RayHitGroup shaders
	enum class ERayHitGroupEntrypoint : uint8
	{
		NotPresent = 0,

		// Hit group types are all stored in a single spirv blob
		// and each have different entry point names
		// NOTE: Not used yet because of compiler issues
		CommonBlob,

		// Hit group types are each stored in a different spirv blob
		// to circumvent DXC compilation issues
		SeparateBlob
	};
	ERayHitGroupEntrypoint RayGroupAnyHit = ERayHitGroupEntrypoint::NotPresent;
	ERayHitGroupEntrypoint RayGroupIntersection = ERayHitGroupEntrypoint::NotPresent;

	TArray<FSpirvInfo>						UniformBufferSpirvInfos;
	TArray<FSpirvInfo>						GlobalSpirvInfos;

	FString									DebugName;

	FVulkanShaderHeader() = default;
	enum EInit
	{
		EZero
	};
	FVulkanShaderHeader(EInit)
		: InOutMask(0)
	{
	}
};

inline FArchive& operator<<(FArchive& Ar, FVulkanShaderHeader::FSpirvInfo& Info)
{
	Ar << Info.DescriptorSetOffset;
	Ar << Info.BindingIndexOffset;
	return Ar;
}

inline FArchive& operator<<(FArchive& Ar, FVulkanShaderHeader::FUniformBufferInfo& UBInfo)
{
	Ar << UBInfo.LayoutHash;
	Ar << UBInfo.ConstantDataOriginalBindingIndex;
	Ar << UBInfo.bOnlyHasResources;
	Ar << UBInfo.bHasResources;
#if VULKAN_ENABLE_BINDING_DEBUG_NAMES
	Ar << UBInfo.DebugName;
#endif
	return Ar;
}

inline FArchive& operator<<(FArchive& Ar, FVulkanShaderHeader::FPackedGlobalInfo& PackedGlobalInfo)
{
	Ar << PackedGlobalInfo.ConstantDataSizeInFloats;
	Ar << PackedGlobalInfo.PackedUBIndex;
#if VULKAN_ENABLE_BINDING_DEBUG_NAMES
	Ar << PackedGlobalInfo.DebugName;
#endif
	return Ar;
}

inline FArchive& operator<<(FArchive& Ar, FVulkanShaderHeader::FPackedUBInfo& PackedUBInfo)
{
	Ar << PackedUBInfo.SizeInBytes;
	Ar << PackedUBInfo.SPIRVDescriptorSetOffset;
	Ar << PackedUBInfo.SPIRVBindingIndexOffset;
	return Ar;
}

inline FArchive& operator<<(FArchive& Ar, FVulkanShaderHeader::FGlobalInfo& GlobalInfo)
{
	Ar << GlobalInfo.TypeIndex;
#if VULKAN_ENABLE_BINDING_DEBUG_NAMES
	Ar << GlobalInfo.DebugName;
#endif
	return Ar;
}

inline FArchive& operator<<(FArchive& Ar, FVulkanShaderHeader::FInputAttachment& AttachmentInfo)
{
	Ar << AttachmentInfo.GlobalIndex;
	Ar << AttachmentInfo.Type;
	return Ar;
}

inline FArchive& operator<<(FArchive& Ar, FVulkanShaderHeader& Header)
{
	Ar << Header.UniformBuffers;
	Ar << Header.Globals;
	Ar << Header.PackedGlobals;
	Ar << Header.PackedUBs;
	Ar << Header.InputAttachments;
	Ar << Header.InOutMask;
	Ar << Header.RayTracingPayloadType;
	Ar << Header.RayTracingPayloadSize;
	Ar << Header.SourceHash;
	Ar << Header.SpirvCRC;
	Ar << Header.WaveSize;
	Ar << Header.RayGroupAnyHit;
	Ar << Header.RayGroupIntersection;
	Ar << Header.UniformBufferSpirvInfos;
	Ar << Header.GlobalSpirvInfos;
	Ar << Header.DebugName;
	return Ar;
}
