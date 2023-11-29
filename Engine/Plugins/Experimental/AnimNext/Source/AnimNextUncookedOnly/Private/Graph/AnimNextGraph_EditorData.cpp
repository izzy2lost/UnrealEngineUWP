// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/AnimNextGraph_EditorData.h"

#include "ControlRigDefines.h"
#include "ExternalPackageHelper.h"
#include "Graph/AnimNextGraph.h"
#include "Graph/AnimNextGraph_Controller.h"
#include "Graph/AnimNextGraph_EdGraphSchema.h"
#include "UncookedOnlyUtils.h"
#include "Graph/AnimNextExecuteContext.h"
#include "Rigs/RigHierarchyPose.h"
#include "RigVMModel/RigVMFunctionLibrary.h"
#include "RigVMModel/RigVMNotifications.h"
#include "RigVMModel/Nodes/RigVMCollapseNode.h"
#include "Curves/CurveFloat.h"
#include "UObject/ObjectSaveContext.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Graph/AnimNextGraphEntry.h"
#include "Param/RigVMDispatch_GetLayerParameter.h"
#include "Param/RigVMDispatch_GetParameter.h"

namespace UE::AnimNext::Graph::Private
{

static UAnimNextGraphEntry* CreateNewGraphEntry(UAnimNextGraph_EditorData* InEditorData, TSubclassOf<UAnimNextGraphEntry> InClass)
{
	UAnimNextGraphEntry* NewEntry = NewObject<UAnimNextGraphEntry>(InEditorData, InClass.Get(), NAME_None, RF_Transactional);
	// If we are a transient asset, dont use external packages
	UAnimNextGraph* Graph = UncookedOnly::FUtils::GetGraph(InEditorData);
	check(Graph);
	if(!Graph->HasAnyFlags(RF_Transient))
	{
		FExternalPackageHelper::SetPackagingMode(NewEntry, InEditorData, true, false, PKG_None);
	}
	return NewEntry;
}

template<typename EntryClassType>
static EntryClassType* CreateNewGraphEntry(UAnimNextGraph_EditorData* InEditorData)
{
	return CastChecked<EntryClassType>(CreateNewGraphEntry(InEditorData, EntryClassType::StaticClass()));
}

}

UAnimNextGraph_EditorData::UAnimNextGraph_EditorData(const FObjectInitializer& ObjectInitializer)
{
	RigVMClient.Reset();
	RigVMClient.SetSchemaClass(UAnimNextGraph_Schema::StaticClass());
	RigVMClient.SetControllerClass(UAnimNextGraph_Controller::StaticClass());
	RigVMClient.SetOuterClientHost(this, GET_MEMBER_NAME_CHECKED(UAnimNextGraph_EditorData, RigVMClient));
	{
		TGuardValue<bool> DisableClientNotifs(RigVMClient.bSuspendNotifications, true);
		RigVMClient.GetOrCreateFunctionLibrary(false, &ObjectInitializer);
	}
	RigVMClient.SetExecuteContextStruct(FAnimNextExecuteContext::StaticStruct());
}


void UAnimNextGraph_EditorData::BroadcastModified()
{
	RecompileVM();

	if(!bSuspendGraphNotifications)
	{
		ModifiedDelegate.Broadcast(this);
	}
}

void UAnimNextGraph_EditorData::ReportError(const TCHAR* InMessage) const
{
#if WITH_EDITOR
	FScriptExceptionHandler::Get().HandleException(ELogVerbosity::Error, InMessage, TEXT(""));
#endif
}

void UAnimNextGraph_EditorData::ReconstructAllNodes()
{
#ifdef WITH_EDITORONLY_DATA
	// Avoid refreshing EdGraph nodes during cook
	if (GIsCookerLoadingPackage)
	{
		return;
	}
	
	if (GetRigVMClient()->GetDefaultModel() == nullptr)
	{
		return;
	}

	TArray<URigVMEdGraphNode*> AllNodes;
	UE::AnimNext::UncookedOnly::FUtils::GetAllNodesOfClass(this, AllNodes);

	for (URigVMEdGraphNode* Node : AllNodes)
	{
		Node->SetFlags(RF_Transient);
	}

	for(URigVMEdGraphNode* Node : AllNodes)
	{
		Node->ReconstructNode();
	}

	for (URigVMEdGraphNode* Node : AllNodes)
	{
		Node->ClearFlags(RF_Transient);
	}
#endif	
}

