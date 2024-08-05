// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compute/PCGDataForGPU.h"

#include "PCGData.h"
#include "PCGParamData.h"
#include "PCGPoint.h"
#include "Compute/PCGComputeCommon.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGAsync.h"
#include "Metadata/PCGMetadata.h"

#include "Async/ParallelFor.h"

using namespace PCGComputeConstants;

namespace PCGDataForGPUConstants
{
	const static FPCGKernelAttributeDesc PointPropertyDescs[NUM_POINT_PROPERTIES] =
	{
		FPCGKernelAttributeDesc(POINT_POSITION_ATTRIBUTE_ID,   EPCGKernelAttributeType::Float3, NAME_None),
		FPCGKernelAttributeDesc(POINT_ROTATION_ATTRIBUTE_ID,   EPCGKernelAttributeType::Quat,   NAME_None),
		FPCGKernelAttributeDesc(POINT_SCALE_ATTRIBUTE_ID,      EPCGKernelAttributeType::Float3, NAME_None),
		FPCGKernelAttributeDesc(POINT_BOUNDS_MIN_ATTRIBUTE_ID, EPCGKernelAttributeType::Float3, NAME_None),
		FPCGKernelAttributeDesc(POINT_BOUNDS_MAX_ATTRIBUTE_ID, EPCGKernelAttributeType::Float3, NAME_None),
		FPCGKernelAttributeDesc(POINT_COLOR_ATTRIBUTE_ID,      EPCGKernelAttributeType::Float4, NAME_None),
		FPCGKernelAttributeDesc(POINT_DENSITY_ATTRIBUTE_ID,    EPCGKernelAttributeType::Float,  NAME_None),
		FPCGKernelAttributeDesc(POINT_SEED_ATTRIBUTE_ID,       EPCGKernelAttributeType::Int,    NAME_None),
		FPCGKernelAttributeDesc(POINT_STEEPNESS_ATTRIBUTE_ID,  EPCGKernelAttributeType::Float,  NAME_None)
	};
}

namespace PCGDataForGPUHelpers
{
	EPCGKernelAttributeType GetAttributeTypeFromMetadataType(EPCGMetadataTypes MetadataType)
	{
		switch (MetadataType)
		{
		case EPCGMetadataTypes::Boolean:
			return EPCGKernelAttributeType::Bool;
		case EPCGMetadataTypes::Float:
		case EPCGMetadataTypes::Double:
			return EPCGKernelAttributeType::Float;
		case EPCGMetadataTypes::Integer32:
		case EPCGMetadataTypes::Integer64:
			return EPCGKernelAttributeType::Int;
		case EPCGMetadataTypes::Vector2:
			return EPCGKernelAttributeType::Float2;
		case EPCGMetadataTypes::Vector:
			return EPCGKernelAttributeType::Float3;
		case EPCGMetadataTypes::Rotator:
			return EPCGKernelAttributeType::Rotator;
		case EPCGMetadataTypes::Vector4:
			return EPCGKernelAttributeType::Float4;
		case EPCGMetadataTypes::Quaternion:
			return EPCGKernelAttributeType::Quat;
		case EPCGMetadataTypes::Transform:
			return EPCGKernelAttributeType::Transform;
		default:
			checkNoEntry();
			return EPCGKernelAttributeType::Float;
		}
	}

	int GetAttributeTypeStrideBytes(EPCGKernelAttributeType Type)
	{
		switch (Type)
		{
		case EPCGKernelAttributeType::Bool:
		case EPCGKernelAttributeType::Int:
		case EPCGKernelAttributeType::Float:
			return 4;
		case EPCGKernelAttributeType::Float2:
			return 8;
		case EPCGKernelAttributeType::Float3:
		case EPCGKernelAttributeType::Rotator:
			return 12;
		case EPCGKernelAttributeType::Float4:
		case EPCGKernelAttributeType::Quat:
			return 16;
		case EPCGKernelAttributeType::Transform:
			return 64;
		default:
			checkNoEntry();
			return 0;
		}
	}

