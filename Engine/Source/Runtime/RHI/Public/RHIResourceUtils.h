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
	template <typename T>
	static FBufferRHIRef CreateBufferWithData(FRHICommandListBase& RHICmdList, EBufferUsageFlags UsageFlags, ERHIAccess ResourceState, const TCHAR* Name, TConstArrayView<T> Data)
	{
		FRHIResourceCreateInfoUploadArray CreateInfo(Name, Data);
		return RHICmdList.CreateBuffer(CreateInfo.GetResourceDataSize(), UsageFlags, Data.GetTypeSize(), ResourceState, CreateInfo);
	}

	template<typename T>
	static FBufferRHIRef CreateBufferWithData(FRHICommandListBase& RHICmdList, EBufferUsageFlags UsageFlags, ERHIAccess ResourceState, const TCHAR* Name, TArrayView<T> Data)
	{
		return CreateBufferWithData(RHICmdList, UsageFlags, ResourceState, Name, TConstArrayView<T>(Data));
	}
}

