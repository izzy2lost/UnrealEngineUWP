// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "DataLayerInstanceNames.generated.h"

USTRUCT()
struct FDataLayerInstanceNames
{
	GENERATED_USTRUCT_BODY()

	FDataLayerInstanceNames()
	: bIsFirstDataLayerIsExternal(false)
	{
#if WITH_EDITOR
		bIsForcedEmptyNonExternalDataLayers = false;
#endif
	}

	FDataLayerInstanceNames(const TArray<FName>& InDataLayers, bool bInIsFirstDataLayerIsExternal)
	: bIsFirstDataLayerIsExternal(bInIsFirstDataLayerIsExternal)
	, DataLayers(InDataLayers)
	{
#if WITH_EDITOR
		bIsForcedEmptyNonExternalDataLayers = false;
#endif
	}

	FDataLayerInstanceNames(const TArray<FName>& InNonExternalDataLayers, FName InExternalDataLayer)
	{
		bIsFirstDataLayerIsExternal = !InExternalDataLayer.IsNone();
		if (bIsFirstDataLayerIsExternal)
		{
			DataLayers.Add(InExternalDataLayer);
		}
		DataLayers.Append(InNonExternalDataLayers);
	}

	const FName GetExternalDataLayer() const
	{
		check(!bIsFirstDataLayerIsExternal || !DataLayers.IsEmpty());
		return bIsFirstDataLayerIsExternal ? DataLayers[0] : NAME_None;
	}

	TArrayView<const FName> GetNonExternalDataLayers() const
	{
		static TArray<FName> EmptyArray;

#if WITH_EDITOR
		if (bIsForcedEmptyNonExternalDataLayers)
		{
			return EmptyArray;
		}
#endif
		check(!bIsFirstDataLayerIsExternal || !DataLayers.IsEmpty());
		const int32 Offset = bIsFirstDataLayerIsExternal ? 1 : 0;
		const int32 NonExternalDataLayersCount = DataLayers.Num() - Offset;
		return NonExternalDataLayersCount > 0 ? MakeArrayView(&DataLayers[Offset], NonExternalDataLayersCount) : EmptyArray;
	}

#if WITH_EDITOR
	TArray<FName> ToArray() const
	{
		if (bIsForcedEmptyNonExternalDataLayers)
		{
			static TArray<FName> EmptyArray;
			return HasExternalDataLayer() ? TArray<FName>({ GetExternalDataLayer() }) : EmptyArray;
		}
		return DataLayers;
	}

	int32 Num() const
	{
		if (bIsForcedEmptyNonExternalDataLayers)
		{
			return HasExternalDataLayer() ? 1 : 0;
		}
		return DataLayers.Num();
	}

	bool IsEmpty() const
	{
		return Num() == 0;
	}
#endif

	bool HasExternalDataLayer() const { return bIsFirstDataLayerIsExternal; }

	const TArray<FName>& GetRawArray() const
	{
		return DataLayers;
	}

private:
#if WITH_EDITOR
	bool IsForcedEmptyNonExternalDataLayers() const { return bIsForcedEmptyNonExternalDataLayers; }
	void SetIsForcedEmptyNonExternalDataLayers(bool bInNewValue) { bIsForcedEmptyNonExternalDataLayers = bInNewValue; }
	bool bIsForcedEmptyNonExternalDataLayers;
#endif

	UPROPERTY()
	bool bIsFirstDataLayerIsExternal;

	UPROPERTY()
	TArray<FName> DataLayers;

	friend class FStreamingGenerationActorDescView;
};