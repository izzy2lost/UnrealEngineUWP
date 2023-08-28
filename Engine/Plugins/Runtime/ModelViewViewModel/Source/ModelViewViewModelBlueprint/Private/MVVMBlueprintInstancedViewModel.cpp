// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVMBlueprintInstancedViewModel.h"

#include "MVVMBlueprintView.h"
#include "MVVMViewModelBase.h"
#include "MVVMWidgetBlueprintExtension_View.h"
#include "ViewModel/MVVMInstancedViewModelGeneratedClass.h"

#include "BlueprintActionDatabase.h"
#include "Engine/Engine.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MVVMBlueprintInstancedViewModel)

/**
 * 
 */
UMVVMBlueprintInstancedViewModelBase::UMVVMBlueprintInstancedViewModelBase()
{
	ParentClass = UMVVMViewModelBase::StaticClass();
}

void UMVVMBlueprintInstancedViewModelBase::GenerateClass()
{
	if (ParentClass.Get() == nullptr)
	{
		ParentClass = UMVVMViewModelBase::StaticClass();
	}
	if (!FKismetEditorUtilities::CanCreateBlueprintOfClass(ParentClass))
	{
		ParentClass = UMVVMViewModelBase::StaticClass();
	}
	if (GeneratedClassType.Get() == nullptr)
	{
		GeneratedClassType = UMVVMInstancedViewModelGeneratedClass::StaticClass();
	}

	auto SafeRename = [](UObject* Object)
	{
		ERenameFlags RenameFlags = REN_ForceNoResetLoaders | REN_NonTransactional | REN_DoNotDirty | REN_DontCreateRedirectors;
		FName TrashName = MakeUniqueObjectName(GetTransientPackage(), Object->GetClass(), *FString::Printf(TEXT("TRASH_%s"), *Object->GetName()));
		Object->Rename(*TrashName.ToString(), GetTransientPackage(), RenameFlags);
	};

	bool bGeneratedNewClass = false;
	if (GeneratedClass == nullptr)
	{
		bGeneratedNewClass = true;
		GeneratedClass = NewObject<UMVVMInstancedViewModelGeneratedClass>(GetOutermost(), GeneratedClassType.Get());
	}

	UObject* PreviousDefaultObject = GeneratedClass->GetDefaultObject(false);
	if (PreviousDefaultObject)
	{
		SafeRename(PreviousDefaultObject);
	}
	for (TFieldIterator<UFunction> FunctionIter(GeneratedClass, EFieldIteratorFlags::ExcludeSuper); FunctionIter; ++FunctionIter)
	{
		FunctionIter->FunctionFlags &= ~FUNC_Native;
		SafeRename(*FunctionIter);
	}

	// Clean class and reset basic properties
	GeneratedClass->PurgeClass(false);
	GeneratedClass->PropertyLink = ParentClass->PropertyLink;
	GeneratedClass->SetSuperStruct(ParentClass);
	GeneratedClass->ClassWithin = UObject::StaticClass();
	GeneratedClass->ClassConfigName = ParentClass->ClassConfigName;
	GeneratedClass->ClassFlags |= CLASS_NotPlaceable;

	// Clean up temporary generate variables
	{
		FromPropertyToCreatedProperty.Empty();
	}

	// Create Properties
	{
		PreAddProperties();
		for (TPropertyValueIterator<FProperty> It(GetSourceStruct(), GetSourceDefaults(), EPropertyValueIteratorFlags::NoRecursion, EFieldIteratorFlags::ExcludeDeprecated); It; ++It)
		{
			const FProperty* Property = It.Key();
			void const* ValuePtr = It.Value();

			AddProperty(Property);
		}
		PostAddProperties();
	}

	// Update the class
	GeneratedClass->Bind();
	GeneratedClass->StaticLink(true);
	GeneratedClass->AssembleReferenceTokenStream();
	ensure(GeneratedClass->ClassGeneratedBy == nullptr);
	ensure(GeneratedClass->GetDefaultObject(false) == nullptr);
	GeneratedClass->GetDefaultObject(true);
	GeneratedClass->UpdateCustomPropertyListForPostConstruction();

	// The class is not public and can only be access inside the UMG. What about inherited UMG???
	GeneratedClass->ClearFlags(RF_Public | RF_Transactional);
	GeneratedClass->GetDefaultObject(true)->ClearFlags(RF_Public);

	// Relink the new default object
	if (PreviousDefaultObject)
	{
		FLinkerLoad::PRIVATE_PatchNewObjectIntoExport(PreviousDefaultObject, GeneratedClass->GetDefaultObject(true));
	}

	// Initialize the default object value
	{
		PreSetDefaultValues();
		for (TPropertyValueIterator<FProperty> It(GetSourceStruct(), GetSourceDefaults(), EPropertyValueIteratorFlags::NoRecursion, EFieldIteratorFlags::ExcludeDeprecated); It; ++It)
		{
			const FProperty* Property = It.Key();
			void const* ValuePtr = It.Value();

			SetDefaultValue(Property, ValuePtr);
		}
		PostSetDefaultValues();
	}

	// Initialize the fields for MVVM
	GeneratedClass->InitializeFieldNotifies();

	// Inform that the class changed
	{
		TMap<UObject*, UObject*> OldToNew;
		if (PreviousDefaultObject)
		{
			OldToNew.Emplace(PreviousDefaultObject, GeneratedClass->GetDefaultObject(true));
		}
		if (bGeneratedNewClass)
		{
			OldToNew.Emplace(GeneratedClass, GeneratedClass);
		}
		if (GEngine && OldToNew.Num() > 0)
		{
			GEngine->NotifyToolsOfObjectReplacement(OldToNew);
		}
	}

	// Rebuild the BP action
	if (FBlueprintActionDatabase* ActionDB = FBlueprintActionDatabase::TryGet())
	{
		// Notify Blueprints that there is a new class to add to the action list
		ActionDB->RefreshClassActions(GeneratedClass);
	}

	// Clean up temporary generate variables
	{
		FromPropertyToCreatedProperty.Empty();
	}

	//GeneratedClass->DestroyPropertiesPendingDestruction();
}

