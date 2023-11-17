// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVMEditorSubsystem.h"

#include "Bindings/MVVMBindingHelper.h"
#include "Bindings/MVVMConversionFunctionHelper.h"
#include "Blueprint/WidgetTree.h"
#include "BlueprintActionDatabase.h"
#include "BlueprintActionFilter.h"
#include "BlueprintNodeSpawner.h"
#include "Engine/Engine.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_VariableGet.h"
#include "MVVMBlueprintInstancedViewModel.h"
#include "MVVMBlueprintView.h"
#include "MVVMBlueprintViewConversionFunction.h"
#include "MVVMBlueprintViewEvent.h"
#include "MVVMDeveloperProjectSettings.h"
#include "MVVMSubsystem.h"
#include "MVVMWidgetBlueprintExtension_View.h"
#include "ScopedTransaction.h"
#include "Types/MVVMAvailableBinding.h"
#include "Types/MVVMBindingSource.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MVVMEditorSubsystem)

#define LOCTEXT_NAMESPACE "MVVMEditorSubsystem"

namespace UE::MVVM::Private
{
	void OnBindingPreEditChange(UMVVMBlueprintView* BlueprintView, FName PropertyName)
	{
		FProperty* ChangedProperty = FMVVMBlueprintViewBinding::StaticStruct()->FindPropertyByName(PropertyName);
		check(ChangedProperty != nullptr);

		FEditPropertyChain EditChain;
		EditChain.AddTail(UMVVMBlueprintView::StaticClass()->FindPropertyByName("Bindings"));
		EditChain.AddTail(ChangedProperty);
		EditChain.SetActivePropertyNode(ChangedProperty);

		BlueprintView->PreEditChange(EditChain);
	}

	void OnBindingPostEditChange(UMVVMBlueprintView* BlueprintView, FName PropertyName)
	{
		FProperty* ChangedProperty = FMVVMBlueprintViewBinding::StaticStruct()->FindPropertyByName(PropertyName);
		check(ChangedProperty != nullptr);

		FEditPropertyChain EditChain;
		EditChain.AddTail(UMVVMBlueprintView::StaticClass()->FindPropertyByName("Bindings"));
		EditChain.AddTail(ChangedProperty);
		EditChain.SetActivePropertyNode(ChangedProperty);

		FPropertyChangedEvent ChangeEvent(ChangedProperty, EPropertyChangeType::ValueSet);
		FPropertyChangedChainEvent ChainEvent(EditChain, ChangeEvent);
		BlueprintView->PostEditChangeChainProperty(ChainEvent);
	}
	
	void OnEventPreEditChange(UMVVMBlueprintViewEvent* Event, FName PropertyName)
	{
		FProperty* ChangedProperty = UMVVMBlueprintViewEvent::StaticClass()->FindPropertyByName(PropertyName);
		check(ChangedProperty != nullptr);

		FEditPropertyChain EditChain;
		EditChain.AddTail(ChangedProperty);
		EditChain.SetActivePropertyNode(ChangedProperty);

		Event->PreEditChange(EditChain);
	}

	void OnEventPostEditChange(UMVVMBlueprintViewEvent* Event, FName PropertyName)
	{
		FProperty* ChangedProperty = UMVVMBlueprintViewEvent::StaticClass()->FindPropertyByName(PropertyName);
		check(ChangedProperty != nullptr);

		FEditPropertyChain EditChain;
		EditChain.AddTail(ChangedProperty);
		EditChain.SetActivePropertyNode(ChangedProperty);

		FPropertyChangedEvent ChangeEvent(ChangedProperty, EPropertyChangeType::ValueSet);
		FPropertyChangedChainEvent ChainEvent(EditChain, ChangeEvent);
		Event->PostEditChangeChainProperty(ChainEvent);
	}

	UK2Node_FunctionResult* FindFunctionResult(UEdGraph* Graph)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_FunctionResult* FunctionResult = Cast<UK2Node_FunctionResult>(Node))
			{
				return FunctionResult;
			}
		}
		return nullptr;
	}

	UEdGraphNode* FindConversionNode(UEdGraph* Graph)
	{
		UK2Node_FunctionResult* FunctionResult = UE::MVVM::Private::FindFunctionResult(Graph);
		if (!ensureMsgf(FunctionResult != nullptr, TEXT("Function result node not found in conversion function wrapper!")))
		{
			return nullptr;
		}

		if (!ensureMsgf(FunctionResult->UserDefinedPins.Num() == 1, TEXT("Function result should have exactly one return value.")))
		{
			return nullptr;
		}

		UEdGraphPin* ResultPin = FunctionResult->FindPin(FunctionResult->UserDefinedPins[0]->PinName, EGPD_Input);
		if (!ensureMsgf(ResultPin != nullptr, TEXT("Function result pin not found.")))
		{
			return nullptr;
		}

		if (!ensureMsgf(ResultPin->LinkedTo.Num() != 0, TEXT("Result pin not linked to anything")))
		{
			return nullptr;
		}

		// finally found our conversion node
		UEdGraphNode* ConversionNode = ResultPin->LinkedTo[0]->GetOwningNode();
		return ConversionNode;
	}

} //namespace Private


UMVVMBlueprintView* UMVVMEditorSubsystem::RequestView(UWidgetBlueprint* WidgetBlueprint) const
{
	UMVVMWidgetBlueprintExtension_View* Extension = UMVVMWidgetBlueprintExtension_View::RequestExtension<UMVVMWidgetBlueprintExtension_View>(WidgetBlueprint);
	if (Extension->GetBlueprintView() == nullptr)
	{
		Extension->CreateBlueprintViewInstance();
	}
	return Extension->GetBlueprintView();
}

UMVVMBlueprintView* UMVVMEditorSubsystem::GetView(const UWidgetBlueprint* WidgetBlueprint) const
{
	if (WidgetBlueprint)
	{
		if (UMVVMWidgetBlueprintExtension_View* ExtensionView = UMVVMWidgetBlueprintExtension_View::GetExtension<UMVVMWidgetBlueprintExtension_View>(WidgetBlueprint))
		{
			return ExtensionView->GetBlueprintView();
		}
	}
	return nullptr;
}

