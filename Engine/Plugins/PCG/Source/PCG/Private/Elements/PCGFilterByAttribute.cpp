// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGFilterByAttribute.h"

#include "PCGContext.h"
#include "PCGData.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGMetadata.h"

#include "Algo/AnyOf.h"

#define LOCTEXT_NAMESPACE "PCGFilterByAttributeElement"

#if WITH_EDITOR
FText UPCGFilterByAttributeSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Filter Data By Attribute");
}
#endif // WITH_EDITOR

FString UPCGFilterByAttributeSettings::GetAdditionalTitleInformation() const
{
#if WITH_EDITOR
	if (IsPropertyOverriddenByPin(GET_MEMBER_NAME_CHECKED(UPCGFilterByAttributeSettings, Attribute)))
	{
		return FString();
	}
	else
#endif
	{
		return Attribute.ToString();
	}
}

FPCGElementPtr UPCGFilterByAttributeSettings::CreateElement() const
{
	return MakeShared<FPCGFilterByAttributeElement>();
}

bool FPCGFilterByAttributeElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGFilterByAttributeElement::Execute);
	check(Context);

	const UPCGFilterByAttributeSettings* Settings = Context->GetInputSettings<UPCGFilterByAttributeSettings>();
	check(Settings);

	TArray<FString> Attributes = PCGHelpers::GetStringArrayFromCommaSeparatedList(Settings->Attribute.ToString());

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	TArray<FName> DataAttributes;
	TArray<EPCGMetadataTypes> DataAttributeTypes;
	TArray<FString> DataAttributeStrings;

	for (const FPCGTaggedData& Input : Inputs)
	{
		FPCGTaggedData& Output = Outputs.Add_GetRef(Input);
		Output.Pin = PCGPinConstants::DefaultOutFilterLabel;

		if (!Input.Data || !Input.Data->ConstMetadata())
		{
			continue;
		}

		// All attributes from the list must have a match in order to put the data in the In Filter pin.
		const UPCGMetadata* Metadata = Input.Data->ConstMetadata();

		DataAttributes.Reset();
		DataAttributeTypes.Reset();
		Metadata->GetAttributes(DataAttributes, DataAttributeTypes);

		DataAttributeStrings.Reset();
		Algo::Transform(DataAttributes, DataAttributeStrings, [](const FName& InAttribute) { return InAttribute.ToString(); });

		bool bInFilter = true;

		for (const FString& Attribute : Attributes)
		{
			if ((Settings->Operator == EPCGStringMatchingOperator::Equal && DataAttributeStrings.Contains(Attribute)) ||
				(Settings->Operator == EPCGStringMatchingOperator::Substring && Algo::AnyOf(DataAttributeStrings, [&Attribute](const FString& DataAttribute) { return DataAttribute.Contains(Attribute); })) ||
				(Settings->Operator == EPCGStringMatchingOperator::Matches && Algo::AnyOf(DataAttributeStrings, [&Attribute](const FString& DataAttribute) { return DataAttribute.MatchesWildcard(Attribute); })))
			{
				// This attribute has found a match, carry on
			}
			else
			{
				bInFilter = false;
				break;
			}
		}

		if (bInFilter)
		{
			Output.Pin = PCGPinConstants::DefaultInFilterLabel;
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE