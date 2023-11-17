// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVMBlueprintViewEvent.h"
#include "MVVMBlueprintView.h"

#include "Bindings/MVVMConversionFunctionHelper.h"
#include "Bindings/MVVMBindingHelper.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "MVVMDeveloperProjectSettings.h"
#include "MVVMWidgetBlueprintExtension_View.h"
#include "WidgetBlueprint.h"

#include "EdGraphSchema_K2.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_VariableSet.h"
#include "KismetCompiler.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MVVMBlueprintViewEvent)

#define LOCTEXT_NAMESPACE "MVVMBlueprintViewEvent"

void UMVVMBlueprintViewEvent::SetEventPath(FMVVMBlueprintPropertyPath InEventPath)
{
	if (InEventPath == EventPath)
	{
		return;
	}

	RemoveWrapperGraph();

	EventPath = MoveTemp(InEventPath);
	GraphName = FName();

	if (EventPath.IsValid())
	{
		TStringBuilder<256> StringBuilder;
		StringBuilder << TEXT("__");
		StringBuilder << FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
		GraphName = StringBuilder.ToString();
	}

	CreateWrapperGraphInternal();
	SavePinValues();
}

void UMVVMBlueprintViewEvent::SetDestinationPath(FMVVMBlueprintPropertyPath InDestinationPath)
{
	if (InDestinationPath == DestinationPath)
	{
		return;
	}

	RemoveWrapperGraph();

	DestinationPath = MoveTemp(InDestinationPath);

	CreateWrapperGraphInternal();
	SavePinValues();
}
	
UEdGraph* UMVVMBlueprintViewEvent::GetOrCreateWrapperGraph()
{
	if (CachedWrapperGraph)
	{
		return CachedWrapperGraph;
	}

	CreateWrapperGraphInternal();
	return CachedWrapperGraph;
}

void UMVVMBlueprintViewEvent::RemoveWrapperGraph()
{
	if (CachedWrapperGraph)
	{
		FBlueprintEditorUtils::RemoveGraph(GetWidgetBlueprintInternal(), CachedWrapperGraph);
		CachedWrapperGraph = nullptr;
		CachedWrapperNode = nullptr;
	}

	Messages.Empty();
	SavedPins.Empty();
}

UEdGraphPin* UMVVMBlueprintViewEvent::GetOrCreateGraphPin(FName PinName)
{
	GetOrCreateWrapperGraph();
	if (CachedWrapperNode)
	{
		return CachedWrapperNode->FindPin(PinName);
	}
	return nullptr;
}

void UMVVMBlueprintViewEvent::SavePinValues()
{
	if (CachedWrapperNode)
	{
		SavedPins.Reset();
		UWidgetBlueprint* Blueprint = GetWidgetBlueprintInternal();
		for (UEdGraphPin* Pin : CachedWrapperNode->Pins)
		{
			if (Pin->PinName != UEdGraphSchema_K2::PN_Self && Pin->PinName != UEdGraphSchema_K2::PN_Execute && Pin->Direction == EGPD_Input)
			{
				SavedPins.Add(FMVVMBlueprintPin::CreateFromPin(Blueprint, Pin));
			}
		}
	}
}

FMVVMBlueprintPropertyPath UMVVMBlueprintViewEvent::GetPinPath(FName PinName) const
{
	const FMVVMBlueprintPin* ViewPin = SavedPins.FindByPredicate([PinName](const FMVVMBlueprintPin& Other) { return PinName == Other.GetName(); });
	return ViewPin ? ViewPin->GetPath() : FMVVMBlueprintPropertyPath();
}