FGuid UMVVMEditorSubsystem::AddViewModel(UWidgetBlueprint* WidgetBlueprint, const UClass* ViewModelClass)
{
	FGuid Result;
	if (ViewModelClass && ViewModelClass->ImplementsInterface(UNotifyFieldValueChanged::StaticClass()))
	{
		if (UMVVMBlueprintView* View = GetView(WidgetBlueprint))
		{
			FString ClassName = ViewModelClass->ClassGeneratedBy != nullptr ? ViewModelClass->ClassGeneratedBy->GetName() : ViewModelClass->GetAuthoredName();
			if (Cast<UBlueprintGeneratedClass>(ViewModelClass) && ClassName.EndsWith(TEXT("_C")))
			{
				ClassName.RemoveAt(ClassName.Len() - 2, 2);
			}
			FString ViewModelName = ClassName;
			FKismetNameValidator NameValidator(WidgetBlueprint);

			int32 Index = 1;
			while (NameValidator.IsValid(ViewModelName) != EValidatorResult::Ok)
			{
				ViewModelName = ClassName + "_";
				ViewModelName.AppendInt(Index);

				++Index;
			}

			FMVVMBlueprintViewModelContext Context = FMVVMBlueprintViewModelContext(ViewModelClass, *ViewModelName);
			if (Context.IsValid())
			{
				const FScopedTransaction Transaction(LOCTEXT("AddViewModel", "Add viewmodel"));
				View->Modify();
				View->AddViewModel(Context);
				Result = Context.GetViewModelId();
			}
		}
	}
	return Result;
}

FGuid UMVVMEditorSubsystem::AddInstancedViewModel(UWidgetBlueprint* WidgetBlueprint)
{
	UMVVMWidgetBlueprintExtension_View* ExtensionView = UMVVMWidgetBlueprintExtension_View::GetExtension<UMVVMWidgetBlueprintExtension_View>(WidgetBlueprint);
	UMVVMBlueprintView* View = ExtensionView ? ExtensionView->GetBlueprintView() : nullptr;

	if (!View)
	{
		return FGuid();
	}

	const FScopedTransaction Transaction(LOCTEXT("AddInstancedViewModel", "Add instanced viewmodel"));

	FGuid Result;

	FName UniqueName = MakeUniqueObjectName(View, UMVVMBlueprintInstancedViewModel_PropertyBag::StaticClass(), "InstancedViewmodel");
	UMVVMBlueprintInstancedViewModel_PropertyBag* NewInstancedViewModel = NewObject<UMVVMBlueprintInstancedViewModel_PropertyBag>(View, UniqueName);
	NewInstancedViewModel->GenerateClass(true);
	FMVVMBlueprintViewModelContext Context = FMVVMBlueprintViewModelContext(NewInstancedViewModel->GetGeneratedClass(), UniqueName);
	if (Context.IsValid())
	{
		Context.InstancedViewModel = NewInstancedViewModel;
		Context.CreationType = EMVVMBlueprintViewModelContextCreationType::CreateInstance;
		View->Modify();
		View->AddViewModel(Context);
		Result = Context.GetViewModelId();
	}
	else
	{
		auto RenameToTransient = [](UObject* ObjectToRename)
		{
			FName TrashName = MakeUniqueObjectName(GetTransientPackage(), ObjectToRename->GetClass(), *FString::Printf(TEXT("TRASH_%s"), *ObjectToRename->GetName()));
			ObjectToRename->Rename(*TrashName.ToString(), GetTransientPackage());
		};
		if (NewInstancedViewModel->GetGeneratedClass())
		{
			RenameToTransient(NewInstancedViewModel->GetGeneratedClass());
		}
		RenameToTransient(NewInstancedViewModel);
	}

	return Result;
}

void UMVVMEditorSubsystem::RemoveViewModel(UWidgetBlueprint* WidgetBlueprint, FName ViewModel)
{
	if (UMVVMBlueprintView* View = GetView(WidgetBlueprint))
	{
		if (const FMVVMBlueprintViewModelContext* ViewModelContext = View->FindViewModel(ViewModel))
		{
			if (ViewModelContext->bCanRemove)
			{
				View->RemoveViewModel(ViewModelContext->GetViewModelId());
			}
		}
	}
}

bool UMVVMEditorSubsystem::VerifyViewModelRename(UWidgetBlueprint* WidgetBlueprint, FName ViewModel, FName NewViewModel, FText& OutError)
{
	FKismetNameValidator Validator(WidgetBlueprint);
	EValidatorResult ValidatorResult = Validator.IsValid(NewViewModel);
	if (ValidatorResult != EValidatorResult::Ok)
	{
		if (ViewModel == NewViewModel && (ValidatorResult == EValidatorResult::AlreadyInUse || ValidatorResult == EValidatorResult::ExistingName))
		{
			// Continue successfully
		}
		else
		{
			OutError = INameValidatorInterface::GetErrorText(NewViewModel.ToString(), ValidatorResult);
			return false;
		}
	}
	return true;
}

bool UMVVMEditorSubsystem::RenameViewModel(UWidgetBlueprint* WidgetBlueprint, FName ViewModel, FName NewViewModel, FText& OutError)
{
	if (!VerifyViewModelRename(WidgetBlueprint, ViewModel, NewViewModel, OutError))
	{
		return false;
	}

	UMVVMBlueprintView* View = GetView(WidgetBlueprint);
	if (View == nullptr)
	{
		return false;
	}

	const FMVVMBlueprintViewModelContext* ViewModelContext = View->FindViewModel(ViewModel);
	if (ViewModelContext && ViewModelContext->bCanRename)
	{
		const FScopedTransaction Transaction(LOCTEXT("RenameViewModel", "Rename Viewmodel"));
		View->Modify();
		return View->RenameViewModel(ViewModel, NewViewModel);
	}
	return false;
}

bool UMVVMEditorSubsystem::ReparentViewModel(UWidgetBlueprint* WidgetBlueprint, FName ViewModel, const UClass* ViewModelClass, FText& OutError)
{
	if (UMVVMBlueprintView* View = GetView(WidgetBlueprint))
	{
		if (const FMVVMBlueprintViewModelContext* ViewModelContext = View->FindViewModel(ViewModel))
		{
			FScopedTransaction Transaction(LOCTEXT("ReparentViewmodel", "Reparent Viewmodel"));
			return View->ReparentViewModel(ViewModelContext->GetViewModelId(), ViewModelClass);
		}
	}
	return false;
}

FMVVMBlueprintViewBinding& UMVVMEditorSubsystem::AddBinding(UWidgetBlueprint* WidgetBlueprint)
{
	UMVVMBlueprintView* View = RequestView(WidgetBlueprint);
	return View->AddDefaultBinding();
}

