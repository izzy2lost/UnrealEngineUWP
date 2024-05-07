// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/AnimNextGraph_EditorData.h"

#include "UncookedOnlyUtils.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Curves/CurveFloat.h"
#include "Graph/AnimNextGraph.h"
#include "Graph/AnimNextGraph_AnimationGraph.h"
#include "Graph/AnimNextGraph_EdGraphSchema.h"
#include "Graph/AnimNextGraph_EventGraph.h"
#include "Graph/AnimNextGraph_EventGraphSchema.h"
#include "Graph/AnimNextGraph_Parameter.h"
#include "Graph/AnimNextGraph_AnimationGraphSchema.h"
#include "RigVMModel/RigVMFunctionLibrary.h"
#include "RigVMModel/RigVMNotifications.h"
#include "RigVMModel/Nodes/RigVMCollapseNode.h"
#include "UObject/AssetRegistryTagsContext.h"
#include "UObject/FortniteMainBranchObjectVersion.h"
#include "UObject/LinkerLoad.h"

void UAnimNextGraph_EditorData::PostLoad()
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
				if(FoundEntry->IsA(UAnimNextGraph_AnimationGraph::StaticClass()))
				{
					Graph->SetSchemaClass(UAnimNextGraph_AnimationGraphSchema::StaticClass());
				}
				else
				{
					Graph->SetSchemaClass(UAnimNextGraph_EventGraphSchema::StaticClass());
				}
			}
			else
			{
				Graph->SetSchemaClass(UAnimNextGraph_AnimationGraphSchema::StaticClass());
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
		
		for(TObjectPtr<UAnimNextGraph_EdGraph> Graph : Graphs_DEPRECATED)
		{
			URigVMGraph* FoundRigVMGraph = GetRigVMGraphForEditorObject(Graph);
			if(FoundRigVMGraph)
			{
				if(UAnimNextRigVMAssetEntry* FoundEntry = FindEntryForRigVMGraph(FoundRigVMGraph))
				{
					if(UAnimNextGraph_AnimationGraph* AnimationGraphEntry = Cast<UAnimNextGraph_AnimationGraph>(FoundEntry))
					{
						AnimationGraphEntry->EdGraph = Graph;
					}
					else if(UAnimNextGraph_EventGraph* EventGraphEntry = Cast<UAnimNextGraph_EventGraph>(FoundEntry))
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
				if(UAnimNextGraph_EventGraph* EventGraphEntry = Cast<UAnimNextGraph_EventGraph>(Entry))
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
			if(UAnimNextGraph_AnimationGraph* AnimationGraphEntry = Cast<UAnimNextGraph_AnimationGraph>(Entry))
			{
				AnimationGraphEntry->Access = EAnimNextExportAccessSpecifier::Public;
			}
			else if(UAnimNextGraph_Parameter* ParameterEntry = Cast<UAnimNextGraph_Parameter>(Entry))
			{
				ParameterEntry->Access = EAnimNextExportAccessSpecifier::Public;
			}
		}
	}
}

void UAnimNextGraph_EditorData::RecompileVM()
{
	CachedExports = FAnimNextParameterProviderAssetRegistryExports();
	UE::AnimNext::UncookedOnly::FUtils::GetAssetParameters(this, CachedExports.GetValue());
	UE::AnimNext::UncookedOnly::FUtils::Compile(GetTypedOuter<UAnimNextGraph>());
	
	if (IAssetRegistry* AssetRegistry = IAssetRegistry::Get())
	{
		AssetRegistry->AssetUpdateTags(GetTypedOuter<UAnimNextGraph>(), EAssetRegistryTagsCaller::Fast);
	}
}

void UAnimNextGraph_EditorData::HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject)
{
	switch(InNotifType)
	{
	case ERigVMGraphNotifType::PinAdded:
		{
			if (URigVMPin* Pin = Cast<URigVMPin>(InSubject))
			{
				if (Pin->IsDecoratorPin())
				{
					RequestAutoVMRecompilation();
				}
			}
			break;
		}
	}

	Super::HandleModifiedEvent(InNotifType, InGraph, InSubject);
}

TConstArrayView<TSubclassOf<UAnimNextRigVMAssetEntry>> UAnimNextGraph_EditorData::GetEntryClasses() const
{
	static const TSubclassOf<UAnimNextRigVMAssetEntry> Classes[] =
	{
		UAnimNextGraph_AnimationGraph::StaticClass(),
		UAnimNextGraph_EventGraph::StaticClass(),
		UAnimNextGraph_Parameter::StaticClass(),
	};
	
	return Classes;
}

