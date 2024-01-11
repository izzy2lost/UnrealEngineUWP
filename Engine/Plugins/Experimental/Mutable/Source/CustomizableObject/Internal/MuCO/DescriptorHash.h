// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCO/CustomizableObjectInstanceDescriptor.h"


/** Hash of the Descriptor.
* Can change and is not backwards compatible. Do not serialize. */
class CUSTOMIZABLEOBJECT_API FDescriptorHash
{
public:
	FDescriptorHash() = default;

	explicit FDescriptorHash(const FCustomizableObjectInstanceDescriptor& Descriptor);

	bool operator==(const FDescriptorHash& Other) const;

	bool operator!=(const FDescriptorHash& Other) const;
	
	/** Return true if this Hash is a subset of the other Hash (i.e., this Descriptor is a subset of the other Descriptor). */
	bool IsSubset(const FDescriptorHash& Other) const;

	void UpdateMinMaxLOD(int32 InMinLOD, int32 InMaxLOD);

	int32 GetMinLOD() const;

	int32 GetMaxLOD() const;

	void UpdateRequestedLODs(const TArray<uint16>& InRequestedLODs);

	const TArray<uint16>& GetRequestedLODs() const;

	FString ToString() const;

private:
	uint32 Hash = 0;

	int32 MinLOD = 0;
	int32 MaxLOD = INT32_MAX;

	// Array of bitmasks that indicate which LODs of each component have been requested
	TArray<uint16> RequestedLODsPerComponent;
};