void UMVVMEditorSubsystem::RemoveBinding(UWidgetBlueprint* WidgetBlueprint, const FMVVMBlueprintViewBinding& Binding)
{
	if (UMVVMBlueprintView* View = GetView(WidgetBlueprint))
	{
		View->RemoveBinding(&Binding);
	}
}

UMVVMBlueprintViewEvent* UMVVMEditorSubsystem::AddEvent(UWidgetBlueprint* WidgetBlueprint)
{
	if (GetDefault<UMVVMDeveloperProjectSettings>()->bAllowBindingEvent)
	{
		UMVVMBlueprintView* View = RequestView(WidgetBlueprint);
		return View->AddDefaultEvent();
	}
	return nullptr;
}

void UMVVMEditorSubsystem::RemoveEvent(UWidgetBlueprint* WidgetBlueprint, UMVVMBlueprintViewEvent* Event)
{
	if (UMVVMBlueprintView* View = GetView(WidgetBlueprint))
	{
		View->RemoveEvent(Event);
	}
}

UFunction* UMVVMEditorSubsystem::GetConversionFunction(const UWidgetBlueprint* WidgetBlueprint, const FMVVMBlueprintViewBinding& Binding, bool bSourceToDestination) const
{
	if (UMVVMBlueprintViewConversionFunction* ConversionFunction = Binding.Conversion.GetConversionFunction(bSourceToDestination))
	{
		TVariant<const UFunction*, TSubclassOf<UK2Node>> Result = ConversionFunction->GetConversionFunction(WidgetBlueprint->SkeletonGeneratedClass);
		if (Result.IsType<const UFunction*>())
		{
			return const_cast<UFunction*>(Result.Get<const UFunction*>());
		}
	}
	return nullptr;
}

UEdGraphPin* UMVVMEditorSubsystem::GetConversionFunctionArgumentPin(const UWidgetBlueprint* WidgetBlueprint, const FMVVMBlueprintViewBinding& Binding, FName ParameterName, bool bSourceToDestination) const
{
	if (UMVVMBlueprintViewConversionFunction* ConversionFunction = Binding.Conversion.GetConversionFunction(bSourceToDestination))
	{
		return ConversionFunction->GetOrCreateGraphPin(const_cast<UWidgetBlueprint*>(WidgetBlueprint), ParameterName);
	}
	return nullptr;
}

void UMVVMEditorSubsystem::SetSourceToDestinationConversionFunction(UWidgetBlueprint* WidgetBlueprint, FMVVMBlueprintViewBinding& Binding, const UFunction* ConversionFunction)
{
	if (UMVVMBlueprintView* View = GetView(WidgetBlueprint))
	{
		if (ConversionFunction == nullptr || IsValidConversionFunction(WidgetBlueprint, ConversionFunction, Binding.SourcePath, Binding.DestinationPath))
		{
			FScopedTransaction Transaction(LOCTEXT("SetConversionFunction", "Set Conversion Function"));

			WidgetBlueprint->Modify();

			UE::MVVM::Private::OnBindingPreEditChange(View, GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewBinding, Conversion));

			if (Binding.Conversion.SourceToDestinationConversion)
			{
				Binding.Conversion.SourceToDestinationConversion->RemoveWrapperGraph(WidgetBlueprint);
				Binding.Conversion.SourceToDestinationConversion = nullptr;
			}

			if (ConversionFunction != nullptr)
			{
				Binding.Conversion.SourceToDestinationConversion = NewObject<UMVVMBlueprintViewConversionFunction>(WidgetBlueprint);
				FName GraphName = UE::MVVM::ConversionFunctionHelper::CreateWrapperName(Binding, true);
				Binding.Conversion.SourceToDestinationConversion->InitFromFunction(WidgetBlueprint, ConversionFunction, GraphName);
			}

			UE::MVVM::Private::OnBindingPostEditChange(View, GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewBinding, Conversion));
			FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);
		}
	}
}

void UMVVMEditorSubsystem::SetDestinationToSourceConversionFunction(UWidgetBlueprint* WidgetBlueprint, FMVVMBlueprintViewBinding& Binding, const UFunction* ConversionFunction)
{
	if (UMVVMBlueprintView* View = GetView(WidgetBlueprint))
	{
		if (ConversionFunction == nullptr || IsValidConversionFunction(WidgetBlueprint, ConversionFunction, Binding.DestinationPath, Binding.SourcePath))
		{
			FScopedTransaction Transaction(LOCTEXT("SetConversionFunction", "Set Conversion Function"));

			WidgetBlueprint->Modify();

			UE::MVVM::Private::OnBindingPreEditChange(View, GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewBinding, Conversion));

			if (Binding.Conversion.DestinationToSourceConversion)
			{
				Binding.Conversion.DestinationToSourceConversion->RemoveWrapperGraph(WidgetBlueprint);
				Binding.Conversion.DestinationToSourceConversion = nullptr;
			}

			if (ConversionFunction != nullptr)
			{
				Binding.Conversion.DestinationToSourceConversion = NewObject<UMVVMBlueprintViewConversionFunction>(WidgetBlueprint);
				FName GraphName = UE::MVVM::ConversionFunctionHelper::CreateWrapperName(Binding, false);
				Binding.Conversion.DestinationToSourceConversion->InitFromFunction(WidgetBlueprint, ConversionFunction, GraphName);
			}

			UE::MVVM::Private::OnBindingPostEditChange(View, GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewBinding, Conversion));
			FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);
		}
	}
}

void UMVVMEditorSubsystem::SetDestinationPathForBinding(UWidgetBlueprint* WidgetBlueprint, FMVVMBlueprintViewBinding& Binding, FMVVMBlueprintPropertyPath PropertyPath)
{
	if (UMVVMBlueprintView* View = GetView(WidgetBlueprint))
	{
		FScopedTransaction Transaction(LOCTEXT("SetBindingProperty", "Set Binding Property"));

		UE::MVVM::Private::OnBindingPreEditChange(View, GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewBinding, DestinationPath));

		bool bSupports = UMVVMBlueprintViewEvent::Supports(WidgetBlueprint, PropertyPath);
		UMVVMBlueprintViewEvent* Event = nullptr;
		if (bSupports)
		{
			Event = AddEvent(WidgetBlueprint);
		}

		if (Event)
		{
			Event->SetEventPath(PropertyPath);
			View->RemoveBinding(&Binding);
		}
		else
		{
			Binding.DestinationPath = PropertyPath;
		}

		UE::MVVM::Private::OnBindingPostEditChange(View, GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewBinding, DestinationPath));
		FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);
	}
}

