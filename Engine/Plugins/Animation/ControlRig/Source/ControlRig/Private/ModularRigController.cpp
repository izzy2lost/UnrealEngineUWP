// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModularRigController.h"

#include "ControlRig.h"
#include "ModularRig.h"
#include "ModularRigModel.h"
#include "Misc/DefaultValueHelper.h"
#include "Rigs/RigHierarchyController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularRigController)

#if WITH_EDITOR
#include "ScopedTransaction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#endif

UModularRigController::UModularRigController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Model(nullptr)
	, bSuspendNotifications(false)
{
}

FString UModularRigController::AddModule(const FName& InModuleName, TSubclassOf<UControlRig> InClass, const FString& InParentModulePath, bool bSetupUndo)
{
	if (!InClass)
	{
		UE_LOG(LogControlRig, Error, TEXT("Invalid InClass"));
		return FString();
	}

	UControlRig* ClassDefaultObject = InClass->GetDefaultObject<UControlRig>();
	if (!ClassDefaultObject->IsRigModule())
	{
		UE_LOG(LogControlRig, Error, TEXT("Class %s is not a rig module"), *InClass->GetClassPathName().ToString());
		return FString();
	}

#if WITH_EDITOR
	TSharedPtr<FScopedTransaction> TransactionPtr;
	if (bSetupUndo)
	{
		TransactionPtr = MakeShared<FScopedTransaction>(NSLOCTEXT("ModularRigController", "AddModuleTransaction", "Add Module"));
		if(UBlueprint* Blueprint = Cast<UBlueprint>(GetOuter()))
		{
			Blueprint->Modify();
		}
	}
#endif 

	FRigModuleReference* NewModule = nullptr;
	if (InParentModulePath.IsEmpty())
	{
		for (FRigModuleReference* Module : Model->RootModules)
		{
			if (Module->Name.ToString() == InModuleName)
			{
				return FString();
			}
		}

		Model->Modules.Add(FRigModuleReference(InModuleName, InClass, FString()));
		NewModule = &Model->Modules.Last();
	}
	else if (FRigModuleReference* ParentModule = FindModule(InParentModulePath))
	{
		for (FRigModuleReference* Module : ParentModule->CachedChildren)
		{
			if (Module->Name.ToString() == InModuleName)
			{
				return FString();
			}
		}

		Model->Modules.Add(FRigModuleReference(InModuleName, InClass, ParentModule->GetPath()));
		NewModule = &Model->Modules.Last();
	}

	Model->UpdateCachedChildren();
	UpdateShortNames();

	if (!NewModule)
	{
		UE_LOG(LogControlRig, Error, TEXT("Error while creating module %s"), *InModuleName.ToString());
		return FString();
	}

	Notify(EModularRigNotification::ModuleAdded, NewModule);

#if WITH_EDITOR
	TransactionPtr.Reset();
#endif

	return NewModule->GetPath();
}

FRigModuleReference* UModularRigController::FindModule(const FString& InPath)
{
	FString Path = InPath;
	Path.RemoveFromEnd(UModularRig::NamespaceSeparator);
	
	TArray<FRigModuleReference*>* Children = &Model->RootModules;
	FString Left = Path, Right;
	while (Left.Split(UModularRig::NamespaceSeparator, &Left, &Right))
	{
		FRigModuleReference** Cur = Children->FindByPredicate([Left](FRigModuleReference* Module)
		{
			return Module->Name == Left;
		});
		if (!Cur)
		{
			return nullptr;
		}
		Children = &(*Cur)->CachedChildren;
		Left = Right;
	}

	FRigModuleReference** Cur = Children->FindByPredicate([Left](FRigModuleReference* Module)
		{
			return Module->Name == Left;
		});
	if (!Cur)
	{
		return nullptr;
	}
	return *Cur;
}