	bool PackAttributeHelper(const FPCGMetadataAttributeBase* InAttributeBase, const FPCGKernelAttributeDesc& InAttributeDesc, PCGMetadataEntryKey InEntryKey, TArray<uint32>& OutPackedDataCollection, uint32 ElementIndex)
	{
		check(InAttributeBase);

		const PCGMetadataValueKey ValueKey = InAttributeBase->GetValueKey(InEntryKey);
		const int16 TypeId = InAttributeBase->GetTypeId();
		const int StrideBytes = PCGDataForGPUHelpers::GetAttributeTypeStrideBytes(InAttributeDesc.Type);

		switch (TypeId)
		{
		case PCG::Private::MetadataTypes<bool>::Id:
		{
			const FPCGMetadataAttribute<bool>* Attribute = static_cast<const FPCGMetadataAttribute<bool>*>(InAttributeBase);
			const bool Value = Attribute->GetValue(ValueKey);
			check(StrideBytes == 4);
			OutPackedDataCollection[ElementIndex + 0] = Value;
			break;
		}
		case PCG::Private::MetadataTypes<float>::Id:
		{
			const FPCGMetadataAttribute<float>* Attribute = static_cast<const FPCGMetadataAttribute<float>*>(InAttributeBase);
			const float Value = Attribute->GetValue(ValueKey);
			check(StrideBytes == 4);
			OutPackedDataCollection[ElementIndex + 0] = FMath::AsUInt(Value);
			break;
		}
		case PCG::Private::MetadataTypes<double>::Id:
		{
			const FPCGMetadataAttribute<double>* Attribute = static_cast<const FPCGMetadataAttribute<double>*>(InAttributeBase);
			const double Value = Attribute->GetValue(ValueKey);
			check(StrideBytes == 4);
			OutPackedDataCollection[ElementIndex + 0] = FMath::AsUInt(static_cast<float>(Value));
			break;
		}
		case PCG::Private::MetadataTypes<int32>::Id:
		{
			const FPCGMetadataAttribute<int32>* Attribute = static_cast<const FPCGMetadataAttribute<int32>*>(InAttributeBase);
			const int32 Value = Attribute->GetValue(ValueKey);
			check(StrideBytes == 4);
			OutPackedDataCollection[ElementIndex + 0] = Value;
			break;
		}
		case PCG::Private::MetadataTypes<int64>::Id:
		{
			const FPCGMetadataAttribute<int64>* Attribute = static_cast<const FPCGMetadataAttribute<int64>*>(InAttributeBase);
			const int64 Value = Attribute->GetValue(ValueKey);
			check(StrideBytes == 4);
			OutPackedDataCollection[ElementIndex + 0] = Value;
			break;
		}
		case PCG::Private::MetadataTypes<FVector2D>::Id:
		{
			const FPCGMetadataAttribute<FVector2D>* Attribute = static_cast<const FPCGMetadataAttribute<FVector2D>*>(InAttributeBase);
			const FVector2D Value = Attribute->GetValue(ValueKey);
			check(StrideBytes == 8);
			OutPackedDataCollection[ElementIndex + 0] = FMath::AsUInt(static_cast<float>(Value.X));
			OutPackedDataCollection[ElementIndex + 1] = FMath::AsUInt(static_cast<float>(Value.Y));
			break;
		}
		case PCG::Private::MetadataTypes<FRotator>::Id:
		{
			const FPCGMetadataAttribute<FRotator>* Attribute = static_cast<const FPCGMetadataAttribute<FRotator>*>(InAttributeBase);
			const FRotator Value = Attribute->GetValue(ValueKey);
			check(StrideBytes == 12);
			OutPackedDataCollection[ElementIndex + 0] = FMath::AsUInt(static_cast<float>(Value.Pitch));
			OutPackedDataCollection[ElementIndex + 1] = FMath::AsUInt(static_cast<float>(Value.Yaw));
			OutPackedDataCollection[ElementIndex + 2] = FMath::AsUInt(static_cast<float>(Value.Roll));
			break;
		}
		case PCG::Private::MetadataTypes<FVector>::Id:
		{
			const FPCGMetadataAttribute<FVector>* Attribute = static_cast<const FPCGMetadataAttribute<FVector>*>(InAttributeBase);
			const FVector Value = Attribute->GetValue(ValueKey);
			check(StrideBytes == 12);
			OutPackedDataCollection[ElementIndex + 0] = FMath::AsUInt(static_cast<float>(Value.X));
			OutPackedDataCollection[ElementIndex + 1] = FMath::AsUInt(static_cast<float>(Value.Y));
			OutPackedDataCollection[ElementIndex + 2] = FMath::AsUInt(static_cast<float>(Value.Z));
			break;
		}
		case PCG::Private::MetadataTypes<FVector4>::Id:
		{
			const FPCGMetadataAttribute<FVector4>* Attribute = static_cast<const FPCGMetadataAttribute<FVector4>*>(InAttributeBase);
			const FVector4 Value = Attribute->GetValue(ValueKey);
			check(StrideBytes == 16);
			OutPackedDataCollection[ElementIndex + 0] = FMath::AsUInt(static_cast<float>(Value.X));
			OutPackedDataCollection[ElementIndex + 1] = FMath::AsUInt(static_cast<float>(Value.Y));
			OutPackedDataCollection[ElementIndex + 2] = FMath::AsUInt(static_cast<float>(Value.Z));
			OutPackedDataCollection[ElementIndex + 3] = FMath::AsUInt(static_cast<float>(Value.W));
			break;
		}
		case PCG::Private::MetadataTypes<FQuat>::Id:
		{
			const FPCGMetadataAttribute<FQuat>* Attribute = static_cast<const FPCGMetadataAttribute<FQuat>*>(InAttributeBase);
			const FQuat Value = Attribute->GetValue(ValueKey);
			check(StrideBytes == 16);
			OutPackedDataCollection[ElementIndex + 0] = FMath::AsUInt(static_cast<float>(Value.X));
			OutPackedDataCollection[ElementIndex + 1] = FMath::AsUInt(static_cast<float>(Value.Y));
			OutPackedDataCollection[ElementIndex + 2] = FMath::AsUInt(static_cast<float>(Value.Z));
			OutPackedDataCollection[ElementIndex + 3] = FMath::AsUInt(static_cast<float>(Value.W));
			break;
		}
		case PCG::Private::MetadataTypes<FTransform>::Id:
		{
			const FPCGMetadataAttribute<FTransform>* Attribute = static_cast<const FPCGMetadataAttribute<FTransform>*>(InAttributeBase);
			const FTransform Value = Attribute->GetValue(ValueKey);
			const FMatrix Matrix = Value.ToMatrixWithScale();
			check(StrideBytes == 64);
			OutPackedDataCollection[ElementIndex + 0] = FMath::AsUInt(static_cast<float>(Matrix.M[0][0]));
			OutPackedDataCollection[ElementIndex + 1] = FMath::AsUInt(static_cast<float>(Matrix.M[0][1]));
			OutPackedDataCollection[ElementIndex + 2] = FMath::AsUInt(static_cast<float>(Matrix.M[0][2]));
			OutPackedDataCollection[ElementIndex + 3] = FMath::AsUInt(static_cast<float>(Matrix.M[0][3]));
			OutPackedDataCollection[ElementIndex + 4] = FMath::AsUInt(static_cast<float>(Matrix.M[1][0]));
			OutPackedDataCollection[ElementIndex + 5] = FMath::AsUInt(static_cast<float>(Matrix.M[1][1]));
			OutPackedDataCollection[ElementIndex + 6] = FMath::AsUInt(static_cast<float>(Matrix.M[1][2]));
			OutPackedDataCollection[ElementIndex + 7] = FMath::AsUInt(static_cast<float>(Matrix.M[1][3]));
			OutPackedDataCollection[ElementIndex + 8] = FMath::AsUInt(static_cast<float>(Matrix.M[2][0]));
			OutPackedDataCollection[ElementIndex + 9] = FMath::AsUInt(static_cast<float>(Matrix.M[2][1]));
			OutPackedDataCollection[ElementIndex + 10] = FMath::AsUInt(static_cast<float>(Matrix.M[2][2]));
			OutPackedDataCollection[ElementIndex + 11] = FMath::AsUInt(static_cast<float>(Matrix.M[2][3]));
			OutPackedDataCollection[ElementIndex + 12] = FMath::AsUInt(static_cast<float>(Matrix.M[3][0]));
			OutPackedDataCollection[ElementIndex + 13] = FMath::AsUInt(static_cast<float>(Matrix.M[3][1]));
			OutPackedDataCollection[ElementIndex + 14] = FMath::AsUInt(static_cast<float>(Matrix.M[3][2]));
			OutPackedDataCollection[ElementIndex + 15] = FMath::AsUInt(static_cast<float>(Matrix.M[3][3]));
			break;
		}
		default:
			return false;
		}

		return true;
	}

	FPCGMetadataAttributeBase* CreateAttributeFromAttributeDesc(UPCGMetadata* Metadata, const FPCGKernelAttributeDesc& AttributeDesc)
	{
		switch (AttributeDesc.Type)
		{
		case EPCGKernelAttributeType::Bool:
		{
			return Metadata->FindOrCreateAttribute<bool>(AttributeDesc.Name);
		}
		case EPCGKernelAttributeType::Int:
		{
			return Metadata->FindOrCreateAttribute<int>(AttributeDesc.Name);
		}
		case EPCGKernelAttributeType::Float:
		{
			return Metadata->FindOrCreateAttribute<float>(AttributeDesc.Name);
		}
		case EPCGKernelAttributeType::Float2:
		{
			return Metadata->FindOrCreateAttribute<FVector2D>(AttributeDesc.Name);
		}
		case EPCGKernelAttributeType::Float3:
		{
			return Metadata->FindOrCreateAttribute<FVector>(AttributeDesc.Name);
		}
		case EPCGKernelAttributeType::Float4:
		{
			return Metadata->FindOrCreateAttribute<FVector4>(AttributeDesc.Name);
		}
		case EPCGKernelAttributeType::Rotator:
		{
			return Metadata->FindOrCreateAttribute<FRotator>(AttributeDesc.Name);
		}
		case EPCGKernelAttributeType::Quat:
		{
			return Metadata->FindOrCreateAttribute<FQuat>(AttributeDesc.Name);
		}
		case EPCGKernelAttributeType::Transform:
		{
			return Metadata->FindOrCreateAttribute<FTransform>(AttributeDesc.Name);
		}
		default:
			return nullptr;
		}
	}

