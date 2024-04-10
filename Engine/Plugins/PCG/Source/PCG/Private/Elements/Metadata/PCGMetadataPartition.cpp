// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Metadata/PCGMetadataPartition.h"

#include "PCGContext.h"
#include "PCGCustomVersion.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadataPartitionCommon.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGMetadataPartition)

#define LOCTEXT_NAMESPACE "PCGMetadataPartitionElement"

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
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (PartitionAttribute_DEPRECATED != NAME_None)
	{
		PartitionAttributeSelectors.Empty();
		PartitionAttributeSelectors.Emplace_GetRef().SetAttributeName(PartitionAttribute_DEPRECATED);
		PartitionAttribute_DEPRECATED = NAME_None;
	}

	if (PartitionAttributeSource_DEPRECATED != FPCGAttributePropertyInputSelector())
	{
		PartitionAttributeSelectors.Empty();
		PartitionAttributeSelectors.Emplace(PartitionAttributeSource_DEPRECATED);
		PartitionAttributeSource_DEPRECATED = FPCGAttributePropertyInputSelector();
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif // WITH_EDITOR
}

#if WITH_EDITOR
void UPCGMetadataPartitionSettings::ApplyDeprecation(UPCGNode* InOutNode)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (DataVersion < FPCGCustomVersion::AttributesAndTagsCanContainSpaces)
	{
		bTokenizeOnWhiteSpace = true;
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	Super::ApplyDeprecation(InOutNode);
}
#endif // WITH_EDITOR

FString UPCGMetadataPartitionSettings::GetAdditionalTitleInformation() const
{
#if WITH_EDITOR
	if (IsPropertyOverriddenByPin(GET_MEMBER_NAME_CHECKED(UPCGMetadataPartitionSettings, PartitionAttributeNames)))
	{
		return FString();
	}
	else
#endif
	{
		if (PartitionAttributeSelectors.IsEmpty())
		{
			return FString();
		}

		FString OutString = PartitionAttributeSelectors[0].GetDisplayText().ToString();
		for (int I = 1; I < PartitionAttributeSelectors.Num(); ++I)
		{
			OutString += ", " + PartitionAttributeSelectors[I].GetDisplayText().ToString();
		}

		return OutString;
	}
}

bool FPCGMetadataPartitionElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGMetadataPartitionElement::Execute);
	check(Context);

	const UPCGMetadataPartitionSettings* Settings = Context->GetInputSettings<UPCGMetadataPartitionSettings>();
	check(Settings);

	// TODO: This is a temporary solution for overrides until arrays are supported
	TArray<FPCGAttributePropertyInputSelector> OverriddenSelectors;

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	const TArray<FString> AttributeNames = Settings->bTokenizeOnWhiteSpace
		? PCGHelpers::GetStringArrayFromCommaSeparatedString(Settings->PartitionAttributeNames, Context)
		: PCGHelpers::GetStringArrayFromCommaSeparatedList(Settings->PartitionAttributeNames);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// If the names are overridden by the user, generate the selectors with them
	OverriddenSelectors.SetNum(AttributeNames.Num());
	for (const FString& AttributeName : AttributeNames)
	{
		OverriddenSelectors.Emplace_GetRef().SetAttributeName(FName(AttributeName));
	}

	const TArray<FPCGAttributePropertyInputSelector>& ActiveSelectors = OverriddenSelectors.IsEmpty() ? Settings->PartitionAttributeSelectors : OverriddenSelectors;

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	TArray<FPCGAttributePropertySelector> PartitionAttributeSources;
	PartitionAttributeSources.SetNum(ActiveSelectors.Num());

	if (PartitionAttributeSources.IsEmpty())
	{
		Outputs = Inputs;
		return true;
	}

	for (const FPCGTaggedData& Input : Inputs)
	{
		const UPCGData* InData = Input.Data;

		for (int I = 0; I < PartitionAttributeSources.Num(); ++I)
		{
			PartitionAttributeSources[I] = static_cast<FPCGAttributePropertySelector>(ActiveSelectors[I].CopyAndFixLast(InData));
		}

		TArray<UPCGData*> PartitionDataArray;
		PartitionDataArray = PCGMetadataPartitionCommon::AttributePartition(InData, PartitionAttributeSources, Context);

		for (UPCGData* PartitionData : PartitionDataArray)
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