void UMVVMBlueprintViewEvent::SetPinPath(FName PinName, const FMVVMBlueprintPropertyPath& Path)
{
	UEdGraphPin* GraphPin = GetOrCreateGraphPin(PinName);

	if (GraphPin)
	{
		UBlueprint* Blueprint = GetWidgetBlueprintInternal();
		// Set the value and make the blueprint as dirty before creating the pin.
		FMVVMBlueprintPin* ViewPin = SavedPins.FindByPredicate([PinName](const FMVVMBlueprintPin& Other) { return PinName == Other.GetName(); });
		if (!ViewPin)
		{
			ViewPin = &SavedPins.Add_GetRef(FMVVMBlueprintPin::CreateFromPin(Blueprint, GraphPin));
		}

		//A property (viewmodel or widget) may not be created yet and the skeletal needs to be recreated.
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

		UE::MVVM::ConversionFunctionHelper::SetPropertyPathForPin(Blueprint, Path, GraphPin);

		// Take the path built in BP, it may had some errors
		ViewPin->SetPath(UE::MVVM::ConversionFunctionHelper::GetPropertyPathForPin(Blueprint, GraphPin, false));
	}
}

void UMVVMBlueprintViewEvent::SetPinPathNoGraphGeneration(FName PinName, const FMVVMBlueprintPropertyPath& Path)
{
	FMVVMBlueprintPin* ViewPin = SavedPins.FindByPredicate([PinName](const FMVVMBlueprintPin& Other) { return PinName == Other.GetName(); });
	if (!ViewPin)
	{
		ViewPin = &SavedPins.Emplace_GetRef(PinName);
		ViewPin->SetPath(Path);
	}

	//A property (viewmodel or widget) may not be created yet and the skeletal needs to be recreated.
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetWidgetBlueprintInternal());
}

UWidgetBlueprint* UMVVMBlueprintViewEvent::GetWidgetBlueprintInternal() const
{
	return GetOuterUMVVMBlueprintView()->GetOuterUMVVMWidgetBlueprintExtension_View()->GetWidgetBlueprint();
}

bool UMVVMBlueprintViewEvent::Supports(const UWidgetBlueprint* WidgetBlueprint, const FMVVMBlueprintPropertyPath& PropertyPath)
{
	return GetDefault<UMVVMDeveloperProjectSettings>()->bAllowBindingEvent && GetEventSignature(WidgetBlueprint, PropertyPath) != nullptr;
}

const UFunction* UMVVMBlueprintViewEvent::GetEventSignature() const
{
	return GetEventSignature(GetWidgetBlueprintInternal(), EventPath);
}

const UFunction* UMVVMBlueprintViewEvent::GetEventSignature(const UWidgetBlueprint* WidgetBlueprint, const FMVVMBlueprintPropertyPath& PropertyPath)
{
	if (PropertyPath.IsValid() && PropertyPath.GetFieldPaths().Num() > 0)
	{
		const FMVVMBlueprintFieldPath& LastPath = PropertyPath.GetFieldPaths().Last();
		UE::MVVM::FMVVMConstFieldVariant LastField = LastPath.GetField(WidgetBlueprint->SkeletonGeneratedClass);
		if (LastField.IsProperty())
		{
			if (const FMulticastDelegateProperty* Property = CastField<FMulticastDelegateProperty>(LastField.GetProperty()))
			{
				return Property->SignatureFunction.Get();
			}
		}
	}
	return nullptr;
}

UEdGraph* UMVVMBlueprintViewEvent::CreateWrapperGraphInternal()
{
	if (GraphName.IsNone() || !DestinationPath.IsValid() || !EventPath.IsValid())
	{
		return nullptr;
	}

	const UFunction* DelegateSignature = GetEventSignature();
	if (DelegateSignature == nullptr)
	{
		return nullptr;
	}

	if (DestinationPath.GetFieldPaths().Num() > 0)
	{
		const FMVVMBlueprintFieldPath& LastPath = DestinationPath.GetFieldPaths().Last();
		UE::MVVM::FMVVMConstFieldVariant LastField = LastPath.GetField(GetWidgetBlueprintInternal()->SkeletonGeneratedClass);
		if (LastField.IsFunction() && LastField.GetFunction() != nullptr)
		{
			return CreateWrapperGraphInternal(DelegateSignature, LastField.GetFunction());
		}
		else if (LastField.IsProperty() && LastField.GetProperty() != nullptr)
		{
			return CreateWrapperGraphInternal(DelegateSignature, LastField.GetProperty());
		}
	}
	else if (DestinationPath.GetViewModelId().IsValid())
	{
		const FMVVMBlueprintViewModelContext* Context = GetOuterUMVVMBlueprintView()->FindViewModel(DestinationPath.GetViewModelId());
		const FProperty* ViewModel = Context ? GetWidgetBlueprintInternal()->SkeletonGeneratedClass->FindPropertyByName(Context->GetViewModelName()) : nullptr;
		if (ViewModel)
		{
			return CreateWrapperGraphInternal(DelegateSignature, ViewModel);
		}
	}
	return nullptr;
}

