// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModularRigController.h"

#include "ControlRig.h"
#include "ModularRig.h"
#include "ControlRigBlueprint.h"
#include "ModularRigModel.h"
#include "Rigs/RigHierarchyController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModularRigController)

UModularRigController::UModularRigController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UModularRigController::AddModule(const FName& InModuleName, TSubclassOf<UBaseControlRig> InClass, const FString& InParentModulePath)
{
	if (!InClass->GetDefaultObject<UBaseControlRig>()->IsRigModule())
	{
		return false;
	}

	FRigModuleReference* NewModule = nullptr;
	if (InParentModulePath.IsEmpty())
	{
		for (FRigModuleReference* Module : Model->RootModules)
		{
			if (Module->Name.ToString() == InModuleName)
			{
				return false;
			}
		}

		Model->Modules.Add(FRigModuleReference(InModuleName, InClass, FString()));
		NewModule = &Model->Modules.Last();
		Model->RootModules.Add(NewModule);
	}
	else if (FRigModuleReference* ParentModule = FindModule(InParentModulePath))
	{
		for (FRigModuleReference* Module : ParentModule->CachedChildren)
		{
			if (Module->Name.ToString() == InModuleName)
			{
				return false;
			}
		}

		Model->Modules.Add(FRigModuleReference(InModuleName, InClass, ParentModule->GetNamespace()));
		NewModule = &Model->Modules.Last();
		ParentModule->CachedChildren.Add(&Model->Modules.Last());
	}

	if (!NewModule)
	{
		return false;
	}

	UControlRig* DefaultModule = InClass->GetDefaultObject<UControlRig>();
	if (!DefaultModule)
	{
		return false;
	}

	const FString NewNamespace = NewModule->GetNamespace();

	// Add Connectors/Sockets
	UControlRigBlueprint* Blueprint = Cast<UControlRigBlueprint>(GetOuter());
	if (URigHierarchyController* Controller = Blueprint->GetHierarchyController())
	{
		if (UBaseControlRig* CDO = Blueprint->GetControlRigClass()->GetDefaultObject<UBaseControlRig>())
		{
			FRigVMExtendedExecuteContext& Context = CDO->GetRigVMExtendedExecuteContext();
			FRigHierarchyExecuteContextBracket HierarchyContextGuard(Controller->GetHierarchy(), &Context);

			// setup the module information
			FControlRigExecuteContext& PublicContext = Context.GetPublicDataSafe<FControlRigExecuteContext>();
			FControlRigExecuteContextRigModuleGuard RigModuleGuard(PublicContext, NewNamespace);

			
			const TArray<FRigModuleConnector>& Connectors = DefaultModule->GetRigModuleSettings().ExposedConnectors;
			for (const FRigModuleConnector& Connector : Connectors)
			{
				Controller->AddConnector(*Connector.Name, Connector.Settings);
			}

			// todo: copy the sockets
			// TArray<FRigSocketElement*> Sockets = DefaultModule->GetHierarchy()->GetElementsOfType<FRigSocketElement>();
			// for (FRigSocketElement* Socket : Sockets)
			// {
			// 	Controller->CopySocket(NewModule->GetPath(), Socket);
			// }
		}
	}

	Notify(EModularRigNotification::ModuleAdded, NewModule);
	
	return true;
}

FRigModuleReference* UModularRigController::FindModule(const FString& InPath)
{
	FString Left = InPath, Right;
	TArray<FString> Tokens;
	while (Left.Split(UModularRig::NamespaceSeparator, &Left, &Right))
	{
		Tokens.Add(Left);
		Left = Right;
	}
	Tokens.Add(Left);

	FRigModuleReference* CurModule = nullptr;
	for (FRigModuleReference* Module : Model->RootModules)
	{
		if (Module->Name.ToString() == Tokens[0])
		{
			CurModule = Module;
			break;
		}
	}
	if (!CurModule)
	{
		return nullptr;
	}

	for (int32 Index=1; Index<Tokens.Num(); ++Index)
	{
		bool bFound = false;
		for (FRigModuleReference* Child : CurModule->CachedChildren)
		{
			if (Child->Name.ToString() == Tokens[Index])
			{
				CurModule = Child;
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			return nullptr;
		}
	}

	return CurModule;
}

bool UModularRigController::ConnectModuleToElement(const FRigElementKey& InConnectorKey, const FRigElementKey& InTargetKey)
{
	FString ConnectorNameSpace, ConnectorName;
	if (!InConnectorKey.Name.ToString().Split(UModularRig::NamespaceSeparator, &ConnectorNameSpace, &ConnectorName, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
	{
		return false;
	}
	
	FRigModuleReference* Module = FindModule(ConnectorNameSpace);
	if (!Module)
	{
		return false;
	}

	UControlRigBlueprint* Blueprint = Cast<UControlRigBlueprint>(GetOuter());
	FRigConnectorElement* Connector = Cast<FRigConnectorElement>(Blueprint->Hierarchy->Find(InConnectorKey));
	if (!Connector)
	{
		return false;
	}

	if (!Blueprint->Hierarchy->Contains(InTargetKey))
	{
		return false;
	}

	const FRigElementKey ConnectorKey(*ConnectorName, ERigElementType::Connector);
	FRigElementKey& TargetKey = Module->Connections.FindOrAdd(ConnectorKey);
	TargetKey = InTargetKey;

	Notify(EModularRigNotification::ConnectionChanged, Module);
	
	return true;
}

bool UModularRigController::RemoveModule(const FString& InModulesPath)
{
	// todo: UE-199050
	return false;
}

bool UModularRigController::RenameModule(const FString& InModulesPath, const FName& InNewName)
{
	// todo: UE-199050
	return false;
}

bool UModularRigController::ReparentModule(const FString& InModulesPath, const FString& InNewParentModulePath)
{
	// todo: UE-199050
	return false;
}

void UModularRigController::Notify(const EModularRigNotification& InNotification, const FRigModuleReference* InElement)
{
	if(!bSuspendNotifications)
	{
		ModifiedEvent.Broadcast(InNotification, InElement);
	}
}