void UAnimNextGraph_EditorData::CreateEdGraphForCollapseNode(URigVMCollapseNode* InNode)
{
	if (InNode->GetGraph()->IsA<URigVMFunctionLibrary>())
	{
		if (URigVMGraph* ContainedGraph = InNode->GetContainedGraph())
		{
			// create a sub graph
			UAnimNextGraph_EdGraph* RigFunctionGraph = NewObject<UAnimNextGraph_EdGraph>(this, *InNode->GetName(), RF_Transactional);
			RigFunctionGraph->Schema = UAnimNextGraph_EdGraphSchema::StaticClass();
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

UEdGraph* UAnimNextGraph_EditorData::CreateEdGraph(URigVMGraph* InRigVMGraph, bool bForce)
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

	UAnimNextGraph_EdGraph* RigFunctionGraph = NewObject<UAnimNextGraph_EdGraph>(CastChecked<UObject>(Entry), NAME_None, RF_Transactional);
	RigFunctionGraph->Schema = UAnimNextGraph_EdGraphSchema::StaticClass();
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

bool UAnimNextGraph_EditorData::RemoveEdGraph(URigVMGraph* InModel)
{
	if(UAnimNextGraph_AnimationGraph* Entry = Cast<UAnimNextGraph_AnimationGraph>(FindEntryForRigVMGraph(InModel)))
	{
		RigVMClient.DestroyObject(Entry->EdGraph);
		Entry->EdGraph = nullptr;
		return true;
	}
	return false;
}

UAnimNextGraph_Parameter* UAnimNextGraphLibrary::AddParameter(UAnimNextGraph* InGraph, FName InName, EPropertyBagPropertyType InValueType,
	EPropertyBagContainerType InContainerType, const UObject* InValueTypeObject, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	return UE::AnimNext::UncookedOnly::FUtils::GetEditorData(InGraph)->AddParameter(InName, FAnimNextParamType(InValueType, InContainerType, InValueTypeObject), bSetupUndoRedo, bPrintPythonCommand);
}

UAnimNextGraph_Parameter* UAnimNextGraph_EditorData::AddParameter(FName InName, FAnimNextParamType InType, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	if(InName == NAME_None)
	{
		ReportError(TEXT("UAnimNextGraph_EditorData::AddParameter: Invalid parameter name supplied."));
		return nullptr;
	}

	// Check for duplicate parameter
	const bool bAlreadyExists = Entries.ContainsByPredicate([InName](const UAnimNextRigVMAssetEntry* InEntry)
	{
		if(const UAnimNextGraph_Parameter* Parameter = Cast<UAnimNextGraph_Parameter>(InEntry))
		{
			return Parameter->ParameterName == InName;
		}
		return false;
	});

	if(bAlreadyExists)
	{
		ReportError(TEXT("UAnimNextGraph_EditorData::AddParameter: A parameter already exists for the supplied parameter name."));
		return nullptr;
	}

	UAnimNextGraph_Parameter* NewEntry = CreateNewSubEntry<UAnimNextGraph_Parameter>(this);
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

UAnimNextGraph_EventGraph* UAnimNextGraphLibrary::AddEventGraph(UAnimNextGraph* InGraph, FName InName, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	return UE::AnimNext::UncookedOnly::FUtils::GetEditorData(InGraph)->AddEventGraph(InName, bSetupUndoRedo, bPrintPythonCommand);
}

UAnimNextGraph_EventGraph* UAnimNextGraph_EditorData::AddEventGraph(FName InName, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	if(InName == NAME_None)
	{
		ReportError(TEXT("UAnimNextGraph_EditorData::AddEventGraph: Invalid graph name supplied."));
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

	UAnimNextGraph_EventGraph* NewEntry = CreateNewSubEntry<UAnimNextGraph_EventGraph>(this);
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
		URigVMGraph* NewGraph = RigVMClient.AddModel(URigVMGraph::StaticClass()->GetFName(), UAnimNextGraph_EventGraphSchema::StaticClass(), bSetupUndoRedo);
		ensure(NewGraph);
		NewEntry->Graph = NewGraph;

		URigVMController* Controller = RigVMClient.GetController(NewGraph);
		UE::AnimNext::UncookedOnly::FUtils::SetupEventGraph(Controller);
	}

	BroadcastModified();

	return NewEntry;
}

UAnimNextGraph_AnimationGraph* UAnimNextGraphLibrary::AddAnimationGraph(UAnimNextGraph* InGraph, FName InName, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	return UE::AnimNext::UncookedOnly::FUtils::GetEditorData(InGraph)->AddAnimationGraph(InName, bSetupUndoRedo, bPrintPythonCommand);
}

UAnimNextGraph_AnimationGraph* UAnimNextGraph_EditorData::AddAnimationGraph(FName InName, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	if(InName == NAME_None)
	{
		ReportError(TEXT("UAnimNextGraph_EditorData::AddAnimationGraph: Invalid graph name supplied."));
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

	UAnimNextGraph_AnimationGraph* NewEntry = CreateNewSubEntry<UAnimNextGraph_AnimationGraph>(this);
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
		URigVMGraph* NewGraph = RigVMClient.AddModel(URigVMGraph::StaticClass()->GetFName(), UAnimNextGraph_AnimationGraphSchema::StaticClass(), bSetupUndoRedo);
		ensure(NewGraph);
		NewEntry->Graph = NewGraph;

		URigVMController* Controller = RigVMClient.GetController(NewGraph);
		UE::AnimNext::UncookedOnly::FUtils::SetupAnimGraph(NewEntry, Controller);
	}

	BroadcastModified();

	return NewEntry;
}