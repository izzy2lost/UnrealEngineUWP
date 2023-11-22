// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVMBlueprintViewEvent.h"
#include "MVVMBlueprintView.h"

#include "Bindings/MVVMConversionFunctionHelper.h"
#include "Bindings/MVVMBindingHelper.h"
#include "Bindings/MVVMFieldPathHelper.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "MVVMDeveloperProjectSettings.h"
#include "MVVMWidgetBlueprintExtension_View.h"
#include "WidgetBlueprint.h"

#include "EdGraphSchema_K2.h"
#include "EdGraph/EdGraphPin.h"
#include "GraphEditAction.h"
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

void UMVVMBlueprintViewEvent::SetCachedWrapperGraphInternal(UEdGraph* Graph, UK2Node* Node)
{
	if (CachedWrapperNode && OnUserDefinedPinRenamedHandle.IsValid())
	{
		CachedWrapperNode->OnUserDefinedPinRenamed().Remove(OnUserDefinedPinRenamedHandle);
	}
	if (CachedWrapperGraph && OnGraphChangedHandle.IsValid())
	{
		CachedWrapperGraph->RemoveOnGraphChangedHandler(OnGraphChangedHandle);
	}

	CachedWrapperGraph = Graph;
	CachedWrapperNode = Node;
	OnGraphChangedHandle.Reset();
	OnUserDefinedPinRenamedHandle.Reset();

	if (CachedWrapperGraph)
	{
		OnGraphChangedHandle = CachedWrapperGraph->AddOnGraphChangedHandler(FOnGraphChanged::FDelegate::CreateUObject(this, &UMVVMBlueprintViewEvent::HandleGraphChanged));
	}
	if (CachedWrapperNode)
	{
		OnUserDefinedPinRenamedHandle = CachedWrapperNode->OnUserDefinedPinRenamed().AddUObject(this, &UMVVMBlueprintViewEvent::HandleUserDefinedPinRenamed);
	}
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
		SetCachedWrapperGraphInternal(nullptr, nullptr);
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
		UWidgetBlueprint* Blueprint = GetWidgetBlueprintInternal();
		SavedPins = FMVVMBlueprintPin::CreateFromNode(Blueprint, CachedWrapperNode);
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

	UWidgetBlueprint* WidgetBlueprint = GetWidgetBlueprintInternal();
	bool bIsConst = false;
	bool bTransient = true;
	TValueOrError<UE::MVVM::ConversionFunctionHelper::FCreateSetterGraphResult, FText> CreateSetterGraphResult = UE::MVVM::ConversionFunctionHelper::CreateSetterGraph(WidgetBlueprint, GraphName, DelegateSignature, DestinationPath, bIsConst, bTransient);
	if (CreateSetterGraphResult.HasError())
	{
		SetCachedWrapperGraphInternal(nullptr, nullptr);
		return nullptr;
	}
	else
	{
		SetCachedWrapperGraphInternal(CreateSetterGraphResult.GetValue().NewGraph, CreateSetterGraphResult.GetValue().WrappedNode);
		LoadPinValuesInternal();
	}

	return CachedWrapperGraph;
}

void UMVVMBlueprintViewEvent::LoadPinValuesInternal()
{
	if (CachedWrapperNode)
	{
		TArray<FMVVMBlueprintPin> MissingPins = FMVVMBlueprintPin::CopyAndReturnMissingPins(GetWidgetBlueprintInternal(), CachedWrapperNode, SavedPins);
		SavedPins.Append(MissingPins);
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

void UMVVMBlueprintViewEvent::AddCompilationToBinding(FMessage MessageToAdd) const
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

void UMVVMBlueprintViewEvent::HandleGraphChanged(const FEdGraphEditAction& EditAction)
{
	if (EditAction.Graph == CachedWrapperGraph && CachedWrapperGraph)
	{
		if (CachedWrapperNode && EditAction.Nodes.Contains(CachedWrapperNode))
		{
			if (EditAction.Action == EEdGraphActionType::GRAPHACTION_RemoveNode)
			{
				CachedWrapperNode = UE::MVVM::ConversionFunctionHelper::GetWrapperNode(CachedWrapperGraph);
				SavePinValues();
				OnWrapperGraphModified.Broadcast();
			}
			else if (EditAction.Action == EEdGraphActionType::GRAPHACTION_EditNode)
			{
				SavePinValues();
				OnWrapperGraphModified.Broadcast();
			}
		}
		else if (CachedWrapperNode == nullptr && EditAction.Action == EEdGraphActionType::GRAPHACTION_AddNode)
		{
			CachedWrapperNode = UE::MVVM::ConversionFunctionHelper::GetWrapperNode(CachedWrapperGraph);
			SavePinValues();
			OnWrapperGraphModified.Broadcast();
		}
	}
}

void UMVVMBlueprintViewEvent::HandleUserDefinedPinRenamed(UK2Node* InNode, FName OldPinName, FName NewPinName)
{
	if (InNode == CachedWrapperNode)
	{
		SavePinValues();
		OnWrapperGraphModified.Broadcast();
	}
}

void UMVVMBlueprintViewEvent::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChainEvent)
{
	Super::PostEditChangeChainProperty(PropertyChainEvent);
	GetOuterUMVVMBlueprintView()->OnEventsUpdated.Broadcast();
}

#undef LOCTEXT_NAMESPACE