UEdGraph* UMVVMBlueprintViewEvent::CreateWrapperGraphInternal(const UFunction* DelegateSignature, const UFunction* Function)
{
	UWidgetBlueprint* WidgetBlueprint = GetWidgetBlueprintInternal();
	bool bIsConst = false;
	bool bTransient = true;
	TPair<UEdGraph*, UK2Node*> Result = UE::MVVM::ConversionFunctionHelper::CreateGraph(WidgetBlueprint, GraphName, DelegateSignature, Function, bIsConst, bTransient);
	CachedWrapperGraph = Result.Get<0>();
	CachedWrapperNode = Result.Get<1>();

	// Add SelfNode if needed
	if (CachedWrapperNode)
	{
		if (UEdGraphPin* SelfPin = CachedWrapperNode->FindPin(UEdGraphSchema_K2::PSC_Self))
		{
			FMVVMBlueprintPropertyPath PathToSet = DestinationPath;
			TArray<UE::MVVM::FMVVMConstFieldVariant> FieldPaths = PathToSet.GetFields(WidgetBlueprint->SkeletonGeneratedClass);
			PathToSet.ResetPropertyPath();
			for (int32 Index = 0; Index < FieldPaths.Num() - 1; ++Index)
			{
				PathToSet.AppendPropertyPath(WidgetBlueprint, FieldPaths[Index]);
			}
			;
			UE::MVVM::ConversionFunctionHelper::SetPropertyPathForPin(WidgetBlueprint, PathToSet, SelfPin);
		}
	}

	LoadPinValuesInternal();
	return CachedWrapperGraph;
}

UEdGraph* UMVVMBlueprintViewEvent::CreateWrapperGraphInternal(const UFunction* DelegateSignature, const FProperty* Property)
{
	UWidgetBlueprint* WidgetBlueprint = GetWidgetBlueprintInternal();
	bool bConst = false;
	bool bTransient = true;
	TPair<UEdGraph*, UK2Node*> Result = UE::MVVM::ConversionFunctionHelper::CreateGraph(WidgetBlueprint, GraphName, DelegateSignature, UK2Node_VariableSet::StaticClass(), bConst, bTransient,
		[WidgetBlueprint, Property](UK2Node* NewNode)
		{
			UK2Node_VariableSet* VariableNode = CastChecked<UK2Node_VariableSet>(NewNode);
			UEdGraphSchema_K2::ConfigureVarNode(VariableNode, Property->GetFName(), Property->GetOwnerStruct(), WidgetBlueprint);
		});
	CachedWrapperGraph = Result.Get<0>();
	CachedWrapperNode = Result.Get<1>();

	// Add SelfNode
	if (CachedWrapperNode)
	{
		FMVVMBlueprintPropertyPath PathToSet = DestinationPath;
		TArray<UE::MVVM::FMVVMConstFieldVariant> FieldPaths = PathToSet.GetFields(WidgetBlueprint->SkeletonGeneratedClass);
		PathToSet.ResetPropertyPath();
		for (int32 Index = 0; Index < FieldPaths.Num() - 1; ++Index)
		{
			PathToSet.AppendPropertyPath(WidgetBlueprint, FieldPaths[Index]);
		}
		UEdGraphPin* SelfPin = CachedWrapperNode->FindPinChecked(UEdGraphSchema_K2::PSC_Self);
		UE::MVVM::ConversionFunctionHelper::SetPropertyPathForPin(WidgetBlueprint, PathToSet, SelfPin);
	}

	LoadPinValuesInternal();
	return CachedWrapperGraph;
}

