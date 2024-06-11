// Copyright Epic Games, Inc. All Rights Reserved.

#include "Module/AnimNextModule_EditorData.h"

#include "UncookedOnlyUtils.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Curves/CurveFloat.h"
#include "Module/AnimNextModule.h"
#include "Graph/AnimNextModule_AnimationGraph.h"
#include "AnimNextEdGraphSchema.h"
#include "Module/AnimNextModule_EventGraph.h"
#include "AnimNextEventGraphSchema.h"
#include "Module/AnimNextModule_Parameter.h"
#include "Graph/AnimNextAnimationGraphSchema.h"
#include "RigVMModel/RigVMFunctionLibrary.h"
#include "RigVMModel/RigVMNotifications.h"
#include "RigVMModel/Nodes/RigVMCollapseNode.h"
#include "UObject/AssetRegistryTagsContext.h"
#include "UObject/FortniteMainBranchObjectVersion.h"
#include "UObject/LinkerLoad.h"

void UAnimNextModule_EditorData::PostLoad()
{
	Super::PostLoad();

	auto FindEntryForRigVMGraph = [this](URigVMGraph* InRigVMGraph)
	{
		UAnimNextRigVMAssetEntry* FoundEntry = nullptr;
		for(UAnimNextRigVMAssetEntry* Entry : Entries)
		{
			if(IAnimNextRigVMGraphInterface* GraphEntry = Cast<IAnimNextRigVMGraphInterface>(Entry))
			{
				if(InRigVMGraph == GraphEntry->GetRigVMGraph())
				{
					FoundEntry = Entry;
					break;
				}
			}
		}
		return FoundEntry;
	};

	if(GetLinkerCustomVersion(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::AnimNextCombineGraphContexts)
	{
		// Must preload entries so their data is populated or we cannot find the appropriate entries for graphs
		for(UAnimNextRigVMAssetEntry* Entry : Entries) 
		{
			Entry->GetLinker()->Preload(Entry);
		}

		TArray<URigVMGraph*> AllModels = RigVMClient.GetAllModels(false, true);
		for(URigVMGraph* Graph : AllModels)
		{
			Graph->SetExecuteContextStruct(FAnimNextExecuteContext::StaticStruct());
			if(UAnimNextRigVMAssetEntry* FoundEntry = FindEntryForRigVMGraph(Graph))
			{
				if(FoundEntry->IsA(UAnimNextModule_AnimationGraph::StaticClass()))
				{
					Graph->SetSchemaClass(UAnimNextAnimationGraphSchema::StaticClass());
				}
				else
				{
					Graph->SetSchemaClass(UAnimNextEventGraphSchema::StaticClass());
				}
			}
			else
			{
				Graph->SetSchemaClass(UAnimNextAnimationGraphSchema::StaticClass());
			}
		}
	}

	if(GetLinkerCustomVersion(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::AnimNextMoveGraphsToEntries)
	{
		// Must preload entries so their data is populated or we cannot find the appropriate entries for graphs
		for(UAnimNextRigVMAssetEntry* Entry : Entries) 
		{
			Entry->GetLinker()->Preload(Entry);
		}
		
		for(TObjectPtr<UAnimNextEdGraph> Graph : Graphs_DEPRECATED)
		{
			URigVMGraph* FoundRigVMGraph = GetRigVMGraphForEditorObject(Graph);
			if(FoundRigVMGraph)
			{
				if(UAnimNextRigVMAssetEntry* FoundEntry = FindEntryForRigVMGraph(FoundRigVMGraph))
				{
					if(UAnimNextModule_AnimationGraph* AnimationGraphEntry = Cast<UAnimNextModule_AnimationGraph>(FoundEntry))
					{
						AnimationGraphEntry->EdGraph = Graph;
					}
					else if(UAnimNextModule_EventGraph* EventGraphEntry = Cast<UAnimNextModule_EventGraph>(FoundEntry))
					{
						EventGraphEntry->EdGraph = Graph;
					}

					Graph->Rename(nullptr, FoundEntry, REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
					Graph->Initialize(this);
				}
			}
		}

		// We used to add a default model that is no longer needed
		URigVMGraph* DefaultModel = RigVMClient.GetDefaultModel();
		if(DefaultModel && DefaultModel->GetName() == TEXT("RigVMGraph"))
		{
			bool bFound = false;
			for(UAnimNextRigVMAssetEntry* Entry : Entries)
			{
				if(UAnimNextModule_EventGraph* EventGraphEntry = Cast<UAnimNextModule_EventGraph>(Entry))
				{
					if(DefaultModel == static_cast<IAnimNextRigVMGraphInterface*>(EventGraphEntry)->GetRigVMGraph())
					{
						bFound = true;
						break;
					}
				}
			}

			if(!bFound)
			{
				TGuardValue<bool> DisablePythonPrint(bSuspendPythonMessagesForRigVMClient, false);
				TGuardValue<bool> DisableAutoCompile(bAutoRecompileVM, false);
				RigVMClient.RemoveModel(DefaultModel->GetNodePath(), false);
			}
		}

		RecompileVM();
	}

	if(GetLinkerCustomVersion(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::AnimNextGraphAccessSpecifiers)
	{
		// Must preload entries so their data is populated as we will be modifying them
		for(UAnimNextRigVMAssetEntry* Entry : Entries) 
		{
			Entry->GetLinker()->Preload(Entry);
		}

		// Force older assets to all have public symbols so they work as-is. Newer assets need user intervention as entries default to private
		for(UAnimNextRigVMAssetEntry* Entry : Entries)
		{
			if(UAnimNextModule_AnimationGraph* AnimationGraphEntry = Cast<UAnimNextModule_AnimationGraph>(Entry))
			{
				AnimationGraphEntry->Access = EAnimNextExportAccessSpecifier::Public;
			}
			else if(UAnimNextModule_Parameter* ParameterEntry = Cast<UAnimNextModule_Parameter>(Entry))
			{
				ParameterEntry->Access = EAnimNextExportAccessSpecifier::Public;
			}
		}
	}
}

void UAnimNextModule_EditorData::RecompileVM()
{
	CachedExports = FAnimNextParameterProviderAssetRegistryExports();
	UE::AnimNext::UncookedOnly::FUtils::GetAssetParameters(this, CachedExports.GetValue());
	UE::AnimNext::UncookedOnly::FUtils::Compile(GetTypedOuter<UAnimNextModule>());
	
	if (IAssetRegistry* AssetRegistry = IAssetRegistry::Get())
	{
		AssetRegistry->AssetUpdateTags(GetTypedOuter<UAnimNextModule>(), EAssetRegistryTagsCaller::Fast);
	}
}

void UAnimNextModule_EditorData::HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject)
{
	switch(InNotifType)
	{
	case ERigVMGraphNotifType::PinAdded:
		{
			if (URigVMPin* Pin = Cast<URigVMPin>(InSubject))
			{
				if (Pin->IsTraitPin())
				{
					RequestAutoVMRecompilation();
				}
			}
			break;
		}
	}

	Super::HandleModifiedEvent(InNotifType, InGraph, InSubject);
}

TConstArrayView<TSubclassOf<UAnimNextRigVMAssetEntry>> UAnimNextModule_EditorData::GetEntryClasses() const
{
	static const TSubclassOf<UAnimNextRigVMAssetEntry> Classes[] =
	{
		UAnimNextModule_AnimationGraph::StaticClass(),
		UAnimNextModule_EventGraph::StaticClass(),
		UAnimNextModule_Parameter::StaticClass(),
	};
	
	return Classes;
}

void UAnimNextModule_EditorData::CreateEdGraphForCollapseNode(URigVMCollapseNode* InNode)
{
	if (InNode->GetGraph()->IsA<URigVMFunctionLibrary>())
	{
		if (URigVMGraph* ContainedGraph = InNode->GetContainedGraph())
		{
			// create a sub graph
			UAnimNextEdGraph* RigFunctionGraph = NewObject<UAnimNextEdGraph>(this, *InNode->GetName(), RF_Transactional);
			RigFunctionGraph->Schema = UAnimNextEdGraphSchema::StaticClass();
			RigFunctionGraph->bAllowRenaming = true;
			RigFunctionGraph->bEditable = true;
			RigFunctionGraph->bAllowDeletion = true;
			RigFunctionGraph->ModelNodePath = ContainedGraph->GetNodePath();
			RigFunctionGraph->bIsFunctionDefinition = true;

			RigFunctionGraph->Initialize(this);

			RigVMClient.GetOrCreateController(ContainedGraph)->ResendAllNotifications();
		}
	}
}

UEdGraph* UAnimNextModule_EditorData::CreateEdGraph(URigVMGraph* InRigVMGraph, bool bForce)
{
	check(InRigVMGraph);

	if(InRigVMGraph->IsA<URigVMFunctionLibrary>())
	{
		return nullptr;
	}

	IAnimNextRigVMGraphInterface* Entry = Cast<IAnimNextRigVMGraphInterface>(FindEntryForRigVMGraph(InRigVMGraph));
	if(Entry == nullptr)
	{
		// Not found, we could be adding a new entry, in which case the graph wont be assigned yet
		check(Entries.Num() > 0);
		check(Cast<IAnimNextRigVMGraphInterface>(Entries.Last()) != nullptr);
		check(Cast<IAnimNextRigVMGraphInterface>(Entries.Last())->GetRigVMGraph() == nullptr);
		Entry = Cast<IAnimNextRigVMGraphInterface>(FindEntryForRigVMGraph(nullptr));
	}

	if(Entry == nullptr)
	{
		return nullptr;
	}
	
	if(bForce)
	{
		RemoveEdGraph(InRigVMGraph);
	}

	FString GraphName = InRigVMGraph->GetName();
	check(!GraphName.IsEmpty());

	UAnimNextEdGraph* RigFunctionGraph = NewObject<UAnimNextEdGraph>(CastChecked<UObject>(Entry), NAME_None, RF_Transactional);
	RigFunctionGraph->Schema = UAnimNextEdGraphSchema::StaticClass();
	RigFunctionGraph->bAllowDeletion = true;
	RigFunctionGraph->bIsFunctionDefinition = false;
	RigFunctionGraph->ModelNodePath = InRigVMGraph->GetNodePath();
	RigFunctionGraph->Initialize(this);

	Entry->SetEdGraph(RigFunctionGraph);
	if(Entry->GetRigVMGraph() == nullptr)
	{
		Entry->SetRigVMGraph(InRigVMGraph);
	}
	else
	{
		check(Entry->GetRigVMGraph() == InRigVMGraph);
	}

	return RigFunctionGraph;
}

bool UAnimNextModule_EditorData::RemoveEdGraph(URigVMGraph* InModel)
{
	if(UAnimNextModule_AnimationGraph* Entry = Cast<UAnimNextModule_AnimationGraph>(FindEntryForRigVMGraph(InModel)))
	{
		RigVMClient.DestroyObject(Entry->EdGraph);
		Entry->EdGraph = nullptr;
		return true;
	}
	return false;
}

UAnimNextModule_Parameter* UAnimNextModuleLibrary::AddParameter(UAnimNextModule* InModule, FName InName, EPropertyBagPropertyType InValueType,
	EPropertyBagContainerType InContainerType, const UObject* InValueTypeObject, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	return UE::AnimNext::UncookedOnly::FUtils::GetEditorData(InModule)->AddParameter(InName, FAnimNextParamType(InValueType, InContainerType, InValueTypeObject), bSetupUndoRedo, bPrintPythonCommand);
}

UAnimNextModule_Parameter* UAnimNextModule_EditorData::AddParameter(FName InName, FAnimNextParamType InType, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	if(InName == NAME_None)
	{
		ReportError(TEXT("UAnimNextModule_EditorData::AddParameter: Invalid parameter name supplied."));
		return nullptr;
	}

	// Check for duplicate parameter
	const bool bAlreadyExists = Entries.ContainsByPredicate([InName](const UAnimNextRigVMAssetEntry* InEntry)
	{
		if(const UAnimNextModule_Parameter* Parameter = Cast<UAnimNextModule_Parameter>(InEntry))
		{
			return Parameter->ParameterName == InName;
		}
		return false;
	});

	if(bAlreadyExists)
	{
		ReportError(TEXT("UAnimNextModule_EditorData::AddParameter: A parameter already exists for the supplied parameter name."));
		return nullptr;
	}

	UAnimNextModule_Parameter* NewEntry = CreateNewSubEntry<UAnimNextModule_Parameter>(this);
	NewEntry->ParameterName = InName;
	NewEntry->Type = InType;

	if(bSetupUndoRedo)
	{
		NewEntry->Modify();
		Modify();
	}
	
	Entries.Add(NewEntry);

	BroadcastModified();

	return NewEntry;
}

UAnimNextModule_EventGraph* UAnimNextModuleLibrary::AddEventGraph(UAnimNextModule* InModule, FName InName, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	return UE::AnimNext::UncookedOnly::FUtils::GetEditorData(InModule)->AddEventGraph(InName, bSetupUndoRedo, bPrintPythonCommand);
}

UAnimNextModule_EventGraph* UAnimNextModule_EditorData::AddEventGraph(FName InName, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	if(InName == NAME_None)
	{
		ReportError(TEXT("UAnimNextModule_EditorData::AddEventGraph: Invalid graph name supplied."));
		return nullptr;
	}

	// Check for duplicate name
	FName NewGraphName = InName;
	auto DuplicateNamePredicate = [&NewGraphName](const UAnimNextRigVMAssetEntry* InEntry)
	{
		return InEntry->GetEntryName() == NewGraphName;
	};

	bool bAlreadyExists = Entries.ContainsByPredicate(DuplicateNamePredicate);
	int32 NameNumber = InName.GetNumber() + 1;
	while(bAlreadyExists)
	{
		NewGraphName = FName(InName, NameNumber++);
		bAlreadyExists =  Entries.ContainsByPredicate(DuplicateNamePredicate);
	}

	UAnimNextModule_EventGraph* NewEntry = CreateNewSubEntry<UAnimNextModule_EventGraph>(this);
	NewEntry->GraphName = NewGraphName;

	if(bSetupUndoRedo)
	{
		NewEntry->Modify();
		Modify();
	}

	Entries.Add(NewEntry);

	// Add new graph
	{
		TGuardValue<bool> EnablePythonPrint(bSuspendPythonMessagesForRigVMClient, !bPrintPythonCommand);
		TGuardValue<bool> DisableAutoCompile(bAutoRecompileVM, false);
		URigVMGraph* NewGraph = RigVMClient.AddModel(URigVMGraph::StaticClass()->GetFName(), UAnimNextEventGraphSchema::StaticClass(), bSetupUndoRedo);
		ensure(NewGraph);
		NewEntry->Graph = NewGraph;

		URigVMController* Controller = RigVMClient.GetController(NewGraph);
		UE::AnimNext::UncookedOnly::FUtils::SetupEventGraph(Controller);
	}

	BroadcastModified();

	return NewEntry;
}

UAnimNextModule_AnimationGraph* UAnimNextModuleLibrary::AddAnimationGraph(UAnimNextModule* InModule, FName InName, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	return UE::AnimNext::UncookedOnly::FUtils::GetEditorData(InModule)->AddAnimationGraph(InName, bSetupUndoRedo, bPrintPythonCommand);
}

UAnimNextModule_AnimationGraph* UAnimNextModule_EditorData::AddAnimationGraph(FName InName, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	if(InName == NAME_None)
	{
		ReportError(TEXT("UAnimNextModule_EditorData::AddAnimationGraph: Invalid graph name supplied."));
		return nullptr;
	}

	// Check for duplicate name
	FName NewGraphName = InName;
	auto DuplicateNamePredicate = [&NewGraphName](const UAnimNextRigVMAssetEntry* InEntry)
	{
		return InEntry->GetEntryName() == NewGraphName;
	};

	bool bAlreadyExists = Entries.ContainsByPredicate(DuplicateNamePredicate);
	int32 NameNumber = InName.GetNumber() + 1;
	while(bAlreadyExists)
	{
		NewGraphName = FName(InName, NameNumber++);
		bAlreadyExists =  Entries.ContainsByPredicate(DuplicateNamePredicate);
	}

	UAnimNextModule_AnimationGraph* NewEntry = CreateNewSubEntry<UAnimNextModule_AnimationGraph>(this);
	NewEntry->GraphName = NewGraphName;

	if(bSetupUndoRedo)
	{
		NewEntry->Modify();
		Modify();
	}
	
	Entries.Add(NewEntry);

	// Add new graph
	{
		TGuardValue<bool> EnablePythonPrint(bSuspendPythonMessagesForRigVMClient, !bPrintPythonCommand);
		TGuardValue<bool> DisableAutoCompile(bAutoRecompileVM, false);
		URigVMGraph* NewGraph = RigVMClient.AddModel(URigVMGraph::StaticClass()->GetFName(), UAnimNextAnimationGraphSchema::StaticClass(), bSetupUndoRedo);
		ensure(NewGraph);
		NewEntry->Graph = NewGraph;

		URigVMController* Controller = RigVMClient.GetController(NewGraph);
		UE::AnimNext::UncookedOnly::FUtils::SetupAnimGraph(NewEntry, Controller);
	}

	BroadcastModified();

	return NewEntry;
}