	bool UnpackAttributeHelper(const void* InPackedData, uint32 ElementIndex, FPCGMetadataAttributeBase* AttributeBase, const FPCGKernelAttributeDesc& AttributeDesc, PCGMetadataEntryKey EntryKey)
	{
		check(InPackedData && AttributeBase);

		const float* DataAsFloat = static_cast<const float*>(InPackedData);
		const int* DataAsInt = static_cast<const int*>(InPackedData);

		switch (AttributeDesc.Type)
		{
		case EPCGKernelAttributeType::Bool:
		{
			FPCGMetadataAttribute<bool>* Attribute = static_cast<FPCGMetadataAttribute<bool>*>(AttributeBase);

			const bool Value = static_cast<bool>(DataAsFloat[ElementIndex]);
			Attribute->SetValue(EntryKey, Value);
			break;
		}
		case EPCGKernelAttributeType::Int:
		{
			FPCGMetadataAttribute<int>* Attribute = static_cast<FPCGMetadataAttribute<int>*>(AttributeBase);

			const int Value = DataAsInt[ElementIndex];
			Attribute->SetValue(EntryKey, Value);
			break;
		}
		case EPCGKernelAttributeType::Float:
		{
			FPCGMetadataAttribute<float>* Attribute = static_cast<FPCGMetadataAttribute<float>*>(AttributeBase);

			const float Value = DataAsFloat[ElementIndex];
			Attribute->SetValue(EntryKey, Value);
			break;
		}
		case EPCGKernelAttributeType::Float2:
		{
			FPCGMetadataAttribute<FVector2D>* Attribute = static_cast<FPCGMetadataAttribute<FVector2D>*>(AttributeBase);

			FVector2D Value;
			Value.X = DataAsFloat[ElementIndex + 0];
			Value.Y = DataAsFloat[ElementIndex + 1];
			Attribute->SetValue(EntryKey, Value);
			break;
		}
		case EPCGKernelAttributeType::Float3:
		{
			FPCGMetadataAttribute<FVector>* Attribute = static_cast<FPCGMetadataAttribute<FVector>*>(AttributeBase);

			FVector Value;
			Value.X = DataAsFloat[ElementIndex + 0];
			Value.Y = DataAsFloat[ElementIndex + 1];
			Value.Z = DataAsFloat[ElementIndex + 2];
			Attribute->SetValue(EntryKey, Value);
			break;
		}
		case EPCGKernelAttributeType::Float4:
		{
			FPCGMetadataAttribute<FVector4>* Attribute = static_cast<FPCGMetadataAttribute<FVector4>*>(AttributeBase);

			FVector4 Value;
			Value.X = DataAsFloat[ElementIndex + 0];
			Value.Y = DataAsFloat[ElementIndex + 1];
			Value.Z = DataAsFloat[ElementIndex + 2];
			Value.W = DataAsFloat[ElementIndex + 3];
			Attribute->SetValue(EntryKey, Value);
			break;
		}
		case EPCGKernelAttributeType::Rotator:
		{
			FPCGMetadataAttribute<FRotator>* Attribute = static_cast<FPCGMetadataAttribute<FRotator>*>(AttributeBase);

			FRotator Value;
			Value.Pitch = DataAsFloat[ElementIndex + 0];
			Value.Yaw = DataAsFloat[ElementIndex + 1];
			Value.Roll = DataAsFloat[ElementIndex + 2];
			Attribute->SetValue(EntryKey, Value);
			break;
		}
		case EPCGKernelAttributeType::Quat:
		{
			FPCGMetadataAttribute<FQuat>* Attribute = static_cast<FPCGMetadataAttribute<FQuat>*>(AttributeBase);

			FQuat Value;
			Value.X = DataAsFloat[ElementIndex + 0];
			Value.Y = DataAsFloat[ElementIndex + 1];
			Value.Z = DataAsFloat[ElementIndex + 2];
			Value.W = DataAsFloat[ElementIndex + 3];
			Attribute->SetValue(EntryKey, Value);
			break;
		}
		case EPCGKernelAttributeType::Transform:
		{
			FPCGMetadataAttribute<FTransform>* Attribute = static_cast<FPCGMetadataAttribute<FTransform>*>(AttributeBase);

			FMatrix Matrix;
			Matrix.M[0][0] = DataAsFloat[ElementIndex + 0];
			Matrix.M[0][1] = DataAsFloat[ElementIndex + 1];
			Matrix.M[0][2] = DataAsFloat[ElementIndex + 2];
			Matrix.M[0][3] = DataAsFloat[ElementIndex + 3];
			Matrix.M[1][0] = DataAsFloat[ElementIndex + 4];
			Matrix.M[1][1] = DataAsFloat[ElementIndex + 5];
			Matrix.M[1][2] = DataAsFloat[ElementIndex + 6];
			Matrix.M[1][3] = DataAsFloat[ElementIndex + 7];
			Matrix.M[2][0] = DataAsFloat[ElementIndex + 8];
			Matrix.M[2][1] = DataAsFloat[ElementIndex + 9];
			Matrix.M[2][2] = DataAsFloat[ElementIndex + 10];
			Matrix.M[2][3] = DataAsFloat[ElementIndex + 11];
			Matrix.M[3][0] = DataAsFloat[ElementIndex + 12];
			Matrix.M[3][1] = DataAsFloat[ElementIndex + 13];
			Matrix.M[3][2] = DataAsFloat[ElementIndex + 14];
			Matrix.M[3][3] = DataAsFloat[ElementIndex + 15];

			const FTransform Value(Matrix);
			Attribute->SetValue(EntryKey, Value);
			break;
		}
		default:
			return false;
		}

		return true;
	}
}

bool FPCGKernelAttributeKey::operator==(const FPCGKernelAttributeKey& Other) const
{
	return Type == Other.Type && Name == Other.Name;
}

uint32 GetTypeHash(const FPCGKernelAttributeKey& In)
{
	return HashCombine(GetTypeHash(In.Type), GetTypeHash(In.Name));
}

bool FPCGKernelAttributeDesc::operator==(const FPCGKernelAttributeDesc& Other) const
{
	return Index == Other.Index && Type == Other.Type && Name == Other.Name;
}

FPCGDataDesc::FPCGDataDesc(EPCGDataType InType, int InElementCount)
	: Type(InType)
	, ElementCount(InElementCount)
{
	InitializeAttributeDescs(nullptr);
}

FPCGDataDesc::FPCGDataDesc(const UPCGData* Data, const TMap<FPCGKernelAttributeKey, int32>& GlobalAttributeLookupTable)
{
	check(Data);

	Type = Data->GetDataType();
	ElementCount = PCGComputeHelpers::GetElementCount(Data);

	InitializeAttributeDescs(Data->ConstMetadata(), GlobalAttributeLookupTable);
}

uint32 FPCGDataDesc::ComputePackedSize() const
{
	uint32 DataSizeBytes = 0;

	if (Type == EPCGDataType::Point)
	{
		DataSizeBytes += POINT_DATA_HEADER_SIZE_BYTES;
	}
	else if (Type == EPCGDataType::Param)
	{
		DataSizeBytes += PARAM_DATA_HEADER_SIZE_BYTES;
	}
	else
	{
		// TODO: Support more types
		checkNoEntry();
	}

	for (const FPCGKernelAttributeDesc& AttributeDesc : AttributeDescs)
	{
		DataSizeBytes += PCGDataForGPUHelpers::GetAttributeTypeStrideBytes(AttributeDesc.Type) * ElementCount;
	}

	return DataSizeBytes;
}

void FPCGDataDesc::InitializeAttributeDescs(const UPCGMetadata* Metadata, const TMap<FPCGKernelAttributeKey, int32>& GlobalAttributeLookupTable)
{
	if (Type == EPCGDataType::Point)
	{
		AttributeDescs.Append(PCGDataForGPUConstants::PointPropertyDescs, NUM_POINT_PROPERTIES);
	}
	else { /* TODO: More types! */ }

	if (Metadata)
	{
		TArray<FName> AttributeNames;
		TArray<EPCGMetadataTypes> AttributeTypes;
		Metadata->GetAttributes(AttributeNames, AttributeTypes);

		TArray<FPCGKernelAttributeKey> DelayedAttributeKeys; // Attribute keys that don't exist in the global lookup table must be delayed so we can append them at the end.
		int NumAttributesFromLUT = 0; // Keep track of how many attributes come from the LUT. This will help give us the starting index for our delayed attributes.

		for (int CustomAttributeIndex = 0; CustomAttributeIndex < AttributeNames.Num(); ++CustomAttributeIndex)
		{
			const FName AttributeName = AttributeNames[CustomAttributeIndex];
			const EPCGKernelAttributeType AttributeType = PCGDataForGPUHelpers::GetAttributeTypeFromMetadataType(AttributeTypes[CustomAttributeIndex]);
			const FPCGKernelAttributeKey AttributeKey = { AttributeType, AttributeName };

			// Ignore excess attributes.
			if (CustomAttributeIndex >= MAX_NUM_CUSTOM_ATTRS)
			{
				// TODO: Would be nice to include the pin label for debug purposes
				UE_LOG(LogPCG, Warning, TEXT("Attempted to exceed max number of custom attributes (%d). Additional attributes will be ignored."), MAX_NUM_CUSTOM_ATTRS);
				break;
			}

			if (AttributeName == NAME_None)
			{
				// TODO: Would be nice to include the pin label for debug purposes
				UE_LOG(LogPCG, Warning, TEXT("'%s' is not a valid name for a kernel attribute. Attribute will be skipped."), *AttributeName.ToString());
				continue;
			}

			if (const int32* AttributeId = GlobalAttributeLookupTable.Find(AttributeKey))
			{
				AttributeDescs.Emplace(*AttributeId, AttributeType, AttributeName);
				++NumAttributesFromLUT;
			}
			else
			{
				DelayedAttributeKeys.Add(AttributeKey);
			}
		}

		for (int DelayedAttributeIndex = 0; DelayedAttributeIndex < DelayedAttributeKeys.Num(); ++DelayedAttributeIndex)
		{
			const FPCGKernelAttributeKey& AttributeKey = DelayedAttributeKeys[DelayedAttributeIndex];
			AttributeDescs.Emplace(NUM_RESERVED_ATTRS + DelayedAttributeIndex + NumAttributesFromLUT, AttributeKey.Type, AttributeKey.Name);
		}
	}
}

FPCGDataCollectionDesc FPCGDataCollectionDesc::BuildFromInputDataCollectionAndInputPinLabel(
	const FPCGDataCollection& InDataCollection,
	FName InputPinLabel,
	const TMap<FPCGKernelAttributeKey, int32>& InAttributeLookupTable)
{
	FPCGDataCollectionDesc Desc;
	TArray<FPCGTaggedData> DataForPin = InDataCollection.GetInputsByPin(InputPinLabel);

	for (const FPCGTaggedData& Data : DataForPin)
	{
		if (!Data.Data || !PCGComputeHelpers::IsTypeAllowedInDataCollection(Data.Data->GetDataType()))
		{
			continue;
		}

		Desc.DataDescs.Emplace(Data.Data, InAttributeLookupTable);
	}

	return Desc;
}

uint32 FPCGDataCollectionDesc::ComputePackedSize(TArray<uint32>* OutDataAddresses) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGDataCollectionDesc::ComputePackedSize);

	const int NumData = DataDescs.Num();
	// Calculation: sizeof(NumDatas) + (sizeof(DataAddress) * NumData)
	const uint32 CollectionHeaderSizeBytes = sizeof(uint32) + (sizeof(uint32) * NumData);
	uint32 TotalCollectionSizeBytes = CollectionHeaderSizeBytes;

	if (OutDataAddresses)
	{
		OutDataAddresses->SetNumUninitialized(NumData);
	}

	for (int DataIndex = 0; DataIndex < NumData; ++DataIndex)
	{
		const FPCGDataDesc& DataDesc = DataDescs[DataIndex];
		const uint32 DataSize = DataDesc.ComputePackedSize();

		if (OutDataAddresses)
		{
			(*OutDataAddresses)[DataIndex] = TotalCollectionSizeBytes;
		}

		TotalCollectionSizeBytes += DataSize;
	}

	return TotalCollectionSizeBytes;
}

