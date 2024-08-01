// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/CustomizableObjectCompiler.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ClothConfig.h"
#include "HAL/RunnableThread.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Materials/MaterialInterface.h"
#include "MessageLogModule.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectPrivate.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableObjectSystem.h"
#include "MuCO/ICustomizableObjectModule.h"
#include "MuCOE/CustomizableObjectCompileRunnable.h"
#include "MuCOE/CustomizableObjectEditorLogger.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceImage.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/ICustomizableObjectPopulationModule.h"
#include "MuCOE/Nodes/CustomizableObjectNodeObjectGroup.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTable.h"
#include "MuCOE/CustomizableObjectEditorModule.h"
#include "UObject/ICookInfo.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Interfaces/ITargetPlatform.h"
#include "Misc/App.h"
#include "MuCO/CustomizableObjectPrivate.h"
#include "MuCO/CustomizableObjectSystemPrivate.h"
#include "MuR/Model.h"
#include "MuR/ModelPrivate.h"
#include "MuCOE/CustomizableObjectVersionBridge.h"

class UTexture2D;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"

#define UE_MUTABLE_COMPILE_REGION		TEXT("Mutable Compile")
#define UE_MUTABLE_PRELOAD_REGION		TEXT("Mutable Preload")
#define UE_MUTABLE_SAVEDD_REGION		TEXT("Mutable SaveDD")

UCustomizableObjectNodeObject* GetRootNode(UCustomizableObject* Object, bool &bOutMultipleBaseObjectsFound);


bool FCustomizableObjectCompiler::Tick(bool bBlocking)
{
	MUTABLE_CPUPROFILER_SCOPE(FCustomizableObjectCompiler::Tick);

	bool bFinished = true;

	if (TryPopCompileRequest())
	{
		bFinished = false;
	}

	if (AsynchronousStreamableHandlePtr)
	{
		bFinished = false;

		if (bBlocking)
		{
			AsynchronousStreamableHandlePtr->CancelHandle();
			UCustomizableObjectSystem::GetInstance()->GetPrivate()->StreamableManager.RequestSyncLoad(ArrayAssetToStream);
			PreloadingReferencerAssetsCallback(false);
		}
	}

	if (CompileTask)
	{
		bFinished = false;
		CompileTask->Tick();

		if (CompileTask->IsCompleted())
		{
			FinishCompilationTask();

			if (SaveDDTask.IsValid())
			{
				SaveCODerivedData();
			}
		}
	}

	if (SaveDDTask)
	{
		bFinished = false;

		if (SaveDDTask->IsCompleted())
		{
			FinishSavingDerivedDataTask();
		}
	}

	if (bFinished && CurrentRequest.IsValid())
	{
		bFinished = CompileRequests.IsEmpty();

		CompleteRequest(ECompilationStatePrivate::Completed, GetCompilationResult());
	}

	if (CompileNotificationHandle.IsValid())
	{
		const int32 NumCompletedRequests = NumCompilationRequests - GetNumRemainingWork();
		FSlateNotificationManager::Get().UpdateProgressNotification(CompileNotificationHandle, NumCompletedRequests, NumCompilationRequests);
	}

	return bFinished;
}


int32 FCustomizableObjectCompiler::GetNumRemainingWork() const
{
	return static_cast<int32>(CurrentRequest.IsValid()) + CompileRequests.Num();
}


void FCustomizableObjectCompiler::PreloadingReferencerAssetsCallback(bool bAsync)
{
	check(IsInGameThread());

	UpdateArrayGCProtect();

	if (AsynchronousStreamableHandlePtr)
	{
		AsynchronousStreamableHandlePtr.Reset();
	}

	UE_LOG(LogMutable, Verbose, TEXT("PROFILE: [ %16.8f ] Preload asynchronously assets end."), FPlatformTime::Seconds());
	TRACE_END_REGION(UE_MUTABLE_PRELOAD_REGION);

	CompileInternal(bAsync);
}


void FCustomizableObjectCompiler::Compile(const TSharedRef<FCompilationRequest>& InCompileRequest)
{
	TRACE_BEGIN_REGION(UE_MUTABLE_COMPILE_REGION);
	
	check(IsInGameThread());
	check(!CurrentRequest);
	
	CurrentRequest = InCompileRequest.ToSharedPtr();
	CurrentObject = InCompileRequest->GetCustomizableObject();
	CurrentOptions = InCompileRequest->GetCompileOptions();

	if (!CurrentObject)
	{
		UE_LOG(LogMutable, Warning, TEXT("Failed to compile Customizable Object. Object is missing."));
		CompleteRequest(ECompilationStatePrivate::Completed, ECompilationResultPrivate::Errors);
		return;
	}

	if (CurrentObject->GetPrivate()->CompilationState == ECompilationStatePrivate::InProgress)
	{
		UE_LOG(LogMutable, Warning, TEXT("Failed to compile Customizable Object [%s]. Object already being compiled."), *CurrentObject->GetName());
		CurrentObject = nullptr; // Someone else is compiling the CO. Invalidate the CurrentObject pointer to avoid changing the state of the ongoing compilation.
		CompleteRequest(ECompilationStatePrivate::Completed, ECompilationResultPrivate::Errors);
		return;
	}

	if (!UCustomizableObjectSystem::IsActive())
	{
		UE_LOG(LogMutable, Warning, TEXT("Failed to compile Customizable Object [%s]. Mutable is disabled. To enable it set the CVar Mutable.Enabled to true."), *CurrentObject->GetName());
		CompleteRequest(ECompilationStatePrivate::Completed, ECompilationResultPrivate::Errors);
		return;
	}

	UCustomizableObject* RootObject = GetRootObject(CurrentObject);
	check(RootObject);

	if (RootObject->VersionBridge && !RootObject->VersionBridge->GetClass()->ImplementsInterface(UCustomizableObjectVersionBridgeInterface::StaticClass()))
	{
		UE_LOG(LogMutable, Warning, TEXT("In Customizable Object [%s], the VersionBridge asset [%s] does not implement the required UCustomizableObjectVersionBridgeInterface."),
			*RootObject->GetName(), *RootObject->VersionBridge.GetName());
		CompleteRequest(ECompilationStatePrivate::Completed, ECompilationResultPrivate::Errors);
		return;
	}

	if (!CurrentOptions.bIsCooking && IsRunningCookCommandlet())
	{
		UE_LOG(LogMutable, Display, TEXT("Editor compilation suspended for Customizable Object [%s]. Can not compile COs when the cook commandlet is running. "), *CurrentObject->GetName());
		CompleteRequest(ECompilationStatePrivate::Completed, ECompilationResultPrivate::Errors);
		return;
	}

	UCustomizableObjectSystem* System = UCustomizableObjectSystem::GetInstanceChecked();

	check(!CurrentObject->GetPrivate()->IsLocked());

	// Lock object during asynchronous asset loading to avoid instance/mip updates and reentrant compilations
	if (!System->LockObject(CurrentObject))
	{
		FString Message = FString::Printf(TEXT("Customizable Object %s is already being compiled or updated. Please wait a few seconds and try again."), *CurrentObject->GetName());
		UE_LOG(LogMutable, Warning, TEXT("%s"), *Message);
		
		FNotificationInfo Info(LOCTEXT("CustomizableObjectBeingCompilerOrUpdated", "Customizable Object compile and/or update still in process. Please wait a few seconds and try again."));
		Info.bFireAndForget = true;
		Info.bUseThrobber = true;
		Info.FadeOutDuration = 1.0f;
		Info.ExpireDuration = 1.0f;
		FSlateNotificationManager::Get().AddNotification(Info);

		CurrentObject = nullptr; // Someone else is compiling the CO. Invalidate the CurrentObject pointer to avoid changing the state of the ongoing compilation.
		CompleteRequest(ECompilationStatePrivate::Completed, ECompilationResultPrivate::Errors);
		return;
	}

	SetCompilationState(ECompilationStatePrivate::InProgress, ECompilationResultPrivate::Unknown);

	CompilationStartTime = FPlatformTime::Seconds();

	// Now that we know for sure that the CO is locked and there are no pending updates of instances using the CO,
	// destroy any live update instances, as they become invalid when recompiling the CO
	for (TObjectIterator<UCustomizableObjectInstance> It; It; ++It)
	{
		UCustomizableObjectInstance* Instance = *It;
		if (IsValid(Instance) &&
			Instance->GetCustomizableObject() == CurrentObject)
		{
			Instance->DestroyLiveUpdateInstance();
		}
	}

	// If we don't have the target platform yet (in editor) we need to get it
	if (!CurrentOptions.TargetPlatform)
	{
		check(!CurrentOptions.bIsCooking);

		CurrentOptions.TargetPlatform = GetTargetPlatformManagerRef().GetRunningTargetPlatform();
		check(CurrentOptions.TargetPlatform != nullptr);
	}

	UE_LOG(LogMutable, Display, TEXT("Compiling Customizable Object %s for platform %s."), *CurrentObject->GetName(), *CurrentOptions.TargetPlatform->PlatformName());

	if (CurrentOptions.bForceLargeLODBias)
	{
		UE_LOG(LogMutable, Display, TEXT("Compiling Customizable Object with %d LODBias."), CurrentOptions.DebugBias);
	}

	// Create and update compilation progress notification
	const FText UpdateMsg = FText::FromString(FString::Printf(TEXT("Compiling Customizable Objects:\n%s"), *CurrentObject->GetName()));
	if (!CompileNotificationHandle.IsValid())
	{
		CompileNotificationHandle = FSlateNotificationManager::Get().StartProgressNotification(UpdateMsg, NumCompilationRequests);
	}
	else
	{
		const int32 NumCompletedRequests = NumCompilationRequests - GetNumRemainingWork();
		FSlateNotificationManager::Get().UpdateProgressNotification(CompileNotificationHandle, NumCompletedRequests, NumCompilationRequests, UpdateMsg);
	}
	
	TRACE_BEGIN_REGION(UE_MUTABLE_PRELOAD_REGION);
	UE_LOG(LogMutable, Verbose, TEXT("PROFILE: [ %16.8f ] Preload asynchronously assets start."), FPlatformTime::Seconds());

	CleanCachedReferencers();
	UpdateArrayGCProtect();
	TArray<FName> ArrayReferenceNames;
	AddCachedReferencers(*CurrentObject->GetOuter()->GetPathName(), ArrayReferenceNames);

	ArrayAssetToStream.Empty();
	for (FAssetData& Element : ArrayAssetData)
	{
		ArrayAssetToStream.Add(Element.GetSoftObjectPath());
	}

	bool bAssetsLoaded = true;

	const bool bAsync = InCompileRequest->IsAsyncCompilation();
	if (ArrayAssetToStream.Num() > 0)
	{
		// Customizations are marked as editoronly on load and are not packaged into the runtime game by default.
		// The ones that need to be kept will be copied into SoftObjectPath on the object during save.
		FCookLoadScope CookLoadScope(ECookLoadType::EditorOnly);

		FStreamableManager& Streamable = System->GetPrivate()->StreamableManager;

		if (bAsync && !CurrentOptions.bIsCooking)
		{
			AddCompileNotification(LOCTEXT("LoadingReferencerAssets", "Loading assets"));

			AsynchronousStreamableHandlePtr = Streamable.RequestAsyncLoad(ArrayAssetToStream, FStreamableDelegate::CreateRaw(this, &FCustomizableObjectCompiler::PreloadingReferencerAssetsCallback, bAsync));
			bAssetsLoaded = false;
		}
		else
		{
			Streamable.RequestSyncLoad(ArrayAssetToStream);
		}
	}

	if (bAssetsLoaded)
	{
		PreloadingReferencerAssetsCallback(bAsync);
	}
}


