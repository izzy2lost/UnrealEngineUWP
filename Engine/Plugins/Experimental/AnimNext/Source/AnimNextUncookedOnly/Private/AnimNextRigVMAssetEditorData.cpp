// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextRigVMAssetEditorData.h"

#include "AnimNextRigVMAsset.h"
#include "AnimNextRigVMAssetEntry.h"
#include "AnimNextRigVMAssetSchema.h"
#include "Module/AnimNextModuleWorkspaceAssetUserData.h"
#include "ControlRigDefines.h"
#include "ExternalPackageHelper.h"
#include "IAnimNextRigVMGraphInterface.h"
#include "IAnimNextRigVMParameterInterface.h"
#include "UncookedOnlyUtils.h"
#include "AnimNextEdGraph.h"
#include "AnimNextEdGraphSchema.h"
#include "Misc/TransactionObjectEvent.h"
#include "Param/AnimNextTag.h"
#include "RigVMModel/RigVMFunctionLibrary.h"
#include "RigVMModel/RigVMNotifications.h"
#include "RigVMModel/Nodes/RigVMCollapseNode.h"
#include "UObject/AssetRegistryTagsContext.h"

void UAnimNextRigVMAssetEditorData::BroadcastModified()
{
	RecompileVM();

	if(!bSuspendEditorDataNotifications)
	{
		ModifiedDelegate.Broadcast(this);
	}
}

void UAnimNextRigVMAssetEditorData::ReportError(const TCHAR* InMessage) const
{
	FScriptExceptionHandler::Get().HandleException(ELogVerbosity::Error, InMessage, TEXT(""));
}

void UAnimNextRigVMAssetEditorData::ReconstructAllNodes()
{
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
	GetAllNodesOfClass(AllNodes);

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
}

void UAnimNextRigVMAssetEditorData::Serialize(FArchive& Ar)
{
	RigVMClient.SetDefaultSchemaClass(UAnimNextRigVMAssetSchema::StaticClass());
	RigVMClient.SetOuterClientHost(this, GET_MEMBER_NAME_CHECKED(UAnimNextRigVMAssetEditorData, RigVMClient));

	const bool bIsDuplicating = (Ar.GetPortFlags() & PPF_Duplicate) != 0;
	if (bIsDuplicating)
	{
		Ar << Entries;
	}

	Super::Serialize(Ar);
}

void UAnimNextRigVMAssetEditorData::Initialize(bool bRecompileVM)
{
	RigVMClient.bDefaultModelCanBeRemoved = true;
	RigVMClient.SetDefaultSchemaClass(UAnimNextRigVMAssetSchema::StaticClass());
	RigVMClient.SetControllerClass(GetControllerClass());
	RigVMClient.SetOuterClientHost(this, GET_MEMBER_NAME_CHECKED(UAnimNextRigVMAssetEditorData, RigVMClient));
	RigVMClient.SetExternalModelHost(this);

	URigVMFunctionLibrary* RigVMFunctionLibrary = nullptr;
	{
		TGuardValue<bool> DisableClientNotifs(RigVMClient.bSuspendNotifications, true);
		RigVMFunctionLibrary = RigVMClient.GetOrCreateFunctionLibrary(false);
	}

	ensure(RigVMFunctionLibrary->GetFunctionHostObjectPathDelegate.IsBound());

	if (RigVMClient.GetController(0) == nullptr)
	{
		if(RigVMClient.GetDefaultModel())
		{
			RigVMClient.GetOrCreateController(RigVMClient.GetDefaultModel());
		}

		check(RigVMFunctionLibrary);
		RigVMClient.GetOrCreateController(RigVMFunctionLibrary);

		if (!FunctionLibraryEdGraph)
		{
			FunctionLibraryEdGraph = NewObject<UAnimNextEdGraph>(CastChecked<UObject>(this), NAME_None, RF_Transactional);

			FunctionLibraryEdGraph->Schema = UAnimNextEdGraphSchema::StaticClass();
			FunctionLibraryEdGraph->bAllowRenaming = 0;
			FunctionLibraryEdGraph->bEditable = 0;
			FunctionLibraryEdGraph->bAllowDeletion = 0;
			FunctionLibraryEdGraph->bIsFunctionDefinition = false;
			FunctionLibraryEdGraph->ModelNodePath = RigVMClient.GetFunctionLibrary()->GetNodePath();
			FunctionLibraryEdGraph->Initialize(this);
		}

		// Init function library controllers
		for(URigVMLibraryNode* LibraryNode : RigVMClient.GetFunctionLibrary()->GetFunctions())
		{
			RigVMClient.GetOrCreateController(LibraryNode->GetContainedGraph());
		}
		
		if(bRecompileVM)
		{
			RecompileVM();
		}
	}

	for(UAnimNextRigVMAssetEntry* Entry : Entries)
	{
		Entry->Initialize(this);
	}

	if (IInterface_AssetUserData* OuterUserData = Cast<IInterface_AssetUserData>(GetOuter()))
	{
		if(!OuterUserData->HasAssetUserDataOfClass(UAnimNextModuleWorkspaceAssetUserData::StaticClass()))
		{
			OuterUserData->AddAssetUserDataOfClass(UAnimNextModuleWorkspaceAssetUserData::StaticClass());
		}
	}
}