void UMVVMEditorSubsystem::SetSourcePathForBinding(UWidgetBlueprint* WidgetBlueprint, FMVVMBlueprintViewBinding& Binding, FMVVMBlueprintPropertyPath Field)
{
	if (UMVVMBlueprintView* View = GetView(WidgetBlueprint))
	{
		FScopedTransaction Transaction(LOCTEXT("SetBindingProperty", "Set Binding Property"));

		UE::MVVM::Private::OnBindingPreEditChange(View, GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewBinding, SourcePath));

		Binding.SourcePath = Field;

		UE::MVVM::Private::OnBindingPostEditChange(View, GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewBinding, SourcePath));
		FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);
	}
}

void UMVVMEditorSubsystem::OverrideExecutionModeForBinding(UWidgetBlueprint* WidgetBlueprint, FMVVMBlueprintViewBinding& Binding, EMVVMExecutionMode Mode)
{
	if (!Binding.bOverrideExecutionMode || Binding.OverrideExecutionMode != Mode)
	{
		if (UMVVMBlueprintView* View = GetView(WidgetBlueprint))
		{
			FScopedTransaction Transaction(LOCTEXT("SetExecutionMode", "Set Execution Mode"));

			UE::MVVM::Private::OnBindingPreEditChange(View, GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewBinding, OverrideExecutionMode));

			Binding.bOverrideExecutionMode = true;
			Binding.OverrideExecutionMode = Mode;

			UE::MVVM::Private::OnBindingPostEditChange(View, GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewBinding, OverrideExecutionMode));
			FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);
		}
	}
}

void UMVVMEditorSubsystem::ResetExecutionModeForBinding(UWidgetBlueprint* WidgetBlueprint, FMVVMBlueprintViewBinding& Binding)
{
	if (Binding.bOverrideExecutionMode)
	{
		if (UMVVMBlueprintView* View = GetView(WidgetBlueprint))
		{
			FScopedTransaction Transaction(LOCTEXT("ResetExecutionMode", "Reset Execution Mode"));

			UE::MVVM::Private::OnBindingPreEditChange(View, GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewBinding, OverrideExecutionMode));

			Binding.bOverrideExecutionMode = false;

			UE::MVVM::Private::OnBindingPostEditChange(View, GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewBinding, OverrideExecutionMode));
			FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);
		}
	}
}

void UMVVMEditorSubsystem::SetBindingTypeForBinding(UWidgetBlueprint* WidgetBlueprint, FMVVMBlueprintViewBinding& Binding, EMVVMBindingMode Type)
{
	if (Binding.BindingType != Type)
	{
		if (UMVVMBlueprintView* View = GetView(WidgetBlueprint))
		{
			FScopedTransaction Transaction(LOCTEXT("SetBindingType", "Set Binding Type"));

			UE::MVVM::Private::OnBindingPreEditChange(View, GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewBinding, BindingType));

			Binding.BindingType = Type;

			UE::MVVM::Private::OnBindingPostEditChange(View, GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewBinding, BindingType));
			FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);
		}
	}
}

void UMVVMEditorSubsystem::SetEnabledForBinding(UWidgetBlueprint* WidgetBlueprint, FMVVMBlueprintViewBinding& Binding, bool bEnabled)
{
	if (Binding.bEnabled != bEnabled)
	{
		if (UMVVMBlueprintView* View = GetView(WidgetBlueprint))
		{
			FScopedTransaction Transaction(LOCTEXT("SetBindingEnabled", "Set Binding Enabled"));

			UE::MVVM::Private::OnBindingPreEditChange(View, GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewBinding, bEnabled));

			Binding.bEnabled = bEnabled;

			UE::MVVM::Private::OnBindingPostEditChange(View, GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewBinding, bEnabled));
			FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);
		}
	}
}

void UMVVMEditorSubsystem::SetCompileForBinding(UWidgetBlueprint* WidgetBlueprint, FMVVMBlueprintViewBinding& Binding, bool bCompile)
{
	if (Binding.bCompile != bCompile)
	{
		if (UMVVMBlueprintView* View = GetView(WidgetBlueprint))
		{
			FScopedTransaction Transaction(LOCTEXT("SetBindingCompiled", "Set Binding Compiled"));

			UE::MVVM::Private::OnBindingPreEditChange(View, GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewBinding, bCompile));

			Binding.bCompile = bCompile;

			UE::MVVM::Private::OnBindingPostEditChange(View, GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewBinding, bCompile));
			FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);
		}
	}
}

void UMVVMEditorSubsystem::SetEventPath(UMVVMBlueprintViewEvent* Event, FMVVMBlueprintPropertyPath PropertyPath)
{
	UMVVMBlueprintView* View = Event ? Event->GetOuterUMVVMBlueprintView() : nullptr;
	if (View)
	{
		UWidgetBlueprint* WidgetBlueprint = Event->GetOuterUMVVMBlueprintView()->GetOuterUMVVMWidgetBlueprintExtension_View()->GetWidgetBlueprint();

		FScopedTransaction Transaction(LOCTEXT("SetEventPath", "Set Event Path"));

		FName EventPath = "EventPath";
		UE::MVVM::Private::OnEventPreEditChange(Event, EventPath);

		bool bSupports = UMVVMBlueprintViewEvent::Supports(WidgetBlueprint, PropertyPath);

		if (bSupports)
		{
			Event->SetEventPath(PropertyPath);
		}
		else
		{
			FMVVMBlueprintViewBinding& Binding = AddBinding(WidgetBlueprint);
			SetDestinationPathForBinding(WidgetBlueprint, Binding, PropertyPath);
			View->RemoveEvent(Event);
		}

		UE::MVVM::Private::OnEventPostEditChange(Event, EventPath);
	}
}

void UMVVMEditorSubsystem::SetEventDestinationPath(UMVVMBlueprintViewEvent* Event, FMVVMBlueprintPropertyPath PropertyPath)
{
	const UMVVMBlueprintView* View = Event ? Event->GetOuterUMVVMBlueprintView() : nullptr;
	if (View)
	{
		FScopedTransaction Transaction(LOCTEXT("SetEventDestinationPath", "Set Destination Path"));

		FName DestinationPath = "DestinationPath";
		UE::MVVM::Private::OnEventPreEditChange(Event, DestinationPath);

		Event->SetDestinationPath(PropertyPath);

		UE::MVVM::Private::OnEventPostEditChange(Event, DestinationPath);
	}
}