bool UMVVMBlueprintInstancedViewModelBase::IsValidFieldName(const FName NewPropertyName) const
{
	if (!FName::IsValidXName(NewPropertyName, INVALID_NAME_CHARACTERS))
	{
		return false;
	}

	// Check if the name already exist. If it does, do not add the property.
	for (TFieldIterator<FField> PropertyIter(GeneratedClass, EFieldIteratorFlags::IncludeSuper); PropertyIter; ++PropertyIter)
	{
		if (PropertyIter->GetFName() == NewPropertyName)
		{
			return false;
		}
	}
	for (TFieldIterator<UField> FunctionIter(GeneratedClass, EFieldIteratorFlags::IncludeSuper); FunctionIter; ++FunctionIter)
	{
		if (FunctionIter->GetFName() == NewPropertyName)
		{
			return false;
		}
	}

	return true;
}

void UMVVMBlueprintInstancedViewModelBase::PreAddProperties()
{

}

void UMVVMBlueprintInstancedViewModelBase::PostAddProperties()
{
}

void UMVVMBlueprintInstancedViewModelBase::AddProperty(const FProperty* FromProperty)
{
	if (!IsValidFieldName(FromProperty->GetFName()))
	{
		return;
	}

	FProperty* NewProperty = CastFieldChecked<FProperty>(FField::Duplicate(FromProperty, GeneratedClass, FromProperty->GetFName()));
#if WITH_EDITOR
	FField::CopyMetaData(FromProperty, NewProperty);
#endif
	FInitializePropertyArgs Args;
	Args.PropertyName = FromProperty->GetFName();
	Args.DisplayName = FromProperty->GetMetaData(FBlueprintMetadata::MD_DisplayName);
	Args.bNetwork = true;
	InitializeProperty(NewProperty, Args);

	LinkProperty(NewProperty);
	FromPropertyToCreatedProperty.Add(FromProperty, NewProperty);
}


void UMVVMBlueprintInstancedViewModelBase::PreSetDefaultValues()
{
}

void UMVVMBlueprintInstancedViewModelBase::PostSetDefaultValues()
{
}