void UAnimNextRigVMAssetEditorData::PostLoad()
{
	Super::PostLoad();

	GraphModels.Reset();
	
	PostLoadExternalPackages();
	RefreshExternalModels();
	Initialize(/*bRecompileVM*/false);
	
	GetRigVMClient()->RefreshAllModels(ERigVMLoadType::PostLoad, false, bIsCompiling);

	GetRigVMClient()->PatchFunctionReferencesOnLoad();
	TMap<URigVMLibraryNode*, FRigVMGraphFunctionHeader> OldHeaders;
	TArray<FName> BackwardsCompatiblePublicFunctions;
	GetRigVMClient()->PatchFunctionsOnLoad(this, BackwardsCompatiblePublicFunctions, OldHeaders);

	// delay compilation until the package has been loaded
	FCoreUObjectDelegates::OnEndLoadPackage.AddUObject(this, &UAnimNextRigVMAssetEditorData::HandlePackageDone);
}

void UAnimNextRigVMAssetEditorData::PostLoadExternalPackages()
{
	FExternalPackageHelper::LoadObjectsFromExternalPackages<UAnimNextRigVMAssetEntry>(this, [this](UAnimNextRigVMAssetEntry* InLoadedEntry)
	{
		check(IsValid(InLoadedEntry));
		InLoadedEntry->Initialize(this);
		Entries.Add(InLoadedEntry);
	});
}

void UAnimNextRigVMAssetEditorData::PostTransacted(const FTransactionObjectEvent& TransactionEvent)
{
	Super::PostTransacted(TransactionEvent);

	if (TransactionEvent.GetEventType() == ETransactionObjectEventType::UndoRedo)
	{
		BroadcastModified();
	}
}

void UAnimNextRigVMAssetEditorData::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Initialize(/*bRecompileVM*/true);
}

void UAnimNextRigVMAssetEditorData::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
	Super::GetAssetRegistryTags(Context);

	// We may not have compiled yet, so cache exports if we havent already
	if(!CachedExports.IsSet())
	{
		CachedExports = FAnimNextParameterProviderAssetRegistryExports();
		UE::AnimNext::UncookedOnly::FUtils::GetAssetParameters(this, CachedExports.GetValue());
	}

	FString TagValue;
	FAnimNextParameterProviderAssetRegistryExports::StaticStruct()->ExportText(TagValue, &CachedExports.GetValue(), nullptr, nullptr, PPF_None, nullptr);
	Context.AddTag(FAssetRegistryTag(UE::AnimNext::ExportsAnimNextAssetRegistryTag, TagValue, FAssetRegistryTag::TT_Hidden));
}

bool UAnimNextRigVMAssetEditorData::Rename(const TCHAR* NewName, UObject* NewOuter, ERenameFlags Flags)
{
	FExternalPackageHelper::FRenameExternalObjectsHelperContext Context(this, Flags);
	return Super::Rename(NewName, NewOuter, Flags);
}

void UAnimNextRigVMAssetEditorData::PreDuplicate(FObjectDuplicationParameters& DupParams)
{
	UObject::PreDuplicate(DupParams);
	FExternalPackageHelper::DuplicateExternalPackages(this, DupParams);
}