void UMVVMEditorSubsystem::SetEventArgumentPath(UMVVMBlueprintViewEvent* Event, FName ArgumentName, const FMVVMBlueprintPropertyPath& Path) const
{
	const UMVVMBlueprintView* View = Event ? Event->GetOuterUMVVMBlueprintView() : nullptr;
	if (View)
	{
		FScopedTransaction Transaction(LOCTEXT("SetEventPath", "Set Event Path"));

		UE::MVVM::Private::OnEventPreEditChange(Event, "SavedPins");
	
		Event->SetPinPath(ArgumentName, Path);

		UE::MVVM::Private::OnEventPostEditChange(Event, "SavedPins");
	}
}

void UMVVMEditorSubsystem::SetEnabledForEvent(UMVVMBlueprintViewEvent* Event, bool bEnabled)
{
	if (Event->bEnabled != bEnabled)
	{
		const UMVVMBlueprintView* View = Event ? Event->GetOuterUMVVMBlueprintView() : nullptr;
		if (View)
		{
			FScopedTransaction Transaction(LOCTEXT("SetBindingEnabled", "Set Binding Enabled"));

			UE::MVVM::Private::OnEventPreEditChange(Event, GET_MEMBER_NAME_CHECKED(UMVVMBlueprintViewEvent, bEnabled));

			Event->bEnabled = bEnabled;

			UE::MVVM::Private::OnEventPostEditChange(Event, GET_MEMBER_NAME_CHECKED(UMVVMBlueprintViewEvent, bEnabled));
		}
	}
}

void UMVVMEditorSubsystem::SetCompileForEvent(UMVVMBlueprintViewEvent* Event, bool bCompile)
{
	if (Event->bCompile != bCompile)
	{
		const UMVVMBlueprintView* View = Event ? Event->GetOuterUMVVMBlueprintView() : nullptr;
		if (View)
		{
			FScopedTransaction Transaction(LOCTEXT("SetBindingCompiled", "Set Binding Compiled"));

			UE::MVVM::Private::OnEventPreEditChange(Event, GET_MEMBER_NAME_CHECKED(UMVVMBlueprintViewEvent, bCompile));

			Event->bCompile = bCompile;

			UE::MVVM::Private::OnEventPostEditChange(Event, GET_MEMBER_NAME_CHECKED(UMVVMBlueprintViewEvent, bCompile));
		}
	}
}

bool UMVVMEditorSubsystem::IsValidConversionFunction(const UWidgetBlueprint* WidgetBlueprint, const UFunction* Function, const FMVVMBlueprintPropertyPath& Source, const FMVVMBlueprintPropertyPath& Destination) const
{
	TValueOrError<const FProperty*, FText> ReturnResult = UE::MVVM::BindingHelper::TryGetReturnTypeForConversionFunction(Function);
	if (ReturnResult.HasError())
	{
		return false;
	}

	const FProperty* ReturnProperty = ReturnResult.GetValue();
	if (ReturnProperty == nullptr)
	{
		return false;
	}

	TValueOrError<TArray<const FProperty*>, FText> ArgumentsResult = UE::MVVM::BindingHelper::TryGetArgumentsForConversionFunction(Function);
	if (ArgumentsResult.HasError())
	{
		return false;
	}

	const FProperty* SourceProperty = nullptr;

	TArray<UE::MVVM::FMVVMConstFieldVariant> SourceFields = Source.GetFields(WidgetBlueprint->SkeletonGeneratedClass);
	if (SourceFields.Num() > 0)
	{
		SourceProperty = SourceFields.Last().IsProperty() ? SourceFields.Last().GetProperty() : UE::MVVM::BindingHelper::GetReturnProperty(SourceFields.Last().GetFunction());
	}

	const FProperty* DestinationProperty = nullptr;

	TArray<UE::MVVM::FMVVMConstFieldVariant> DestFields = Destination.GetFields(WidgetBlueprint->SkeletonGeneratedClass);
	if (DestFields.Num() > 0)
	{
		DestinationProperty = DestFields.Last().IsProperty() ? DestFields.Last().GetProperty() : UE::MVVM::BindingHelper::GetFirstArgumentProperty(DestFields.Last().GetFunction());
	}

	// check that at least one source -> argument binding is compatible
	bool bAnyCompatible = false;

	const TArray<const FProperty*>& ConversionArgProperties = ArgumentsResult.GetValue();
	for (const FProperty* ArgumentProperty : ConversionArgProperties)
	{

		if (SourceProperty == nullptr || UE::MVVM::BindingHelper::ArePropertiesCompatible(SourceProperty, ArgumentProperty))
		{
			bAnyCompatible = true;
		}
	}

	if (!bAnyCompatible)
	{
		return false;
	}

	// check that the return -> dest is valid
	if (DestinationProperty != nullptr &&
		!UE::MVVM::BindingHelper::ArePropertiesCompatible(ReturnProperty, DestinationProperty))
	{
		return false;
	}

	return GetDefault<UMVVMDeveloperProjectSettings>()->IsConversionFunctionAllowed(WidgetBlueprint, Function);
}

bool UMVVMEditorSubsystem::IsSimpleConversionFunction(const UFunction* Function) const
{
	TValueOrError<const FProperty*, FText> ReturnResult = UE::MVVM::BindingHelper::TryGetReturnTypeForConversionFunction(Function);
	if (ReturnResult.HasError())
	{
		return false;
	}

	const FProperty* ReturnProperty = ReturnResult.GetValue();
	if (ReturnProperty == nullptr)
	{
		return false;
	}

	TValueOrError<TArray<const FProperty*>, FText> ArgumentsResult = UE::MVVM::BindingHelper::TryGetArgumentsForConversionFunction(Function);
	if (ArgumentsResult.HasError())
	{
		return false;
	}

	return ArgumentsResult.GetValue().Num() == 1;
}

UEdGraph* UMVVMEditorSubsystem::GetConversionFunctionGraph(const UWidgetBlueprint* WidgetBlueprint, const FMVVMBlueprintViewBinding& Binding, bool bSourceToDestination) const
{
	if (const UMVVMBlueprintViewConversionFunction* Found = Binding.Conversion.GetConversionFunction(bSourceToDestination))
	{
		return Found->GetWrapperGraph();
	}
	return nullptr;
}

