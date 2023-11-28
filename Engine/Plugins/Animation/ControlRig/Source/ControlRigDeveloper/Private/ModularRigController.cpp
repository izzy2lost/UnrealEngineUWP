// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModularRigController.h"

#include "ControlRig.h"
#include "ModularRig.h"
#include "ControlRigBlueprint.h"
#include "ModularRigModel.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/DefaultValueHelper.h"
#include "Rigs/RigHierarchyController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularRigController)

#if WITH_EDITOR
#include "ScopedTransaction.h"
#endif

UModularRigController::UModularRigController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
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
		if(UControlRigBlueprint* Blueprint = Cast<UControlRigBlueprint>(GetOuter()))
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

bool UModularRigController::CanConnectConnectorToElement(const FRigModuleConnector& InConnector, const FRigElementKey& InTargetKey, FText& OutErrorMessage)
{
	// TODO: Check rules are satisfied
	return true;
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

	FText ErrorMessage;
	if (!CanConnectConnectorToElement(*ModuleConnector, InTargetKey, ErrorMessage))
	{
		UE_LOG(LogControlRig, Error, TEXT("Cannot connect connector %s to target %s: %s"),
			*InConnectorKey.Name.ToString(), *InTargetKey.ToString(), *ErrorMessage.ToString());
		return false;
	}

	UControlRigBlueprint* Blueprint = Cast<UControlRigBlueprint>(GetOuter());
	FRigConnectorElement* Connector = Cast<FRigConnectorElement>(Blueprint->Hierarchy->Find(InConnectorKey));
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

	TArray<uint8, TAlignedHeapAllocator<16>> TempStorage;
	TempStorage.AddZeroed(Property->GetSize());
	uint8* TempMemory = TempStorage.GetData();
	Property->InitializeValue(TempMemory);

	if (!FBlueprintEditorUtils::PropertyValueFromString_Direct(Property, InValue, TempMemory))
	{
		UE_LOG(LogControlRig, Error, TEXT("Value %s for variable %s in module %s is not valid"), *InValue, *InVariableName.ToString(), *InModulePath);
		return false;
	}

#if WITH_EDITOR
	TSharedPtr<FScopedTransaction> TransactionPtr;
	if (bSetupUndo)
	{
		TransactionPtr = MakeShared<FScopedTransaction>(NSLOCTEXT("ModularRigController", "ConfigureModuleValueTransaction", "Configure Module Value"));
		if(UControlRigBlueprint* Blueprint = Cast<UControlRigBlueprint>(GetOuter()))
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
	if(UControlRigBlueprint* Blueprint = Cast<UControlRigBlueprint>(GetOuter()))
	{
		TArray<FRigVMExternalVariable> Variables = Blueprint->GetControlRigClass()->GetDefaultObject<UControlRig>()->GetExternalVariables();
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
				const FString SourceVariablePath = FString::Printf(TEXT("%s:%s"), *CurModulePath, *Variable.Name.ToString());
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
		if(UControlRigBlueprint* Blueprint = Cast<UControlRigBlueprint>(GetOuter()))
		{
			SourceProperty = Blueprint->GetControlRigClass()->FindPropertyByName(*SourceVariableName);
		}
	}
	if (!SourceProperty)
	{
		OutErrorMessage = FText::FromString(FString::Printf(TEXT("Could not find source variable %s"), *InSourcePath));
		return false;
	}

	FString SourcePath = (SourceModulePath.IsEmpty()) ? SourceVariableName : FString::Printf(TEXT("%s:%s"), *SourceModulePath, *SourceVariableName);
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
		UE_LOG(LogControlRig, Error, TEXT("Could not bind module variable %s:%s : %s"), *InModulePath, *InVariableName.ToString(), *ErrorMessage.ToString());
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
		if(UControlRigBlueprint* Blueprint = Cast<UControlRigBlueprint>(GetOuter()))
		{
			SourceProperty = Blueprint->GetControlRigClass()->FindPropertyByName(*SourceVariableName);
		}
	}

	FString SourcePath = (SourceModulePath.IsEmpty()) ? SourceVariableName : FString::Printf(TEXT("%s:%s"), *SourceModulePath, *SourceVariableName);

#if WITH_EDITOR
	TSharedPtr<FScopedTransaction> TransactionPtr;
	if (bSetupUndo)
	{
		TransactionPtr = MakeShared<FScopedTransaction>(NSLOCTEXT("ModularRigController", "BindModuleVariableTransaction", "Bind Module Variable"));
		if(UControlRigBlueprint* Blueprint = Cast<UControlRigBlueprint>(GetOuter()))
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
		if(UControlRigBlueprint* Blueprint = Cast<UControlRigBlueprint>(GetOuter()))
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
		if(UControlRigBlueprint* Blueprint = Cast<UControlRigBlueprint>(GetOuter()))
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

bool UModularRigController::RenameModule(const FString& InModulePath, const FName& InNewName, bool bSetupUndo)
{
	FRigModuleReference* Module = FindModule(InModulePath);
	if (!Module)
	{
		UE_LOG(LogControlRig, Error, TEXT("Could not find module %s"), *InModulePath);
		return false;
	}

	FText ErrorMessage;
	if (!CanRenameModule(InModulePath, InNewName, ErrorMessage))
	{
		UE_LOG(LogControlRig, Error, TEXT("Could not rename module %s: %s"), *InModulePath, *ErrorMessage.ToString());
		return false;
	}

	const FString OldName = Module->Name.ToString();
	const FString NewName = InNewName.ToString();
	if (OldName.Equals(NewName))
	{
		return true;
	}
	
#if WITH_EDITOR
	TSharedPtr<FScopedTransaction> TransactionPtr;
	if (bSetupUndo)
	{
		TransactionPtr = MakeShared<FScopedTransaction>(NSLOCTEXT("ModularRigController", "RenameModuleTransaction", "Rename Module"));
		if(UControlRigBlueprint* Blueprint = Cast<UControlRigBlueprint>(GetOuter()))
		{
			Blueprint->Modify();
		}
	}
#endif
	
	const FString OldPath = (Module->ParentPath.IsEmpty()) ? OldName : FString::Printf(TEXT("%s:%s"), *Module->ParentPath, *OldName);
	const FString NewPath = (Module->ParentPath.IsEmpty()) ? *NewName :  FString::Printf(TEXT("%s:%s"), *Module->ParentPath, *NewName);
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
				Binding.Value = FString::Printf(TEXT("%s:%s"), *NewPath, *VariableName);
			}
		};
	}

	Notify(EModularRigNotification::ModuleRenamed, Module);

