// Copyright Epic Games, Inc. All Rights Reserved.

#include "Metadata/PCGMetadataPartitionCommon.h"

#include "PCGContext.h"
#include "PCGElement.h"
#include "PCGModule.h"
#include "PCGParamData.h"
#include "Data/PCGPointData.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataCommon.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"

#include "Algo/IndexOf.h"

#define LOCTEXT_NAMESPACE "PCGMetadataPartitionCommon"

namespace PCGMetadataPartitionCommon
{
	/**
	* Partition a given attribute, by first partitioning all value keys that point to the same value
	* and then for each unique value key, list of index in the keys that match for this value.
	*/
	TArray<TArray<int32>> AttributePartition(const FPCGMetadataAttributeBase* InAttribute, const IPCGAttributeAccessorKeys& InKeys, FPCGContext* InOptionalContext)
	{
		check(InAttribute);

		const int32 NumberOfEntries = InKeys.GetNum();

		if (NumberOfEntries <= 0)
		{
			return {};
		}

		TMap<PCGMetadataValueKey, UPCGPointData*> ValueToData;

		// Get all value keys (-1 + 0 - N)
		const int64 MetadataValueKeyCount = InAttribute->GetValueKeyOffsetForChild();

		// For every value key, check if it should be merged with the default value
		TMap<PCGMetadataValueKey, int32> ValueKeyMapping;
		ValueKeyMapping.Reserve(MetadataValueKeyCount);

		int32 NumUniqueValueKeys = 0;

		const bool bUsesValueKeys = InAttribute->UsesValueKeys();

		if (!bUsesValueKeys)
		{
			TArray<PCGMetadataValueKey> UniqueValueKeys;

			for (PCGMetadataValueKey ValueKey = 0; ValueKey < MetadataValueKeyCount; ++ValueKey)
			{
				if (InAttribute->IsEqualToDefaultValue(ValueKey))
				{
					ValueKeyMapping.Add(ValueKey, -1);
					continue;
				}

				// TODO: Might want to upgrade to something better wince it can be quadractic and grow quickly.
				int32 UniqueValueKeyIndex = Algo::IndexOfByPredicate(UniqueValueKeys, [ValueKey, InAttribute](const PCGMetadataValueKey& Key)
				{
					return InAttribute->AreValuesEqual(ValueKey, Key);
				});

				if (UniqueValueKeyIndex == INDEX_NONE)
				{
					ValueKeyMapping.Add(ValueKey, UniqueValueKeys.Num());
					UniqueValueKeys.Add(ValueKey);
					NumUniqueValueKeys++;
				}
				else
				{
					ValueKeyMapping.Add(ValueKey, UniqueValueKeyIndex);
				}
			}
		}
		else
		{
			NumUniqueValueKeys = MetadataValueKeyCount;
		}

		TArray<TArray<int32>> PartitionedData;
		PartitionedData.SetNum(1 + NumUniqueValueKeys);

		constexpr int32 ChunkSize = 256;
		TArray<const PCGMetadataEntryKey*, TInlineAllocator<ChunkSize>> TempEntries;
		TempEntries.SetNum(ChunkSize);

		const int32 NumberOfIterations = (NumberOfEntries + ChunkSize - 1) / ChunkSize;

		for (int32 i = 0; i < NumberOfIterations; ++i)
		{
			const int32 StartIndex = i * ChunkSize;
			const int32 Range = FMath::Min(NumberOfEntries - StartIndex, ChunkSize);
			TArrayView<const PCGMetadataEntryKey*> View(TempEntries.GetData(), Range);
			InKeys.GetKeys(StartIndex, View);

			for (int32 j = 0; j < Range; ++j)
			{
				PCGMetadataValueKey ValueKey = InAttribute->GetValueKey(*TempEntries[j]);
				// Remap if not using value keys
				if (!bUsesValueKeys && ValueKey != PCGDefaultValueKey)
				{
					ValueKey = ValueKeyMapping[ValueKey];
				}

				const int32 PartitionDataIndex = 1 + ValueKey;
				PartitionedData[PartitionDataIndex].Add(StartIndex + j);
			}
		}

		// Since we partition on the value array, it is not guarenteed that the values appears in the same order than the entries.
		// So sort the final array using the first index as a sort criteria. Empty partitions will be at the beginning too.
		PartitionedData.Sort([](const TArray<int32>& LHS, const TArray<int32>& RHS) -> bool
		{ 
			if (LHS.IsEmpty())
			{
				return true;
			}
			else if (RHS.IsEmpty())
			{
				return false;
			}
			else
			{
				return LHS[0] < RHS[0];
			}
		});

		return PartitionedData;
	}

	/**
	* Partition a given accessor that iterate on all values, find the identical ones,
	* and then for each unique value, list of index in the keys that match for this value.
	*/
	template <typename T>
	TArray<TArray<int32>> ValuePartition(const IPCGAttributeAccessor& InAccessor, const IPCGAttributeAccessorKeys& InKeys, FPCGContext* InOptionalContext)
	{
		TArray<T> UniqueValues;
		TArray<TArray<int32>> PartitionedData;

		PCGMetadataElementCommon::ApplyOnAccessor<T>(InKeys, InAccessor, [&PartitionedData, &UniqueValues](const T& InValue, int32 InIndex)
		{
			// TODO: Might want to upgrade to something better wince it can be quadractic and grow quickly.
			int32 UniqueValueIndex = UniqueValues.IndexOfByPredicate([&InValue](const T& OtherValue)
			{
				// For consistency with the attribute part, use MetadataTraits::Equal
				return PCG::Private::MetadataTraits<T>::Equal(InValue, OtherValue);
			});

			if (UniqueValueIndex == INDEX_NONE)
			{
				UniqueValueIndex = UniqueValues.Add(InValue);
				PartitionedData.Emplace();
			}

			PartitionedData[UniqueValueIndex].Add(InIndex);
		});

		return PartitionedData;
	}

	/**
	* Dispatch the partition according to the data and selector.
	*/
	TArray<TArray<int32>> AttributeGenericPartition(const UPCGData* InData, const FPCGAttributePropertySelector& InSelector, FPCGContext* InOptionalContext)
	{
		if (!InData)
		{
			return {};
		}

		TUniquePtr<const IPCGAttributeAccessorKeys> Keys = PCGAttributeAccessorHelpers::CreateConstKeys(InData, InSelector);
		if (!Keys.IsValid())
		{
			PCGLog::LogErrorOnGraph(FText::Format(LOCTEXT("InvalidKeys", "Could not create keys for the input data with selector {0}"), InSelector.GetDisplayText()), InOptionalContext);
			return {};
		}

		if (InSelector.IsBasicAttribute())
		{
			const UPCGMetadata* Metadata = InData->ConstMetadata();
			if (!Metadata)
			{
				PCGLog::LogErrorOnGraph(FText::Format(LOCTEXT("InvalidMetadata", "Input data does not have metadata, while requesting an attribute {0}"), InSelector.GetDisplayText()), InOptionalContext);
				return {};
			}

			const FPCGMetadataAttributeBase* Attribute = Metadata->GetConstAttribute(InSelector.GetName());
			if (!Attribute)
			{
				PCGLog::LogErrorOnGraph(FText::Format(LOCTEXT("InvalidAttribute", "Attribute {0} not found"), InSelector.GetDisplayText()), InOptionalContext);
				return {};
			}

			return AttributePartition(Attribute, *Keys, InOptionalContext);
		}
		else
		{
			TUniquePtr<const IPCGAttributeAccessor> Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(InData, InSelector);
			if (!Accessor.IsValid())
			{
				PCGLog::LogErrorOnGraph(FText::Format(LOCTEXT("InvalidAccessor", "Attribute {0} not found"), InSelector.GetDisplayText()), InOptionalContext);
				return {};
			}

			auto Operation = [&Accessor, &Keys, &InSelector, InOptionalContext](auto Dummy) -> TArray<TArray<int32>>
			{
				// Rotators don't have a hash, convert them to Quat
				using AttributeType = std::conditional_t<std::is_same_v<decltype(Dummy), FRotator>, FQuat, decltype(Dummy)>;

				// Can't partition on a transform.
				if constexpr (std::is_same_v<AttributeType, FTransform>)
				{
					PCGLog::LogErrorOnGraph(FText::Format(LOCTEXT("InvalidType", "Attribute {0} is a transform, partition transforms is not supported"), InSelector.GetDisplayText()), InOptionalContext);
					return {};
				}
				else
				{
					return ValuePartition<AttributeType>(*Accessor, *Keys, InOptionalContext);
				}
			};

			return PCGMetadataAttribute::CallbackWithRightType(Accessor->GetUnderlyingType(), Operation);
		}
	}