void UAnimNextGraph_EditorData::Serialize(FArchive& Ar)
{
	RigVMClient.SetOuterClientHost(this, GET_MEMBER_NAME_CHECKED(UAnimNextGraph_EditorData, RigVMClient));

	UObject::Serialize(Ar);
}

void UAnimNextGraph_EditorData::Initialize(bool bRecompileVM)
{
	UAnimNextGraph* AnimNextGraph = GetTypedOuter<UAnimNextGraph>();

	if (RigVMClient.GetController(0) == nullptr)
	{
		check(RigVMClient.Num() == 1);
		check(RigVMClient.GetFunctionLibrary());
		
		RigVMClient.GetOrCreateController(RigVMClient.GetDefaultModel());
		RigVMClient.GetOrCreateController(RigVMClient.GetFunctionLibrary());

		// Init function library controllers
		for(URigVMLibraryNode* LibraryNode : RigVMClient.GetFunctionLibrary()->GetFunctions())
		{
			RigVMClient.GetOrCreateController(LibraryNode->GetContainedGraph());
		}
		
		if(bRecompileVM)
		{
			UE::AnimNext::UncookedOnly::FUtils::Compile(AnimNextGraph);
		}
	}

	for(UAnimNextGraph_EdGraph* Graph : Graphs)
	{
		Graph->Initialize(this);
	}
}

void UAnimNextGraph_EditorData::PostLoad()
{
	Super::PostLoad();
	
	Initialize(/*bRecompileVM*/false);
	RefreshAllModels(ERigVMLoadType::PostLoad);

#if WITH_EDITOR
	FExternalPackageHelper::LoadObjectsFromExternalPackages<UAnimNextGraphEntry>(this, [this](UAnimNextGraphEntry* InLoadedEntry)
	{
		check(IsValid(InLoadedEntry));
		Entries.Add(InLoadedEntry);
	});
	
	// delay compilation until the package has been loaded
	FCoreUObjectDelegates::OnEndLoadPackage.AddUObject(this, &UAnimNextGraph_EditorData::HandlePackageDone);
#else // !WITH_EDITOR
	RecompileVMIfRequired();
#endif // WITH_EDITOR
}

void UAnimNextGraph_EditorData::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Initialize(/*bRecompileVM*/true);
}

#if WITH_EDITOR
void UAnimNextGraph_EditorData::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	UObject::GetAssetRegistryTags(OutTags);

	FAnimNextParameterProviderAssetRegistryExports ExportParameters;
	
	for (const TObjectPtr<UAnimNextGraph_EdGraph>& Graph : Graphs)
	{
		if (const URigVMGraph* RigGraph = Graph->GetModel())
		{
			UE::AnimNext::UncookedOnly::FUtils::GetGraphParameters(RigGraph, ExportParameters);
		}
	}

	if (ExportParameters.Parameters.Num())
	{
		FString TagValue;	
		FAnimNextParameterProviderAssetRegistryExports::StaticStruct()->ExportText(TagValue, &ExportParameters, nullptr, nullptr, PPF_None, nullptr);
		OutTags.Add(FAssetRegistryTag(UE::AnimNext::ExportsAnimNextAssetRegistryTag, TagValue, FAssetRegistryTag::TT_Hidden));	
	}
}

void UAnimNextGraph_EditorData::HandlePackageDone(const FEndLoadPackageContext& Context)
{
	if (!Context.LoadedPackages.Contains(GetPackage()))
	{
		return;
	}
	HandlePackageDone();
}

void UAnimNextGraph_EditorData::HandlePackageDone()
{
	FCoreUObjectDelegates::OnEndLoadPackage.RemoveAll(this);

	RecompileVM();

	ReconstructAllNodes();
}