void FCustomizableObjectCompiler::Compile(const TArray<TSharedRef<FCompilationRequest>>& InCompileRequests)
{
	NumCompilationRequests += InCompileRequests.Num();
	CompileRequests.Append(InCompileRequests);
}


bool FCustomizableObjectCompiler::IsTickable() const
{
	return NumCompilationRequests > 0 || CurrentRequest;
}


void FCustomizableObjectCompiler::Tick(float InDeltaTime)
{
	MUTABLE_CPUPROFILER_SCOPE(FCustomizableObjectCompiler::Tick);
	Tick();
}


TStatId FCustomizableObjectCompiler::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FCustomizableObjectCompiler, STATGROUP_Tickables);
}


void FCustomizableObjectCompiler::TickCook(float DeltaTime, bool bCookCompete)
{
	MUTABLE_CPUPROFILER_SCOPE(FCustomizableObjectCompiler::TickCook);
	Tick();
}


bool FCustomizableObjectCompiler::IsRequestQueued(const TSharedRef<FCompilationRequest>& InCompileRequest) const
{
	return (CurrentRequest == InCompileRequest) || 
		(CompileRequests.ContainsByPredicate([&InCompileRequest](const TSharedRef<FCompilationRequest>& Other)
		{
			return InCompileRequest.Get() == Other.Get(); // Compare the content of the request not the ref
		}));
}


void FCustomizableObjectCompiler::AddReferencedObjects(FReferenceCollector& Collector)
{
	// While compilation takes place, no COs involved can be garbage-collected
	const int32 MaxIndex = ArrayGCProtect.Num();

	for (int32 i = 0; i < MaxIndex; ++i)
	{
		Collector.AddReferencedObject(ArrayGCProtect[i]);
	}

	if (CurrentObject)
	{
		Collector.AddReferencedObject(CurrentObject);
	}
}


void FCustomizableObjectCompiler::UpdateArrayGCProtect()
{
	check(IsInGameThread());

	const int32 MaxIndex = ArrayAssetData.Num();
	ArrayGCProtect.SetNum(MaxIndex);

	for (int i = 0; i < MaxIndex; ++i)
	{
		ArrayGCProtect[i] = Cast<UCustomizableObject>(ArrayAssetData[i].GetAsset());
	}
}


void FCustomizableObjectCompiler::ProcessChildObjectsRecursively(UCustomizableObject* ParentObject, FMutableGraphGenerationContext& GenerationContext)
{
	TArray<FName> ArrayReferenceNames;
	AddCachedReferencers(*ParentObject->GetOuter()->GetPathName(), ArrayReferenceNames);
	UpdateArrayGCProtect();

	bool bMultipleBaseObjectsFound = false;

	for (const FName& ReferenceName : ArrayReferenceNames)
	{
		if (ArrayAlreadyProcessedChild.Contains(ReferenceName))
		{
			continue;
		}

		const FAssetData* AssetData = GetCachedAssetData(ReferenceName.ToString());

		UCustomizableObject* ChildObject = AssetData ? Cast<UCustomizableObject>(AssetData->GetAsset()) : nullptr;
		if (!ChildObject || ChildObject->HasAnyFlags(RF_Transient))
		{
			ArrayAlreadyProcessedChild.Add(ReferenceName);
			continue;
		}

		UCustomizableObjectNodeObject* Root = GetRootNode(ChildObject, bMultipleBaseObjectsFound);
		if (Root->ParentObject != ParentObject)
		{
			continue;
		}

		if (ChildObject->VersionStruct.IsValid())
		{
			if (!GenerationContext.RootVersionBridge)
			{
				UE_LOG(LogMutable, Warning, TEXT("The child Customizable Object [%s] defines its VersionStruct Property but its root CustomizableObject doesn't define the VersionBridge property. There's no way to verify the VersionStruct has to be included in this compilation, so the child CustomizableObject will be omitted."), 
					*ChildObject->GetName());
				continue;
			}

			ICustomizableObjectVersionBridgeInterface* CustomizableObjectVersionBridgeInterface = Cast<ICustomizableObjectVersionBridgeInterface>(GenerationContext.RootVersionBridge);

			if (CustomizableObjectVersionBridgeInterface)
			{
				if (!CustomizableObjectVersionBridgeInterface->IsVersionStructIncludedInCurrentRelease(ChildObject->VersionStruct))
				{
					continue;
				}
			}
			else
			{
				// This should never happen as the ICustomizableObjectVersionBridgeInterface was already checked at the start of the compilation
				ensure(false);
			}
		}

		ArrayAlreadyProcessedChild.Add(ReferenceName);

		if (!bMultipleBaseObjectsFound)
		{
			if (const FGroupNodeIdsTempData* GroupGuid = GenerationContext.DuplicatedGroupNodeIds.FindPair(ParentObject, FGroupNodeIdsTempData(Root->ParentObjectGroupId)))
			{
				Root->ParentObjectGroupId = GroupGuid->NewGroupNodeId;
			}

			GenerationContext.GroupIdToExternalNodeMap.Add(Root->ParentObjectGroupId, Root);

			TArray<UCustomizableObjectNodeObjectGroup*> GroupNodes;
			ChildObject->GetPrivate()->GetSource()->GetNodesOfClass<UCustomizableObjectNodeObjectGroup>(GroupNodes);

			if (GroupNodes.Num() > 0) // Only grafs with group nodes should have child grafs
			{
				for (int32 i = 0; i < GroupNodes.Num(); ++i)
				{
					const FGuid NodeId = GenerationContext.GetNodeIdUnique(GroupNodes[i]);
					if (NodeId != GroupNodes[i]->NodeGuid)
					{
						GenerationContext.DuplicatedGroupNodeIds.Add(ChildObject, FGroupNodeIdsTempData(GroupNodes[i]->NodeGuid, NodeId));
						GroupNodes[i]->NodeGuid = NodeId;
					}
				}

				ProcessChildObjectsRecursively(ChildObject, GenerationContext);
			}
		}
	}
}