UK2Node_CallFunction* UMVVMEditorSubsystem::GetConversionFunctionNode(const UWidgetBlueprint* WidgetBlueprint, const FMVVMBlueprintViewBinding& Binding, bool bSourceToDestination) const
{
	if (const UMVVMBlueprintViewConversionFunction* Found = Binding.Conversion.GetConversionFunction(bSourceToDestination))
	{
		return Cast<UK2Node_CallFunction>(Found->GetWrapperNode());
	}
	return nullptr;
}

TArray<UFunction*> UMVVMEditorSubsystem::GetAvailableConversionFunctions(const UWidgetBlueprint* WidgetBlueprint, const FMVVMBlueprintPropertyPath& Source, const FMVVMBlueprintPropertyPath& Destination) const
{
	TArray<UFunction*> ConversionFunctions;

	auto AddFunction = [this, &Source, &Destination, WidgetBlueprint, &ConversionFunctions](const UFunction* Function)
	{
		// functions in the widget blueprint can do anything they want, other functions have to be static functions in a BlueprintFunctionLibrary
		const UClass* FunctionClass = Function->GetOuterUClass();

		bool bIsFromWidgetBlueprint = WidgetBlueprint->GeneratedClass && WidgetBlueprint->GeneratedClass->IsChildOf(FunctionClass) && Function->HasAllFunctionFlags(FUNC_BlueprintPure);
		bool bIsFromSkeletonWidgetBlueprint = WidgetBlueprint->SkeletonGeneratedClass && WidgetBlueprint->SkeletonGeneratedClass->IsChildOf(FunctionClass) && Function->HasAllFunctionFlags(FUNC_BlueprintPure);
		bool bFromBlueprintFunctionLibrary = FunctionClass->IsChildOf<UBlueprintFunctionLibrary>() && Function->HasAllFunctionFlags(FUNC_Static | FUNC_BlueprintPure);
		if (bIsFromWidgetBlueprint || bIsFromSkeletonWidgetBlueprint || bFromBlueprintFunctionLibrary)
		{
			if (IsValidConversionFunction(WidgetBlueprint, Function, Source, Destination))
			{
				ConversionFunctions.Add(const_cast<UFunction*>(Function));
			}
		}
	};

	EMVVMDeveloperConversionFunctionFilterType FilterType = GetDefault<UMVVMDeveloperProjectSettings>()->GetConversionFunctionFilter();
	if (FilterType == EMVVMDeveloperConversionFunctionFilterType::BlueprintActionRegistry)
	{
		const FBlueprintActionDatabase::FActionRegistry& ActionRegistry = FBlueprintActionDatabase::Get().GetAllActions();
		for (auto It = ActionRegistry.CreateConstIterator(); It; ++It)
		{
			if (UObject* ActionObject = It->Key.ResolveObjectPtr())
			{
				for (const UBlueprintNodeSpawner* NodeSpawner : It->Value)
				{
					FBlueprintActionInfo BlueprintAction(ActionObject, NodeSpawner);
					const UFunction* Function = BlueprintAction.GetAssociatedFunction();
					if (Function != nullptr)
					{
						AddFunction(Function);
					}
				}
			}
		}
	}
	else if (FilterType == EMVVMDeveloperConversionFunctionFilterType::AllowedList)
	{
		auto IsInheritedBlueprintFunction = [](const UFunction* Function)
		{
			bool bIsBpInheritedFunc = false;
			if (UClass* FuncClass = Function->GetOwnerClass())
			{
				if (UBlueprint* BpOwner = Cast<UBlueprint>(FuncClass->ClassGeneratedBy))
				{
					FName FuncName = Function->GetFName();
					if (UClass* ParentClass = BpOwner->ParentClass)
					{
						bIsBpInheritedFunc = (ParentClass->FindFunctionByName(FuncName, EIncludeSuperFlag::IncludeSuper) != nullptr);
					}
				}
			}
			return bIsBpInheritedFunc;
		};

		TArray<const UClass*> Classes = GetDefault<UMVVMDeveloperProjectSettings>()->GetAllowedConversionFunctionClasses();
		Classes.Add(WidgetBlueprint->GeneratedClass);
		for (const UClass* Class : Classes)
		{
			for (TFieldIterator<UFunction> FunctionIt(Class, EFieldIteratorFlags::ExcludeSuper); FunctionIt; ++FunctionIt)
			{
				UFunction* Function = *FunctionIt;
				if (IsInheritedBlueprintFunction(Function))
				{
					continue;
				}
				if (!Function->HasAllFunctionFlags(FUNC_BlueprintCallable))
				{
					continue;
				}

				AddFunction(Function);
			}
		}
	}

	ConversionFunctions.Sort([](const UFunction& A, const UFunction& B) -> bool
		{
			return A.GetFName().LexicalLess(B.GetFName());
		});
	return ConversionFunctions;
}

FMVVMBlueprintPropertyPath UMVVMEditorSubsystem::GetPathForConversionFunctionArgument(const UWidgetBlueprint* WidgetBlueprint, const FMVVMBlueprintViewBinding& Binding, FName ArgumentName, bool bSourceToDestination) const
{
	UMVVMBlueprintViewConversionFunction* ConversionFunction = Binding.Conversion.GetConversionFunction(bSourceToDestination);
	if (ConversionFunction == nullptr || !ConversionFunction->NeedsWrapperGraph())
	{
		// simple conversion function
		if (bSourceToDestination)
		{
			return Binding.SourcePath;
		}
		else
		{
			return Binding.DestinationPath;
		}
	}

	UEdGraphPin* GraphPin = ConversionFunction->GetOrCreateGraphPin(const_cast<UWidgetBlueprint*>(WidgetBlueprint), ArgumentName);
	if (GraphPin == nullptr)
	{
		return FMVVMBlueprintPropertyPath();
	}

	return UE::MVVM::ConversionFunctionHelper::GetPropertyPathForPin(WidgetBlueprint, GraphPin, false);
}