void UAnimNextGraph_EditorData::RefreshAllModels(ERigVMLoadType InLoadType)
{
	const bool bIsPostLoad = InLoadType == ERigVMLoadType::PostLoad;

	TGuardValue<bool> IsCompilingGuard(bIsCompiling, true);
	TGuardValue<bool> ClientIgnoreModificationsGuard(RigVMClient.bIgnoreModelNotifications, true);

	TArray<URigVMGraph*> GraphsToDetach = RigVMClient.GetAllModels(true, false);
	TMap<const URigVMGraph*, TArray<URigVMController::FLinkedPath>> LinkedPaths;
	
	if (ensure(IsInGameThread()))
	{
		for (const URigVMGraph* GraphToDetach : GraphsToDetach)
		{
			URigVMController* Controller = RigVMClient.GetOrCreateController(GraphToDetach);
			// temporarily disable default value validation during load time, serialized values should always be accepted
			TGuardValue<bool> PerGraphDisablePinDefaultValueValidation(Controller->bValidatePinDefaults, false);
			FRigVMControllerNotifGuard NotifGuard(Controller, true);
			LinkedPaths.Add(GraphToDetach, Controller->GetLinkedPaths());
			Controller->FastBreakLinkedPaths(LinkedPaths.FindChecked(GraphToDetach));
			TArray<URigVMNode*> Nodes = GraphToDetach->GetNodes();
			for (URigVMNode* Node : Nodes)
			{
				Controller->RepopulatePinsOnNode(Node, true, true);
			}
		}
		//SetupPinRedirectorsForBackwardsCompatibility();
	}

	for (const URigVMGraph* GraphToDetach : GraphsToDetach)
	{
		URigVMController* Controller = RigVMClient.GetOrCreateController(GraphToDetach);
		// at this stage, allow all links to be reattached,
		// RecomputeAllTemplateFilteredPermutations() later should break any invalid links
		FRigVMControllerNotifGuard NotifGuard(Controller, true);
		Controller->RestoreLinkedPaths(LinkedPaths.FindChecked(GraphToDetach));
	}

	if (bIsPostLoad)
	{
		//PatchTemplateNodesWithPreferredPermutation();
	}

	TArray<URigVMGraph*> GraphsToClean = RigVMClient.GetAllModels(true, true);

	// Sort from leaf graphs to root
	TArray<URigVMGraph*> SortedGraphsToClean;
	SortedGraphsToClean.Reserve(GraphsToClean.Num());
	while (SortedGraphsToClean.Num() < GraphsToClean.Num())
	{
		bool bGraphAdded = false;
		for (URigVMGraph* Graph : GraphsToClean)
		{
			if (SortedGraphsToClean.Contains(Graph))
			{
				continue;
			}

			TArray<URigVMGraph*> ContainedGraphs;
			for (URigVMNode* Node : Graph->GetNodes())
			{
				if (URigVMLibraryNode* LibraryNode = Cast<URigVMLibraryNode>(Node))
				{
					if (URigVMFunctionReferenceNode* FunctionReferenceNode = Cast<URigVMFunctionReferenceNode>(LibraryNode))
					{
						if (FunctionReferenceNode->GetReferencedFunctionHeader().LibraryPointer.LibraryNode.GetLongPackageName() != GetPackage()->GetPathName())
						{
							continue;
						}
						if (URigVMLibraryNode* ReferencedNode = FunctionReferenceNode->LoadReferencedNode())
						{
							ContainedGraphs.Add(ReferencedNode->GetContainedGraph());
							continue;
						}
					}

					if (URigVMGraph* ContainedGraph = LibraryNode->GetContainedGraph())
					{
						ContainedGraphs.Add(ContainedGraph);
					}
				}
			}

			bool bAllContained = true;
			for (URigVMGraph* Contained : ContainedGraphs)
			{
				if (!SortedGraphsToClean.Contains(Contained))
				{
					bAllContained = false;
					break;
				}
			}
			if (bAllContained)
			{
				SortedGraphsToClean.Add(Graph);
				bGraphAdded = true;
			}
		}
		ensure(bGraphAdded);
	}

	for (int32 GraphIndex = 0; GraphIndex < SortedGraphsToClean.Num(); GraphIndex++)
	{
		URigVMGraph* GraphToClean = SortedGraphsToClean[GraphIndex];
		URigVMController* Controller = RigVMClient.GetOrCreateController(GraphToClean);
		//TGuardValue<bool> GuardEditGraph(GraphToClean->bEditable, true);
		FRigVMControllerNotifGuard NotifGuard(Controller, true);

		for (URigVMNode* ModelNode : GraphToClean->GetNodes())
		{
			Controller->RemoveUnusedOrphanedPins(ModelNode);
		}
	}
}