void FCustomizableObjectCompiler::DisplayParameterWarning(FMutableGraphGenerationContext& GenerationContext)
{
	for (const TPair<FString, TArray<const UObject*>>& It : GenerationContext.ParameterNamesMap)
	{
		if (It.Key == "")
		{
			FText MessageWarning = LOCTEXT("NodeWithNoName", ". There is at least one node with no name.");
			CompilerLog(MessageWarning, It.Value, EMessageSeverity::Warning, true);
		}
		else if (It.Value.Num() > 1)
		{
			FText MessageWarning = FText::Format(LOCTEXT("NodeWithRepeatedName", ". Several nodes have repeated name \"{0}\""), FText::FromString(It.Key));
			CompilerLog(MessageWarning, It.Value, EMessageSeverity::Warning, true);
		}
	}
}


void FCustomizableObjectCompiler::DisplayDuplicatedNodeIdsWarning(FMutableGraphGenerationContext & GenerationContext)
{
	for (const TPair<FGuid, TArray<const UObject*>>& It : GenerationContext.NodeIdsMap)
	{
		if (It.Value.Num() > 1)
		{
			FText MessageWarning = LOCTEXT("NodeWithRepeatedIds", ". Several nodes have repeated NodeIds, reconstruct the nodes.");
			CompilerLog(MessageWarning, It.Value, EMessageSeverity::Warning, true);
		}
	}
}


void FCustomizableObjectCompiler::DisplayUnnamedNodeObjectWarning(FMutableGraphGenerationContext& GenerationContext)
{
	FText Message = LOCTEXT("Unnamed Node Object", "Unnamed Node Object");
	for (const UCustomizableObjectNode* It : GenerationContext.NoNameNodeObjectArray)
	{
		CompilerLog(Message, It, EMessageSeverity::Warning, true);
	}
}


void FCustomizableObjectCompiler::DisplayOrphanNodesWarning(FMutableGraphGenerationContext& GenerationContext)
{
	for (const TPair<FGeneratedKey, FGeneratedData>& It : GenerationContext.Generated)
	{
		if (const UCustomizableObjectNode* Node = Cast<UCustomizableObjectNode>(It.Value.Source))
		{
			if (Node->GetAllOrphanPins().Num() > 0)
			{
				CompilerLog(LOCTEXT("OrphanPinsWarningCompiler", "Node contains deprecated pins"), Node, EMessageSeverity::Warning, false);
			}
		}
	}
}


mu::NodeObjectPtr FCustomizableObjectCompiler::GenerateMutableRoot( 
	UCustomizableObject* Object, 
	FMutableGraphGenerationContext& GenerationContext, 
	FText& ErrorMsg, 
	bool& bOutIsRootObject)
{
	check(Object);
	
	if (!Object->GetPrivate()->GetSource())
	{
		ErrorMsg = LOCTEXT("NoSource", "Object with no valid graph found. Object not build.");

		if (IsRunningCookCommandlet() || IsRunningCookOnTheFly())
		{
			UE_LOG(LogMutable, Warning, TEXT("Compilation failed! Missing EDITORONLY data for Customizable Object [%s]. The object might have been loaded outside the Cooking context."), *Object->GetName());
		}

		return nullptr;
	}

	bool bMultipleBaseObjectsFound;
	UCustomizableObjectNodeObject* Root = GetRootNode(Object, bMultipleBaseObjectsFound);

	if (bMultipleBaseObjectsFound)
	{
		ErrorMsg = LOCTEXT("MultipleBaseRoot","Multiple base object nodes found.");
		return nullptr;
	}

	if (!Root)
	{
		ErrorMsg = LOCTEXT("NoRootBase","No base object node found. Object not built.");
		return nullptr;
	}

	bOutIsRootObject = Root->ParentObject == nullptr;

	UCustomizableObject* ActualRootObject = GetRootObject(Object);
	check(ActualRootObject);

	GenerationContext.RootVersionBridge = ActualRootObject->VersionBridge;

	UCustomizableObjectNodeObject* ActualRoot = GetRootNode(ActualRootObject, bMultipleBaseObjectsFound);

	if (bMultipleBaseObjectsFound)
	{
		ErrorMsg = LOCTEXT("MultipleBaseActualRoot", "Multiple base object nodes found.");
		return nullptr;
	}

	if (!ActualRoot)
	{
		ErrorMsg = LOCTEXT("NoActualRootBase", "No base object node found in root Customizable Object. Object not built.");
		return nullptr;
	}

	ArrayAlreadyProcessedChild.Empty();

	if (Root->ObjectName.IsEmpty())
	{
		GenerationContext.NoNameNodeObjectArray.AddUnique(Root);
	}

	if ((Object->MeshCompileType == EMutableCompileMeshType::Full) || CurrentOptions.bIsCooking)
	{
		if (Root->ParentObject!=nullptr && CurrentOptions.bIsCooking)
		{
			// This happens while packaging.
			return nullptr;
		}

		// We cannot load while saving. This should only happen in cooking and all assets should have been preloaded.
		if (!GIsSavingPackage)
		{
			UE_LOG(LogMutable, Verbose, TEXT("PROFILE: [ %16.8f ] Begin search for children."), FPlatformTime::Seconds());

			// The object doesn't reference a root object but is a root object, look for all the objects that reference it and get their root nodes
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
			ProcessChildObjectsRecursively(ActualRootObject, GenerationContext);
			UE_LOG(LogMutable, Verbose, TEXT("PROFILE: [ %16.8f ] End search for children."), FPlatformTime::Seconds());
		}
	}
	else
	{
		// Local, local with children and working set modes: add parents until whole CO graph root
		TArray<UCustomizableObjectNodeObject*> ArrayNodeObject;
		TArray<UCustomizableObject*> ArrayCustomizableObject;
		
		if (!GetParentsUntilRoot(Object, ArrayNodeObject, ArrayCustomizableObject))
		{
			CompilerLog(LOCTEXT("SkeletalMeshCycleFound", "Error! Cycle detected in the Customizable Object hierarchy."), Root);
			return nullptr;
		}

		if ((Object->MeshCompileType == EMutableCompileMeshType::AddWorkingSetNoChildren) ||
			(Object->MeshCompileType == EMutableCompileMeshType::AddWorkingSetAndChildren))
		{
			const int32 MaxIndex = Object->WorkingSet.Num();
			for (int32 i = 0; i < MaxIndex; ++i)
			{
				if (UCustomizableObject* WorkingSetObject = Object->WorkingSet[i].LoadSynchronous())
				{
					ArrayCustomizableObject.Reset();

					if (!GetParentsUntilRoot(WorkingSetObject, ArrayNodeObject, ArrayCustomizableObject))
					{
						CompilerLog(LOCTEXT("NoReferenceMesh", "Error! Cycle detected in the Customizable Object hierarchy."), Root);
						return nullptr;
					}
				}
			}
		}

		if ((Object->MeshCompileType == EMutableCompileMeshType::LocalAndChildren) ||
			(Object->MeshCompileType == EMutableCompileMeshType::AddWorkingSetAndChildren))
		{
			TArray<UCustomizableObjectNodeObjectGroup*> GroupNodes;
			Object->GetPrivate()->GetSource()->GetNodesOfClass<UCustomizableObjectNodeObjectGroup>(GroupNodes);

			if (GroupNodes.Num() > 0) // Only graphs with group nodes should have child graphs
			{
				FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
				ProcessChildObjectsRecursively(Object, GenerationContext);
			}
		}

		for (int32 i = 0; i < ArrayNodeObject.Num(); ++i)
		{
			if (GenerationContext.GroupIdToExternalNodeMap.FindKey(ArrayNodeObject[i]) == nullptr)
			{
				GenerationContext.GroupIdToExternalNodeMap.Add(ArrayNodeObject[i]->ParentObjectGroupId, ArrayNodeObject[i]);
			}
		}
	}

	// Ensure that the CO has a valid AutoLODStrategy on the ActualRoot.
	if (ActualRoot->AutoLODStrategy == ECustomizableObjectAutomaticLODStrategy::Inherited)
	{
		CompilerLog(LOCTEXT("RootInheritsFromParent", "Error! Base CustomizableObject's LOD Strategy can't be set to 'Inherit from parent object'"), ActualRoot, EMessageSeverity::Error);
		return nullptr;
	}

	if (ActualRootObject->GetPrivate()->MutableMeshComponents.IsEmpty())
	{
		CompilerLog(LOCTEXT("NoComponentsError", "Error! There are no components defined in the Object Properties Tab."), ActualRoot, EMessageSeverity::Error);
		return nullptr;
	}

	TArray<const FName> ComponentNames;

	// Make sure we have a valid Name, Reference SkeletalMesh and Skeleton for each component
	for (int32 ComponentIndex = 0; ComponentIndex < ActualRootObject->GetPrivate()->MutableMeshComponents.Num(); ++ComponentIndex)
	{
		FName ComponentName = ActualRootObject->GetPrivate()->MutableMeshComponents[ComponentIndex].Name;

		if (ComponentName.IsNone())
		{
			CompilerLog(LOCTEXT("EmptyComponentNameError", "Error! Missing name in a component of the Customizable Object."), ActualRoot, EMessageSeverity::Error);
			return nullptr;
		}
		else if (ComponentNames.Contains(ComponentName))
		{
			CompilerLog(FText::Format(LOCTEXT("RepeatedComponentName", "Error! Repeated name [{0}] used in more than one Component"),
				FText::FromName(ComponentName)), ActualRoot, EMessageSeverity::Error);
			return nullptr;
		}

		ComponentNames.Add(ComponentName);

		USkeletalMesh* RefSkeletalMesh = ActualRootObject->GetRefSkeletalMesh(ComponentIndex);
		if (!RefSkeletalMesh)
		{
			CompilerLog(LOCTEXT("NoReferenceMeshObjectTab", "Error! Missing reference mesh in the Object Properties Tab"), ActualRoot, EMessageSeverity::Error);
			return nullptr;
		}

		USkeleton* RefSkeleton = RefSkeletalMesh->GetSkeleton();
		if(!RefSkeleton)
		{
			FText Msg = FText::Format(LOCTEXT("NoReferenceSkeleton", "Error! Missing skeleton in the reference mesh [{0}]"),
				FText::FromString(GenerationContext.CustomizableObjectWithCycle->GetPathName()));

			CompilerLog(Msg, ActualRoot, EMessageSeverity::Error);
			return nullptr;
		}

		// Add a new entry to the list of Component Infos
		GenerationContext.ComponentInfos.Add(FMutableComponentInfo(ComponentName, RefSkeletalMesh));

		// Make sure the Skeleton from the reference mesh is added to the list of referenced Skeletons.
		GenerationContext.ReferencedSkeletons.Add(RefSkeleton);

		// Add reference meshes to the participating objects
		GenerationContext.AddParticipatingObject(*RefSkeletalMesh);
	}

	// Copy component data to the object being compiled
	Object->GetPrivate()->MutableMeshComponents = ActualRootObject->GetPrivate()->MutableMeshComponents;

    GenerationContext.RealTimeMorphTargetsOverrides = ActualRoot->RealTimeMorphSelectionOverrides;
    GenerationContext.RealTimeMorphTargetsOverrides.Reset();

	if (!GenerationContext.ParamNamesToSelectedOptions.IsEmpty())
	{
		GenerationContext.TableToParamNames = Object->GetPrivate()->GetModelResources().TableToParamNames;
	}

	// Generate the object expression
	UE_LOG(LogMutable, Verbose, TEXT("PROFILE: [ %16.8f ] GenerateMutableSource start."), FPlatformTime::Seconds());
	mu::NodeObjectPtr MutableRoot = GenerateMutableSource(ActualRoot->OutputPin(), GenerationContext, !bOutIsRootObject);
	UE_LOG(LogMutable, Verbose, TEXT("PROFILE: [ %16.8f ] GenerateMutableSource end."), FPlatformTime::Seconds());

    ActualRoot->RealTimeMorphSelectionOverrides = GenerationContext.RealTimeMorphTargetsOverrides;
	GenerationContext.GenerateClippingCOInternalTags();

	GenerationContext.GenerateSharedSurfacesUniqueIds();

	// Generate ReferenceSkeletalMeshes data;
	PopulateReferenceSkeletalMeshesData(GenerationContext);
	
	//for (const USkeletalMesh* RefSkeletalMesh : Object->ReferenceSkeletalMeshes)
	//{
	//	GenerationContext.CheckPhysicsAssetInSkeletalMesh(RefSkeletalMesh);
	//}

	DisplayParameterWarning( GenerationContext );
	DisplayUnnamedNodeObjectWarning( GenerationContext );
	DisplayDuplicatedNodeIdsWarning( GenerationContext );
	//DisplayDiscardedPhysicsAssetSingleWarning( GenerationContext );
	DisplayOrphanNodesWarning( GenerationContext );

	if (GenerationContext.CustomizableObjectWithCycle)
	{
		ErrorMsg = FText::Format(LOCTEXT("CycleDetected","Cycle detected in graph of CustomizableObject {0}. Object not built."),
			FText::FromString(GenerationContext.CustomizableObjectWithCycle->GetPathName()));

		return nullptr;
	}

	return MutableRoot;
}