void UMVVMEditorSubsystem::SetPathForConversionFunctionArgument(UWidgetBlueprint* WidgetBlueprint, FMVVMBlueprintViewBinding& Binding, FName ArgumentName, const FMVVMBlueprintPropertyPath& Path, bool bSourceToDestination) const
{
	UMVVMBlueprintViewConversionFunction* ConversionFunction = Binding.Conversion.GetConversionFunction(bSourceToDestination);
	if (ConversionFunction == nullptr || !ConversionFunction->NeedsWrapperGraph())
	{
		// simple conversion function
		if (bSourceToDestination)
		{
			Binding.SourcePath = Path;
		}
		else
		{
			Binding.DestinationPath = Path;
		}
	}
	else
	{
		check(ConversionFunction);
		ConversionFunction->SetGraphPin(WidgetBlueprint, ArgumentName, Path);
	}
	FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);
}

namespace Private
{
	UEdGraphPin* GetGraphPin(const UMVVMEditorSubsystem* Subsystem, const UWidgetBlueprint* WidgetBlueprint, const FMVVMBlueprintViewBinding& ViewBinding, FName PinName, bool bSourceToDestination)
	{
		if (WidgetBlueprint == nullptr)
		{
			return nullptr;
		}
		return Subsystem->GetConversionFunctionArgumentPin(WidgetBlueprint, ViewBinding, PinName, bSourceToDestination);
	}

	UEdGraphPin* GetGraphPin(const UWidgetBlueprint* WidgetBlueprint, UMVVMBlueprintViewEvent* ViewEvent, FName PinName)
	{
		if (WidgetBlueprint == nullptr || ViewEvent == nullptr)
		{
			return nullptr;
		}

		return ViewEvent->GetOrCreateGraphPin(PinName);
	}
}

void UMVVMEditorSubsystem::SplitPin(UWidgetBlueprint* WidgetBlueprint, FMVVMBlueprintViewBinding& Binding, FName PinName, bool bSourceToDestination) const
{
	UEdGraphPin* GraphPin = Private::GetGraphPin(this, WidgetBlueprint, Binding, PinName, bSourceToDestination);
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	if (GraphPin == nullptr || !K2Schema->CanSplitStructPin(*GraphPin))
	{
		return;
	}

	UMVVMBlueprintView* View = GetView(WidgetBlueprint);
	UMVVMBlueprintViewConversionFunction* ConversionFunction = Binding.Conversion.GetConversionFunction(bSourceToDestination);

	FScopedTransaction Transaction(LOCTEXT("BreakPin", "Split Struct Pin"));
	UE::MVVM::Private::OnBindingPreEditChange(View, GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewBinding, Conversion));
	K2Schema->SplitPin(GraphPin);
	ConversionFunction->SavePinValues(WidgetBlueprint);
	UE::MVVM::Private::OnBindingPostEditChange(View, GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewBinding, Conversion));
}

bool UMVVMEditorSubsystem::CanSplitPin(const UWidgetBlueprint* WidgetBlueprint, const FMVVMBlueprintViewBinding& Binding, FName PinName, bool bSourceToDestination) const
{
	UEdGraphPin* GraphPin = Private::GetGraphPin(this, WidgetBlueprint, Binding, PinName, bSourceToDestination);
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	return GraphPin ? K2Schema->CanSplitStructPin(*GraphPin) : false;
}

void UMVVMEditorSubsystem::SplitPin(UWidgetBlueprint* WidgetBlueprint, UMVVMBlueprintViewEvent* ViewEvent, FName PinName) const
{
	UEdGraphPin* GraphPin = Private::GetGraphPin(WidgetBlueprint, ViewEvent, PinName);
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	if (GraphPin == nullptr || !K2Schema->CanSplitStructPin(*GraphPin))
	{
		return;
	}

	FName NAME_SavedPin = "SavedPins";
	FScopedTransaction Transaction(LOCTEXT("BreakPin", "Split Struct Pin"));
	UE::MVVM::Private::OnEventPreEditChange(ViewEvent, NAME_SavedPin);
	K2Schema->SplitPin(GraphPin);
	ViewEvent->SavePinValues();
	UE::MVVM::Private::OnEventPostEditChange(ViewEvent, NAME_SavedPin);
}

bool UMVVMEditorSubsystem::CanSplitPin(const UWidgetBlueprint* WidgetBlueprint, UMVVMBlueprintViewEvent* Event, FName PinName) const
{
	UEdGraphPin* GraphPin = Private::GetGraphPin(WidgetBlueprint, Event, PinName);
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	return GraphPin ? K2Schema->CanSplitStructPin(*GraphPin) : false;
}

void UMVVMEditorSubsystem::RecombinePin(UWidgetBlueprint* WidgetBlueprint, FMVVMBlueprintViewBinding& Binding, FName PinName, bool bSourceToDestination) const
{
	UEdGraphPin* GraphPin = Private::GetGraphPin(this, WidgetBlueprint, Binding, PinName, bSourceToDestination);
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	if (GraphPin == nullptr || !K2Schema->CanRecombineStructPin(*GraphPin))
	{
		return;
	}

	UMVVMBlueprintView* View = GetView(WidgetBlueprint);
	UMVVMBlueprintViewConversionFunction* ConversionFunction = Binding.Conversion.GetConversionFunction(bSourceToDestination);

	FScopedTransaction Transaction(LOCTEXT("RecombinePin", "Recombine Struct Pin"));
	UE::MVVM::Private::OnBindingPreEditChange(View, GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewBinding, Conversion));
	K2Schema->RecombinePin(GraphPin);
	ConversionFunction->SavePinValues(WidgetBlueprint);
	UE::MVVM::Private::OnBindingPostEditChange(View, GET_MEMBER_NAME_CHECKED(FMVVMBlueprintViewBinding, Conversion));
}

bool UMVVMEditorSubsystem::CanRecombinePin(const UWidgetBlueprint* WidgetBlueprint, const FMVVMBlueprintViewBinding& Binding, FName PinName, bool bSourceToDestination) const
{
	UEdGraphPin* GraphPin = Private::GetGraphPin(this, WidgetBlueprint, Binding, PinName, bSourceToDestination);
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	return GraphPin ? K2Schema->CanRecombineStructPin(*GraphPin) : false;
}

