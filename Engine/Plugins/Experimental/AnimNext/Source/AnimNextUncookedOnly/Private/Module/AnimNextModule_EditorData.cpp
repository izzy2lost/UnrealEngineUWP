// Copyright Epic Games, Inc. All Rights Reserved.

#include "Module/AnimNextModule_EditorData.h"

#include "ExternalPackageHelper.h"
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
#include "RigVMModel/Nodes/RigVMAggregateNode.h"
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

					Graph->Rename(nullptr, FoundEntry, REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
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

void UAnimNextModule_EditorData::CreateEdGraphForCollapseNode(URigVMCollapseNode* InNode, bool bForce)
{
	check(InNode);
	URigVMGraph* CollapseNodeGraph = InNode->GetGraph();
	check(CollapseNodeGraph);

	if (bForce)
	{
		RemoveEdGraphForCollapseNode(InNode, false);
	}

	// For Function node
	if (InNode->GetGraph()->IsA<URigVMFunctionLibrary>())
	{
		if (URigVMGraph* ContainedGraph = InNode->GetContainedGraph())
		{
			bool bFunctionGraphExists = false;
			for (UEdGraph* FunctionGraph : FunctionEdGraphs)
			{
				if (URigVMEdGraph* RigFunctionGraph = Cast<URigVMEdGraph>(FunctionGraph))
				{
					if (RigFunctionGraph->ModelNodePath == ContainedGraph->GetNodePath())
					{
						bFunctionGraphExists = true;
						break;
					}
				}
			}

			if (!bFunctionGraphExists)
			{
				const FName SubGraphName = RigVMClient.GetUniqueName(this, *InNode->GetName());
				// create a sub graph
				UAnimNextEdGraph* RigFunctionGraph = NewObject<UAnimNextEdGraph>(this, SubGraphName, RF_Transactional);
				RigFunctionGraph->Schema = UAnimNextEdGraphSchema::StaticClass();
				RigFunctionGraph->bAllowRenaming = true;
				RigFunctionGraph->bEditable = true;
				RigFunctionGraph->bAllowDeletion = true;
				RigFunctionGraph->ModelNodePath = ContainedGraph->GetNodePath();
				RigFunctionGraph->bIsFunctionDefinition = true;

				RigFunctionGraph->Initialize(this);

				FunctionEdGraphs.Add(RigFunctionGraph);

				RigVMClient.GetOrCreateController(ContainedGraph)->ResendAllNotifications();
			}
		}
	}
	// --- For Collapse nodes ---
	else if (URigVMEdGraph* RigEdGraph = Cast<URigVMEdGraph>(GetEditorObjectForRigVMGraph(InNode->GetGraph())))
	{
		if (URigVMGraph* ContainedGraph = InNode->GetContainedGraph())
		{
			bool bSubGraphExists = false;

			const FString ContainedGraphNodePath = ContainedGraph->GetNodePath();
			for (UEdGraph* SubGraph : RigEdGraph->SubGraphs)
			{
				if (UAnimNextEdGraph* SubRigGraph = Cast<UAnimNextEdGraph>(SubGraph))
				{
					if (SubRigGraph->ModelNodePath == ContainedGraphNodePath)
					{
						bSubGraphExists = true;
						break;
					}
				}
			}

			if (!bSubGraphExists)
			{
				bool bEditable = true;
				if (InNode->IsA<URigVMAggregateNode>())
				{
					bEditable = false;
				}

				UObject* Outer = FindEntryForRigVMGraph(CollapseNodeGraph->GetRootGraph());
				if (Outer == nullptr)
				{
					Outer = this; // function library graph has no entry
				}

				const FName SubGraphName = RigVMClient.GetUniqueName(Outer, *InNode->GetEditorSubGraphName());
				// create a sub graph, no need to set external package if outer is an Entry
				UAnimNextEdGraph* SubRigGraph = NewObject<UAnimNextEdGraph>(Outer, SubGraphName, RF_Transactional);
				SubRigGraph->Schema = UAnimNextEdGraphSchema::StaticClass();
				SubRigGraph->bAllowRenaming = 1;
				SubRigGraph->bEditable = bEditable;
				SubRigGraph->bAllowDeletion = 1;
				SubRigGraph->ModelNodePath = ContainedGraphNodePath;
				SubRigGraph->bIsFunctionDefinition = false;

				RigEdGraph->SubGraphs.Add(SubRigGraph);

				SubRigGraph->Initialize(this);

				GetOrCreateController(ContainedGraph)->ResendAllNotifications();
			}
		}
	}
}

void UAnimNextModule_EditorData::RemoveEdGraphForCollapseNode(URigVMCollapseNode* InNode, bool bNotify)
{
	check(InNode);

	if (InNode->GetGraph()->IsA<URigVMFunctionLibrary>())
	{
		if (URigVMGraph* ContainedGraph = InNode->GetContainedGraph())
		{
			for (UEdGraph* FunctionGraph : FunctionEdGraphs)
			{
				if (URigVMEdGraph* RigFunctionGraph = Cast<URigVMEdGraph>(FunctionGraph))
				{
					if (RigFunctionGraph->ModelNodePath == ContainedGraph->GetNodePath())
					{
						if (URigVMController* SubController = GetController(ContainedGraph))
						{
							SubController->OnModified().RemoveAll(RigFunctionGraph);
						}

						if (RigVMGraphModifiedEvent.IsBound() && bNotify)
						{
							RigVMGraphModifiedEvent.Broadcast(ERigVMGraphNotifType::NodeRemoved, InNode->GetGraph(), InNode);
						}

						FunctionEdGraphs.Remove(RigFunctionGraph);
						RigFunctionGraph->Rename(nullptr, GetTransientPackage(), REN_ForceNoResetLoaders | REN_DontCreateRedirectors);
						RigFunctionGraph->MarkAsGarbage();
						break;
					}
				}
			}
		}
	}
	else if (URigVMEdGraph* RigGraph = Cast<URigVMEdGraph>(GetEditorObjectForRigVMGraph(InNode->GetGraph())))
	{
		if (URigVMGraph* ContainedGraph = InNode->GetContainedGraph())
		{
			for (UEdGraph* SubGraph : RigGraph->SubGraphs)
			{
				if (URigVMEdGraph* SubRigGraph = Cast<URigVMEdGraph>(SubGraph))
				{
					if (SubRigGraph->ModelNodePath == ContainedGraph->GetNodePath())
					{
						if (URigVMController* SubController = GetController(ContainedGraph))
						{
							SubController->OnModified().RemoveAll(SubRigGraph);
						}

						if (RigVMGraphModifiedEvent.IsBound() && bNotify)
						{
							RigVMGraphModifiedEvent.Broadcast(ERigVMGraphNotifType::NodeRemoved, InNode->GetGraph(), InNode);
						}

						RigGraph->SubGraphs.Remove(SubRigGraph);
						SubRigGraph->Rename(nullptr, GetTransientPackage(), REN_ForceNoResetLoaders | REN_DontCreateRedirectors);
						SubRigGraph->MarkAsGarbage();
						break;
					}
				}
			}
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

	const FName GraphName = RigVMClient.GetUniqueName(CastChecked<UObject>(Entry), Entry->GetGraphName());
	UAnimNextEdGraph* RigFunctionGraph = NewObject<UAnimNextEdGraph>(CastChecked<UObject>(Entry), GraphName, RF_Transactional);
	RigFunctionGraph->Schema = UAnimNextEdGraphSchema::StaticClass();

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

		// Editor data has to be the graph outer, or RigVM unique name generator will not work
		URigVMGraph* NewRigVMGraphModel = RigVMClient.CreateModel(URigVMGraph::StaticClass()->GetFName(), UAnimNextAnimationGraphSchema::StaticClass(), bSetupUndoRedo, this);
		if (ensure(NewRigVMGraphModel))
		{
			// Then, to avoid the graph losing ref due to external package, set the same package as the Entry
			if (!NewRigVMGraphModel->HasAnyFlags(RF_Transient))
			{
				NewRigVMGraphModel->SetExternalPackage(CastChecked<UObject>(NewEntry)->GetExternalPackage());
			}

			NewEntry->Graph = NewRigVMGraphModel;
		
			RefreshExternalModels();
			RigVMClient.AddModel(NewRigVMGraphModel, true);

			URigVMController* Controller = RigVMClient.GetController(NewRigVMGraphModel);
			UE::AnimNext::UncookedOnly::FUtils::SetupAnimGraph(NewEntry, Controller);
		}
	}

	BroadcastModified();

	return NewEntry;
}