void FCustomizableObjectCompiler::LaunchMutableCompile()
{
	AddCompileNotification(LOCTEXT("CustomizableObjectCompileInProgress", "Compiling"));

	// Even for async build, we spawn a thread, so that we can set a large stack. 
	// Thread names need to be unique, apparently.
	static int32 ThreadCount = 0;
	FString ThreadName = FString::Printf(TEXT("MutableCompile-%03d"), ++ThreadCount);
	CompileThread = MakeShareable(FRunnableThread::Create(CompileTask.Get(), *ThreadName, 16 * 1024 * 1024, TPri_Normal));
}


void FCustomizableObjectCompiler::SaveCODerivedData()
{
	if (!SaveDDTask.IsValid())
	{
		return;
	}

	AddCompileNotification(LOCTEXT("SavingCustomizableObjectDerivedData", "Saving Data"));

	// Even for async saving derived data.
	static int SDDThreadCount = 0;
	FString ThreadName = FString::Printf(TEXT("MutableSDD-%03d"), ++SDDThreadCount);
	SaveDDThread = MakeShareable(FRunnableThread::Create(SaveDDTask.Get(), *ThreadName));
}


void FCustomizableObjectCompiler::AddCachedReferencers(const FName& PathName, TArray<FName>& ArrayReferenceNames)
{
	ArrayReferenceNames.Empty();
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	AssetRegistryModule.Get().GetReferencers(PathName, ArrayReferenceNames, UE::AssetRegistry::EDependencyCategory::Package, UE::AssetRegistry::EDependencyQuery::Hard);

	// Required to make compilations deterministic within editor runs.
	ArrayReferenceNames.Sort([](const FName& A, const FName& B)
		{
			return A.LexicalLess(B);
		});

	FARFilter Filter;
	for (const FName& ReferenceName : ArrayReferenceNames)
	{
		if (!IsCachedInAssetData(ReferenceName.ToString()) && !ReferenceName.ToString().StartsWith(TEXT("/TempAutosave")))
		{
			Filter.PackageNames.Add(ReferenceName);
		}
	}

	Filter.bIncludeOnlyOnDiskAssets = false;

	TArray<FAssetData> ArrayAssetDataTemp;
	AssetRegistryModule.Get().GetAssets(Filter, ArrayAssetDataTemp);

	// Store only those which have static class type Customizable Object, to avoid loading not needed elements
	const int32 MaxIndex = ArrayAssetDataTemp.Num();
	for (int32 i = 0; i < MaxIndex; ++i)
	{
		if (ArrayAssetDataTemp[i].GetClass() == UCustomizableObject::StaticClass())
		{
			ArrayAssetData.Add(ArrayAssetDataTemp[i]);
		}
	}
}


void FCustomizableObjectCompiler::CleanCachedReferencers()
{
	ArrayAssetData.Empty();
}


bool FCustomizableObjectCompiler::IsCachedInAssetData(const FString& PackageName)
{
	const int32 MaxIndex = ArrayAssetData.Num();

	for (int32 i = 0; i < MaxIndex; ++i)
	{
		if (ArrayAssetData[i].PackageName.ToString() == PackageName)
		{
			return true;
		}
	}

	return false;
}


FAssetData* FCustomizableObjectCompiler::GetCachedAssetData(const FString& PackageName)
{
	const int32 MaxIndex = ArrayAssetData.Num();

	for (int32 i = 0; i < MaxIndex; ++i)
	{
		if (ArrayAssetData[i].PackageName.ToString() == PackageName)
		{
			return &ArrayAssetData[i];
		}
	}

	return nullptr;
}



ECompilationResultPrivate FCustomizableObjectCompiler::GetCompilationResult() const
{
	if (CompilationLogsContainer.GetErrorCount())
	{
		return ECompilationResultPrivate::Errors;
	}
	else if (CompilationLogsContainer.GetWarningCount(true))
	{
		return ECompilationResultPrivate::Warnings;
	}
	else
	{
		return ECompilationResultPrivate::Success;
	}
}


