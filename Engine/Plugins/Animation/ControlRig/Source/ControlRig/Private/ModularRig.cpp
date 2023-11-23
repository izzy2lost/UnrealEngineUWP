// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModularRig.h"
#include "Units/Execution/RigUnit_BeginExecution.h"
#include "Units/Execution/RigUnit_InverseExecution.h"
#include "Units/Execution/RigUnit_PrepareForExecution.h"
#include "Units/Execution/RigUnit_InteractionExecution.h"
#include "ControlRigObjectBinding.h"
#include "Rigs/RigHierarchyController.h"
#include "ControlRigComponent.h"
#include "RigVMCore/RigVMExecuteContext.h"

#define LOCTEXT_NAMESPACE "ModularRig"

const FString UModularRig::NamespaceSeparator = TEXT(":");

////////////////////////////////////////////////////////////////////////////////
// FModuleInstanceHandle
////////////////////////////////////////////////////////////////////////////////

FModuleInstanceHandle::FModuleInstanceHandle(UModularRig* InModularRig, const FString& InPath)
: ModularRig(InModularRig)
, Path(InPath)
{
}

FModuleInstanceHandle::FModuleInstanceHandle(UModularRig* InModularRig, const FRigModuleInstance* InModule)
: ModularRig(InModularRig)
, Path(InModule->GetPath())
{
}

