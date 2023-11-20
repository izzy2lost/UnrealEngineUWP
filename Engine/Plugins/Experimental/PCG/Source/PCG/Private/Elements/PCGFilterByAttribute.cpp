// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGFilterByAttribute.h"

#include "PCGContext.h"
#include "PCGData.h"
#include "Metadata/PCGMetadata.h"

#define LOCTEXT_NAMESPACE "PCGFilterByAttributeElement"

namespace PCGFilterByAttributeConstants
{
	const FText NodeTitle = LOCTEXT("NodeTitle", "Filter Data By Attribute");
}

#if WITH_EDITOR
FText UPCGFilterByAttributeSettings::GetDefaultNodeTitle() const
{
	return PCGFilterByAttributeConstants::NodeTitle;
}
#endif // WITH_EDITOR

FName UPCGFilterByAttributeSettings::AdditionalTaskName() const
{
#if WITH_EDITOR
	FProperty* AttributeProperty = GetClass() ? FindFProperty<FProperty>(GetClass(), GET_MEMBER_NAME_CHECKED(UPCGFilterByAttributeSettings, Attribute)) : nullptr;
	if (AttributeProperty && IsPropertyOverriddenByPin(AttributeProperty))
	{
		return NAME_None;
	}
	else
#endif
	{
		return FName(FString::Printf(TEXT("%s (%s)"), *PCGFilterByAttributeConstants::NodeTitle.ToString(), *Attribute.ToString()));
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