void FPCGDataCollectionDesc::PackDataCollection(const FPCGDataCollection& InDataCollection, FName InPin, TArray<uint32>& OutPackedDataCollection) const
{
	const TArray<FPCGTaggedData> InputData = InDataCollection.GetInputsByPin(InPin);
	const uint32 NumData = InputData.Num();

	TArray<uint32> DataAddresses;
	const uint32 PackedDataCollectionSizeBytes = ComputePackedSize(&DataAddresses);

	OutPackedDataCollection.SetNumZeroed(PackedDataCollectionSizeBytes / sizeof(uint32));
	OutPackedDataCollection[0] = NumData;

	for (uint32 DataIndex = 0; DataIndex < NumData; ++DataIndex)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGDataCollectionDesc::PackDataItem);

		// Write the data addresses
		const uint32 CurrentDataAddress = DataAddresses[DataIndex];
		const uint32 CurrentDataIndex = CurrentDataAddress / sizeof(uint32);
		OutPackedDataCollection[DataIndex + 1] = CurrentDataAddress;

		// DataHeader: (TypeId, NumAttrs, AttrHeaderStartOffset, TypeInfo), Attr0 Header, Attr1 Header, ..., Attr255 Header
		// Data: Attr0, Attr1, ...
		if (const UPCGPointData* PointData = Cast<UPCGPointData>(InputData[DataIndex].Data))
		{
			const UPCGMetadata* Metadata = PointData->ConstMetadata();
			const TArray<FPCGPoint>& Points = PointData->GetPoints();
			const uint32 NumElements = Points.Num();

			const TArray<FPCGKernelAttributeDesc>& AttributeDescs = DataDescs[DataIndex].AttributeDescs;
			const uint32 NumAttributes = AttributeDescs.Num();

			OutPackedDataCollection[CurrentDataIndex + 0] = /*PointDataTypeId=*/POINT_DATA_TYPE_ID;
			OutPackedDataCollection[CurrentDataIndex + 1] = NumAttributes;
			OutPackedDataCollection[CurrentDataIndex + 2] = POINT_DATA_HEADER_PREAMBLE_SIZE_BYTES;
			OutPackedDataCollection[CurrentDataIndex + 3] = NumElements; // TypeInfo for PointData is just NumPoints

			const uint32 BaseAttributeHeaderAddress = CurrentDataAddress + POINT_DATA_HEADER_PREAMBLE_SIZE_BYTES;
			uint32 CurrentAttributeAddress = CurrentDataAddress + POINT_DATA_HEADER_SIZE_BYTES;

			for (const FPCGKernelAttributeDesc& AttributeDesc : AttributeDescs)
			{
				const uint32 AttributeId = AttributeDesc.Index;
				const uint32 AttributeStrideBytes = PCGDataForGPUHelpers::GetAttributeTypeStrideBytes(AttributeDesc.Type);
				const uint32 AttributeNumComponents = AttributeStrideBytes / sizeof(uint32); // E.g. float3 has 3 components

				// Pack Position (24 bits for AttributeId, 8 bits for Stride)
				const uint32 PackedIdAndStride = (AttributeId << 8) + AttributeStrideBytes;
				const uint32 AttributeIndex = CurrentAttributeAddress / sizeof(uint32);

				const uint32 AttributeHeaderIndex = (BaseAttributeHeaderAddress + AttributeId * ATTRIBUTE_HEADER_SIZE_BYTES) / sizeof(uint32);
				OutPackedDataCollection[AttributeHeaderIndex + 0] = PackedIdAndStride;
				OutPackedDataCollection[AttributeHeaderIndex + 1] = CurrentAttributeAddress;

				const FPCGMetadataAttributeBase* AttributeBase = (AttributeId >= NUM_RESERVED_ATTRS) ? Metadata->GetConstAttribute(AttributeDesc.Name) : nullptr;

				for (uint32 ElementIndex = 0; ElementIndex < NumElements; ++ElementIndex)
				{
					const uint32 PackedDataElementIndex = AttributeIndex + (ElementIndex * AttributeNumComponents);

					if (AttributeBase) // Pack attribute
					{
						ensure(PCGDataForGPUHelpers::PackAttributeHelper(AttributeBase, AttributeDesc, Points[ElementIndex].MetadataEntry, OutPackedDataCollection, PackedDataElementIndex));
					}
					else // Pack property
					{
						switch (AttributeId)
						{
						case POINT_POSITION_ATTRIBUTE_ID:
						{
							const FVector Position = Points[ElementIndex].Transform.GetLocation();

							OutPackedDataCollection[PackedDataElementIndex + 0] = FMath::AsUInt(static_cast<float>(Position.X));
							OutPackedDataCollection[PackedDataElementIndex + 1] = FMath::AsUInt(static_cast<float>(Position.Y));
							OutPackedDataCollection[PackedDataElementIndex + 2] = FMath::AsUInt(static_cast<float>(Position.Z));
							break;
						}
						case POINT_ROTATION_ATTRIBUTE_ID:
						{
							const FQuat Rotation = Points[ElementIndex].Transform.GetRotation();

							OutPackedDataCollection[PackedDataElementIndex + 0] = FMath::AsUInt(static_cast<float>(Rotation.X));
							OutPackedDataCollection[PackedDataElementIndex + 1] = FMath::AsUInt(static_cast<float>(Rotation.Y));
							OutPackedDataCollection[PackedDataElementIndex + 2] = FMath::AsUInt(static_cast<float>(Rotation.Z));
							OutPackedDataCollection[PackedDataElementIndex + 3] = FMath::AsUInt(static_cast<float>(Rotation.W));
							break;
						}
						case POINT_SCALE_ATTRIBUTE_ID:
						{
							const FVector Scale = Points[ElementIndex].Transform.GetScale3D();

							OutPackedDataCollection[PackedDataElementIndex + 0] = FMath::AsUInt(static_cast<float>(Scale.X));
							OutPackedDataCollection[PackedDataElementIndex + 1] = FMath::AsUInt(static_cast<float>(Scale.Y));
							OutPackedDataCollection[PackedDataElementIndex + 2] = FMath::AsUInt(static_cast<float>(Scale.Z));
							break;
						}
						case POINT_BOUNDS_MIN_ATTRIBUTE_ID:
						{
							const FVector& BoundsMin = Points[ElementIndex].BoundsMin;

							OutPackedDataCollection[PackedDataElementIndex + 0] = FMath::AsUInt(static_cast<float>(BoundsMin.X));
							OutPackedDataCollection[PackedDataElementIndex + 1] = FMath::AsUInt(static_cast<float>(BoundsMin.Y));
							OutPackedDataCollection[PackedDataElementIndex + 2] = FMath::AsUInt(static_cast<float>(BoundsMin.Z));
							break;
						}
						case POINT_BOUNDS_MAX_ATTRIBUTE_ID:
						{
							const FVector& BoundsMax = Points[ElementIndex].BoundsMax;

							OutPackedDataCollection[PackedDataElementIndex + 0] = FMath::AsUInt(static_cast<float>(BoundsMax.X));
							OutPackedDataCollection[PackedDataElementIndex + 1] = FMath::AsUInt(static_cast<float>(BoundsMax.Y));
							OutPackedDataCollection[PackedDataElementIndex + 2] = FMath::AsUInt(static_cast<float>(BoundsMax.Z));
							break;
						}
						case POINT_COLOR_ATTRIBUTE_ID:
						{
							const FVector4& Color = Points[ElementIndex].Color;

							OutPackedDataCollection[PackedDataElementIndex + 0] = FMath::AsUInt(static_cast<float>(Color.X));
							OutPackedDataCollection[PackedDataElementIndex + 1] = FMath::AsUInt(static_cast<float>(Color.Y));
							OutPackedDataCollection[PackedDataElementIndex + 2] = FMath::AsUInt(static_cast<float>(Color.Z));
							OutPackedDataCollection[PackedDataElementIndex + 3] = FMath::AsUInt(static_cast<float>(Color.W));
							break;
						}
						case POINT_DENSITY_ATTRIBUTE_ID:
						{
							const float Density = Points[ElementIndex].Density;

							OutPackedDataCollection[PackedDataElementIndex + 0] = FMath::AsUInt(Density);
							break;
						}
						case POINT_SEED_ATTRIBUTE_ID:
						{
							const int Seed = Points[ElementIndex].Seed;

							OutPackedDataCollection[PackedDataElementIndex + 0] = Seed;
							break;
						}
						case POINT_STEEPNESS_ATTRIBUTE_ID:
						{
							const float Steepness = Points[ElementIndex].Steepness;

							OutPackedDataCollection[PackedDataElementIndex + 0] = FMath::AsUInt(Steepness);
							break;
						}
						default:
							checkNoEntry();
							break;
						}
					}
				}

				CurrentAttributeAddress += NumElements * AttributeNumComponents * 4;
			}
		}
		else if (const UPCGParamData* ParamData = Cast<UPCGParamData>(InputData[DataIndex].Data))
		{
			const UPCGMetadata* Metadata = ParamData->ConstMetadata();

			const FPCGDataDesc& DataDesc = DataDescs[DataIndex];
			const uint32 NumElements = DataDesc.ElementCount;

			const TArray<FPCGKernelAttributeDesc>& AttributeDescs = DataDesc.AttributeDescs;
			const uint32 NumAttributes = AttributeDescs.Num();

			OutPackedDataCollection[CurrentDataIndex + 0] = /*ParamDataTypeId=*/PARAM_DATA_TYPE_ID;
			OutPackedDataCollection[CurrentDataIndex + 1] = NumAttributes;
			OutPackedDataCollection[CurrentDataIndex + 2] = PARAM_DATA_HEADER_PREAMBLE_SIZE_BYTES;
			OutPackedDataCollection[CurrentDataIndex + 3] = NumElements; // TypeInfo for ParamData is # of elements

			const uint32 BaseAttributeHeaderAddress = CurrentDataAddress + PARAM_DATA_HEADER_PREAMBLE_SIZE_BYTES;
			uint32 CurrentAttributeAddress = CurrentDataAddress + PARAM_DATA_HEADER_SIZE_BYTES;

			for (const FPCGKernelAttributeDesc& AttributeDesc : AttributeDescs)
			{
				const uint32 AttributeId = AttributeDesc.Index;
				const uint32 AttributeStrideBytes = PCGDataForGPUHelpers::GetAttributeTypeStrideBytes(AttributeDesc.Type);
				const uint32 AttributeNumComponents = AttributeStrideBytes / sizeof(uint32); // E.g. float3 has 3 components

				// Pack Position (24 bits for AttributeId, 8 bits for Stride)
				const uint32 PackedIdAndStride = (AttributeId << 8) + AttributeStrideBytes;
				const uint32 AttributeIndex = CurrentAttributeAddress / sizeof(uint32);

				const uint32 AttributeHeaderIndex = (BaseAttributeHeaderAddress + AttributeId * ATTRIBUTE_HEADER_SIZE_BYTES) / sizeof(uint32);
				OutPackedDataCollection[AttributeHeaderIndex + 0] = PackedIdAndStride;
				OutPackedDataCollection[AttributeHeaderIndex + 1] = CurrentAttributeAddress;

				const FPCGMetadataAttributeBase* AttributeBase = Metadata->GetConstAttribute(AttributeDesc.Name);

				for (uint32 ElementIndex = 0; ElementIndex < NumElements; ++ElementIndex)
				{
					const uint32 PackedDataElementIndex = AttributeIndex + (ElementIndex * AttributeNumComponents);
					const int64 MetadataKey = ElementIndex;

					if (AttributeBase) // Pack attribute
					{
						ensure(PCGDataForGPUHelpers::PackAttributeHelper(AttributeBase, AttributeDesc, MetadataKey, OutPackedDataCollection, PackedDataElementIndex));
					}
				}

				CurrentAttributeAddress += NumElements * AttributeNumComponents * 4;
			}
		}
		else { /* TODO: Support non-point data. */ }
	}
}