void FCustomizableObjectCompiler::SetCompilationState(ECompilationStatePrivate State, ECompilationResultPrivate Result) const
{
	check(CurrentRequest);
	CurrentRequest->SetCompilationState(State, Result);

	if (CurrentObject)
	{
		CurrentObject->GetPrivate()->CompilationState = State;
		CurrentObject->GetPrivate()->CompilationResult = Result;
	}
}


void FCustomizableObjectCompiler::CompileInternal(bool bAsync)
{
	MUTABLE_CPUPROFILER_SCOPE(FCustomizableObjectCompiler::Compile)
	
	UE_LOG(LogMutable, Verbose, TEXT("PROFILE: [ %16.8f ] FCustomizableObjectCompiler::Compile start."), FPlatformTime::Seconds());
	
	// This is redundant but necessary to keep static analysis happy.
	if (!CurrentObject)
	{
		CompleteRequest(ECompilationStatePrivate::Completed, ECompilationResultPrivate::Errors);
		return;
	}

	FMutableGraphGenerationContext GenerationContext(CurrentObject, this, CurrentOptions);
	GenerationContext.ParamNamesToSelectedOptions = CurrentRequest->GetParameterNamesToSelectedOptions();

	// Clear Messages from previous Compilations
	CompilationLogsContainer.ClearMessageCounters();
	CompilationLogsContainer.ClearMessagesArray();

	// Generate the mutable node expression
	FText ErrorMessage = FText(LOCTEXT("FailedToGenerateRoot", "Failed to generate the mutable node graph. Object not built."));
	bool bIsRootObject = false;
	mu::NodeObjectPtr MutableRoot = GenerateMutableRoot(CurrentObject, GenerationContext, ErrorMessage, bIsRootObject);

	if (!MutableRoot)
	{
		CompilerLog(ErrorMessage);
		CompleteRequest(ECompilationStatePrivate::Completed, ECompilationResultPrivate::Errors);
	}
	else
	{
		FModelResources ModelResources;
		
		ModelResources.ReferenceSkeletalMeshesData = MoveTemp(GenerationContext.ReferenceSkeletalMeshesData);

		ModelResources.Skeletons.Reserve(GenerationContext.ReferencedSkeletons.Num());
		for (const USkeleton* Skeleton : GenerationContext.ReferencedSkeletons)
		{
			ModelResources.Skeletons.Emplace(const_cast<USkeleton*>(Skeleton));
		}
		
		ModelResources.Materials.Reserve(GenerationContext.ReferencedMaterials.Num());
		for (const UMaterialInterface* Material : GenerationContext.ReferencedMaterials)
		{
			ModelResources.Materials.Emplace(const_cast<UMaterialInterface*>(Material));
		}

		for (const TPair<TSoftObjectPtr<USkeletalMesh>, FMutableGraphGenerationContext::FGeneratedReferencedMesh>& Pair : GenerationContext.PassthroughMeshMap)
		{
			check(Pair.Value.ID == ModelResources.PassThroughMeshes.Num());
			ModelResources.PassThroughMeshes.Add(Pair.Key);
		}

		for (const TPair<TSoftObjectPtr<UTexture>, FMutableGraphGenerationContext::FGeneratedReferencedTexture>& Pair : GenerationContext.PassthroughTextureMap)
		{
			check(Pair.Value.ID == ModelResources.PassThroughTextures.Num());
			ModelResources.PassThroughTextures.Add(Pair.Key);
		}

		for (const TPair<TSoftObjectPtr<const UTexture>, FMutableGraphGenerationContext::FGeneratedReferencedTexture>& Pair : GenerationContext.RuntimeReferencedTextureMap)
		{
			check(Pair.Value.ID == ModelResources.RuntimeReferencedTextures.Num());
			ModelResources.RuntimeReferencedTextures.Add(Pair.Key);
		}
		
		ModelResources.PhysicsAssets = MoveTemp(GenerationContext.PhysicsAssets);

		ModelResources.AnimBPs = MoveTemp(GenerationContext.AnimBPAssets);
		ModelResources.AnimBpOverridePhysiscAssetsInfo = MoveTemp(GenerationContext.AnimBpOverridePhysicsAssetsInfo);

		ModelResources.MaterialSlotNames = MoveTemp(GenerationContext.ReferencedMaterialSlotNames);
		ModelResources.SocketArray = MoveTemp(GenerationContext.SocketArray);

		const int32 NumBones = GenerationContext.UniqueBoneNames.Num() + GenerationContext.RemappedBoneNames.Num();
		ModelResources.BoneNamesMap.Reserve(NumBones);

		for (auto& It : GenerationContext.UniqueBoneNames)
		{
			ModelResources.BoneNamesMap.Add(It.Value, It.Key.Id);
		}

		for (auto& It : GenerationContext.RemappedBoneNames)
		{
			ModelResources.BoneNamesMap.Add(It.Key, It.Value.Id);
		}

		ModelResources.SkinWeightProfilesInfo = MoveTemp(GenerationContext.SkinWeightProfilesInfo);

		TArray<FGeneratedImageProperties> ImageProperties;
		GenerationContext.ImageProperties.GenerateValueArray(ImageProperties);
		
		// Must sort image properties by ImagePropertiesIndex so that ImageNames point to the right properties.
		ImageProperties.Sort([](const FGeneratedImageProperties& PropsA, const FGeneratedImageProperties& PropsB)
			{ return PropsA.ImagePropertiesIndex < PropsB.ImagePropertiesIndex;	});

		ModelResources.ImageProperties.Empty(ImageProperties.Num());

		for (const FGeneratedImageProperties& ImageProp : ImageProperties)
		{
			ModelResources.ImageProperties.Add({ ImageProp.TextureParameterName,
										ImageProp.Filter,
										ImageProp.SRGB,
										ImageProp.bFlipGreenChannel,
										ImageProp.bIsPassThrough,
										ImageProp.LODBias,
										ImageProp.MipGenSettings,
										ImageProp.LODGroup,
										ImageProp.AddressX, ImageProp.AddressY });
		}

		ModelResources.ParameterUIDataMap = MoveTemp(GenerationContext.ParameterUIDataMap);
		ModelResources.StateUIDataMap = MoveTemp(GenerationContext.StateUIDataMap);

		// Create the RealTimeMorphsTargets Blocks from the per mesh Morph data.
		uint64 RealTimeMorphDataSize = 0;
		for (const TPair<uint32, FRealTimeMorphMeshData>& MeshData : GenerationContext.RealTimeMorphTargetPerMeshData)
		{
			RealTimeMorphDataSize += MeshData.Value.Data.Num();
		}
		
		ModelResources.RealTimeMorphStreamables.Empty(32);
		ModelResources.EditorOnlyMorphTargetReconstructionData.Empty(RealTimeMorphDataSize);

		uint64 RealTimeMorphDataOffsetInBytes = 0;
		for (const TPair<uint32, FRealTimeMorphMeshData>& MeshData : GenerationContext.RealTimeMorphTargetPerMeshData)
		{
			const uint32 DataSizeInBytes = (uint32)MeshData.Value.Data.Num()*sizeof(FMorphTargetVertexData); 
			FRealTimeMorphStreamable& ResourceMeshData = ModelResources.RealTimeMorphStreamables.FindOrAdd(MeshData.Key);
			
			check(ResourceMeshData.NameResolutionMap.IsEmpty());
			check(ResourceMeshData.Size == 0);

			ResourceMeshData.NameResolutionMap = MeshData.Value.NameResolutionMap;
			ResourceMeshData.Size = DataSizeInBytes;
			mu::ERomFlags Flags = mu::ERomFlags::None;
			ResourceMeshData.Block = FMutableStreamableBlock { uint32(0), uint32(Flags), RealTimeMorphDataOffsetInBytes };

			RealTimeMorphDataOffsetInBytes += DataSizeInBytes;
			ModelResources.EditorOnlyMorphTargetReconstructionData.Append(MeshData.Value.Data);
		}
	
		// Create the Clothing Blocks from the per mesh Morph data.
		uint64 ClothingDataNum = 0;
		for (const TPair<uint32, FClothingMeshData>& MeshData : GenerationContext.ClothingPerMeshData)
		{
			ClothingDataNum += MeshData.Value.Data.Num();
		}
		
		ModelResources.ClothingStreamables.Empty(32);
		ModelResources.EditorOnlyClothingMeshToMeshVertData.Empty(ClothingDataNum);

		uint64 ClothingDataOffsetInBytes = 0;
		for (const TPair<uint32, FClothingMeshData>& MeshData : GenerationContext.ClothingPerMeshData)
		{
			const uint32 DataSizeInBytes = (uint32)MeshData.Value.Data.Num()*sizeof(FCustomizableObjectMeshToMeshVertData); 
			FClothingStreamable& ResourceMeshData = ModelResources.ClothingStreamables.FindOrAdd(MeshData.Key);
			
			check(ResourceMeshData.ClothingAssetIndex == INDEX_NONE);
			check(ResourceMeshData.ClothingAssetLOD == INDEX_NONE);
			check(ResourceMeshData.Size == 0);

			ResourceMeshData.ClothingAssetIndex = MeshData.Value.ClothingAssetIndex;
			ResourceMeshData.ClothingAssetLOD = MeshData.Value.ClothingAssetLOD;
			ResourceMeshData.PhysicsAssetIndex = MeshData.Value.PhysicsAssetIndex;
			ResourceMeshData.Size = DataSizeInBytes;
			mu::ERomFlags Flags = mu::ERomFlags::None;
			ResourceMeshData.Block = FMutableStreamableBlock{ uint32(0), uint32(Flags), ClothingDataOffsetInBytes };

			ClothingDataOffsetInBytes += DataSizeInBytes;
			ModelResources.EditorOnlyClothingMeshToMeshVertData.Append(MeshData.Value.Data);
		}

		ModelResources.ClothingAssetsData = MoveTemp(GenerationContext.ClothingAssetsData);

		// A clothing backend, e.g. Chaos cloth, can use 2 config files, one owned by the asset, and another that is shared 
		// among all assets in a SkeletalMesh. When merging different assets in a skeletalmesh we need to make sure only one of 
		// the shared is used. In that case we will keep the first visited of a type and will be stored separated from the asset.
		// TODO: Shared configs, which typically controls the quality of the simulation (iterations, etc), probably should be specified 
		// somewhere else to give more control with which config ends up used. 
		auto IsSharedConfigData = [](const FCustomizableObjectClothConfigData& ConfigData) -> bool
		{
			 const UClass* ConfigClass = FindObject<UClass>(nullptr, *ConfigData.ClassPath);
			 return ConfigClass ? static_cast<bool>(Cast<UClothSharedConfigCommon>(ConfigClass->GetDefaultObject())) : false;
		};
		
		// Find shared configs to be used (One of each type) 
		for (FCustomizableObjectClothingAssetData& ClothingAssetData : ModelResources.ClothingAssetsData)
		{
			 for (FCustomizableObjectClothConfigData& ClothConfigData : ClothingAssetData.ConfigsData)
			 {
				  if (IsSharedConfigData(ClothConfigData))
				  {
					  FCustomizableObjectClothConfigData* FoundConfig = ModelResources.ClothSharedConfigsData.FindByPredicate(
						   [Name = ClothConfigData.ConfigName](const FCustomizableObjectClothConfigData& Other)
						   {
							   return Name == Other.ConfigName;
						   });

					  if (!FoundConfig)
					  {
						   ModelResources.ClothSharedConfigsData.AddDefaulted_GetRef() = ClothConfigData;
					  }
				  }
			 }
		}
		
		// Remove shared configs
		for (FCustomizableObjectClothingAssetData& ClothingAssetData : ModelResources.ClothingAssetsData)
		{
			 ClothingAssetData.ConfigsData.RemoveAllSwap(IsSharedConfigData);
		}

		ModelResources.MeshMetadata = MoveTemp(GenerationContext.MeshMetadata);
		ModelResources.SurfaceMetadata = MoveTemp(GenerationContext.SurfaceMetadata);

		CurrentObject->GetPrivate()->GroupNodeMap = GenerationContext.GroupNodeMap;

		if (GenerationContext.Options.OptimizationLevel == 0)
		{
			// If the optimization level is "none" disable texture streaming, because textures are all referenced
			// unreal assets and progressive generation is not supported.
			CurrentObject->GetPrivate()->bDisableTextureStreaming = true;
		}
		else
		{
			CurrentObject->GetPrivate()->bDisableTextureStreaming = false;
		}
		
		CurrentObject->GetPrivate()->bIsCompiledWithoutOptimization = GenerationContext.Options.OptimizationLevel < UE_MUTABLE_MAX_OPTIMIZATION;

		CurrentObject->GetPrivate()->GetAlwaysLoadedExtensionData() = MoveTemp(GenerationContext.AlwaysLoadedExtensionData);

		CurrentObject->GetPrivate()->GetStreamedExtensionData().Empty(GenerationContext.StreamedExtensionData.Num());
		for (UCustomizableObjectResourceDataContainer* Container : GenerationContext.StreamedExtensionData)
		{
			CurrentObject->GetPrivate()->GetStreamedExtensionData().Emplace(Container);
		}

#if WITH_EDITORONLY_DATA
		CurrentObject->GetPrivate()->CustomizableObjectPathMap = GenerationContext.CustomizableObjectPathMap;

		// Cache the tables that are used by more than one param so that CompileOnlySelected can work properly
		ModelResources.TableToParamNames = GenerationContext.TableToParamNames;
#endif

		ModelResources.NumComponents = GenerationContext.NumMeshComponentsInRoot + GenerationContext.NumExplicitMeshComponents;
		ModelResources.NumLODs = GenerationContext.NumLODsInRoot;
		ModelResources.NumLODsToStream = GenerationContext.bEnableLODStreaming ? GenerationContext.NumMaxLODsToStream : 0;
		ModelResources.FirstLODAvailable = GenerationContext.FirstLODAvailable;

		if (CurrentOptions.bGatherReferences)
		{
			CurrentObject->GetPrivate()->References = ModelResources;
			CurrentObject->GetPrivate()->References.RuntimeReferencedTextures.Empty(); // Empty in case the of none optimization. In maximum optimization, they are Mutable textures. 
			CurrentObject->Modify();
		}

		// Always work with the ModelResources (Editor) when compiling. They'll be copied to the cooked version during PreSave.
		CurrentObject->GetPrivate()->GetModelResources(false) = MoveTemp(ModelResources);

		CurrentObject->GetPrivate()->GetStreamedResourceData() = MoveTemp(GenerationContext.StreamedResourceData);

		// Pass-through textures
		TArray<FMutableSourceTextureData> NewCompileTimeReferencedTextures;
		for (const TPair<TSoftObjectPtr<const UTexture>, FMutableGraphGenerationContext::FGeneratedReferencedTexture>& Pair : GenerationContext.CompileTimeTextureMap)
		{
			check(Pair.Value.ID == NewCompileTimeReferencedTextures.Num());

			FMutableSourceTextureData Tex(*Pair.Key.LoadSynchronous());
			NewCompileTimeReferencedTextures.Add(Tex);
		}

		if (!CurrentRequest->GetParameterNamesToSelectedOptions().Num())
		{
			// Get possible objects used in the compilation that are not directly referenced.
			// Due to this check being done also in PIE (to detect out of date compilations), it has to be performant. Therefore we are gathering a relaxed set.
			// For example, a referencing Customizable Object may not be used if it is not assigned in any Group Node. In the relaxed set we include those regardless.
			// Notice that, to avoid automatic compilations/warnings, the set of referencing objects set found here must coincide with the set found when loading the
			// model (discard previous compilations) or when showing PIE warnings.
			TArray<FName> ReferencingObjectNames;
			GetReferencingPackages(*CurrentObject, ReferencingObjectNames);

			for (const FName& ReferencingObjectName : ReferencingObjectNames)
			{
				const TSoftObjectPtr<UObject> SoftObjectPtr = TSoftObjectPtr<UObject>(FSoftObjectPath(ReferencingObjectName.ToString()));

				if (const UObject* ReferencingObject = SoftObjectPtr.LoadSynchronous())
				{
					GenerationContext.AddParticipatingObject(*ReferencingObject);					
				}
			}
			
			// Copy final array of participating objects
			CurrentObject->GetPrivate()->ParticipatingObjects = MoveTemp(GenerationContext.ParticipatingObjects);
			CurrentObject->GetPrivate()->DirtyParticipatingObjects.Empty();
		}

		CompileTask = MakeShareable(new FCustomizableObjectCompileRunnable(MutableRoot));
		CompileTask->Options = CurrentOptions;
		CompileTask->ReferencedTextures = NewCompileTimeReferencedTextures;

		if (!bAsync)
		{
			CompileTask->Init();
			CompileTask->Run();
			FinishCompilationTask();

			if (SaveDDTask.IsValid())
			{
				SaveDDTask->Init();
				SaveDDTask->Run();
				FinishSavingDerivedDataTask();
			}

			CompleteRequest(ECompilationStatePrivate::Completed, GetCompilationResult());
		}
		else
		{
			LaunchMutableCompile();
		}
	}

	for (UCustomizableObjectNode* Node : GenerationContext.GeneratedNodes)
	{
		Node->ResetAttachedErrorData();
	}

	// Population Recompilation
	if (MutableRoot)
	{
		//Checking if there is the population plugin
		if (FModuleManager::Get().IsModuleLoaded("CustomizableObjectPopulation"))
		{
			ICustomizableObjectPopulationModule::Get().RecompilePopulations(CurrentObject);
		}
	}
}