void UAnimNextRigVMAssetEditorData::HandlePackageDone(const FEndLoadPackageContext& Context)
{
	if (!Context.LoadedPackages.Contains(GetPackage()))
	{
		return;
	}
	HandlePackageDone();
}

void UAnimNextRigVMAssetEditorData::HandlePackageDone()
{
	FCoreUObjectDelegates::OnEndLoadPackage.RemoveAll(this);

	RecompileVM();

	ReconstructAllNodes(); // If this is not executed on a node for whatever reason, it will appear transparent in the editor
}

void UAnimNextRigVMAssetEditorData::RefreshAllModels(ERigVMLoadType InLoadType)
{
}

void UAnimNextRigVMAssetEditorData::OnRigVMRegistryChanged()
{
	GetRigVMClient()->RefreshAllModels(ERigVMLoadType::PostLoad, false, bIsCompiling);
	//RebuildGraphFromModel(); // TODO zzz : Move from blueprint to client
}

void UAnimNextRigVMAssetEditorData::RequestRigVMInit()
{
	// TODO zzz : How we do this on AnimNext ?
}

URigVMGraph* UAnimNextRigVMAssetEditorData::GetModel(const UEdGraph* InEdGraph) const
{
	return RigVMClient.GetModel(InEdGraph);
}

URigVMGraph* UAnimNextRigVMAssetEditorData::GetModel(const FString& InNodePath) const
{
	return RigVMClient.GetModel(InNodePath);
}

URigVMGraph* UAnimNextRigVMAssetEditorData::GetDefaultModel() const 
{
	return RigVMClient.GetDefaultModel();
}

TArray<URigVMGraph*> UAnimNextRigVMAssetEditorData::GetAllModels() const
{
	return RigVMClient.GetAllModels(true, true);
}

URigVMFunctionLibrary* UAnimNextRigVMAssetEditorData::GetLocalFunctionLibrary() const
{
	return RigVMClient.GetFunctionLibrary();
}

URigVMGraph* UAnimNextRigVMAssetEditorData::AddModel(FString InName, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	TGuardValue<bool> EnablePythonPrint(bSuspendPythonMessagesForRigVMClient, !bPrintPythonCommand);
	return RigVMClient.AddModel(InName, bSetupUndoRedo, bPrintPythonCommand);
}

bool UAnimNextRigVMAssetEditorData::RemoveModel(FString InName, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	TGuardValue<bool> EnablePythonPrint(bSuspendPythonMessagesForRigVMClient, !bPrintPythonCommand);
	return RigVMClient.RemoveModel(InName, bSetupUndoRedo, bPrintPythonCommand);
}

FRigVMGetFocusedGraph& UAnimNextRigVMAssetEditorData::OnGetFocusedGraph()
{
	return RigVMClient.OnGetFocusedGraph();
}

const FRigVMGetFocusedGraph& UAnimNextRigVMAssetEditorData::OnGetFocusedGraph() const
{
	return RigVMClient.OnGetFocusedGraph();
}

URigVMGraph* UAnimNextRigVMAssetEditorData::GetFocusedModel() const
{
	return RigVMClient.GetFocusedModel();
}

URigVMController* UAnimNextRigVMAssetEditorData::GetController(const URigVMGraph* InGraph) const
{
	return RigVMClient.GetController(InGraph);
};

URigVMController* UAnimNextRigVMAssetEditorData::GetControllerByName(const FString InGraphName) const
{
	return RigVMClient.GetControllerByName(InGraphName);
};

URigVMController* UAnimNextRigVMAssetEditorData::GetOrCreateController(URigVMGraph* InGraph)
{
	return RigVMClient.GetOrCreateController(InGraph);
};

URigVMController* UAnimNextRigVMAssetEditorData::GetController(const UEdGraph* InEdGraph) const
{
	return RigVMClient.GetController(InEdGraph);
};

URigVMController* UAnimNextRigVMAssetEditorData::GetOrCreateController(const UEdGraph* InEdGraph)
{
	return RigVMClient.GetOrCreateController(InEdGraph);
};