const FRigModuleInstance* FModuleInstanceHandle::Get() const
{
	if(ModularRig.IsValid())
	{
		return ModularRig->FindModule(Path);
	}
	return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// UModularRig
////////////////////////////////////////////////////////////////////////////////

UModularRig::UModularRig(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITOR
	FCoreUObjectDelegates::OnObjectsReplaced.AddUObject(this, &UModularRig::OnObjectsReplaced);
#endif
}

void UModularRig::BeginDestroy()
{
	ResetModules();
	
	Super::BeginDestroy();
	
#if WITH_EDITOR
	FCoreUObjectDelegates::OnObjectsReplaced.RemoveAll(this);
#endif
}

FString FRigModuleInstance::GetPath() const
{
	if (!ParentPath.IsEmpty())
	{
		return FString::Printf(TEXT("%s:%s"), *ParentPath, *Name.ToString()); 
	}
	return Name.ToString();
}

FString FRigModuleInstance::GetNamespace() const
{
	return FString::Printf(TEXT("%s:"), *GetPath());
}

void UModularRig::Initialize(bool bRequestInit)
{
	Super::Initialize(bRequestInit);

	UpdateCachedChildren();
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

void UModularRig::InitializeFromCDO()
{
	Super::InitializeFromCDO();

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		const UModularRig* CDO = GetClass()->GetDefaultObject<UModularRig>();

		// Generate the rig module tree based on the CDO
		ResetModules();
		CDO->ForEachModule([this](const FRigModuleInstance* CDOModule) -> bool
		{
			if (FRigModuleInstance* NewModule = AddModuleInstance(CDOModule))
			{
				NewModule->Rig->Initialize();
			}
			return true;
		});

		SupportedEvents = CDO->SupportedEvents;
	}
}

bool UModularRig::Execute_Internal(const FName& InEventName)
{
	if (VM)
	{
		FRigVMExtendedExecuteContext& ModularRigContext = GetRigVMExtendedExecuteContext();
		const FControlRigExecuteContext& PublicContext = ModularRigContext.GetPublicDataSafe<FControlRigExecuteContext>();
		const FRigUnitContext& UnitContext = PublicContext.UnitContext;
		const URigHierarchy* Hierarchy = GetHierarchy();

		ForEachModule([&InEventName, this, Hierarchy, UnitContext](FRigModuleInstance* Module) -> bool
		{
			if (Module->Rig.IsValid())
			{
				UControlRig* Rig = Module->Rig.Get();

				if (!Rig->SupportsEvent(InEventName))
				{
					return true;
				}

				// Only emit interaction event on this module if any of the interaction elements
				// belong to the module's namespace
				if (InEventName == FRigUnit_InteractionExecution::EventName)
				{
					const FString ModuleNamespace = Module->GetNamespace();
					const bool bIsInteracting = UnitContext.ElementsBeingInteracted.ContainsByPredicate(
						[ModuleNamespace, Hierarchy](const FRigElementKey& InteractionElement)
						{
							return ModuleNamespace == Hierarchy->GetNameMetadata(InteractionElement, URigHierarchy::NameSpaceMetadataName, NAME_None);
						});
					if (!bIsInteracting)
					{
						return true;
					}
				}

				ExecutionQueue.Add(FRigModuleExecutionElement(Module, InEventName));
			}
			return true;
		});

		ExecuteQueue();
		return true;
	}
	return false;
}

void UModularRig::Evaluate_AnyThread()
{
	ResetExecutionQueue();
	Super::Evaluate_AnyThread();
}

void UModularRig::ExecuteQueue()
{
	FRigVMExtendedExecuteContext& Context = GetRigVMExtendedExecuteContext();
	URigHierarchy* Hierarchy = GetHierarchy();
	
	while(ExecutionQueue.IsValidIndex(ExecutionQueueFront))
	{
		FRigModuleExecutionElement& ExecutionElement = ExecutionQueue[ExecutionQueueFront];
		if (ExecutionElement.ModuleInstance->Rig.IsValid())
		{
			UControlRig* Rig = ExecutionElement.ModuleInstance->Rig.Get();

			if (!Rig->SupportsEvent(ExecutionElement.EventName))
			{
				ExecutionQueueFront++;
				continue;
			}
				
			// Make sure the hierarchy has the correct element redirector from this module rig
			FRigHierarchyRedirectorGuard ElementRedirectorGuard(Rig);

			FRigVMExtendedExecuteContext& RigExtendedExecuteContext= Rig->GetRigVMExtendedExecuteContext();

			// Make sure the hierarchy has the correct execute context with the rig module namespace
			FRigHierarchyExecuteContextBracket ExecuteContextBracket(Hierarchy, &RigExtendedExecuteContext);

			FControlRigExecuteContext& PublicContext = Context.GetPublicDataSafe<FControlRigExecuteContext>();
			FControlRigExecuteContext& RigPublicContext = RigExtendedExecuteContext.GetPublicDataSafe<FControlRigExecuteContext>();
			FRigUnitContext& RigUnitContext = RigPublicContext.UnitContext;
			RigUnitContext = PublicContext.UnitContext;

			// Update the interaction elements to show only the ones belonging to this module
			const FString ModuleNamespace = FString::Printf(TEXT("%s:"), *ExecutionElement.ModulePath);
			RigUnitContext.ElementsBeingInteracted = RigUnitContext.ElementsBeingInteracted.FilterByPredicate(
				[ModuleNamespace, Hierarchy](const FRigElementKey& Key)
			{
				return ModuleNamespace == Hierarchy->GetNameMetadata(Key, URigHierarchy::NameSpaceMetadataName, NAME_None);
			});
			RigUnitContext.InteractionType = RigUnitContext.ElementsBeingInteracted.IsEmpty() ?
				(uint8) EControlRigInteractionType::None
				: RigUnitContext.InteractionType;

			// Make sure the module's rig has the corrct user data
			// The rig will combine the user data of the
			// - skeleton
			// - skeletalmesh
			// - SkeletalMeshComponent
			// - default control rig module
			// - outer modular rig
			// - external variables
			{
				RigPublicContext.AssetUserData.Reset();
				if(const TArray<UAssetUserData*>* ControlRigUserDataArray = Rig->GetAssetUserDataArray())
				{
					for(const UAssetUserData* ControlRigUserData : *ControlRigUserDataArray)
					{
						RigPublicContext.AssetUserData.Add(ControlRigUserData);
					}
				}
				RigPublicContext.AssetUserData.Remove(nullptr);
			}

			// Copy variable bindings
			for (TPair<FName, FRigVMExternalVariable>& Pair : ExecutionElement.ModuleInstance->VariableBindings)
			{
				const FRigVMExternalVariable TargetVariable = ExecutionElement.ModuleInstance->Rig->GetPublicVariableByName(Pair.Key);
				if(ensure(TargetVariable.Property))
				{
					if (RigVMTypeUtils::AreCompatible(Pair.Value.Property, TargetVariable.Property))
					{
						Pair.Value.Property->CopyCompleteValue(TargetVariable.Memory, Pair.Value.Memory);
					}
				}
			}
			
			Rig->Execute_Internal(ExecutionElement.EventName);
			ExecutionElement.bExecuted = true;
		}
		
		ExecutionQueueFront++;
	}
}

void UModularRig::ResetExecutionQueue()
{
	ExecutionQueue.Reset();
	ExecutionQueueFront = 0;
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
	UpdateSupportedEvents();
}

void UModularRig::ResetModules()
{
	for (FRigModuleInstance& Module : Modules)
	{
		Module.CachedChildren.Reset();
		TSoftObjectPtr<UControlRig> Rig = Module.Rig;
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
	
	RootModules.Reset();
	Modules.Reset();
	SupportedEvents.Reset();
}

void UModularRig::UpdateCachedChildren()
{
	TMap<FString, FRigModuleInstance*> PathToModule;
	for (FRigModuleInstance& Module : Modules)
	{
		Module.CachedChildren.Reset();
		PathToModule.Add(Module.GetPath(), &Module);
	}
	
	RootModules.Reset();
	for (FRigModuleInstance& Module : Modules)
	{
		if (Module.ParentPath.IsEmpty())
		{
			RootModules.Add(&Module);
		}
		else
		{
			if (FRigModuleInstance** ParentModule = PathToModule.Find(Module.ParentPath))
			{
				(*ParentModule)->CachedChildren.Add(&Module);
			}
		}
	}
}

void UModularRig::UpdateSupportedEvents()
{
	SupportedEvents.Reset();
	ForEachModule([this](const FRigModuleInstance* Module) -> bool
	{
		if (Module->Rig.IsValid())
		{
			const TArray<FName>& ModuleEvents = Module->Rig->GetSupportedEvents();
			for (const FName& EventName : ModuleEvents)
			{
				SupportedEvents.AddUnique(EventName);
			}
		}
		return true;
	});
}

bool UModularRig::AddModuleInstance(const FName& InModuleName, TSubclassOf<UControlRig> InModuleClass, FString InParentPath,
	const TMap<FRigElementKey, FRigElementKey>& InConnectionMap, const TMap<FName, FString>& InVariableDefaultValues, const TMap<FName, FString>& InVariableBindings )
{
	FRigModuleInstance* ParentModule = const_cast<FRigModuleInstance*>(FindModule(InParentPath));
	return AddModuleInstance(InModuleName, InModuleClass, ParentModule, InConnectionMap, InVariableDefaultValues, InVariableBindings) != nullptr;
}

FRigModuleInstance* UModularRig::AddModuleInstance(const FName& InModuleName, TSubclassOf<UControlRig> InModuleClass, FRigModuleInstance* InParent,
	const TMap<FRigElementKey, FRigElementKey>& InConnectionMap, const TMap<FName, FString>& InVariableDefaultValues, const TMap<FName, FString>& InVariableBindings ) 
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
		NewModule.ParentPath = InParent->GetPath();
	}
	UpdateCachedChildren();
	for (const FName& EventName : NewModule.Rig->GetSupportedEvents())
	{
		SupportedEvents.AddUnique(EventName);
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

		for (const TPair<FName, FString>& Variable : InVariableDefaultValues )
		{
			NewModule.Rig->SetVariableFromString(Variable.Key, Variable.Value);
		}

		for (const TPair<FName, FString>& Pair : InVariableBindings)
		{
			FString SourceModulePath, SourceVariableName = Pair.Value;
			Pair.Value.Split(NamespaceSeparator, &SourceModulePath, &SourceVariableName, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
			FRigVMExternalVariable SourceVariable;
			if (SourceModulePath.IsEmpty())
			{
				if (FProperty* Property = GetClass()->FindPropertyByName(*SourceVariableName))
				{
					SourceVariable = FRigVMExternalVariable::Make(Property, (UObject*)this);
				}
			}
			else if(const FRigModuleInstance* SourceModule = FindModule(SourceModulePath))
			{
				SourceVariable = SourceModule->Rig->GetPublicVariableByName(*SourceVariableName);
			}
			SourceVariable.Name = *Pair.Value; // Adapt the name of the variable to contain the full path
			check(SourceVariable.Property);
			NewModule.VariableBindings.Add(Pair.Key, SourceVariable);
		}
	}
	
	return &NewModule;
}

FRigModuleInstance* UModularRig::AddModuleInstance(const FRigModuleInstance* InOtherModule)
{
	if (!InOtherModule->Rig.IsValid())
	{
		return nullptr;
	}

	// Figure out the ConnectionMap
	const FRigElementKeyRedirector& Redirector = InOtherModule->Rig->GetElementKeyRedirector();
	const TMap<FRigElementKey, FRigElementKey> ConnectionMap = Redirector.ExternalKeys;

	// Figure out the DefaultValues (which is the current value on the CDO)
	TMap<FName, FString> DefaultValues;
	const TArray<FRigVMExternalVariable> Variables = InOtherModule->Rig->GetPublicVariables();
	for (const FRigVMExternalVariable& Variable : Variables)
	{
		const FString Value = InOtherModule->Rig->GetVariableAsString(Variable.Name);
		DefaultValues.Add(Variable.Name, Value);
	}

	// Figure out the variable bindings (where the source is either a root variable or a variable on another module)
	TMap<FName, FString> VariableBindings;
	for (const TPair<FName, FRigVMExternalVariable>& Pair : InOtherModule->VariableBindings)
	{
		VariableBindings.Add(Pair.Key, Pair.Value.Name.ToString());
	}

	// Figure out the parent module
	FRigModuleInstance* ParentModule = const_cast<FRigModuleInstance*>(FindModule(InOtherModule->ParentPath));
	
	return AddModuleInstance(InOtherModule->Name, InOtherModule->Rig->GetClass(), ParentModule, ConnectionMap, DefaultValues, VariableBindings);
}

const FRigModuleInstance* UModularRig::FindModule(const FString& InPath) const
{
	const TArray<FRigModuleInstance*>* Children = &RootModules;
	FString Left = InPath, Right;
	while (Left.Split(NamespaceSeparator, &Left, &Right))
	{
		FRigModuleInstance* const * Cur = Children->FindByPredicate([Left](FRigModuleInstance* Module)
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

	FRigModuleInstance* const * Cur = Children->FindByPredicate([Left](FRigModuleInstance* Module)
		{
			return Module->Name == Left;
		});
	if (!Cur)
	{
		return nullptr;
	}
	return *Cur;
}

const FRigModuleInstance* UModularRig::FindModule(const UControlRig* InModuleInstance) const
{
	const FRigModuleInstance* FoundModule = nullptr;
	ForEachModule([InModuleInstance, &FoundModule](const FRigModuleInstance* Module) -> bool
	{
		if (Module->Rig.IsValid())
		{
			if(Module->Rig.Get() == InModuleInstance)
			{
				FoundModule = Module;
				// don't continue
				return false;
			}
		}
		return true;
	});

	return FoundModule;
}

FString UModularRig::GetParentPath(const FString& InPath) const
{
	if (const FRigModuleInstance* Element = FindModule(InPath))
	{
		return Element->ParentPath;
	}
	return FString();
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

void UModularRig::ForEachModule(TFunctionRef<bool(const FRigModuleInstance*)> PerModuleFunction) const
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