void FCustomizableObjectCompiler::CompleteRequest(ECompilationStatePrivate State, ECompilationResultPrivate Result)
{
	check(IsInGameThread());
	check(CurrentRequest);

	ECompilationStatePrivate CurrentState = CurrentRequest->GetCompilationState();
	SetCompilationState(State, Result);

	if (CurrentState == ECompilationStatePrivate::InProgress && CurrentObject)
	{
		// Unlock the object so that instances can be updated
		UCustomizableObjectSystem* System = UCustomizableObjectSystem::IsCreated() ? UCustomizableObjectSystem::GetInstance() : nullptr;
		if (System && !System->HasAnyFlags(EObjectFlags::RF_BeginDestroyed))
		{	
			System->UnlockObject(CurrentObject);
		}

		if (CurrentModel)
		{
			CurrentModel->GetPrivate()->UnloadRoms();
		}

		if (Result == ECompilationResultPrivate::Success || Result == ECompilationResultPrivate::Warnings)
		{
			CurrentObject->GetPrivate()->SetModel(CurrentModel, GenerateIdentifier(*CurrentObject));
		}
		else
		{
			CurrentObject->GetPrivate()->SetModel(nullptr, {});
		}

		CurrentObject->GetPrivate()->PostCompile();

		UE_LOG(LogMutable, Display, TEXT("Finished compiling Customizable Object %s. Compilation took %5.3f seconds to complete."),
			*CurrentObject->GetName(), FPlatformTime::Seconds() - CompilationStartTime);
	}

	// Remove referenced objects
	CleanCachedReferencers();
	UpdateArrayGCProtect();

	// Notifications
	RemoveCompileNotification();
	NotifyCompilationErrors();

	// Update compilation progress notification
	if (CompileNotificationHandle.IsValid())
	{
		const int32 NumCompletedRequests = NumCompilationRequests - CompileRequests.Num();
		FSlateNotificationManager::Get().UpdateProgressNotification(CompileNotificationHandle, NumCompletedRequests, NumCompilationRequests);

		if (NumCompletedRequests == NumCompilationRequests)
		{
			// Remove progress bar
			FSlateNotificationManager::Get().CancelProgressNotification(CompileNotificationHandle);
			CompileNotificationHandle.Reset();
			NumCompilationRequests = 0;
		}
	}

	// Copy warnings and errors to the request
	CompilationLogsContainer.GetMessages(CurrentRequest->GetWarnings(), CurrentRequest->GetErrors());
	
	// Clear Messages
	CompilationLogsContainer.ClearMessageCounters();
	CompilationLogsContainer.ClearMessagesArray();

	if (GEngine)
	{
		GEngine->ForceGarbageCollection();
	}

	// Request completed, reset pointers and state
	CurrentObject = nullptr;
	CurrentRequest.Reset();
	CurrentModel.Reset();

	UE_LOG(LogMutable, Verbose, TEXT("PROFILE: [ %16.8f ] Completed compile request."), FPlatformTime::Seconds());
	UE_LOG(LogMutable, Verbose, TEXT("PROFILE: -----------------------------------------------------------"));
}