void UAnimNextGraph_EditorData::OnRigVMRegistryChanged()
{
	RefreshAllModels(ERigVMLoadType::PostLoad);
	//RebuildGraphFromModel(); // TODO zzz : How we do this on AnimNext ?
}

void UAnimNextGraph_EditorData::RequestRigVMInit()
{
	// TODO zzz : How we do this on AnimNext ?
}

URigVMGraph* UAnimNextGraph_EditorData::GetModel(const UEdGraph* InEdGraph) const
{
	return RigVMClient.GetModel(InEdGraph);
}

URigVMGraph* UAnimNextGraph_EditorData::GetModel(const FString& InNodePath) const
{
	return RigVMClient.GetModel(InNodePath);
}

URigVMGraph* UAnimNextGraph_EditorData::GetDefaultModel() const 
{
	return RigVMClient.GetDefaultModel();
}

TArray<URigVMGraph*> UAnimNextGraph_EditorData::GetAllModels() const
{
	return RigVMClient.GetAllModels(true, true);
}

URigVMFunctionLibrary* UAnimNextGraph_EditorData::GetLocalFunctionLibrary() const
{
	return RigVMClient.GetFunctionLibrary();
}

URigVMGraph* UAnimNextGraph_EditorData::AddModel(FString InName, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	TGuardValue<bool> EnablePythonPrint(bSuspendPythonMessagesForRigVMClient, !bPrintPythonCommand);
	return RigVMClient.AddModel(InName, bSetupUndoRedo, bPrintPythonCommand);
}

bool UAnimNextGraph_EditorData::RemoveModel(FString InName, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	TGuardValue<bool> EnablePythonPrint(bSuspendPythonMessagesForRigVMClient, !bPrintPythonCommand);
	return RigVMClient.RemoveModel(InName, bSetupUndoRedo, bPrintPythonCommand);
}

FRigVMGetFocusedGraph& UAnimNextGraph_EditorData::OnGetFocusedGraph()
{
	return RigVMClient.OnGetFocusedGraph();
}

const FRigVMGetFocusedGraph& UAnimNextGraph_EditorData::OnGetFocusedGraph() const
{
	return RigVMClient.OnGetFocusedGraph();
}

URigVMGraph* UAnimNextGraph_EditorData::GetFocusedModel() const
{
	return RigVMClient.GetFocusedModel();
}

URigVMController* UAnimNextGraph_EditorData::GetController(const URigVMGraph* InGraph) const
{
	return RigVMClient.GetController(InGraph);
};

URigVMController* UAnimNextGraph_EditorData::GetControllerByName(const FString InGraphName) const
{
	return RigVMClient.GetControllerByName(InGraphName);
};

URigVMController* UAnimNextGraph_EditorData::GetOrCreateController(URigVMGraph* InGraph)
{
	return RigVMClient.GetOrCreateController(InGraph);
};

URigVMController* UAnimNextGraph_EditorData::GetController(const UEdGraph* InEdGraph) const
{
	return RigVMClient.GetController(InEdGraph);
};

URigVMController* UAnimNextGraph_EditorData::GetOrCreateController(const UEdGraph* InEdGraph)
{
	return RigVMClient.GetOrCreateController(InEdGraph);
};

TArray<FString> UAnimNextGraph_EditorData::GeneratePythonCommands(const FString InNewBlueprintName)
{
	return TArray<FString>();
}