#if WITH_EDITOR
	TransactionPtr.Reset();
#endif
	
	return true;
}

bool UModularRigController::CanRenameModule(const FString& InModulePath, const FName& InNewName, FText& OutErrorMessage)
{
	if (InNewName.IsNone() || InNewName.ToString().IsEmpty())
	{
		OutErrorMessage = FText::FromString(TEXT("Name is empty."));
		return false;
	}

	FRigModuleReference* Module = FindModule(InModulePath);
	if (!Module)
	{
		OutErrorMessage = FText::FromString(FString::Printf(TEXT("Module %s not found."), *InModulePath));
		return false;
	}

	const FString NewNameStr = InNewName.ToString();
	const FString NewPath = (Module->ParentPath.IsEmpty()) ? NewNameStr : FString::Printf(TEXT("%s:%s"), *Module->ParentPath, *NewNameStr);
	if (InModulePath == NewPath)
	{
		return true;
	}

	// Check there are no siblings with the same name
	{
		FRigModuleReference* Parent = FindModule(Module->ParentPath);
		TArray<FRigModuleReference*>* Siblings = &Model->RootModules;
		if (Parent)
		{
			Siblings = &Parent->CachedChildren;
		}
		for (FRigModuleReference* Sibling : *Siblings)
		{
			if (Sibling->Name.ToString() == NewNameStr)
			{
				OutErrorMessage = FText::FromString(FString::Printf(TEXT("Sibling with path %s already exists"), *Sibling->GetPath()));
				return false;
			}
		}
	}
	return true;
}

bool UModularRigController::ReparentModule(const FString& InModulePath, const FString& InNewParentModulePath, bool bSetupUndo)
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
		if(UControlRigBlueprint* Blueprint = Cast<UControlRigBlueprint>(GetOuter()))
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
	Module->Name = GetSafeNewName(Module->GetPath());
	const FString NewPath = Module->GetPath();

	// Fix all the subtree namespaces
	TArray<FRigModuleReference*> SubTree = Module->CachedChildren;
	for (int32 Index=0; Index<SubTree.Num(); ++Index)
	{
		SubTree[Index]->ParentPath.ReplaceInline(*OldPath, *NewPath);
		SubTree.Append(SubTree[Index]->CachedChildren);
	}

	Model->UpdateCachedChildren();

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
				Binding.Value = FString::Printf(TEXT("%s:%s"), *NewPath, *VariableName);
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
	
	return false;
}

FName UModularRigController::GetSafeNewName(const FString& InModuleDesiredPath)
{
	FString ParentPath, DesiredName = InModuleDesiredPath;
	InModuleDesiredPath.Split(UModularRig::NamespaceSeparator, &ParentPath, &DesiredName, ESearchCase::CaseSensitive, ESearchDir::FromEnd);

	TArray<FRigModuleReference*>* Children = &Model->RootModules;
	if (!ParentPath.IsEmpty())
	{
		if (FRigModuleReference* Parent = FindModule(ParentPath))
		{
			Children = &Parent->CachedChildren;
		}
	}

	bool bSafeToUse = false;
	FString NewName = DesiredName;
	int32 Index = 0;
	while (!bSafeToUse)
	{
		bSafeToUse = true;
		for (FRigModuleReference* Child : *Children)
		{
			if (Child->Name == *NewName)
			{
				bSafeToUse = false;
				NewName = FString::Printf(TEXT("%s%d"), *DesiredName, ++Index);
				break;
			}
		}
	}
	return *NewName;
}

void UModularRigController::Notify(const EModularRigNotification& InNotification, const FRigModuleReference* InElement)
{
	if(!bSuspendNotifications)
	{
		ModifiedEvent.Broadcast(InNotification, InElement);
	}
}
