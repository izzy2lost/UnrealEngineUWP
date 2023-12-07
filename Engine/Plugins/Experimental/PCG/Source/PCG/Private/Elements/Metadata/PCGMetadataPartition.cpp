// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Metadata/PCGMetadataPartition.h"

#include "PCGContext.h"
#include "Metadata/PCGMetadataPartitionCommon.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGMetadataPartition)

#define LOCTEXT_NAMESPACE "PCGMetadataPartitionElement"

TArray<FPCGPinProperties> UPCGMetadataPartitionSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultInputLabel, EPCGDataType::Any);

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGMetadataPartitionSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Any);

	return PinProperties;
}

FPCGElementPtr UPCGMetadataPartitionSettings::CreateElement() const
{
	return MakeShared<FPCGMetadataPartitionElement>();
}

void UPCGMetadataPartitionSettings::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	if (PartitionAttribute_DEPRECATED != NAME_None)
	{
		PartitionAttributeSource.SetAttributeName(PartitionAttribute_DEPRECATED);
		PartitionAttribute_DEPRECATED = NAME_None;
	}
#endif // WITH_EDITOR
}

FString UPCGMetadataPartitionSettings::GetAdditionalTitleInformation() const
{
#if WITH_EDITOR
	if (IsPropertyOverriddenByPin(GET_MEMBER_NAME_CHECKED(UPCGMetadataPartitionSettings, PartitionAttributeSource)))
	{
		return FString();
	}
	else
#endif
	{
		return PartitionAttributeSource.GetDisplayText().ToString();
	}
}

bool FPCGMetadataPartitionElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGMetadataPartitionElement::Execute);
	check(Context);

	const UPCGMetadataPartitionSettings* Settings = Context->GetInputSettings<UPCGMetadataPartitionSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	for (const FPCGTaggedData& Input : Inputs)
	{
		const UPCGData* InData = Input.Data;
		const FPCGAttributePropertyInputSelector PartitionAttributeSelector = Settings->PartitionAttributeSource.CopyAndFixLast(InData);

		for (UPCGData* PartitionData : PCGMetadataPartitionCommon::AttributePartition(InData, PartitionAttributeSelector, Context))
		{
			if (PartitionData)
			{
				FPCGTaggedData& Output = Outputs.Add_GetRef(Input);
				Output.Data = PartitionData;
			}
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