void UAnimNextGraph_EditorData::GetAllGraphs(TArray<UEdGraph*>& OutGraphs) const
{
	OutGraphs.Reset();
	OutGraphs.Append(Graphs);
}

#endif // WITH_EDITOR

FRigVMClient* UAnimNextGraph_EditorData::GetRigVMClient()
{
	return &RigVMClient;
}

const FRigVMClient* UAnimNextGraph_EditorData::GetRigVMClient() const
{
	return &RigVMClient;
}

IRigVMGraphFunctionHost* UAnimNextGraph_EditorData::GetRigVMGraphFunctionHost()
{
	return this;
}

const IRigVMGraphFunctionHost* UAnimNextGraph_EditorData::GetRigVMGraphFunctionHost() const
{
	return this;
}


void UAnimNextGraph_EditorData::HandleRigVMGraphAdded(const FRigVMClient* InClient, const FString& InNodePath)
{
	if(URigVMGraph* RigVMGraph = InClient->GetModel(InNodePath))
	{
		RigVMGraph->SetExecuteContextStruct(FAnimNextExecuteContext::StaticStruct());

		if(!HasAnyFlags(RF_ClassDefaultObject | RF_NeedInitialization | RF_NeedLoad | RF_NeedPostLoad) &&
			GetOuter() != GetTransientPackage())
		{
			CreateEdGraph(RigVMGraph, true);
			RecompileVM();
		}
	}
}

void UAnimNextGraph_EditorData::HandleRigVMGraphRemoved(const FRigVMClient* InClient, const FString& InNodePath)
{
	if(URigVMGraph* RigVMGraph = InClient->GetModel(InNodePath))
	{
		RemoveEdGraph(RigVMGraph);
		RecompileVM();
	}
}

void UAnimNextGraph_EditorData::HandleConfigureRigVMController(const FRigVMClient* InClient,
                                                                    URigVMController* InControllerToConfigure)
{
	InControllerToConfigure->OnModified().AddUObject(this, &UAnimNextGraph_EditorData::HandleModifiedEvent);

	TWeakObjectPtr<UAnimNextGraph_EditorData> WeakThis(this);

	// this delegate is used by the controller to determine variable validity
	// during a bind process. the controller itself doesn't own the variables,
	// so we need a delegate to request them from the owning blueprint
	InControllerToConfigure->GetExternalVariablesDelegate.BindLambda([](URigVMGraph* InGraph) -> TArray<FRigVMExternalVariable>
	{
		if (InGraph)
		{
			if(UAnimNextGraph_EditorData* EditorData = InGraph->GetTypedOuter<UAnimNextGraph_EditorData>())
			{
				if (UAnimNextGraph* Graph = EditorData->GetTypedOuter<UAnimNextGraph>())
				{
					return Graph->GetRigVMExternalVariables();
				}
			}
		}
		return TArray<FRigVMExternalVariable>();
	});

	// this delegate is used by the controller to retrieve the current bytecode of the VM
	InControllerToConfigure->GetCurrentByteCodeDelegate.BindLambda([WeakThis]() -> const FRigVMByteCode*
	{
		if (WeakThis.IsValid())
		{
			if(UAnimNextGraph* Graph = WeakThis->GetTypedOuter<UAnimNextGraph>())
			{
				if (Graph->VM)
				{
					return &Graph->VM->GetByteCode();
				}
			}
		}
		return nullptr;

	});

#if WITH_EDITOR
	InControllerToConfigure->SetupDefaultUnitNodeDelegates(TDelegate<FName(FRigVMExternalVariable, FString)>::CreateLambda(
		[](FRigVMExternalVariable InVariableToCreate, FString InDefaultValue) -> FName
		{
			return NAME_None;
		}
	));
#endif

}

UObject* UAnimNextGraph_EditorData::GetEditorObjectForRigVMGraph(URigVMGraph* InVMGraph) const
{
	if(InVMGraph)
	{
		for(UAnimNextGraph_EdGraph* EdGraph : Graphs)
		{
			if(EdGraph)
			{
				if(EdGraph->ModelNodePath == InVMGraph->GetNodePath())
				{
					return EdGraph;
				}
			}
		}
	}
	return nullptr;
}

