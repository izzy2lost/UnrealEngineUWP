// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGFilterByAttribute.h"

#include "PCGContext.h"
#include "PCGData.h"
#include "Metadata/PCGMetadata.h"

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

	const FName Attribute = FName(Settings->Attribute);

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	for (const FPCGTaggedData& Input : Inputs)
	{
		FPCGTaggedData& Output = Outputs.Add_GetRef(Input);
		Output.Pin = PCGPinConstants::DefaultOutFilterLabel;
		
		const UPCGMetadata* Metadata = Input.Data ? Input.Data->ConstMetadata() : nullptr;
		if (Metadata && Metadata->HasAttribute(Attribute))
		{
			Output.Pin = PCGPinConstants::DefaultInFilterLabel;
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE