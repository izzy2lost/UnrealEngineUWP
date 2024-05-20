// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

class FString;
class UVCamOutputProviderBase;

namespace UE::VCamCore
{
	/** @return Gets the output provider at Index int the UVCamComponent that owns OutputProvider. */
	VCAMCORE_API UVCamOutputProviderBase* GetOtherOutputProviderByIndex(const UVCamOutputProviderBase& OutputProvider, int32 Index);

	/** @return Gets the index of OutputProvider in the UVCamComponent that owns OutputProvider. */
	VCAMCORE_API int32 FindOutputProviderIndex(const UVCamOutputProviderBase& OutputProvider);

	/**
	 * Generates a unique name for OutputProvider following the pattern %s_%d where
	 * - %s is the label of the owning actor if it is unique across all actors having a UVCamComponent in the world, and the owning actor's name otherwise
	 * - %d is the result of FindOutputProviderIndex
	 */
	VCAMCORE_API FString GenerateUniqueOutputProviderName(const UVCamOutputProviderBase& OutputProvider);
}