bool FCustomizableObjectCompiler::TryPopCompileRequest()
{
	if (CurrentRequest.IsValid() || CompileRequests.IsEmpty())
	{
		return false;
	}

	Compile(CompileRequests.Pop());
	return true;
}


mu::NodePtr FCustomizableObjectCompiler::Export(UCustomizableObject* Object, const FCompilationOptions& InCompilerOptions, 
	TArray<TSoftObjectPtr<const UTexture>>& OutRuntimeReferencedTextures,
	TArray<FMutableSourceTextureData>& OutCompilerReferencedTextures )
{
	UE_LOG(LogMutable, Log, TEXT("Started Customizable Object Export %s."), *Object->GetName());

	FNotificationInfo Info(LOCTEXT("CustomizableObjectExportInProgress", "Exported Customizable Object"));
	Info.bFireAndForget = true;
	Info.bUseThrobber = true;
	Info.FadeOutDuration = 1.0f;
	Info.ExpireDuration = 1.0f;
	FSlateNotificationManager::Get().AddNotification(Info);

	FCompilationOptions CompilerOptions = InCompilerOptions;
	CompilerOptions.bRealTimeMorphTargetsEnabled = Object->bEnableRealTimeMorphTargets;
	CompilerOptions.bClothingEnabled = Object->bEnableClothing;
	CompilerOptions.b16BitBoneWeightsEnabled = Object->bEnable16BitBoneWeights;
	CompilerOptions.bSkinWeightProfilesEnabled = Object->bEnableAltSkinWeightProfiles;
	CompilerOptions.bPhysicsAssetMergeEnabled = Object->bEnablePhysicsAssetMerge;
	CompilerOptions.bAnimBpPhysicsManipulationEnabled = Object->bEnableAnimBpPhysicsAssetsManipualtion;

	FMutableGraphGenerationContext GenerationContext(Object, this, CompilerOptions);
	if (CurrentRequest)
	{
		GenerationContext.ParamNamesToSelectedOptions = CurrentRequest->GetParameterNamesToSelectedOptions();
	}

	// Generate the mutable node expression
	FText ErrorMsg;
	bool bIsRootObject = false;
	mu::NodeObjectPtr MutableRoot = GenerateMutableRoot(Object, GenerationContext, ErrorMsg, bIsRootObject);

	if (!MutableRoot)
	{
		if (!ErrorMsg.IsEmpty())
		{
			FCustomizableObjectCompiler::CompilerLog(ErrorMsg, nullptr);
		}
		else
		{
			FCustomizableObjectCompiler::CompilerLog(LOCTEXT("FailedToExport","Failed to generate the mutable node graph. Object not built."), nullptr);
		}
		return nullptr;
	}

	// Pass out the references textures
	OutRuntimeReferencedTextures.Empty();
	for (const TPair<TSoftObjectPtr<const UTexture>, FMutableGraphGenerationContext::FGeneratedReferencedTexture>& Pair : GenerationContext.RuntimeReferencedTextureMap)
	{
		check(Pair.Value.ID == OutRuntimeReferencedTextures.Num());
		OutRuntimeReferencedTextures.Add(Pair.Key);
	}

	OutCompilerReferencedTextures.Empty();
	for (const TPair<TSoftObjectPtr<const UTexture>, FMutableGraphGenerationContext::FGeneratedReferencedTexture>& Pair : GenerationContext.CompileTimeTextureMap)
	{
		check(Pair.Value.ID == OutCompilerReferencedTextures.Num());

		FMutableSourceTextureData Tex(*Pair.Key.LoadSynchronous());
		OutCompilerReferencedTextures.Add(Tex);
	}

	return MutableRoot;
}


void FCustomizableObjectCompiler::FinishCompilationTask()
{
	check(CompileTask.IsValid());

	UpdateCompilerLogData();
	CurrentModel = CompileTask->Model;

	// Generate a map that using the resource id tells the offset and size of the resource inside the bulk data
	// At this point it is assumed that all data goes into a single file.
	if (CurrentModel)
	{
		// Always work with the ModelResources (Editor) when compiling. They'll be copied to the cooked version during PreSave.
		FModelResources& ModelResources = CurrentObject->GetPrivate()->GetModelResources(false);

		const int32 NumStreamingFiles = CurrentModel->GetRomCount();

		TSharedPtr<TMap<uint32, FMutableStreamableBlock>> HashToStreamableBlock = MakeShared<TMap<uint32, FMutableStreamableBlock>>();
		HashToStreamableBlock->Empty(NumStreamingFiles);

		uint64 Offset = 0;
		for (int32 FileIndex = 0; FileIndex < NumStreamingFiles; ++FileIndex)
		{
			const uint32 ResourceId = CurrentModel->GetRomId(FileIndex);
			const uint32 ResourceSize = CurrentModel->GetRomSize(FileIndex);
			mu::ERomFlags Flags = CurrentModel->GetRomFlags(FileIndex);
			HashToStreamableBlock->Add(ResourceId, FMutableStreamableBlock{ 0, uint32(Flags), Offset });
			Offset += ResourceSize;
		}

		ModelResources.HashToStreamableBlock = *(HashToStreamableBlock.Get());
	}

	// Generate ParameterProperties and IntParameterLookUpTable
	CurrentObject->GetPrivate()->UpdateParameterPropertiesFromModel(CurrentModel);

	// Order matters
	CompileThread.Reset();
	CompileTask.Reset();

	UE_LOG(LogMutable, Verbose, TEXT("PROFILE: [ %16.8f ] Finishing Compilation task for CO [%s]."), FPlatformTime::Seconds(), *CurrentObject->GetName());
	TRACE_END_REGION(UE_MUTABLE_COMPILE_REGION);

	// Create SaveDD task
	TRACE_BEGIN_REGION(UE_MUTABLE_SAVEDD_REGION);
	SaveDDTask = MakeShareable(new FCustomizableObjectSaveDDRunnable(CurrentObject, CurrentOptions, CurrentModel));
}