void UMVVMBlueprintInstancedViewModelBase::SetDefaultValue(const FProperty* Property, void const* ValuePtr)
{
	FProperty* NewProperty = nullptr;
	if (FProperty** NewPropertyPtr = FromPropertyToCreatedProperty.Find(Property))
	{
		NewProperty = *NewPropertyPtr;
	}
	else
	{
		NewProperty = GeneratedClass->FindPropertyByName(Property->GetFName());
	}

	if (NewProperty)
	{
		//void* DestinationPtr = reinterpret_cast<uint8*>(GeneratedClass->GetDefaultObject()) + NewProperty->GetOffset_ForInternal();
		void* DestinationPtr = NewProperty->ContainerPtrToValuePtr<void>(GeneratedClass->GetDefaultObject());
		NewProperty->CopyCompleteValue(DestinationPtr, ValuePtr);
	}
}

void UMVVMBlueprintInstancedViewModelBase::InitializeProperty(FProperty* NewProperty, FInitializePropertyArgs& Args)
{
	if (Args.bNetwork)
	{
		NewProperty->RepNotifyFunc = AddOnRepFunction(Args.PropertyName);
	}

	EPropertyFlags NewFlags = NewProperty->GetPropertyFlags() | EPropertyFlags::CPF_Edit;
	NewFlags |= !NewProperty->RepNotifyFunc.IsNone() ? EPropertyFlags::CPF_Net | EPropertyFlags::CPF_RepNotify : EPropertyFlags::CPF_None;
	NewFlags |= !Args.bPrivate ? EPropertyFlags::CPF_BlueprintVisible : EPropertyFlags::CPF_None;
	NewFlags |= Args.bReadOnly ? EPropertyFlags::CPF_BlueprintReadOnly : EPropertyFlags::CPF_None;
	NewProperty->SetPropertyFlags(NewFlags);
	if (Args.bFieldNotify)
	{
		NewProperty->SetMetaData(FBlueprintMetadata::MD_FieldNotify, TEXT(""));
		GeneratedClass->FieldNotifies.Add(FFieldNotificationId(Args.PropertyName));
	}
}

void UMVVMBlueprintInstancedViewModelBase::LinkProperty(FProperty* NewProperty)
{
	NewProperty->Next = GeneratedClass->ChildProperties;
	GeneratedClass->ChildProperties = NewProperty;
}

FName UMVVMBlueprintInstancedViewModelBase::AddOnRepFunction(FName PropertyName)
{
	FString OnRepCallFunctionName = FString::Printf(TEXT("__OnRep_%s"), *PropertyName.ToString());
	FName Name_OnRepCallFunctionName = *OnRepCallFunctionName;
	UObject* PreviousObj = StaticFindObjectFastInternal(nullptr, GeneratedClass, Name_OnRepCallFunctionName, true);
	if (PreviousObj)
	{
		// The function or property already exist. Something is wrong.
		return FName();
	}
	UFunction* Func = NewObject<UFunction>(GeneratedClass, Name_OnRepCallFunctionName);
	Func->FunctionFlags |= FUNC_Native | FUNC_Event | FUNC_BlueprintEvent | FUNC_BlueprintCallable;
	GeneratedClass->AddNativeFunction(*OnRepCallFunctionName, &UMVVMInstancedViewModelGeneratedClass::K2_CallNativeOnRep);
	GeneratedClass->AddFunctionToFunctionMap(Func, Func->GetFName());
	Func->Bind();
	Func->StaticLink(true);
	Func->Next = GeneratedClass->Children;
	GeneratedClass->Children = Func;
	return Func->GetFName();
}

#if WITH_EDITOR
void UMVVMBlueprintInstancedViewModel::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	FProperty* VariableProperty = GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMVVMBlueprintInstancedViewModel, Variables));
	FEditPropertyChain::TDoubleLinkedListNode* CurrentPropertyNode = PropertyChangedEvent.PropertyChain.GetActiveMemberNode();
	while (CurrentPropertyNode)
	{
		if (CurrentPropertyNode->GetValue() == VariableProperty)
		{
			FBlueprintEditorUtils::MarkBlueprintAsModified(GetOuterUMVVMBlueprintView()->GetOuterUMVVMWidgetBlueprintExtension_View()->GetWidgetBlueprint());
			break;
		}
		CurrentPropertyNode = CurrentPropertyNode->GetNextNode();
	}
}
#endif