URigVMGraph* UAnimNextGraph_EditorData::GetRigVMGraphForEditorObject(UObject* InObject) const
{
	if(const UAnimNextGraph_EdGraph* Graph = Cast<UAnimNextGraph_EdGraph>(InObject))
	{
		if (Graph->bIsFunctionDefinition)
		{
			if (URigVMLibraryNode* LibraryNode = RigVMClient.GetFunctionLibrary()->FindFunction(*Graph->ModelNodePath))
			{
				return LibraryNode->GetContainedGraph();
			}
		}
		else
		{
			return RigVMClient.GetModel(Graph->ModelNodePath);
		}
	}

	return nullptr;
}

FRigVMGraphFunctionStore* UAnimNextGraph_EditorData::GetRigVMGraphFunctionStore()
{
	return &GraphFunctionStore;
}

const FRigVMGraphFunctionStore* UAnimNextGraph_EditorData::GetRigVMGraphFunctionStore() const
{
	return &GraphFunctionStore;
}

void UAnimNextGraph_EditorData::RecompileVM()
{
	UE::AnimNext::UncookedOnly::FUtils::Compile(GetTypedOuter<UAnimNextGraph>());
}

void UAnimNextGraph_EditorData::RecompileVMIfRequired()
{
	if (bVMRecompilationRequired)
	{
		RecompileVM();
	}
}

void UAnimNextGraph_EditorData::RequestAutoVMRecompilation()
{
	bVMRecompilationRequired = true;
	if (bAutoRecompileVM && VMRecompilationBracket == 0)
	{
		RecompileVMIfRequired();
	}
}

void UAnimNextGraph_EditorData::SetAutoVMRecompile(bool bAutoRecompile)
{
	bAutoRecompileVM = bAutoRecompile;
}

bool UAnimNextGraph_EditorData::GetAutoVMRecompile() const
{
	return bAutoRecompileVM;
}

void UAnimNextGraph_EditorData::IncrementVMRecompileBracket()
{
	VMRecompilationBracket++;
}

void UAnimNextGraph_EditorData::DecrementVMRecompileBracket()
{
	if (VMRecompilationBracket == 1)
	{
		if (bAutoRecompileVM)
		{
			RecompileVMIfRequired();
		}
		VMRecompilationBracket = 0;
	}
	else if (VMRecompilationBracket > 0)
	{
		VMRecompilationBracket--;
	}
}

void UAnimNextGraph_EditorData::HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject)
{
	bool bNotifForOthersPending = true;

	switch(InNotifType)
	{
	case ERigVMGraphNotifType::InteractionBracketOpened:
		{
			IncrementVMRecompileBracket();
			break;
		}
	case ERigVMGraphNotifType::InteractionBracketClosed:
	case ERigVMGraphNotifType::InteractionBracketCanceled:
		{
			DecrementVMRecompileBracket();
			break;
		}
	case ERigVMGraphNotifType::NodeAdded:
		{
			if (URigVMCollapseNode* CollapseNode = Cast<URigVMCollapseNode>(InSubject))
			{
				CreateEdGraphForCollapseNode(CollapseNode);
				break;
			}
		}
		// Fall through to the next case
	case ERigVMGraphNotifType::LinkAdded:
	case ERigVMGraphNotifType::LinkRemoved:
	case ERigVMGraphNotifType::PinArraySizeChanged:
	case ERigVMGraphNotifType::PinDirectionChanged:
		{
			RequestAutoVMRecompilation();
			break;
		}
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
	case ERigVMGraphNotifType::PinDefaultValueChanged:
		{
			if (InGraph->GetRuntimeAST().IsValid())
			{
				URigVMPin* RootPin = CastChecked<URigVMPin>(InSubject)->GetRootPin();
				FRigVMASTProxy RootPinProxy = FRigVMASTProxy::MakeFromUObject(RootPin);
				const FRigVMExprAST* Expression = InGraph->GetRuntimeAST()->GetExprForSubject(RootPinProxy);
				if (Expression == nullptr)
				{
					InGraph->ClearAST();
				}
				else if (Expression->NumParents() > 1)
				{
					InGraph->ClearAST();
				}
			}

			RequestAutoVMRecompilation();	// We need to rebuild our metadata when a default value changes
			break;
		}
	}
	
	// if the notification still has to be sent...
	if (bNotifForOthersPending && !bSuspendModelNotificationsForOthers)
	{
		if (RigVMGraphModifiedEvent.IsBound())
		{
			RigVMGraphModifiedEvent.Broadcast(InNotifType, InGraph, InSubject);
		}
	}
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

	if(bForce)
	{
		RemoveEdGraph(InRigVMGraph);
	}

	FString GraphName = InRigVMGraph->GetName();

	if(GraphName.IsEmpty())
	{
		GraphName = URigVMEdGraphSchema::GraphName_RigVM.ToString();
	}

	GraphName = RigVMClient.GetUniqueName(*GraphName).ToString();

	UAnimNextGraph_EdGraph* RigFunctionGraph = NewObject<UAnimNextGraph_EdGraph>(this, *GraphName, RF_Transactional);
	RigFunctionGraph->Schema = UAnimNextGraph_EdGraphSchema::StaticClass();
	RigFunctionGraph->bAllowDeletion = true;
	RigFunctionGraph->bIsFunctionDefinition = false;
	RigFunctionGraph->ModelNodePath = InRigVMGraph->GetNodePath();
	RigFunctionGraph->Initialize(this);

	Graphs.Add(RigFunctionGraph);

	return RigFunctionGraph;
}

bool UAnimNextGraph_EditorData::RemoveEdGraph(URigVMGraph* InModel)
{
	if(UAnimNextGraph_EdGraph* RigFunctionGraph = Cast<UAnimNextGraph_EdGraph>(GetEditorObjectForRigVMGraph(InModel)))
	{
		if(Graphs.Contains(RigFunctionGraph))
		{
			Modify();
			Graphs.Remove(RigFunctionGraph);
		}
		RigVMClient.DestroyObject(RigFunctionGraph);
		return true;
	}
	return false;
}

UAnimNextGraphEntry* UAnimNextGraphLibrary::AddGraph(UAnimNextGraph* InGraph, FName InName, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	return UE::AnimNext::UncookedOnly::FUtils::GetEditorData(InGraph)->AddGraph(InName, bSetupUndoRedo, bPrintPythonCommand);
}

UAnimNextGraphEntry* UAnimNextGraph_EditorData::AddGraph(FName InName, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	if(InName == NAME_None)
	{
		ReportError(TEXT("UAnimNextGraph_EditorData::AddGraph: Invalid graph name supplied."));
		return nullptr;
	}

	// Check for duplicate name
	FName NewGraphName = InName;
	auto DuplicateNamePredicate = [&NewGraphName](const UAnimNextGraphEntry* InEntry)
	{
		return InEntry->GraphName == NewGraphName;
	};

	bool bAlreadyExists = Entries.ContainsByPredicate(DuplicateNamePredicate);
	int32 NameNumber = InName.GetNumber() + 1;
	while(bAlreadyExists)
	{
		NewGraphName = FName(InName, NameNumber);
		bAlreadyExists =  Entries.ContainsByPredicate(DuplicateNamePredicate);
	}

	UAnimNextGraphEntry* NewEntry = UE::AnimNext::Graph::Private::CreateNewGraphEntry<UAnimNextGraphEntry>(this);
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
		URigVMGraph* NewGraph = RigVMClient.AddModel(NewGraphName, bSetupUndoRedo);
		ensure(NewGraph);
		NewEntry->Graph = NewGraph;

		URigVMController* Controller = RigVMClient.GetController(NewGraph);
		UE::AnimNext::UncookedOnly::FUtils::SetupAnimGraph(Controller);
	}

	BroadcastModified();

	return NewEntry;
}