void UMVVMEditorSubsystem::RecombinePin(UWidgetBlueprint* WidgetBlueprint, UMVVMBlueprintViewEvent* ViewEvent, FName PinName) const
{
	UEdGraphPin* GraphPin = Private::GetGraphPin(WidgetBlueprint, ViewEvent, PinName);
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	if (GraphPin == nullptr || !K2Schema->CanRecombineStructPin(*GraphPin))
	{
		return;
	}

	FName NAME_SavedPin = "SavedPins";
	FScopedTransaction Transaction(LOCTEXT("RecombinePin", "Recombine Struct Pin"));
	UE::MVVM::Private::OnEventPreEditChange(ViewEvent, NAME_SavedPin);
	K2Schema->RecombinePin(GraphPin);
	ViewEvent->SavePinValues();
	UE::MVVM::Private::OnEventPostEditChange(ViewEvent, NAME_SavedPin);
}

bool UMVVMEditorSubsystem::CanRecombinePin(const UWidgetBlueprint* WidgetBlueprint, UMVVMBlueprintViewEvent* Event, FName PinName) const
{
	UEdGraphPin* GraphPin = Private::GetGraphPin(WidgetBlueprint, Event, PinName);
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	return GraphPin ? K2Schema->CanRecombineStructPin(*GraphPin) : false;
}

TArray<UE::MVVM::FBindingSource> UMVVMEditorSubsystem::GetBindableWidgets(const UWidgetBlueprint* WidgetBlueprint) const
{
	TArray<UE::MVVM::FBindingSource> Sources;

	const UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	if (WidgetTree == nullptr)
	{
		return Sources;
	}

	TArray<UWidget*> AllWidgets;
	WidgetTree->GetAllWidgets(AllWidgets);

	Sources.Reserve(AllWidgets.Num() + 1);

	// Add current widget as a possible binding source
	if (UClass* BPClass = WidgetBlueprint->GeneratedClass)
	{
		TArray<FMVVMAvailableBinding> Bindings = UMVVMSubsystem::GetAvailableBindings(BPClass, WidgetBlueprint->GeneratedClass);
		if (Bindings.Num() > 0)
		{
			// at least one valid property, add it to our list
			Sources.Add(UE::MVVM::FBindingSource::CreateForBlueprint(WidgetBlueprint));
		}
	}

	for (const UWidget* Widget : AllWidgets)
	{
		TArray<FMVVMAvailableBinding> Bindings = UMVVMSubsystem::GetAvailableBindings(Widget->GetClass(), WidgetBlueprint->GeneratedClass);
		if (Bindings.Num() > 0)
		{
			// at least one valid property, add it to our list
			Sources.Add(UE::MVVM::FBindingSource::CreateForWidget(WidgetBlueprint, Widget));
		}
	}

	return Sources;
}

TArray<UE::MVVM::FBindingSource> UMVVMEditorSubsystem::GetAllViewModels(const UWidgetBlueprint* WidgetBlueprint) const
{
	TArray<UE::MVVM::FBindingSource> Sources;

	if (UMVVMBlueprintView* View = GetView(WidgetBlueprint))
	{
		const TArrayView<const FMVVMBlueprintViewModelContext> ViewModels = View->GetViewModels();
		Sources.Reserve(ViewModels.Num());

		for (const FMVVMBlueprintViewModelContext& ViewModelContext : ViewModels)
		{
			Sources.Add(UE::MVVM::FBindingSource::CreateForViewModel(WidgetBlueprint, ViewModelContext));
		}
	}

	return Sources;
}

TArray<FMVVMAvailableBinding> UMVVMEditorSubsystem::GetChildViewModels(TSubclassOf<UObject> Class, TSubclassOf<UObject> Accessor)
{
	if (Class.Get() == nullptr)
	{
		return TArray<FMVVMAvailableBinding>();
	}

	TArray<FMVVMAvailableBinding> ViewModelAvailableBindingsList = UMVVMSubsystem::GetAvailableBindings(Class, Accessor);
	ViewModelAvailableBindingsList.RemoveAllSwap([Class](const FMVVMAvailableBinding& Value)
		{
			UE::MVVM::FMVVMFieldVariant Variant = UE::MVVM::BindingHelper::FindFieldByName(Class.Get(), Value.GetBindingName());
			const FProperty* Property = nullptr;
			if (Variant.IsProperty())
			{
				Property = Variant.GetProperty();
			}
			else if (Variant.IsFunction() && Variant.GetFunction())
			{
				Property = UE::MVVM::BindingHelper::GetReturnProperty(Variant.GetFunction());
			}

			if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
			{
				return !ObjectProperty->PropertyClass->ImplementsInterface(UNotifyFieldValueChanged::StaticClass());
			}
			return true;
		});

	return ViewModelAvailableBindingsList;
}

FGuid UMVVMEditorSubsystem::GetFirstBindingThatUsesViewModel(const UWidgetBlueprint* WidgetBlueprint, FGuid ViewModelId) const
{
	if (UMVVMBlueprintView* View = GetView(WidgetBlueprint))
	{
		for (const FMVVMBlueprintViewBinding& Binding : View->GetBindings())
		{
			if (Binding.SourcePath.GetViewModelId() == ViewModelId)
			{
				return Binding.BindingId;
			}
			if (Binding.DestinationPath.GetViewModelId() == ViewModelId)
			{
				return Binding.BindingId;
			}

			auto TestConversionFunction = [&](bool bForward)
			{
				if (UE::MVVM::IsForwardBinding(Binding.BindingType))
				{
					const UFunction* Function = GetConversionFunction(WidgetBlueprint, Binding, bForward);
					if (Function != nullptr)
					{
						TValueOrError<TArray<const FProperty*>, FText> ArgumentsResult = UE::MVVM::BindingHelper::TryGetArgumentsForConversionFunction(Function);
						if (ArgumentsResult.HasValue())
						{
							for (const FProperty* Property : ArgumentsResult.GetValue())
							{
								FMVVMBlueprintPropertyPath Path = GetPathForConversionFunctionArgument(WidgetBlueprint, Binding, Property->GetFName(), bForward);
								if (Path.GetViewModelId() == ViewModelId)
								{
									return Binding.BindingId;
								}
							}
						}
					}
				}
				return FGuid();
			};

			if (UE::MVVM::IsForwardBinding(Binding.BindingType))
			{
				FGuid Forward = TestConversionFunction(true);
				if (Forward.IsValid())
				{
					return Forward;
				}
			}

			if (UE::MVVM::IsBackwardBinding(Binding.BindingType))
			{
				FGuid Backward = TestConversionFunction(false);
				if (Backward.IsValid())
				{
					return Backward;
				}
			}
		}
	}
	return FGuid();
}

#undef LOCTEXT_NAMESPACE