void FPCGDataCollectionDesc::PrepareBufferForKernelOutput(TArray<uint32>& OutPackedDataCollection)
{
	const uint32 NumData = DataDescs.Num();

	TArray<uint32> DataAddresses;
	const uint32 PackedDataCollectionSizeBytes = ComputePackedSize(&DataAddresses);

	OutPackedDataCollection.SetNumZeroed(PackedDataCollectionSizeBytes / sizeof(uint32));
	
	// Num data - set to zero if writing kernel executes. If kernel doesn't execute, 0 means data collection is empty.
	OutPackedDataCollection[0] = 0;

	for (uint32 DataIndex = 0; DataIndex < NumData; ++DataIndex)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGDataCollectionDesc::PackDataItem);

		// Write the data addresses
		const uint32 CurrentDataAddress = DataAddresses[DataIndex];
		const uint32 CurrentDataIndex = CurrentDataAddress / sizeof(uint32);
		OutPackedDataCollection[DataIndex + 1] = CurrentDataAddress;

		// DataHeader: (TypeId, NumAttrs, AttrHeaderStartOffset, TypeInfo), Attr0 Header, Attr1 Header, ..., Attr255 Header
		// Data: Attr0, Attr1, ...
		if (DataDescs[DataIndex].Type == EPCGDataType::Point)
		{
			const uint32 NumElements = DataDescs[DataIndex].ElementCount;

			const TArray<FPCGKernelAttributeDesc>& AttributeDescs = DataDescs[DataIndex].AttributeDescs;
			const uint32 NumAttributes = AttributeDescs.Num();

			OutPackedDataCollection[CurrentDataIndex + 0] = /*PointDataTypeId=*/POINT_DATA_TYPE_ID;
			OutPackedDataCollection[CurrentDataIndex + 1] = NumAttributes;
			OutPackedDataCollection[CurrentDataIndex + 2] = POINT_DATA_HEADER_PREAMBLE_SIZE_BYTES;
			OutPackedDataCollection[CurrentDataIndex + 3] = NumElements; // TypeInfo for PointData is just NumPoints

			const uint32 BaseAttributeHeaderAddress = CurrentDataAddress + POINT_DATA_HEADER_PREAMBLE_SIZE_BYTES;
			uint32 CurrentAttributeAddress = CurrentDataAddress + POINT_DATA_HEADER_SIZE_BYTES;

			for (const FPCGKernelAttributeDesc& AttributeDesc : AttributeDescs)
			{
				const uint32 AttributeId = AttributeDesc.Index;
				const uint32 AttributeStrideBytes = PCGDataForGPUHelpers::GetAttributeTypeStrideBytes(AttributeDesc.Type);
				const uint32 AttributeNumComponents = AttributeStrideBytes / sizeof(uint32); // E.g. float3 has 3 components
				const uint32 AttributeHeaderIndex = (BaseAttributeHeaderAddress + AttributeId * ATTRIBUTE_HEADER_SIZE_BYTES) / sizeof(uint32);

				// Pack Position (24 bits for AttributeId, 8 bits for Stride)
				const uint32 PackedIdAndStride = (AttributeId << 8) + AttributeStrideBytes;
				OutPackedDataCollection[AttributeHeaderIndex + 0] = PackedIdAndStride;

				OutPackedDataCollection[AttributeHeaderIndex + 1] = CurrentAttributeAddress;
				CurrentAttributeAddress += NumElements * AttributeNumComponents * 4;
			}
		}
		if (DataDescs[DataIndex].Type == EPCGDataType::Param)
		{
			const uint32 NumElements = DataDescs[DataIndex].ElementCount;

			const TArray<FPCGKernelAttributeDesc>& AttributeDescs = DataDescs[DataIndex].AttributeDescs;
			const uint32 NumAttributes = AttributeDescs.Num();

			OutPackedDataCollection[CurrentDataIndex + 0] = /*ParamDataTypeId=*/PARAM_DATA_TYPE_ID;
			OutPackedDataCollection[CurrentDataIndex + 1] = NumAttributes;
			OutPackedDataCollection[CurrentDataIndex + 2] = PARAM_DATA_HEADER_PREAMBLE_SIZE_BYTES;
			OutPackedDataCollection[CurrentDataIndex + 3] = NumElements; // TypeInfo for ParamData is # of elems

			const uint32 BaseAttributeHeaderAddress = CurrentDataAddress + PARAM_DATA_HEADER_PREAMBLE_SIZE_BYTES;
			uint32 CurrentAttributeAddress = CurrentDataAddress + PARAM_DATA_HEADER_SIZE_BYTES;

			for (const FPCGKernelAttributeDesc& AttributeDesc : AttributeDescs)
			{
				const uint32 AttributeId = AttributeDesc.Index;
				const uint32 AttributeStrideBytes = PCGDataForGPUHelpers::GetAttributeTypeStrideBytes(AttributeDesc.Type);
				const uint32 AttributeNumComponents = AttributeStrideBytes / sizeof(uint32); // E.g. float3 has 3 components
				const uint32 AttributeHeaderIndex = (BaseAttributeHeaderAddress + AttributeId * ATTRIBUTE_HEADER_SIZE_BYTES) / sizeof(uint32);

				// Pack Position (24 bits for AttributeId, 8 bits for Stride)
				const uint32 PackedIdAndStride = (AttributeId << 8) + AttributeStrideBytes;
				OutPackedDataCollection[AttributeHeaderIndex + 0] = PackedIdAndStride;

				OutPackedDataCollection[AttributeHeaderIndex + 1] = CurrentAttributeAddress;
				CurrentAttributeAddress += NumElements * AttributeNumComponents * 4;
			}
		}
		else { /* TODO: Support non-point data. */ }
	}
}

