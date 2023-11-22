// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVMBlueprintPin.h"

#include "Bindings/MVVMConversionFunctionHelper.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MVVMBlueprintPin)

FMVVMBlueprintPin::FMVVMBlueprintPin(FName InPinName)
	: PinName(InPinName)
{
}

FMVVMBlueprintPin FMVVMBlueprintPin::CreateFromPin(const UBlueprint* Blueprint, const UEdGraphPin* Pin)
{
	FMVVMBlueprintPin Result;
	Result.PinId = Pin->PinId;
	Result.PinName = Pin->PinName;
	Result.Path = UE::MVVM::ConversionFunctionHelper::GetPropertyPathForPin(Blueprint, Pin, true);
	Result.DefaultObject = Pin->DefaultObject;
	Result.DefaultString = Pin->DefaultValue;
	Result.DefaultText = Pin->DefaultTextValue;
	Result.bSplit = Pin->SubPins.Num() > 0;
	Result.Status = Pin->bOrphanedPin ? EMVVMBlueprintPinStatus::Orphaned : EMVVMBlueprintPinStatus::Valid;
	return Result;
}

void FMVVMBlueprintPin::SetDefaultValue(UObject* Value)
{
	Reset();
	DefaultObject = Value;
}

void FMVVMBlueprintPin::SetDefaultValue(const FText& Value)
{
	Reset();
	DefaultText = Value;
}

void FMVVMBlueprintPin::SetDefaultValue(const FString& Value)
{
	Reset();
	DefaultString = Value;
}

void FMVVMBlueprintPin::SetPath(const FMVVMBlueprintPropertyPath& Value)
{
	Reset();
	Path = Value;
}

FString FMVVMBlueprintPin::GetValueAsString(const UClass* SelfContext) const
{
	if (Path.IsValid())
	{
		return Path.GetPropertyPath(SelfContext);
	}
	else if (DefaultObject)
	{
		return DefaultObject.GetPathName();
	}
	else if (!DefaultText.IsEmpty())
	{
		FString TextAsString;
		FTextStringHelper::WriteToBuffer(TextAsString, DefaultText);
		return TextAsString;
	}
	else
	{
		return DefaultString;
	}
}

bool FMVVMBlueprintPin::IsInputPin(const UEdGraphPin* Pin)
{
	return Pin->PinName != UEdGraphSchema_K2::PN_Self && Pin->PinName != UEdGraphSchema_K2::PN_Execute && Pin->Direction == EGPD_Input && (!Pin->bOrphanedPin || Pin->ShouldSavePinIfOrphaned()) && !Pin->bHidden;
}

TArray<FMVVMBlueprintPin> FMVVMBlueprintPin::CopyAndReturnMissingPins(UBlueprint* Blueprint, UEdGraphNode* GraphNode, const TArray<FMVVMBlueprintPin>& Pins)
{
	check(GraphNode);
	check(Blueprint);

	TSet<const UEdGraphPin*> AllGraphPins;
	AllGraphPins.Reserve(GraphNode->Pins.Num());
	for (const FMVVMBlueprintPin& Pin : Pins)
	{
		Pin.CopyTo(Blueprint, GraphNode);
		if (UEdGraphPin* GraphPin = Pin.FindGraphPin(GraphNode))
		{
			AllGraphPins.Add(GraphPin);
		}
	}

	// Create the missing pins.
	TArray<FMVVMBlueprintPin> MissingPins;
	for (const UEdGraphPin* GraphPin : GraphNode->Pins)
	{
		if (IsInputPin(GraphPin) && !AllGraphPins.Contains(GraphPin))
		{
			MissingPins.Add(FMVVMBlueprintPin::CreateFromPin(Blueprint, GraphPin));
		}
	}
	return MissingPins;
}

void FMVVMBlueprintPin::CopyTo(const UBlueprint* Blueprint, UEdGraphNode* Node) const
{
	Status = EMVVMBlueprintPinStatus::Orphaned;
	if (UEdGraphPin* GraphPin = FindGraphPin(Node))
	{
		if (IsInputPin(GraphPin) && !GraphPin->bOrphanedPin)
		{
			Status = EMVVMBlueprintPinStatus::Valid;
			if (bSplit && GraphPin->SubPins.Num() == 0 && GetDefault<UEdGraphSchema_K2>()->CanSplitStructPin(*GraphPin))
			{
				GetDefault<UEdGraphSchema_K2>()->SplitPin(GraphPin, false);
			}

			GraphPin->DefaultObject = DefaultObject;
			GraphPin->DefaultValue = DefaultString;
			GraphPin->DefaultTextValue = DefaultText;
			UE::MVVM::ConversionFunctionHelper::SetPropertyPathForPin(Blueprint, Path, GraphPin);
		}
	}
}

TArray<FMVVMBlueprintPin> FMVVMBlueprintPin::CreateFromNode(UBlueprint* Blueprint, UEdGraphNode* GraphNode)
{
	TArray<FMVVMBlueprintPin> Result;
	Result.Reserve(GraphNode->Pins.Num());
	for (const UEdGraphPin* GraphPin : GraphNode->Pins)
	{
		if (IsInputPin(GraphPin))
		{
			Result.Add(FMVVMBlueprintPin::CreateFromPin(Blueprint, GraphPin));
		}
	}
	return Result;
}

UEdGraphPin* FMVVMBlueprintPin::FindGraphPin(UEdGraphNode* Node) const
{
	if (PinId.IsValid())
	{
		return Node->FindPinById(PinId);
	}
	return Node->FindPin(PinName, EGPD_Input);
}

void FMVVMBlueprintPin::Reset()
{
	PinId.Invalidate();
	Path = FMVVMBlueprintPropertyPath();
	DefaultString.Empty();
	DefaultText = FText();
	DefaultObject = nullptr;
	bSplit = false;
	Status = EMVVMBlueprintPinStatus::Valid;
}