TArray<FString> UAnimNextRigVMAssetEditorData::GeneratePythonCommands(const FString InNewBlueprintName)
{
	return TArray<FString>();
}

void UAnimNextRigVMAssetEditorData::SetupPinRedirectorsForBackwardsCompatibility()
{
}

FRigVMClient* UAnimNextRigVMAssetEditorData::GetRigVMClient()
{
	return &RigVMClient;
}

const FRigVMClient* UAnimNextRigVMAssetEditorData::GetRigVMClient() const
{
	return &RigVMClient;
}

IRigVMGraphFunctionHost* UAnimNextRigVMAssetEditorData::GetRigVMGraphFunctionHost()
{
	return this;
}

const IRigVMGraphFunctionHost* UAnimNextRigVMAssetEditorData::GetRigVMGraphFunctionHost() const
{
	return this;
}

void UAnimNextRigVMAssetEditorData::HandleRigVMGraphAdded(const FRigVMClient* InClient, const FString& InNodePath)
{
	if(URigVMGraph* RigVMGraph = InClient->GetModel(InNodePath))
	{
		RigVMGraph->SetExecuteContextStruct(GetExecuteContextStruct());

		if(!HasAnyFlags(RF_ClassDefaultObject | RF_NeedInitialization | RF_NeedLoad | RF_NeedPostLoad) &&
			GetOuter() != GetTransientPackage())
		{
			CreateEdGraph(RigVMGraph, true);
			RequestAutoVMRecompilation();
		}
		
#if WITH_EDITOR
		if(!bSuspendPythonMessagesForRigVMClient)
		{
			const FString AssetName = RigVMGraph->GetSchema()->GetSanitizedName(GetName(), true, false);
			RigVMPythonUtils::Print(AssetName, FString::Printf(TEXT("asset.add_graph('%s')"), *RigVMGraph->GetName()));
		}
#endif
	}
}

void UAnimNextRigVMAssetEditorData::HandleRigVMGraphRemoved(const FRigVMClient* InClient, const FString& InNodePath)
{
	if(URigVMGraph* RigVMGraph = InClient->GetModel(InNodePath))
	{
		if (UAnimNextRigVMAssetEntry* Entry = FindEntryForRigVMGraph(RigVMGraph))
		{
			if (IAnimNextRigVMGraphInterface* GraphInterface = Cast<IAnimNextRigVMGraphInterface>(Entry))
			{
				GraphInterface->SetRigVMGraph(nullptr);
			}
		}
		GraphModels.Remove(RigVMGraph);

		RemoveEdGraph(RigVMGraph);
		RecompileVM();

#if WITH_EDITOR
		if(!bSuspendPythonMessagesForRigVMClient)
		{
			const FString AssetName = RigVMGraph->GetSchema()->GetSanitizedName(GetName(), true, false);
			RigVMPythonUtils::Print(AssetName, FString::Printf(TEXT("asset.add_graph('%s')"), *RigVMGraph->GetName()));
		}
#endif
	}
}

