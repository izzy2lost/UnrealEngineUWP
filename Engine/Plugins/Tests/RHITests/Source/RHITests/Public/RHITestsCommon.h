// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderUtils.h"
#include "RenderingThread.h"

DECLARE_LOG_CATEGORY_EXTERN(LogRHIUnitTestCommandlet, Log, All);

#define RUN_TEST(x) do { bool Ret = x; bResult = bResult && Ret; } while (false)

bool IsZeroMem(const void* Ptr, uint32 Size);

bool RunOnRenderThreadSynchronous(TFunctionRef<bool(FRHICommandListImmediate&)> TestFunc);

template <typename ValueType>
static inline FString ClearValueToString(const ValueType & ClearValue)
{
	if constexpr (std::is_same_v<ValueType, FVector4>)
	{
		return FString::Printf(TEXT("%f %f %f %f"), ClearValue.X, ClearValue.Y, ClearValue.Z, ClearValue.W);
	}
	else
	{
		return FString::Printf(TEXT("0x%08x 0x%08x 0x%08x 0x%08x"), ClearValue.X, ClearValue.Y, ClearValue.Z, ClearValue.W);
	}
}

template <typename T>
static FBufferRHIRef CreateBufferWithData(EBufferUsageFlags UsageFlags, ERHIAccess ResourceState, const TCHAR* Name, TConstArrayView<T> Data)
{
	FRHICommandListBase& RHICmdList = FRHICommandListImmediate::Get();
	uint32 BufferSize = sizeof(T) * Data.Num();
	FRHIResourceCreateInfo CreateInfo(Name);
	FBufferRHIRef Buffer = RHICmdList.CreateBuffer(BufferSize, UsageFlags, sizeof(T), ResourceState, CreateInfo);
	void* MappedData = RHICmdList.LockBuffer(Buffer, 0, BufferSize, EResourceLockMode::RLM_WriteOnly);
	FMemory::Memcpy(MappedData, Data.GetData(), BufferSize);
	RHICmdList.UnlockBuffer(Buffer);
	return Buffer;
}

template<typename T>
static FBufferRHIRef CreateBufferWithData(EBufferUsageFlags UsageFlags, ERHIAccess ResourceState, const TCHAR* Name, TArrayView<T> Data)
{
	return CreateBufferWithData(UsageFlags, ResourceState, Name, TConstArrayView<T>(Data.GetData(), Data.Num()));
}