	/**
	* Do a partition on the given point data for the selector
	*/
	TArray<UPCGData*> AttributePointPartition(const UPCGPointData* InData, const FPCGAttributePropertySelector& InSelector, FPCGContext* InOptionalContext)
	{
		TArray<TArray<int32>> Partition = AttributeGenericPartition(InData, InSelector, InOptionalContext);
		if (Partition.IsEmpty())
		{
			return {};
		}

		TArray<UPCGData*> PartitionedData;
		PartitionedData.Reserve(Partition.Num());
		const TArray<FPCGPoint>& Points = InData->GetPoints();

		for (TArray<int32>& Indices : Partition)
		{
			if (Indices.IsEmpty())
			{
				continue;
			}

			UPCGPointData* CurrentPointData = NewObject<UPCGPointData>();
			PartitionedData.Add(CurrentPointData);
			CurrentPointData->InitializeFromData(InData);

			TArray<FPCGPoint>& OutPoints = CurrentPointData->GetMutablePoints();
			OutPoints.Reserve(Indices.Num());
			for (int32 Index : Indices)
			{
				OutPoints.Add(Points[Index]);
			}
		}

		return PartitionedData;
	}

	TArray<UPCGData*> AttributeParamSpatialPartition(const UPCGData* InData, const FPCGAttributePropertySelector& InSelector, FPCGContext* InOptionalContext)
	{
		const UPCGSpatialData* InSpatialData = Cast<const UPCGSpatialData>(InData);
		const UPCGParamData* InParamData = Cast<const UPCGParamData>(InData);

		if (!InSpatialData && !InParamData)
		{
			PCGLog::LogErrorOnGraph(LOCTEXT("InvalidDataType", "Input data is not an attribute set nor a spatial data. Operation not supported."), InOptionalContext);
			return {};
		}

		TArray<TArray<int32>> Partition = AttributeGenericPartition(InData, InSelector, InOptionalContext);
		if (Partition.IsEmpty())
		{
			return {};
		}

		TArray<UPCGData*> PartitionedData;
		PartitionedData.Reserve(Partition.Num());

		TArray<FName> AttributeNames;
		TArray<EPCGMetadataTypes> AttributeTypes;
		const UPCGMetadata* OriginalMetadata = InData->ConstMetadata();
		OriginalMetadata->GetAttributes(AttributeNames, AttributeTypes);

		for (TArray<int32>& Indices : Partition)
		{
			if (Indices.IsEmpty())
			{
				continue;
			}

			UPCGMetadata* NewMetadata = nullptr;

			if (InSpatialData)
			{
				UPCGSpatialData* NewData = NewObject<UPCGSpatialData>();
				NewData->InitializeFromData(InSpatialData);
				NewMetadata = NewData->Metadata;
				PartitionedData.Add(NewData);
			}
			else
			{
				UPCGParamData* NewData = NewObject<UPCGParamData>();
				NewData->Metadata->AddAttributes(OriginalMetadata);
				NewMetadata = NewData->Metadata;
				PartitionedData.Add(NewData);
			}

			TArray<PCGMetadataEntryKey> EntryKeys;
			EntryKeys.Reserve(Indices.Num());
			for (int32 i = 0; i < Indices.Num(); ++i)
			{
				EntryKeys.Add(NewMetadata->AddEntry());
			}

			for (const FName AttributeName : AttributeNames)
			{
				const FPCGMetadataAttributeBase* OriginalAttribute = OriginalMetadata->GetConstAttribute(AttributeName);
				FPCGMetadataAttributeBase* NewAttribute = NewMetadata->GetMutableAttribute(AttributeName);
				check(OriginalAttribute && NewAttribute);

				for (int32 i = 0; i < Indices.Num(); ++i)
				{
					NewAttribute->SetValue(EntryKeys[i], OriginalAttribute, Indices[i]);
				}

				if (Indices.Num() == 1)
				{
					NewAttribute->SetDefaultValueToFirstEntry();
				}
			}
		}

		return PartitionedData;
	}

	TArray<UPCGData*> AttributePartition(const UPCGData* InData, const FPCGAttributePropertySelector& InSelector, FPCGContext* InOptionalContext)
	{
		if (const UPCGPointData* InPointData = Cast<UPCGPointData>(InData))
		{
			return AttributePointPartition(InPointData, InSelector, InOptionalContext);
		}
		else
		{
			return AttributeParamSpatialPartition(InData, InSelector, InOptionalContext);
		}
	}
}

#undef LOCTEXT_NAMESPACE