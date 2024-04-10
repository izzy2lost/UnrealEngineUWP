// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGAddTag.h"

#include "PCGContext.h"
#include "PCGCustomVersion.h"
#include "PCGPin.h"
#include "Helpers/PCGHelpers.h"

#define LOCTEXT_NAMESPACE "PCGAddTagElement"

#if WITH_EDITOR
void UPCGAddTagSettings::ApplyDeprecation(UPCGNode* InOutNode)
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

TArray<FPCGPinProperties> UPCGAddTagSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Any);

	return PinProperties;
}

FPCGElementPtr UPCGAddTagSettings::CreateElement() const
{
	return MakeShared<FPCGAddTagElement>();
}

bool FPCGAddTagElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGAddTagElement::Execute);
	
	check(Context);

	const UPCGAddTagSettings* Settings = Context->GetInputSettings<UPCGAddTagSettings>();
	check(Settings);
	
	Context->OutputData.TaggedData = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);

    PRAGMA_DISABLE_DEPRECATION_WARNINGS
	const TArray<FString> TagsArray = Settings->bTokenizeOnWhiteSpace
		? PCGHelpers::GetStringArrayFromCommaSeparatedString(Settings->TagsToAdd, Context)
		: PCGHelpers::GetStringArrayFromCommaSeparatedList(Settings->TagsToAdd);
    PRAGMA_ENABLE_DEPRECATION_WARNINGS

	for (const FString& Tag : TagsArray)
	{
		for (FPCGTaggedData& OutputTaggedData : Context->OutputData.TaggedData)
		{
				OutputTaggedData.Tags.Add(Tag);
		}
	}
	
	return true;
}

#undef LOCTEXT_NAMESPACE