void UMVVMBlueprintViewEvent::LoadPinValuesInternal()
{
	if (CachedWrapperNode)
	{
		TArray<UEdGraphPin*> AllPins;
		AllPins.Reserve(CachedWrapperNode->Pins.Num());
		for (UEdGraphPin* Pin : CachedWrapperNode->Pins)
		{
			if (Pin->PinName != UEdGraphSchema_K2::PN_Self && Pin->PinName != UEdGraphSchema_K2::PN_Execute && Pin->Direction == EGPD_Input)
			{
				AllPins.Add(Pin);
			}
		}

		UBlueprint* Blueprint = GetWidgetBlueprintInternal();
		for (int32 Index = SavedPins.Num() - 1; Index >= 0; --Index)
		{
			const FMVVMBlueprintPin& Pin = SavedPins[Index];
			if (UEdGraphPin* GraphPin = CachedWrapperNode->FindPin(Pin.GetName()))
			{
				AllPins.RemoveSingleSwap(GraphPin);
				Pin.CopyTo(Blueprint, GraphPin);
			}
			else
			{
				// pin doesn't exist anymore
				SavedPins.RemoveAt(Index);
			}
		}

		// Create the reminding pin.
		for (UEdGraphPin* Pin : AllPins)
		{
			SavedPins.Add(FMVVMBlueprintPin::CreateFromPin(Blueprint, Pin));
		}
	}
}

TArray<FText> UMVVMBlueprintViewEvent::GetCompilationMessages(EMessageType InMessageType) const
{
	TArray<FText> Result;
	Result.Reset(Messages.Num());
	for (const FMessage& Msg : Messages)
	{
		if (Msg.MessageType == InMessageType)
		{
			Result.Add(Msg.MessageText);
		}
	}
	return Result;
}

bool UMVVMBlueprintViewEvent::HasCompilationMessage(EMessageType InMessageType) const
{
	return Messages.ContainsByPredicate([InMessageType](const FMessage& Other)
	{
		return Other.MessageType == InMessageType;
	});
}

void UMVVMBlueprintViewEvent::AddCompilationToBinding(FMessage MessageToAdd)
{
	Messages.Add(MoveTemp(MessageToAdd));
}

void UMVVMBlueprintViewEvent::ResetCompilationMessages()
{
	Messages.Reset();
}

FText UMVVMBlueprintViewEvent::GetDisplayName(bool bUseDisplayName) const
{
	TArray<FText> JoinArgs;
	for (const FMVVMBlueprintPin& Pin : GetPins())
	{
		if (Pin.UsedPathAsValue())
		{
			JoinArgs.Add(Pin.GetPath().ToText(GetWidgetBlueprintInternal(), bUseDisplayName));
		}
	}

	return FText::Format(LOCTEXT("BlueprintViewEventDisplayNameFormat", "{0} => {1}({2})")
		, EventPath.ToText(GetWidgetBlueprintInternal(), bUseDisplayName)
		, DestinationPath.ToText(GetWidgetBlueprintInternal(), bUseDisplayName)
		, FText::Join(LOCTEXT("PathDelimiter", ", "), JoinArgs)
		);
}

FString UMVVMBlueprintViewEvent::GetSearchableString() const
{
	TStringBuilder<256> Builder;
	Builder << EventPath.ToString(GetWidgetBlueprintInternal(), true, true);
	Builder << TEXT(' ');
	Builder << DestinationPath.ToString(GetWidgetBlueprintInternal(), true, true);
	Builder << TEXT('(');
	bool bFirst = true;
	for (const FMVVMBlueprintPin& Pin : GetPins())
	{
		if (!bFirst)
		{
			Builder << TEXT(", ");
		}
		if (Pin.UsedPathAsValue())
		{
			Builder << Pin.GetPath().ToString(GetWidgetBlueprintInternal(), true, true);
		}
		bFirst = false;
	}
	Builder << TEXT(')');
	return Builder.ToString();
}

void UMVVMBlueprintViewEvent::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChainEvent)
{
	Super::PostEditChangeChainProperty(PropertyChainEvent);
	GetOuterUMVVMBlueprintView()->OnEventsUpdated.Broadcast();
}

#undef LOCTEXT_NAMESPACE