bool UModularRigController::ConnectConnectorToElement(const FRigElementKey& InConnectorKey, const FRigElementKey& InTargetKey, bool bSetupUndo)
{
	FString ConnectorParentPath, ConnectorName;
	if (!InConnectorKey.Name.ToString().Split(UModularRig::NamespaceSeparator, &ConnectorParentPath, &ConnectorName, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
	{
		UE_LOG(LogControlRig, Error, TEXT("Connector %s does not contain a namespace"), *InConnectorKey.ToString());
		return false;
	}
	
	FRigModuleReference* Module = FindModule(ConnectorParentPath);
	if (!Module)
	{
		UE_LOG(LogControlRig, Error, TEXT("Could not find module %s"), *ConnectorParentPath);
		return false;
	}

	UControlRig* RigCDO = Module->Class->GetDefaultObject<UControlRig>();
	if (!RigCDO)
	{
		UE_LOG(LogControlRig, Error, TEXT("Invalid rig module class %s"), *Module->Class->GetPathName());
		return false;
	}

	const FRigModuleConnector* ModuleConnector = RigCDO->GetRigModuleSettings().ExposedConnectors.FindByPredicate(
		[ConnectorName](FRigModuleConnector& Connector)
		{
			return Connector.Name == ConnectorName;
		});
	if (!ModuleConnector)
	{
		UE_LOG(LogControlRig, Error, TEXT("Could not find connector %s in class %s"), *ConnectorName, *Module->Class->GetPathName());
		return false;
	}

	/*
	FText ErrorMessage;
	if (!CanConnectConnectorToElement(*ModuleConnector, InTargetKey, ErrorMessage))
	{
		UE_LOG(LogControlRig, Error, TEXT("Cannot connect connector %s to target %s: %s"),
			*InConnectorKey.Name.ToString(), *InTargetKey.ToString(), *ErrorMessage.ToString());
		return false;
	}
	*/

	UBlueprint* Blueprint = Cast<UBlueprint>(GetOuter());
	const IRigHierarchyProvider* HierarchyProvider = CastChecked<IRigHierarchyProvider>(Blueprint);
	const FRigConnectorElement* Connector = Cast<FRigConnectorElement>(HierarchyProvider->GetHierarchy()->Find(InConnectorKey));
	if (!Connector)
	{
		UE_LOG(LogControlRig, Error, TEXT("Could not find connector %s"), *InConnectorKey.ToString());
		return false;
	}

#if WITH_EDITOR
	TSharedPtr<FScopedTransaction> TransactionPtr;
	if (bSetupUndo)
	{
		TransactionPtr = MakeShared<FScopedTransaction>(NSLOCTEXT("ModularRigController", "ConnectModuleToElementTransaction", "Connect to Element"));
		Blueprint->Modify();
	}
#endif 

	const FRigElementKey ConnectorKey(*ConnectorName, ERigElementType::Connector);
	FRigElementKey& TargetKey = Module->Connections.FindOrAdd(ConnectorKey);
	TargetKey = InTargetKey;

	Notify(EModularRigNotification::ConnectionChanged, Module);

#if WITH_EDITOR
	TransactionPtr.Reset();
#endif
	
	return true;
}

bool UModularRigController::SetConfigValueInModule(const FString& InModulePath, const FName& InVariableName, const FString& InValue, bool bSetupUndo)
{
	FRigModuleReference* Module = FindModule(InModulePath);
	if (!Module)
	{
		UE_LOG(LogControlRig, Error, TEXT("Could not find module %s"), *InModulePath);
		return false;
	}

	if (!Module->Class.IsValid())
	{
		UE_LOG(LogControlRig, Error, TEXT("Class defined in module %s is not valid"), *InModulePath);
		return false;
	}

	const FProperty* Property = Module->Class->FindPropertyByName(InVariableName);
	if (!Property)
	{
		UE_LOG(LogControlRig, Error, TEXT("Could not find variable %s in module %s"), *InVariableName.ToString(), *InModulePath);
		return false;
	}

	if (Property->HasAllPropertyFlags(CPF_BlueprintReadOnly))
	{
		UE_LOG(LogControlRig, Error, TEXT("The target variable %s in module %s is read only"), *InVariableName.ToString(), *InModulePath);
		return false;
	}

#if WITH_EDITOR
	TArray<uint8, TAlignedHeapAllocator<16>> TempStorage;
	TempStorage.AddZeroed(Property->GetSize());
	uint8* TempMemory = TempStorage.GetData();
	Property->InitializeValue(TempMemory);

	if (!FBlueprintEditorUtils::PropertyValueFromString_Direct(Property, InValue, TempMemory))
	{
		UE_LOG(LogControlRig, Error, TEXT("Value %s for variable %s in module %s is not valid"), *InValue, *InVariableName.ToString(), *InModulePath);
		return false;
	}

	TSharedPtr<FScopedTransaction> TransactionPtr;
	if (bSetupUndo)
	{
		TransactionPtr = MakeShared<FScopedTransaction>(NSLOCTEXT("ModularRigController", "ConfigureModuleValueTransaction", "Configure Module Value"));
		if(UBlueprint* Blueprint = Cast<UBlueprint>(GetOuter()))
		{
			Blueprint->Modify();
		}
	}
#endif 

	Module->ConfigValues.FindOrAdd(InVariableName) = InValue;

	Notify(EModularRigNotification::ModuleConfigValueChanged, Module);

#if WITH_EDITOR
	TransactionPtr.Reset();
#endif

	return true;
}

TArray<FString> UModularRigController::GetPossibleBindings(const FString& InModulePath, const FName& InVariableName)
{
	TArray<FString> PossibleBindings;
	const FRigModuleReference* Module = FindModule(InModulePath);
	if (!Module)
	{
		return PossibleBindings;
	}

	if (!Module->Class.IsValid())
	{
		return PossibleBindings;
	}

	const FProperty* TargetProperty = Module->Class->FindPropertyByName(InVariableName);
	if (!TargetProperty)
	{
		return PossibleBindings;
	}

	if (TargetProperty->HasAnyPropertyFlags(CPF_BlueprintReadOnly | CPF_DisableEditOnInstance))
	{
		return PossibleBindings;
	}

	// Add possible blueprint variables
	if(UBlueprint* Blueprint = Cast<UBlueprint>(GetOuter()))
	{
		TArray<FRigVMExternalVariable> Variables = Blueprint->GeneratedClass->GetDefaultObject<UControlRig>()->GetExternalVariables();
		for (const FRigVMExternalVariable& Variable : Variables)
		{
			FText ErrorMessage;
			const FString VariableName = Variable.Name.ToString();
			if (CanBindModuleVariable(InModulePath, InVariableName, VariableName, ErrorMessage))
			{
				PossibleBindings.Add(VariableName);
			}
		}
	}

	// Add possible module variables
	const FString InvalidModulePrefix = InModulePath + UModularRig::NamespaceSeparator;
	Model->ForEachModule([this, &PossibleBindings, InModulePath, InVariableName, InvalidModulePrefix](const FRigModuleReference* InModule) -> bool
	{
		const FString CurModulePath = InModule->GetPath();
		if (InModulePath != CurModulePath && !CurModulePath.StartsWith(InvalidModulePrefix))
		{
			TArray<FRigVMExternalVariable> Variables = InModule->Class->GetDefaultObject<UControlRig>()->GetExternalVariables();
			for (const FRigVMExternalVariable& Variable : Variables)
			{
				FText ErrorMessage;
				const FString SourceVariablePath = URigHierarchy::JoinNameSpace(CurModulePath, Variable.Name.ToString());
				if (CanBindModuleVariable(InModulePath, InVariableName, SourceVariablePath, ErrorMessage))
				{
					PossibleBindings.Add(SourceVariablePath);
				}
			}
		}		
		return true;
	});

	return PossibleBindings;
}

bool UModularRigController::CanBindModuleVariable(const FString& InModulePath, const FName& InVariableName, const FString& InSourcePath, FText& OutErrorMessage)
{
	FRigModuleReference* Module = FindModule(InModulePath);
	if (!Module)
	{
		OutErrorMessage = FText::FromString(FString::Printf(TEXT("Could not find module %s"), *InModulePath));
		return false;
	}

	if (!Module->Class.IsValid())
	{
		OutErrorMessage = FText::FromString(FString::Printf(TEXT("Class defined in module %s is not valid"), *InModulePath));
		return false;
	}

	const FProperty* TargetProperty = Module->Class->FindPropertyByName(InVariableName);
	if (!TargetProperty)
	{
		OutErrorMessage = FText::FromString(FString::Printf(TEXT("Could not find variable %s in module %s"), *InVariableName.ToString(), *InModulePath));
		return false;
	}

	if (TargetProperty->HasAnyPropertyFlags(CPF_BlueprintReadOnly | CPF_DisableEditOnInstance))
	{
		OutErrorMessage = FText::FromString(FString::Printf(TEXT("The target variable %s in module %s is read only"), *InVariableName.ToString(), *InModulePath));
		return false;
	}

	FString SourceModulePath, SourceVariableName = InSourcePath;
	InSourcePath.Split(UModularRig::NamespaceSeparator, &SourceModulePath, &SourceVariableName, ESearchCase::CaseSensitive, ESearchDir::FromEnd);

	FRigModuleReference* SourceModule = nullptr;
	if (!SourceModulePath.IsEmpty())
	{
		SourceModule = FindModule(SourceModulePath);
		if (!SourceModule)
		{
			OutErrorMessage = FText::FromString(FString::Printf(TEXT("Could not find source module %s"), *SourceModulePath));
			return false;
		}

		if (SourceModulePath.StartsWith(InModulePath))
		{
			OutErrorMessage = FText::FromString(FString::Printf(TEXT("Cannot bind variable of module %s to a variable of module %s because the source module is a child of the target module"), *InModulePath, *SourceModulePath));
			return false;
		}
	}

	const FProperty* SourceProperty = nullptr;
	if (SourceModule)
	{
		SourceProperty = SourceModule->Class->FindPropertyByName(*SourceVariableName);
	}
	else
	{
		if(const UBlueprint* Blueprint = Cast<UBlueprint>(GetOuter()))
		{
			SourceProperty = Blueprint->GeneratedClass->FindPropertyByName(*SourceVariableName);
		}
	}
	if (!SourceProperty)
	{
		OutErrorMessage = FText::FromString(FString::Printf(TEXT("Could not find source variable %s"), *InSourcePath));
		return false;
	}

	FString SourcePath = (SourceModulePath.IsEmpty()) ? SourceVariableName : URigHierarchy::JoinNameSpace(SourceModulePath, SourceVariableName);
	if (!RigVMTypeUtils::AreCompatible(SourceProperty, TargetProperty))
	{
		FString TargetPath = FString::Printf(TEXT("%s.%s"), *InModulePath, *InVariableName.ToString());
		OutErrorMessage = FText::FromString(FString::Printf(TEXT("Property %s of type %s and %s of type %s are not compatible"), *SourcePath, *SourceProperty->GetCPPType(), *TargetPath, *TargetProperty->GetCPPType()));
		return false;
	}

	return true;
}

bool UModularRigController::BindModuleVariable(const FString& InModulePath, const FName& InVariableName, const FString& InSourcePath, bool bSetupUndo)
{
	FText ErrorMessage;
	if (!CanBindModuleVariable(InModulePath, InVariableName, InSourcePath, ErrorMessage))
	{
		UE_LOG(LogControlRig, Error, TEXT("Could not bind module variable %s : %s"), *URigHierarchy::JoinNameSpace(InModulePath, InVariableName.ToString()), *ErrorMessage.ToString());
		return false;
	}
	
	FRigModuleReference* Module = FindModule(InModulePath);
	const FProperty* TargetProperty = Module->Class->FindPropertyByName(InVariableName);

	FString SourceModulePath, SourceVariableName = InSourcePath;
	InSourcePath.Split(UModularRig::NamespaceSeparator, &SourceModulePath, &SourceVariableName, ESearchCase::CaseSensitive, ESearchDir::FromEnd);

	FRigModuleReference* SourceModule = nullptr;
	if (!SourceModulePath.IsEmpty())
	{
		SourceModule = FindModule(SourceModulePath);
	}

	const FProperty* SourceProperty = nullptr;
	if (SourceModule)
	{
		SourceProperty = SourceModule->Class->FindPropertyByName(*SourceVariableName);
	}
	else
	{
		if(const UBlueprint* Blueprint = Cast<UBlueprint>(GetOuter()))
		{
			SourceProperty = Blueprint->GeneratedClass->FindPropertyByName(*SourceVariableName);
		}
	}

	FString SourcePath = (SourceModulePath.IsEmpty()) ? SourceVariableName : URigHierarchy::JoinNameSpace(SourceModulePath, SourceVariableName);

#if WITH_EDITOR
	TSharedPtr<FScopedTransaction> TransactionPtr;
	if (bSetupUndo)
	{
		TransactionPtr = MakeShared<FScopedTransaction>(NSLOCTEXT("ModularRigController", "BindModuleVariableTransaction", "Bind Module Variable"));
		if(UBlueprint* Blueprint = Cast<UBlueprint>(GetOuter()))
		{
			Blueprint->Modify();
		}
	}
#endif

	FString& SourceStr = Module->Bindings.FindOrAdd(InVariableName);
	SourceStr = SourcePath;

	Notify(EModularRigNotification::ModuleConfigValueChanged, Module);

#if WITH_EDITOR
	TransactionPtr.Reset();
#endif

	return true;
}

bool UModularRigController::UnBindModuleVariable(const FString& InModulePath, const FName& InVariableName, bool bSetupUndo)
{
	FRigModuleReference* Module = FindModule(InModulePath);
	if (!Module)
	{
		UE_LOG(LogControlRig, Error, TEXT("Could not find module %s"), *InModulePath);
		return false;
	}

	if (!Module->Bindings.Contains(InVariableName))
	{
		UE_LOG(LogControlRig, Error, TEXT("Variable %s in module %s is not bound"), *InVariableName.ToString(), *InModulePath);
		return false;
	}

#if WITH_EDITOR
	TSharedPtr<FScopedTransaction> TransactionPtr;
	if (bSetupUndo)
	{
		TransactionPtr = MakeShared<FScopedTransaction>(NSLOCTEXT("ModularRigController", "BindModuleVariableTransaction", "Bind Module Variable"));
		if(UBlueprint* Blueprint = Cast<UBlueprint>(GetOuter()))
		{
			Blueprint->Modify();
		}
	}
#endif

	Module->Bindings.Remove(InVariableName);

	Notify(EModularRigNotification::ModuleConfigValueChanged, Module);

#if WITH_EDITOR
	TransactionPtr.Reset();
#endif

	return true;
}

bool UModularRigController::DeleteModule(const FString& InModulePath, bool bSetupUndo)
{
	FRigModuleReference* Module = FindModule(InModulePath);
	if (!Module)
	{
		UE_LOG(LogControlRig, Error, TEXT("Could not find module %s"), *InModulePath);
		return false;
	}

#if WITH_EDITOR
	TSharedPtr<FScopedTransaction> TransactionPtr;
	if (bSetupUndo)
	{
		TransactionPtr = MakeShared<FScopedTransaction>(NSLOCTEXT("ModularRigController", "RenameModuleTransaction", "Rename Module"));
		if(UBlueprint* Blueprint = Cast<UBlueprint>(GetOuter()))
		{
			Blueprint->Modify();
		}
	}
#endif

	// Unparent children (add them to root)
	for (FRigModuleReference* Child : Module->CachedChildren)
	{
		ReparentModule(Child->GetPath(), FString(), bSetupUndo);
	}

	Model->DeletedModules.Add(*Module);
	Model->Modules.RemoveSingle(*Module);
	Model->UpdateCachedChildren();
	UpdateShortNames();

	// Fix bindings
	for (FRigModuleReference& Reference : Model->Modules)
	{
		Reference.Bindings = Reference.Bindings.FilterByPredicate([InModulePath](const TPair<FName, FString>& Binding)
		{
			FString ModulePath, VariableName = Binding.Value;
			Binding.Value.Split(UModularRig::NamespaceSeparator, &ModulePath, &VariableName, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
			if (ModulePath == InModulePath)
			{
				return false;
			}
			return true;
		});
	}

	Notify(EModularRigNotification::ModuleRemoved, &Model->DeletedModules.Last());

	Model->DeletedModules.Reset();

#if WITH_EDITOR
	TransactionPtr.Reset();
#endif
	return false;
}

FString UModularRigController::RenameModule(const FString& InModulePath, const FName& InNewName, bool bSetupUndo)
{
	FRigModuleReference* Module = FindModule(InModulePath);
	if (!Module)
	{
		UE_LOG(LogControlRig, Error, TEXT("Could not find module %s"), *InModulePath);
		return FString();
	}

	FText ErrorMessage;
	if (!CanRenameModule(InModulePath, InNewName, ErrorMessage))
	{
		UE_LOG(LogControlRig, Error, TEXT("Could not rename module %s: %s"), *InModulePath, *ErrorMessage.ToString());
		return FString();
	}

	const FString OldName = Module->Name.ToString();
	const FString NewName = InNewName.ToString();
	if (OldName.Equals(NewName))
	{
		return Module->GetPath();
	}
	
#if WITH_EDITOR
	TSharedPtr<FScopedTransaction> TransactionPtr;
	if (bSetupUndo)
	{
		TransactionPtr = MakeShared<FScopedTransaction>(NSLOCTEXT("ModularRigController", "RenameModuleTransaction", "Rename Module"));
		if(UBlueprint* Blueprint = Cast<UBlueprint>(GetOuter()))
		{
			Blueprint->Modify();
		}
	}
#endif
	
	const FString OldPath = (Module->ParentPath.IsEmpty()) ? OldName : URigHierarchy::JoinNameSpace(Module->ParentPath, OldName);
	const FString NewPath = (Module->ParentPath.IsEmpty()) ? *NewName :  URigHierarchy::JoinNameSpace(Module->ParentPath, NewName);
	Module->PreviousName = Module->Name;
	Module->Name = InNewName;
	TArray<FRigModuleReference*> Children;
	Children.Append(Module->CachedChildren);
	for (int32 i=0; i<Children.Num(); ++i)
	{
		FRigModuleReference* Child = Children[i];
		Child->ParentPath.ReplaceInline(*OldPath, *NewPath);

		Children.Append(Child->CachedChildren);
	}

	// Fix bindings
	for (FRigModuleReference& Reference : Model->Modules)
	{
		for (TPair<FName, FString>& Binding : Reference.Bindings)
		{
			FString ModulePath, VariableName = Binding.Value;
			Binding.Value.Split(UModularRig::NamespaceSeparator, &ModulePath, &VariableName, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
			if (ModulePath == OldPath)
			{
				Binding.Value = URigHierarchy::JoinNameSpace(NewPath, VariableName);
			}
		};
	}

	UpdateShortNames();
	Notify(EModularRigNotification::ModuleRenamed, Module);

#if WITH_EDITOR
	TransactionPtr.Reset();
#endif
	
	return NewPath;
}

bool UModularRigController::CanRenameModule(const FString& InModulePath, const FName& InNewName, FText& OutErrorMessage) const
{
	if (InNewName.IsNone() || InNewName.ToString().IsEmpty())
	{
		OutErrorMessage = FText::FromString(TEXT("Name is empty."));
		return false;
	}

	if(InNewName.ToString().Contains(UModularRig::NamespaceSeparator))
	{
		OutErrorMessage = NSLOCTEXT("ModularRigController", "NameContainsNamespaceSeparator", "Name contains namespace separator ':'.");
		return false;
	}

	const FRigModuleReference* Module = const_cast<UModularRigController*>(this)->FindModule(InModulePath);
	if (!Module)
	{
		OutErrorMessage = FText::FromString(FString::Printf(TEXT("Module %s not found."), *InModulePath));
		return false;
	}

	FString ErrorMessage;
	if(!IsNameAvailable(Module->ParentPath, InNewName, &ErrorMessage))
	{
		OutErrorMessage = FText::FromString(ErrorMessage);
		return false;
	}
	return true;
}

FString UModularRigController::ReparentModule(const FString& InModulePath, const FString& InNewParentModulePath, bool bSetupUndo)
{
	FRigModuleReference* Module = FindModule(InModulePath);
	if (!Module)
	{
		UE_LOG(LogControlRig, Error, TEXT("Could not find module %s"), *InModulePath);
		return FString();
	}

#if WITH_EDITOR
	TSharedPtr<FScopedTransaction> TransactionPtr;
	if (bSetupUndo)
	{
		TransactionPtr = MakeShared<FScopedTransaction>(NSLOCTEXT("ModularRigController", "ReparentModuleTransaction", "Reparent Module"));
		if(UBlueprint* Blueprint = Cast<UBlueprint>(GetOuter()))
		{
			Blueprint->Modify();
		}
	}
#endif

	// Reparent or unparent children
	FRigModuleReference* NewParentModule = FindModule(InNewParentModulePath);
	const FString OldPath = Module->GetPath();
	Module->PreviousParentPath = Module->ParentPath;
	Module->ParentPath = (NewParentModule) ? NewParentModule->GetPath() : FString();
	Module->Name = GetSafeNewName(Module->ParentPath, FRigName(Module->Name));
	const FString NewPath = Module->GetPath();

	// Fix all the subtree namespaces
	TArray<FRigModuleReference*> SubTree = Module->CachedChildren;
	for (int32 Index=0; Index<SubTree.Num(); ++Index)
	{
		SubTree[Index]->ParentPath.ReplaceInline(*OldPath, *NewPath);
		SubTree.Append(SubTree[Index]->CachedChildren);
	}


	Model->UpdateCachedChildren();
	UpdateShortNames();

	// Fix bindings
	for (FRigModuleReference& Reference : Model->Modules)
	{
		const FString ReferencePath = Reference.GetPath();
		for (TPair<FName, FString>& Binding : Reference.Bindings)
		{
			FString ModulePath, VariableName = Binding.Value;
			Binding.Value.Split(UModularRig::NamespaceSeparator, &ModulePath, &VariableName, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
			if (ModulePath == OldPath)
			{
				Binding.Value = URigHierarchy::JoinNameSpace(NewPath, VariableName);
				ModulePath = NewPath;
			}

			// Remove any child dependency
			if (ModulePath.Contains(ReferencePath))
			{
				UE_LOG(LogControlRig, Warning, TEXT("Binding lost due to source %s contained in child module of %s"), *Binding.Value, *ReferencePath);
				Binding.Value.Reset();
			}
		};

		Reference.Bindings = Reference.Bindings.FilterByPredicate([](const TPair<FName, FString>& Binding)
		{
			return !Binding.Value.IsEmpty();
		});
	}

	Notify(EModularRigNotification::ModuleReparented, Module);
	
	return NewPath;
}

bool UModularRigController::SetModuleShortName(const FString& InModulePath, const FString& InNewShortName, bool bSetupUndo)
{
	FRigModuleReference* Module = FindModule(InModulePath);
	if (!Module)
	{
		UE_LOG(LogControlRig, Error, TEXT("Could not find module %s"), *InModulePath);
		return false;
	}

	FText ErrorMessage;
	if (!CanSetModuleShortName(InModulePath, InNewShortName, ErrorMessage))
	{
		UE_LOG(LogControlRig, Error, TEXT("Could not rename module %s: %s"), *InModulePath, *ErrorMessage.ToString());
		return false;
	}

	const FString OldShortName = Module->GetShortName();
	const FString NewShortName = InNewShortName;
	if (OldShortName.Equals(NewShortName))
	{
		return true;
	}
	
#if WITH_EDITOR
	TSharedPtr<FScopedTransaction> TransactionPtr;
	if (bSetupUndo)
	{
		TransactionPtr = MakeShared<FScopedTransaction>(NSLOCTEXT("ModularRigController", "SetModuleShortNameTransaction", "Set Module Display Name"));
		if(UBlueprint* Blueprint = Cast<UBlueprint>(GetOuter()))
		{
			Blueprint->Modify();
		}
	}
#endif
	
	Module->ShortName = InNewShortName;
	Module->bShortNameBasedOnPath = false;

	Notify(EModularRigNotification::ModuleShortNameChanged, Module);

	// update all other display named to avoid collision
	UpdateShortNames();

#if WITH_EDITOR
	TransactionPtr.Reset();
#endif
	
	return true;
}

bool UModularRigController::CanSetModuleShortName(const FString& InModulePath, const FString& InNewShortName, FText& OutErrorMessage) const
{
	FString ErrorMessage;
	if(!IsShortNameAvailable(FRigName(InNewShortName), &ErrorMessage))
	{
		OutErrorMessage = FText::FromString(ErrorMessage);
		return false;
	}
	return true;
}

void UModularRigController::SanitizeName(FRigName& InOutName, bool bAllowNameSpaces)
{
	// Sanitize the name
	FString SanitizedNameString = InOutName.GetName();
	bool bChangedSomething = false;
	for (int32 i = 0; i < SanitizedNameString.Len(); ++i)
	{
		TCHAR& C = SanitizedNameString[i];

		const bool bGoodChar = FChar::IsAlpha(C) ||					 // Any letter
			(C == '_') || (C == '-') || (C == '.') || (C == '|') ||	 // _  - .  | anytime
			(FChar::IsDigit(C)) ||									 // 0-9 anytime
			((i > 0) && (C== ' '));									 // Space after the first character to support virtual bones

		if (!bGoodChar)
		{
			if(bAllowNameSpaces && C == ':')
			{
				continue;
			}
			
			C = '_';
			bChangedSomething = true;
		}
	}

	if (SanitizedNameString.Len() > GetMaxNameLength())
	{
		SanitizedNameString.LeftChopInline(SanitizedNameString.Len() - GetMaxNameLength());
		bChangedSomething = true;
	}

	if(bChangedSomething)
	{
		InOutName.SetName(SanitizedNameString);
	}
}

FRigName UModularRigController::GetSanitizedName(const FRigName& InName, bool bAllowNameSpaces)
{
	FRigName Name = InName;
	SanitizeName(Name, bAllowNameSpaces);
	return Name;
}

bool UModularRigController::IsNameAvailable(const FString& InParentModulePath, const FRigName& InDesiredName, FString* OutErrorMessage) const
{
	const FRigName DesiredName = GetSanitizedName(InDesiredName, false);
	if(DesiredName != InDesiredName)
	{
		if(OutErrorMessage)
		{
			static const FString ContainsInvalidCharactersMessage = TEXT("Name contains invalid characters.");
			*OutErrorMessage = ContainsInvalidCharactersMessage;
		}
		return false;
	}

	TArray<FRigModuleReference*>* Children = &Model->RootModules;
	if (!InParentModulePath.IsEmpty())
	{
		if (FRigModuleReference* Parent = const_cast<UModularRigController*>(this)->FindModule(InParentModulePath))
		{
			Children = &Parent->CachedChildren;
		}
	}

	for (const FRigModuleReference* Child : *Children)
	{
		if (FRigName(Child->Name) == DesiredName)
		{
			if(OutErrorMessage)
			{
				static const FString NameAlreadyInUse = TEXT("This name is already in use.");
				*OutErrorMessage = NameAlreadyInUse;
			}
			return false;
		}
	}
	return true;
}

bool UModularRigController::IsShortNameAvailable(const FRigName& InDesiredShortName, FString* OutErrorMessage) const
{
	const FRigName DesiredShortName = GetSanitizedName(InDesiredShortName, false);
	if(DesiredShortName != InDesiredShortName)
	{
		if(OutErrorMessage)
		{
			static const FString ContainsInvalidCharactersMessage = TEXT("Display Name contains invalid characters.");
			*OutErrorMessage = ContainsInvalidCharactersMessage;
		}
		return false;
	}

	for (const FRigModuleReference& Child : Model->Modules)
	{
		if (InDesiredShortName == FRigName(Child.GetShortName()))
		{
			if(OutErrorMessage)
			{
				static const FString NameAlreadyInUse = TEXT("This name is already in use.");
				*OutErrorMessage = NameAlreadyInUse;
			}
			return false;
		}
	}
	return true;
}

FRigName UModularRigController::GetSafeNewName(const FString& InParentModulePath, const FRigName& InDesiredName) const
{
	bool bSafeToUse = false;

	// create a copy of the desired name so that the string conversion can be cached
	const FRigName DesiredName = GetSanitizedName(InDesiredName, false);
	FRigName NewName = DesiredName;
	int32 Index = 0;
	while (!bSafeToUse)
	{
		bSafeToUse = true;
		if(!IsNameAvailable(InParentModulePath, NewName))
		{
			bSafeToUse = false;
			NewName = FString::Printf(TEXT("%s_%d"), *DesiredName.ToString(), ++Index);
		}
	}
	return NewName;
}

FRigName UModularRigController::GetSafeNewShortName(const FRigName& InDesiredShortName) const
{
	bool bSafeToUse = false;

	// create a copy of the desired name so that the string conversion can be cached
	const FRigName DesiredShortName = GetSanitizedName(InDesiredShortName, true);
	FRigName NewShortName = DesiredShortName;
	int32 Index = 0;
	while (!bSafeToUse)
	{
		bSafeToUse = true;
		if(!IsShortNameAvailable(NewShortName))
		{
			bSafeToUse = false;
			NewShortName = FString::Printf(TEXT("%s_%d"), *DesiredShortName.ToString(), ++Index);
		}
	}
	return NewShortName;
}

void UModularRigController::Notify(const EModularRigNotification& InNotification, const FRigModuleReference* InElement)
{
	if(!bSuspendNotifications)
	{
		ModifiedEvent.Broadcast(InNotification, InElement);
	}
}

void UModularRigController::UpdateShortNames()
{
	TMap<FString, int32> TokenToCount;

	// collect all usages of all paths and their segments
	for(const FRigModuleReference& Module : Model->Modules)
	{
		if(Module.bShortNameBasedOnPath)
		{
			FString RemainingPath = Module.GetPath();
			TokenToCount.FindOrAdd(RemainingPath, 0)++;
			while(RemainingPath.Split(UModularRig::NamespaceSeparator, nullptr, &RemainingPath, ESearchCase::IgnoreCase, ESearchDir::FromStart))
			{
				TokenToCount.FindOrAdd(RemainingPath, 0)++;
			}
		}
		else
		{
			TokenToCount.FindOrAdd(Module.ShortName, 0)++;
		}
	}

	for(FRigModuleReference& Module : Model->Modules)
	{
		if(Module.bShortNameBasedOnPath)
		{
			FString ShortPath = Module.GetPath();
			if(!Module.ParentPath.IsEmpty())
			{
				FString Left, Right, RemainingPath = Module.GetPath();
				ShortPath.Reset();

				while (RemainingPath.Split(UModularRig::NamespaceSeparator, &Left, &Right, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
				{
					ShortPath = ShortPath.IsEmpty() ? Right : URigHierarchy::JoinNameSpace(Right, ShortPath);

					// if the short path only exists once - that's what we use for the display name
					if(TokenToCount.FindChecked(ShortPath) == 1)
					{
						RemainingPath.Reset();
						break;
					}

					RemainingPath = Left;
				}

				if(!RemainingPath.IsEmpty())
				{
					ShortPath = URigHierarchy::JoinNameSpace(RemainingPath, ShortPath);
				}
			}

			if(!Module.ShortName.Equals(ShortPath, ESearchCase::CaseSensitive))
			{
				Module.ShortName = ShortPath;
				Notify(EModularRigNotification::ModuleShortNameChanged, &Module);
			}
		}
		else
		{
			// the display name is user defined so we won't touch it
		}
		
	}
}
