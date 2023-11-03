// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModularRig.h"
#include "Units/Execution/RigUnit_BeginExecution.h"
#include "Units/Execution/RigUnit_InverseExecution.h"
#include "Units/Execution/RigUnit_PrepareForExecution.h"
#include "Units/Execution/RigUnit_InteractionExecution.h"
#include "ControlRigObjectBinding.h"
#include "Rigs/RigHierarchyController.h"
#include "ControlRigComponent.h"

#define LOCTEXT_NAMESPACE "ModularRig"

const FString UModularRig::NamespaceSeparator = TEXT(":");

FRigModuleInstance::~FRigModuleInstance()
{
	CachedChildren.Reset();
	if (Rig && Rig.IsValid())
	{
		static int32 ObjectIndexToBeDestroyed = 0;
		static constexpr TCHAR ObjectNameFormat[] = TEXT("FRigModuleInstance_ObjectToBeDestroyed_%d");
		const FString NewObjectName = FString::Printf(ObjectNameFormat, ObjectIndexToBeDestroyed++);
		Rig->Rename(*NewObjectName, GetTransientPackage(), REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
		if(!Rig->IsRooted())
		{
			Rig->MarkAsGarbage();
		}
		Rig = nullptr;
	}
}

UModularRig::UModularRig(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITOR
	FCoreUObjectDelegates::OnObjectsReplaced.AddUObject(this, &UModularRig::OnObjectsReplaced);
#endif
}

void UModularRig::BeginDestroy()
{
	Super::BeginDestroy();
	
#if WITH_EDITOR
	FCoreUObjectDelegates::OnObjectsReplaced.RemoveAll(this);
#endif
}

void UModularRig::InitializeVMs(bool bRequestInit)
{
	URigVMHost::Initialize(bRequestInit);
	ForEachModule([bRequestInit](FRigModuleInstance* Module) -> bool
	{
		if (Module->Rig.IsValid())
		{
			Module->Rig->InitializeVMs(bRequestInit);
		}
		return true;
	});
}

bool UModularRig::InitializeVMs(const FName& InEventName)
{
	URigVMHost::InitializeVM(InEventName);
	ForEachModule([InEventName](FRigModuleInstance* Module) -> bool
	{
		if (Module->Rig.IsValid())
		{
			Module->Rig->InitializeVMs(InEventName);
		}
		return true;
	});
	return true;
}

bool UModularRig::Execute_Internal(const FName& InEventName)
{
	if (VM)
	{
		FRigVMExtendedExecuteContext& Context = GetRigVMExtendedExecuteContext();
		URigHierarchy* Hierarchy = GetHierarchy();
#if WITH_EDITOR

		bool bRecordTransformsAtRuntime = true;
		if(const UObject* Outer = GetOuter())
		{
			if(Outer->IsA<UControlRigComponent>())
			{
				bRecordTransformsAtRuntime = false;
			}
		}
		TGuardValue<bool> RecordTransformsPerInstructionGuard(Hierarchy->bRecordTransformsAtRuntime, bRecordTransformsAtRuntime);
		
		if(Hierarchy->bRecordTransformsAtRuntime)
		{
			Hierarchy->ReadTransformsAtRuntime.Reset();
			Hierarchy->WrittenTransformsAtRuntime.Reset();
		}
		
#endif
		
		ForEachModule([&InEventName, this, Hierarchy](FRigModuleInstance* Module) -> bool
		{
			if (Module->Rig.IsValid())
			{
				UControlRig* Rig = Module->Rig.Get();
				
				// Make sure the hierarchy has the correct element redirector from this module rig
				FRigHierarchyRedirectorGuard ElementRedirectorGuard(Rig);

				// Make sure the hierarchy has the correct execute context with the rig module namespace
				FRigHierarchyExecuteContextBracket ExecuteContextBracket(Hierarchy, &Rig->GetRigVMExtendedExecuteContext());
				
				Rig->Execute_Internal(InEventName);
			}
			return true;
		});

		return true;
	}
	return false;
}

void UModularRig::OnObjectsReplaced(const TMap<UObject*, UObject*>& OldToNewInstanceMap)
{
	for (FRigModuleInstance& ModuleInstance : Modules)
	{
		if (UObject*const * NewObject = OldToNewInstanceMap.Find(ModuleInstance.Rig.Get()))
		{
			ModuleInstance.Rig = Cast<UControlRig>(*NewObject);
		}
	}
	InitializeVMs(true);
}

void UModularRig::ResetModules()
{
	RootModules.Reset();
	Modules.Reset();
}

bool UModularRig::AddModuleInstance(const FName& InModuleName, TSubclassOf<UControlRig> InModuleClass, FString InParentPath,
	const TMap<FRigElementKey, FRigElementKey>& InConnectionMap, const TMap<FName, FString>& InVariableDefaultValues )
{
	FRigModuleInstance* ParentModule = FindModule(InParentPath);
	return AddModuleInstance(InModuleName, InModuleClass, ParentModule, InConnectionMap, InVariableDefaultValues ) != nullptr;
}

FRigModuleInstance* UModularRig::AddModuleInstance(const FName& InModuleName, TSubclassOf<UControlRig> InModuleClass, FRigModuleInstance* InParent,
	const TMap<FRigElementKey, FRigElementKey>& InConnectionMap, const TMap<FName, FString>& InVariableDefaultValues ) 
{
	// Make sure there are no name clashes
	if (InParent)
	{
		for (FRigModuleInstance* Child : InParent->CachedChildren)
		{
			if (Child->Name == InModuleName)
			{
				return nullptr;
			}
		}
	}
	else
	{
		for (FRigModuleInstance* RootModule : RootModules)
		{
			if (RootModule->Name == InModuleName)
			{
				return nullptr;
			}
		}
	}

	// For now, lets only allow rig modules
	if (!InModuleClass->GetDefaultObject<UControlRig>()->IsRigModule())
	{
		return nullptr;
	}

	FString Name = (InParent) ? InParent->Name.ToString() + NamespaceSeparator + InModuleName.ToString() : InModuleName.ToString();
	FRigModuleInstance& NewModule = Modules.Add_GetRef(FRigModuleInstance());
	NewModule.Rig = NewObject<UControlRig>(this, InModuleClass, *Name);
	NewModule.Name = InModuleName;

	if (InParent)
	{
		InParent->CachedChildren.Add(&NewModule);
		NewModule.ParentPath = InParent->ParentPath.IsEmpty() ? InParent->Name.ToString() : InParent->ParentPath + NamespaceSeparator + InParent->Name.ToString();
	}
	else
	{
		RootModules.Add(&NewModule);
	}

	// Configure module
	{
		URigHierarchy* Hierarchy = GetHierarchy();
		FRigVMExtendedExecuteContext& ModuleContext = NewModule.Rig->GetRigVMExtendedExecuteContext();
		FControlRigExecuteContext& ModulePublicContext = ModuleContext.GetPublicDataSafe<FControlRigExecuteContext>();
		NewModule.Rig->bCopyHierarchyBeforeConstruction = false;
		NewModule.Rig->SetDynamicHierarchy(Hierarchy);
		ModulePublicContext.Hierarchy = Hierarchy;
		ModulePublicContext.RigModuleNameSpace = NewModule.Rig->GetRigModuleNameSpace();
		ModulePublicContext.RigModuleNameSpaceHash = GetTypeHash(ModulePublicContext.RigModuleNameSpace);
		NewModule.Rig->SetElementKeyRedirector(FRigElementKeyRedirector(InConnectionMap, Hierarchy));

		for (TPair<FName, FString> Variable : InVariableDefaultValues )
		{
			NewModule.Rig->SetVariableFromString(Variable.Key, Variable.Value);
		}
	}
	
	return &NewModule;
}

FRigModuleInstance* UModularRig::FindModule(const FString& InPath)
{
	TArray<FRigModuleInstance*>* Children = &RootModules;
	FString Left = InPath, Right;
	while (Left.Split(NamespaceSeparator, &Left, &Right))
	{
		FRigModuleInstance** Cur = Children->FindByPredicate([Left](FRigModuleInstance* Module)
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

	FRigModuleInstance** Cur = Children->FindByPredicate([Left](FRigModuleInstance* Module)
		{
			return Module->Name == Left;
		});
	if (!Cur)
	{
		return nullptr;
	}
	return *Cur;
}

void UModularRig::ForEachModule(TFunctionRef<bool(FRigModuleInstance*)> PerModuleFunction)
{
	TArray<FRigModuleInstance*> ModuleInstances = RootModules;
	for (int32 ModuleIndex = 0; ModuleIndex < ModuleInstances.Num(); ++ModuleIndex)
	{
		if (!PerModuleFunction(ModuleInstances[ModuleIndex]))
		{
			break;
		}
		ModuleInstances.Append(ModuleInstances[ModuleIndex]->CachedChildren);
	}
}

#undef LOCTEXT_NAMESPACE