EPCGUnpackDataCollectionResult FPCGDataCollectionDesc::UnpackDataCollection(const TArray<uint8>& InPackedData, FName InPin, FPCGDataCollection& OutDataCollection) const
{
	const void* PackedData = InPackedData.GetData();
	const float* DataAsFloat = static_cast<const float*>(PackedData);
	const uint32* DataAsUint = static_cast<const uint32*>(PackedData);
	const int* DataAsInt = static_cast<const int*>(PackedData);

	const uint32 NumPackedFloats = InPackedData.Num() / 4;
	const uint32 NumData = DataAsUint[0];

	if (NumData != DataDescs.Num())
	{
		return EPCGUnpackDataCollectionResult::DataMismatch;
	}

	TArray<FPCGTaggedData>& OutData = OutDataCollection.TaggedData;

	for (uint32 DataIndex = 0; DataIndex < NumData; ++DataIndex)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGDataCollectionDesc::UnpackDataItem);

		const uint32 CurrentDataAddress = DataAsUint[DataIndex + 1];
		const uint32 CurrentDataIndex = CurrentDataAddress / sizeof(uint32);
		const uint32 TypeId =                      DataAsUint[CurrentDataIndex + 0];
		const uint32 NumAttributes =               DataAsUint[CurrentDataIndex + 1];
		const uint32 DataHeaderPreambleSizeBytes = DataAsUint[CurrentDataIndex + 2];
		const uint32 NumElements =                 DataAsUint[CurrentDataIndex + 3];

		const TArray<FPCGKernelAttributeDesc>& AttributeDescs = DataDescs[DataIndex].AttributeDescs;
		check(NumAttributes == AttributeDescs.Num());

		if (TypeId == POINT_DATA_TYPE_ID)
		{
			UPCGPointData* OutPointData = nullptr;
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(InitalizeOutput);
				OutPointData = NewObject<UPCGPointData>();
				OutPointData->GetMutablePoints().SetNumUninitialized(NumElements);
			}

			UPCGMetadata* Metadata = OutPointData->MutableMetadata();
			TArray<FPCGPoint>& OutPoints = OutPointData->GetMutablePoints();

			{
				// This can take an age as it populates the memory hierarchy.
				TRACE_CPUPROFILER_EVENT_SCOPE(MetadataEntry);
				ParallelFor(NumElements, [&OutPoints](int32 ElementIndex)
				{
					OutPoints[ElementIndex].MetadataEntry = -1;
				});
			}

			FPCGTaggedData& OutTaggedData = OutData.Emplace_GetRef();
			OutTaggedData.Data = OutPointData;
			OutTaggedData.Pin = InPin;

			const uint32 AttributeHeadersIndex = CurrentDataIndex + POINT_DATA_HEADER_PREAMBLE_SIZE_BYTES / sizeof(uint32);

			// Loop over attributes.
			for (const FPCGKernelAttributeDesc& AttributeDesc : AttributeDescs)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(WriteAttribute);

				const uint32 AttributeId = AttributeDesc.Index;
				const uint32 AttributeNumComponents = PCGDataForGPUHelpers::GetAttributeTypeStrideBytes(AttributeDesc.Type) / sizeof(uint32);
				const uint32 AttributeHeaderIndex = AttributeHeadersIndex + AttributeDesc.Index * ATTRIBUTE_HEADER_SIZE_BYTES / sizeof(uint32);
				const uint32 AttributeIndex = DataAsUint[AttributeHeaderIndex + 1] / sizeof(uint32);

				FPCGMetadataAttributeBase* AttributeBase = (AttributeId >= NUM_RESERVED_ATTRS) ? PCGDataForGPUHelpers::CreateAttributeFromAttributeDesc(Metadata, AttributeDesc) : nullptr;

				// 2. Parse each element in the attribute.

				ParallelFor(NumElements, [&](int32 ElementIndex)
				{
					const uint32 PackedDataElementIndex = AttributeIndex + ElementIndex * AttributeNumComponents;
					check(PackedDataElementIndex + AttributeNumComponents <= NumPackedFloats);

					if (AttributeBase) // Unpack attribute
					{
						Metadata->InitializeOnSet(OutPoints[ElementIndex].MetadataEntry);
						ensure(PCGDataForGPUHelpers::UnpackAttributeHelper(PackedData, PackedDataElementIndex, AttributeBase, AttributeDesc, OutPoints[ElementIndex].MetadataEntry));
					}
					else // Unpack property
					{
						// We tried hoisting this decision to a lambda but it didn't appear to help.
						switch (AttributeId)
						{
						case POINT_POSITION_ATTRIBUTE_ID:
						{
							const FVector Location = FVector
							(
								DataAsFloat[PackedDataElementIndex + 0],
								DataAsFloat[PackedDataElementIndex + 1],
								DataAsFloat[PackedDataElementIndex + 2]
							);

							OutPoints[ElementIndex].Transform.SetLocation(Location);
							break;
						}
						case POINT_ROTATION_ATTRIBUTE_ID:
						{
							FQuat Rotation = FQuat
							(
								DataAsFloat[PackedDataElementIndex + 0],
								DataAsFloat[PackedDataElementIndex + 1],
								DataAsFloat[PackedDataElementIndex + 2],
								DataAsFloat[PackedDataElementIndex + 3]
							);

							// Normalize here with default tolerance (zero quat will return identity).
							OutPoints[ElementIndex].Transform.SetRotation(Rotation.GetNormalized());
							break;
						}
						case POINT_SCALE_ATTRIBUTE_ID:
						{
							const FVector Scale = FVector
							(
								DataAsFloat[PackedDataElementIndex + 0],
								DataAsFloat[PackedDataElementIndex + 1],
								DataAsFloat[PackedDataElementIndex + 2]
							);

							OutPoints[ElementIndex].Transform.SetScale3D(Scale);
							break;
						}
						case POINT_BOUNDS_MIN_ATTRIBUTE_ID:
						{
							const FVector BoundsMin = FVector
							(
								DataAsFloat[PackedDataElementIndex + 0],
								DataAsFloat[PackedDataElementIndex + 1],
								DataAsFloat[PackedDataElementIndex + 2]
							);

							OutPoints[ElementIndex].BoundsMin = BoundsMin;
							break;
						}
						case POINT_BOUNDS_MAX_ATTRIBUTE_ID:
						{
							const FVector BoundsMax = FVector
							(
								DataAsFloat[PackedDataElementIndex + 0],
								DataAsFloat[PackedDataElementIndex + 1],
								DataAsFloat[PackedDataElementIndex + 2]
							);

							OutPoints[ElementIndex].BoundsMax = BoundsMax;
							break;
						}
						case POINT_COLOR_ATTRIBUTE_ID:
						{
							const FVector4 Color = FVector4
							(
								DataAsFloat[PackedDataElementIndex + 0],
								DataAsFloat[PackedDataElementIndex + 1],
								DataAsFloat[PackedDataElementIndex + 2],
								DataAsFloat[PackedDataElementIndex + 2]
							);

							OutPoints[ElementIndex].Color = Color;
							break;
						}
						case POINT_DENSITY_ATTRIBUTE_ID:
						{
							const float Density = DataAsFloat[PackedDataElementIndex];

							OutPoints[ElementIndex].Density = Density;
							break;
						}
						case POINT_SEED_ATTRIBUTE_ID:
						{
							const int Seed = DataAsInt[PackedDataElementIndex];

							OutPoints[ElementIndex].Seed = Seed;
							break;
						}
						case POINT_STEEPNESS_ATTRIBUTE_ID:
						{
							const float Steepness = DataAsFloat[PackedDataElementIndex];

							OutPoints[ElementIndex].Steepness = Steepness;
							break;
						}
						default:
							checkNoEntry();
							break;
						}
					}
				});
			}

			// TODO: It may be more efficient to create a mapping from input point index to final output point index and do everything in one pass.
			auto DiscardInvalidPoints = [&OutPoints](int32 Index, FPCGPoint& OutPoint) -> bool
			{
				if (!FMath::IsFinite(OutPoints[Index].Density))
				{
					return false;
				}

				OutPoint = OutPoints[Index];
				return true;
			};

			FPCGAsync::AsyncPointProcessing(/*Context=*/nullptr, OutPoints.Num(), OutPoints, DiscardInvalidPoints);
		}
		else if (TypeId == PARAM_DATA_TYPE_ID)
		{
			UPCGParamData* OutParamData = NewObject<UPCGParamData>();
			UPCGMetadata* Metadata = OutParamData->MutableMetadata();

			FPCGTaggedData& OutTaggedData = OutData.Emplace_GetRef();
			OutTaggedData.Data = OutParamData;
			OutTaggedData.Pin = InPin;

			const uint32 AttributeHeadersIndex = CurrentDataIndex + PARAM_DATA_HEADER_PREAMBLE_SIZE_BYTES / sizeof(uint32);

			TArray<TTuple</*EntryKey=*/int64, /*ParentEntryKey=*/int64>> AllMetadataEntries;
			AllMetadataEntries.SetNumUninitialized(NumElements);

			ParallelFor(NumElements, [&](int32 ElementIndex)
			{
				AllMetadataEntries[ElementIndex] = MakeTuple(Metadata->AddEntryPlaceholder(), PCGInvalidEntryKey);
			});

			Metadata->AddDelayedEntries(AllMetadataEntries);

			// Loop over attributes.
			for (const FPCGKernelAttributeDesc& AttributeDesc : AttributeDescs)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(WriteAttribute);

				const uint32 AttributeNumComponents = PCGDataForGPUHelpers::GetAttributeTypeStrideBytes(AttributeDesc.Type) / sizeof(uint32);
				const uint32 AttributeHeaderIndex = AttributeHeadersIndex + AttributeDesc.Index * ATTRIBUTE_HEADER_SIZE_BYTES / sizeof(uint32);
				const uint32 AttributeIndex = DataAsUint[AttributeHeaderIndex + 1] / sizeof(uint32);

				FPCGMetadataAttributeBase* AttributeBase = PCGDataForGPUHelpers::CreateAttributeFromAttributeDesc(Metadata, AttributeDesc);

				ParallelFor(NumElements, [&](int32 ElementIndex)
				{
					if (AttributeBase)
					{
						const uint32 PackedDataElementIndex = AttributeIndex + ElementIndex * AttributeNumComponents;
						check(PackedDataElementIndex + AttributeNumComponents <= NumPackedFloats);

						ensure(PCGDataForGPUHelpers::UnpackAttributeHelper(PackedData, PackedDataElementIndex, AttributeBase, AttributeDesc, ElementIndex));
					}
				});
			}
		}
		else { /* TODO: Support non-point data. */ }
	}

	return EPCGUnpackDataCollectionResult::Success;
}

uint32 FPCGDataCollectionDesc::ComputeDataElementCount(EPCGDataType InDataType) const
{
	uint32 ElementCount = 0;

	for (const FPCGDataDesc& DataDesc : DataDescs)
	{
		if (DataDesc.Type == InDataType)
		{
			ElementCount += DataDesc.ElementCount;
		}
	}

	return ElementCount;
}

void FPCGDataCollectionDesc::Combine(const FPCGDataCollectionDesc& Other)
{
	DataDescs.Append(Other.DataDescs);
}