void FCustomizableObjectCompiler::FinishSavingDerivedDataTask()
{
	MUTABLE_CPUPROFILER_SCOPE(FinishSavingDerivedDataTask)

	check(SaveDDTask.IsValid());

	if (CurrentOptions.bIsCooking)
	{
		MUTABLE_CPUPROFILER_SCOPE(CachePlatformData);
		const ITargetPlatform* TargetPlatform = CurrentOptions.TargetPlatform;

		FString PlatformName = TargetPlatform ? TargetPlatform->PlatformName() : FPlatformProperties::PlatformName();

		check(!CurrentObject->GetPrivate()->CachedPlatformsData.Find(PlatformName));

		FMutableCachedPlatformData& Data = CurrentObject->GetPrivate()->CachedPlatformsData.Add(PlatformName);

		// Cache CO data and mu::Model
		FMemoryWriter64 MemoryWriter(Data.ModelData);
		CurrentObject->GetPrivate()->SaveCompiledData(MemoryWriter, true);
		Data.ModelData.Append(SaveDDTask->ModelBytes);

		// Cache streamable bulk data
		Data.ModelStreamableData = MoveTemp(SaveDDTask->ModelStreamableData);
		Data.MorphData = MoveTemp(SaveDDTask->MorphDataBytes);
		Data.ClothingData = MoveTemp(SaveDDTask->ClothingDataBytes);
	}

	// Order matters
	SaveDDThread.Reset();
	SaveDDTask.Reset();

	UE_LOG(LogMutable, Verbose, TEXT("PROFILE: [ %16.8f ] Finished Saving Derived Data task for CO [%s]."), FPlatformTime::Seconds(), *CurrentObject->GetName());

	TRACE_END_REGION(UE_MUTABLE_SAVEDD_REGION);
}


void FCustomizableObjectCompiler::ForceFinishCompilation()
{
	if (AsynchronousStreamableHandlePtr)
	{
		AsynchronousStreamableHandlePtr->CancelHandle();
		AsynchronousStreamableHandlePtr.Reset();
	}

	else if (CompileTask.IsValid())
	{
		// Compilation needs game thread tasks every now and then. Wait for compilation to finish while
		// giving execution time for these tasks.
		// TODO: interruptible compilations?
		while (!CompileTask->IsCompleted())
		{
			FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
		}

		// Order matters
		CompileThread.Reset();
		CompileTask.Reset();

		UE_LOG(LogMutable, Verbose, TEXT("Force Finish Compilation task for Object."));
		TRACE_END_REGION(UE_MUTABLE_COMPILE_REGION);
	}

	else if (SaveDDTask.IsValid())
	{
		SaveDDThread->WaitForCompletion();

		// Order matters
		SaveDDThread.Reset();
		SaveDDTask.Reset();

		UE_LOG(LogMutable, Verbose, TEXT("Forced Finish Saving Derived Data task."));
		TRACE_END_REGION(UE_MUTABLE_SAVEDD_REGION);
	}

	if (CurrentRequest)
	{
		CompleteRequest(ECompilationStatePrivate::Completed, ECompilationResultPrivate::Errors);
	}
}

void FCustomizableObjectCompiler::ClearCompileRequests()
{
	CompileRequests.Empty();
}


void FCustomizableObjectCompiler::AddCompileNotification(const FText& CompilationStep) const
{
	const FText Text = CurrentObject ? FText::FromString(FString::Printf(TEXT("Compiling %s"), *CurrentObject->GetName())) : LOCTEXT("CustomizableObjectCompileInProgressNotification", "Compiling Customizable Object");
	
	FCustomizableObjectEditorLogger::CreateLog(Text)
	.SubText(CompilationStep)
	.Category(ELoggerCategory::Compilation)
	.Notification(!CurrentOptions.bSilentCompilation)
	.CustomNotification()
	.FixNotification()
	.Log();
}


void FCustomizableObjectCompiler::RemoveCompileNotification()
{
	FCustomizableObjectEditorLogger::DismissNotification(ELoggerCategory::Compilation);
}


void FCustomizableObjectCompiler::NotifyCompilationErrors() const
{
	const uint32 NumWarnings = CompilationLogsContainer.GetWarningCount(false);
	const uint32 NumErrors = CompilationLogsContainer.GetErrorCount();
	const uint32 NumIgnoreds = CompilationLogsContainer.GetIgnoredCount();
	const bool NoWarningsOrErrors = !(NumWarnings || NumErrors);

	const EMessageSeverity::Type Severity = [&]
	{
		if (NumErrors)
		{
			return EMessageSeverity::Error;
		}
		else if (NumWarnings)
		{
			return EMessageSeverity::Warning;
		}
		else
		{
			return EMessageSeverity::Info;
		}
	}();

	const FText Prefix = FText::FromString(CurrentObject ? CurrentObject->GetName() : "Customizable Object");

	const FText Message = NoWarningsOrErrors ?
		FText::Format(LOCTEXT("CompilationFinishedSuccessfully", "{0} finished compiling."), Prefix) :
		NumIgnoreds > 0 ?
		FText::Format(LOCTEXT("CompilationFinished_WithIgnoreds", "{0} finished compiling with {1} {1}|plural(one=warning,other=warnings), {2} {2}|plural(one=error,other=errors) and {3} more similar warnings."), Prefix, NumWarnings, NumErrors, NumIgnoreds)
		:
		FText::Format(LOCTEXT("CompilationFinished_WithoutIgnoreds", "{0} finished compiling with {1} {1}|plural(one=warning,other=warnings) and {2} {2}|plural(one=error,other=errors)."), Prefix, NumWarnings, NumErrors);
	
	FCustomizableObjectEditorLogger::CreateLog(Message)
	.Category(ELoggerCategory::Compilation)
	.Severity(Severity)
	.Notification(!CurrentOptions.bSilentCompilation || !NoWarningsOrErrors)
	.CustomNotification()
	.Log();
}


void FCustomizableObjectCompiler::CompilerLog(const FText& Message, const TArray<const UObject*>& Context, const EMessageSeverity::Type MessageSeverity, const bool bAddBaseObjectInfo, const ELoggerSpamBin SpamBin)
{
	if (CompilationLogsContainer.AddMessage(Message, Context, MessageSeverity, SpamBin)) // Cache the message for later reference
	{
		FCustomizableObjectEditorLogger::CreateLog(Message)
			.Severity(MessageSeverity)
			.Context(Context)
			.BaseObject(bAddBaseObjectInfo)
			.SpamBin(SpamBin)
			.Log();
	}
}


void FCustomizableObjectCompiler::CompilerLog(const FText& Message, const UObject* Context, const EMessageSeverity::Type MessageSeverity, const bool bAddBaseObjectInfo, const ELoggerSpamBin SpamBin)
{
	TArray<const UObject*> ContextArray;
	if (Context)
	{
		ContextArray.Add(Context);
	}
	CompilerLog(Message, ContextArray, MessageSeverity, bAddBaseObjectInfo, SpamBin);
}


void FCustomizableObjectCompiler::UpdateCompilerLogData()
{
	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	MessageLogModule.RegisterLogListing(FName("Mutable"), LOCTEXT("MutableLog", "Mutable"));
	const TArray<FCustomizableObjectCompileRunnable::FError>& ArrayCompileErrors = CompileTask->GetArrayErrors();

	const FText ObjectName = CurrentObject ? FText::FromString(CurrentObject->GetName()) : LOCTEXT("Unknown Object", "Unknown Object");

	for (const FCustomizableObjectCompileRunnable::FError& CompileError : ArrayCompileErrors)
	{
		const UObject* Object = static_cast<const UObject*>(CompileError.Context); // Context are always UObjects

		if (const UCustomizableObjectNode* Node = Cast<UCustomizableObjectNode>(Object))
		{
			if (CompileError.AttachedData)
			{
				UCustomizableObjectNode::FAttachedErrorDataView ErrorDataView;
				ErrorDataView.UnassignedUVs = { CompileError.AttachedData->UnassignedUVs.GetData(),
												CompileError.AttachedData->UnassignedUVs.Num() };

				const_cast<UCustomizableObjectNode*>(Node)->AddAttachedErrorData(ErrorDataView);
			}			
		}

		FText FullMsg = FText::Format(LOCTEXT("MutableMessage", "{0} : {1}"), ObjectName, CompileError.Message);
		CompilerLog(FullMsg, Object, CompileError.Severity, true, CompileError.SpamBin);
	}
}

#undef LOCTEXT_NAMESPACE
