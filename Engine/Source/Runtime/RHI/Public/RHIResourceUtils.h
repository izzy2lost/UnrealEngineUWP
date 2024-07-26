// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RHICommandList.h"
#include "RHIResources.h"
#include "Containers/ResourceArray.h"

struct FRHIResourceCreateInfoUploadArray : public FRHIResourceCreateInfo
{
	FResourceArrayUploadArrayView UploadView;

	FRHIResourceCreateInfoUploadArray(const TCHAR* InDebugName, const void* InData, uint32 InSizeInBytes)
		: FRHIResourceCreateInfo(InDebugName, &UploadView)
		, UploadView(InData, InSizeInBytes)
	{
	}

	template<typename ElementType>
	FRHIResourceCreateInfoUploadArray(const TCHAR* InDebugName, TConstArrayView<ElementType> InView)
		: FRHIResourceCreateInfo(InDebugName, &UploadView)
		, UploadView(InView)
	{
	}

	template<typename ElementType>
	FRHIResourceCreateInfoUploadArray(const TCHAR* InDebugName, TArrayView<ElementType> InView)
	: FRHIResourceCreateInfoUploadArray(InDebugName, TConstArrayView<ElementType>(InView))
	{
	}

	template<typename ElementType, typename AllocatorType>
	FRHIResourceCreateInfoUploadArray(const TCHAR* InDebugName, const TArray<ElementType, AllocatorType>& InArray)
	: FRHIResourceCreateInfoUploadArray(InDebugName, TConstArrayView<ElementType>(InArray))
	{
	}

	uint32 GetResourceDataSize() const
	{
		return UploadView.GetResourceDataSize();
	}
};

namespace UE::RHIResourceUtils
{
	template <typename TElementType>
	static FBufferRHIRef CreateBufferFromArray(FRHICommandListBase& RHICmdList, const TCHAR* Name, EBufferUsageFlags UsageFlags, ERHIAccess ResourceState, TConstArrayView<TElementType> Array)
	{
		FRHIResourceCreateInfoUploadArray CreateInfo(Name, Array);
		return RHICmdList.CreateBuffer(CreateInfo.GetResourceDataSize(), UsageFlags, Array.GetTypeSize(), ResourceState, CreateInfo);
	}

	template<typename TElementType>
	static FBufferRHIRef CreateVertexBufferFromArray(FRHICommandListBase& RHICmdList, const TCHAR* Name, EBufferUsageFlags ExtraFlags, TConstArrayView<TElementType> Array)
	{
		return CreateBufferFromArray<TElementType>(RHICmdList, Name, EBufferUsageFlags::VertexBuffer | ExtraFlags, ERHIAccess::VertexOrIndexBuffer, Array);
	}

	template<typename TElementType>
	static FBufferRHIRef CreateVertexBufferFromArray(FRHICommandListBase& RHICmdList, const TCHAR* Name, TConstArrayView<TElementType> Array)
	{
		return CreateVertexBufferFromArray<TElementType>(RHICmdList, Name, EBufferUsageFlags::None, Array);
	}

	template<typename TElementType>
	static FBufferRHIRef CreateIndexBufferFromArray(FRHICommandListBase& RHICmdList, const TCHAR* Name, EBufferUsageFlags ExtraFlags, TConstArrayView<TElementType> Array)
	{
		return CreateBufferFromArray<TElementType>(RHICmdList, Name, EBufferUsageFlags::IndexBuffer | ExtraFlags, ERHIAccess::VertexOrIndexBuffer, Array);
	}

	template<typename TElementType>
	static FBufferRHIRef CreateIndexBufferFromArray(FRHICommandListBase& RHICmdList, const TCHAR* Name, TConstArrayView<TElementType> Array)
	{
		return CreateIndexBufferFromArray<TElementType>(RHICmdList, Name, EBufferUsageFlags::None, Array);
	}
}
