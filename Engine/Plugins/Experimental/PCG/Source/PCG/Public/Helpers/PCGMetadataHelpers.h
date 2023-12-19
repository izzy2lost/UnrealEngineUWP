// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

class FPCGMetadataAttributeBase;
class UPCGData;
class UPCGMetadata;
struct FPCGAttributePropertyInputSelector;
struct FPCGAttributePropertyOutputSelector;
struct FPCGContext;
struct FSoftObjectPath;
template <typename FuncType> class TFunction;

namespace PCGMetadataHelpers
{
	PCG_API bool HasSameRoot(const UPCGMetadata* Metadata1, const UPCGMetadata* Metadata2);
	PCG_API const UPCGMetadata* GetParentMetadata(const UPCGMetadata* Metadata);

	// Helpers functions to cast in spatial/param and return metadata. Nullptr if data doesn't have metadata
	PCG_API const UPCGMetadata* GetConstMetadata(const UPCGData* InData);
	PCG_API UPCGMetadata* GetMutableMetadata(UPCGData* InData);
	
	/** Create a lambda that will construct a soft object path from an underlying attribute of type FSoftObjectPath or FString. Returns true if successful. */
	UE_NODISCARD PCG_API bool CreateObjectPathGetter(const FPCGMetadataAttributeBase* InAttributeBase, TFunction<void(int64, FSoftObjectPath&)>& OutGetter);

	/** Create a lambda that will construct a soft object path from an underlying attribute of type FSoftObjectPath or FString. Returns true if successful. */
	UE_NODISCARD PCG_API bool CreateObjectOrClassPathGetter(const FPCGMetadataAttributeBase* InAttributeBase, TFunction<void(int64, FSoftObjectPath&)>& OutGetter);

	/** Copy the attribute coming from Source Data into Target Data.
	* Can set bSameOrigin to true if SourceData and TargetData have the same origin (if TargetData was initialized from SourceData). 
	*/
	PCG_API bool CopyAttributes(const UPCGData* SourceData, const FPCGAttributePropertyInputSelector& InputSource, UPCGData* TargetData, const FPCGAttributePropertyOutputSelector& OutputTarget, bool bSameOrigin, FPCGContext* OptionalContext = nullptr);

	/** Copy all the attributes coming from Source Data into Target Data. */
	PCG_API bool CopyAllAttributes(const UPCGData* SourceData, UPCGData* TargetData, FPCGContext* OptionalContext = nullptr);
}

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