void UAnimNextRigVMAssetEditorData::HandleConfigureRigVMController(const FRigVMClient* InClient, URigVMController* InControllerToConfigure)
{
	InControllerToConfigure->OnModified().AddUObject(this, &UAnimNextRigVMAssetEditorData::HandleModifiedEvent);

	TWeakObjectPtr<UAnimNextRigVMAssetEditorData> WeakThis(this);

	// this delegate is used by the controller to retrieve the current bytecode of the VM
	InControllerToConfigure->GetCurrentByteCodeDelegate.BindLambda([WeakThis]() -> const FRigVMByteCode*
	{
		if (WeakThis.IsValid())
		{
			if(UAnimNextRigVMAsset* Asset = WeakThis->GetTypedOuter<UAnimNextRigVMAsset>())
			{
				if (Asset->VM)
				{
					return &Asset->VM->GetByteCode();
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

UObject* UAnimNextRigVMAssetEditorData::GetEditorObjectForRigVMGraph(URigVMGraph* InVMGraph) const
{
	if(InVMGraph)
	{
		if (InVMGraph->IsA<URigVMFunctionLibrary>())
		{
			return Cast<UObject>(FunctionLibraryEdGraph.Get());
		}

		const auto FindSubgraph = ([](const FString SearchGraphNodePath, URigVMEdGraph* EdGraph) -> URigVMEdGraph*
		{
			TArray<UEdGraph*> SubGraphs;
			EdGraph->GetAllChildrenGraphs(SubGraphs);
			for (UEdGraph* SubGraph : SubGraphs)
			{
				if (URigVMEdGraph* RigVMEdGraph = Cast<URigVMEdGraph>(SubGraph))
				{
					if (RigVMEdGraph->ModelNodePath == SearchGraphNodePath)
					{
						return RigVMEdGraph;
					}
				}
			}
			return nullptr;
		});

		const FString GraphNodePath = InVMGraph->GetNodePath();
		for(UAnimNextRigVMAssetEntry* Entry : Entries)
		{
			if(IAnimNextRigVMGraphInterface* GraphInterface = Cast<IAnimNextRigVMGraphInterface>(Entry))
			{
				URigVMEdGraph* EdGraph = GraphInterface->GetEdGraph();

				if (const URigVMGraph* RigVMGraph = GraphInterface->GetRigVMGraph())
				{
					if (RigVMGraph == InVMGraph)
					{
						return EdGraph;
					}
				}

				if (URigVMEdGraph* RigVMEdGraph = FindSubgraph(GraphNodePath, EdGraph))
				{
					return RigVMEdGraph;
				}
			}
		}

		for (const TObjectPtr<URigVMEdGraph>& FunctionEdGraph : FunctionEdGraphs)
		{
			if (FunctionEdGraph->ModelNodePath == GraphNodePath)
			{
				return FunctionEdGraph;
			}

			if (URigVMEdGraph* RigVMEdGraph = FindSubgraph(GraphNodePath, FunctionEdGraph))
			{
				return RigVMEdGraph;
			}
		}
	}
	return nullptr;
}

URigVMGraph* UAnimNextRigVMAssetEditorData::GetRigVMGraphForEditorObject(UObject* InObject) const
{
	if(const URigVMEdGraph* Graph = Cast<URigVMEdGraph>(InObject))
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

FRigVMGraphFunctionStore* UAnimNextRigVMAssetEditorData::GetRigVMGraphFunctionStore()
{
	return &GraphFunctionStore;
}

const FRigVMGraphFunctionStore* UAnimNextRigVMAssetEditorData::GetRigVMGraphFunctionStore() const
{
	return &GraphFunctionStore;
}

TObjectPtr<URigVMGraph> UAnimNextRigVMAssetEditorData::CreateContainedGraphModel(URigVMCollapseNode* CollapseNode, const FName& Name)
{
	check(CollapseNode);

	TObjectPtr<URigVMGraph> Model = NewObject<URigVMGraph>(CollapseNode, Name);
	Model->SetSchemaClass(RigVMClient.GetDefaultSchemaClass());

	URigVMGraph* CollapseNodeModelRootGraph = CollapseNode->GetRootGraph();
	check(CollapseNodeModelRootGraph);

	// If we are a transient asset, dont use external packages
	if (!CollapseNodeModelRootGraph->HasAnyFlags(RF_Transient))
	{
		Model->SetExternalPackage(CollapseNodeModelRootGraph->GetExternalPackage());
	}

	return Model;
}

void UAnimNextRigVMAssetEditorData::RecompileVMIfRequired()
{
	if (bVMRecompilationRequired)
	{
		RecompileVM();
	}
}

void UAnimNextRigVMAssetEditorData::RequestAutoVMRecompilation()
{
	bVMRecompilationRequired = true;
	if (bAutoRecompileVM && VMRecompilationBracket == 0)
	{
		RecompileVMIfRequired();
	}
}

void UAnimNextRigVMAssetEditorData::SetAutoVMRecompile(bool bAutoRecompile)
{
	bAutoRecompileVM = bAutoRecompile;
}

bool UAnimNextRigVMAssetEditorData::GetAutoVMRecompile() const
{
	return bAutoRecompileVM;
}

void UAnimNextRigVMAssetEditorData::IncrementVMRecompileBracket()
{
	VMRecompilationBracket++;
}

void UAnimNextRigVMAssetEditorData::DecrementVMRecompileBracket()
{
	if (VMRecompilationBracket == 1)
	{
		if (bAutoRecompileVM)
		{
			RecompileVMIfRequired();
		}
		VMRecompilationBracket = 0;

		if (InteractionBracketFinished.IsBound())
		{
			InteractionBracketFinished.Broadcast(this);
		}
	}
	else if (VMRecompilationBracket > 0)
	{
		VMRecompilationBracket--;
	}
}

void UAnimNextRigVMAssetEditorData::HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject)
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
				CreateEdGraphForCollapseNode(CollapseNode, false);
				break;
			}
			RequestAutoVMRecompilation();
			break;
	}
	case ERigVMGraphNotifType::NodeRemoved:
	{
		if (URigVMCollapseNode* CollapseNode = Cast<URigVMCollapseNode>(InSubject))
		{
			RemoveEdGraphForCollapseNode(CollapseNode, false);
			break;
		}
		RequestAutoVMRecompilation();
		break;
	}
	case ERigVMGraphNotifType::LinkAdded:
	case ERigVMGraphNotifType::LinkRemoved:
	case ERigVMGraphNotifType::PinArraySizeChanged:
	case ERigVMGraphNotifType::PinDirectionChanged:
		{
			RequestAutoVMRecompilation();
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

TArray<UEdGraph*> UAnimNextRigVMAssetEditorData::GetAllEdGraphs() const
{
	TArray<UEdGraph*> Graphs;
	for(UAnimNextRigVMAssetEntry* Entry : Entries)
	{
		if(IAnimNextRigVMGraphInterface* GraphInterface = Cast<IAnimNextRigVMGraphInterface>(Entry))
		{
			Graphs.Add(GraphInterface->GetEdGraph());
		}
	}
	for (URigVMEdGraph* RigVMEdGraph : FunctionEdGraphs)
	{
		Graphs.Add(RigVMEdGraph);
	}

	return Graphs;
}

UAnimNextRigVMAssetEntry* UAnimNextRigVMAssetLibrary::FindEntry(UAnimNextRigVMAsset* InAsset, FName InName)
{
	return UE::AnimNext::UncookedOnly::FUtils::GetEditorData(InAsset)->FindEntry(InName);
}

UAnimNextRigVMAssetEntry* UAnimNextRigVMAssetEditorData::FindEntry(FName InName) const
{
	if(InName == NAME_None)
	{
		ReportError(TEXT("UAnimNextRigVMAssetEditorData::FindEntry: Invalid name supplied."));
		return nullptr;
	}

	const TObjectPtr<UAnimNextRigVMAssetEntry>* FoundEntry = Entries.FindByPredicate([InName](const UAnimNextRigVMAssetEntry* InEntry)
	{
		return InEntry->GetEntryName() == InName;
	});

	return FoundEntry != nullptr ? *FoundEntry : nullptr;
}

bool UAnimNextRigVMAssetLibrary::RemoveEntry(UAnimNextRigVMAsset* InAsset, UAnimNextRigVMAssetEntry* InEntry,  bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	return UE::AnimNext::UncookedOnly::FUtils::GetEditorData(InAsset)->RemoveEntry(InEntry, bSetupUndoRedo, bPrintPythonCommand);
}

bool UAnimNextRigVMAssetEditorData::RemoveEntry(UAnimNextRigVMAssetEntry* InEntry, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	if(InEntry == nullptr)
	{
		ReportError(TEXT("UAnimNextRigVMAssetEditorData::RemoveEntry: Invalid entry supplied."));
		return false;
	}
	
	TObjectPtr<UAnimNextRigVMAssetEntry>* EntryToRemovePtr = Entries.FindByKey(InEntry);
	if(EntryToRemovePtr == nullptr)
	{
		ReportError(TEXT("UAnimNextRigVMAssetEditorData::RemoveEntry: Asset does not contain the supplied entry."));
		return false;
	}

	if(bSetupUndoRedo)
	{
		Modify();
	}

	// Remove from internal array
	UAnimNextRigVMAssetEntry* EntryToRemove = *EntryToRemovePtr;

	bool bResult = true;
	if(const IAnimNextRigVMGraphInterface* GraphInterface = Cast<IAnimNextRigVMGraphInterface>(EntryToRemove))
	{
		// Remove any graphs
		if(URigVMGraph* RigVMGraph = GraphInterface->GetRigVMGraph())
		{
			TGuardValue<bool> EnablePythonPrint(bSuspendPythonMessagesForRigVMClient, !bPrintPythonCommand);
			TGuardValue<bool> DisableAutoCompile(bAutoRecompileVM, false);
			bResult = RigVMClient.RemoveModel(RigVMGraph->GetNodePath(), bSetupUndoRedo);
		}
	}

	if (bSetupUndoRedo)
	{
		EntryToRemove->Modify();
	}
	Entries.Remove(EntryToRemove);
	RefreshExternalModels();

	// This will cause any external package to be removed when saved
	EntryToRemove->MarkAsGarbage();

	BroadcastModified();

	return bResult;
}

bool UAnimNextRigVMAssetLibrary::RemoveEntries(UAnimNextRigVMAsset* InAsset, const TArray<UAnimNextRigVMAssetEntry*>& InEntries,  bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	return UE::AnimNext::UncookedOnly::FUtils::GetEditorData(InAsset)->RemoveEntries(InEntries, bSetupUndoRedo, bPrintPythonCommand);
}

bool UAnimNextRigVMAssetEditorData::RemoveEntries(const TArray<UAnimNextRigVMAssetEntry*>& InEntries, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	bool bResult = false;
	{
		TGuardValue<bool> DisableEditorDataNotifications(bSuspendEditorDataNotifications, true);
		TGuardValue<bool> DisableAutoCompile(bAutoRecompileVM, false);
		for(UAnimNextRigVMAssetEntry* Entry : InEntries)
		{
			bResult |= RemoveEntry(Entry, bSetupUndoRedo, bPrintPythonCommand);
		}
	}

	BroadcastModified();

	return bResult;
}

UObject* UAnimNextRigVMAssetEditorData::CreateNewSubEntry(UAnimNextRigVMAssetEditorData* InEditorData, TSubclassOf<UObject> InClass)
{
	UObject* NewEntry = NewObject<UObject>(InEditorData, InClass.Get(), NAME_None, RF_Transactional);
	// If we are a transient asset, dont use external packages
	UAnimNextRigVMAsset* Asset = UE::AnimNext::UncookedOnly::FUtils::GetAsset(InEditorData);
	check(Asset);
	if(!Asset->HasAnyFlags(RF_Transient))
	{
		FExternalPackageHelper::SetPackagingMode(NewEntry, InEditorData, true, false, PKG_None);
	}
	return NewEntry;
}

UAnimNextRigVMAssetEntry* UAnimNextRigVMAssetEditorData::FindEntryForRigVMGraph(const URigVMGraph* InRigVMGraph) const
{
	for(UAnimNextRigVMAssetEntry* Entry : Entries)
	{
		if(IAnimNextRigVMGraphInterface* GraphInterface = Cast<IAnimNextRigVMGraphInterface>(Entry))
		{
			if (const URigVMGraph* RigVMGraph = GraphInterface->GetRigVMGraph())
			{
				if(RigVMGraph == InRigVMGraph)
				{
					return Entry;
				}
			}
		}
	}

	return nullptr;
}

UAnimNextRigVMAssetEntry* UAnimNextRigVMAssetEditorData::FindEntryForRigVMEdGraph(const URigVMEdGraph* InRigVMEdGraph) const
{
	for (UAnimNextRigVMAssetEntry* Entry : Entries)
	{
		if (IAnimNextRigVMGraphInterface* GraphInterface = Cast<IAnimNextRigVMGraphInterface>(Entry))
		{
			if (GraphInterface->GetEdGraph() == InRigVMEdGraph)
			{
				return Entry;
			}
		}
	}

	return nullptr;
}

void UAnimNextRigVMAssetEditorData::RefreshExternalModels()
{
	GraphModels.Reset();

	for (UAnimNextRigVMAssetEntry* Entry : Entries)
	{
		if (IAnimNextRigVMGraphInterface* GraphInterface = Cast<IAnimNextRigVMGraphInterface>(Entry))
		{
			if(URigVMGraph* Model = GraphInterface->GetRigVMGraph())
			{
				GraphModels.Add(Model);
			}
		}
	}

}
