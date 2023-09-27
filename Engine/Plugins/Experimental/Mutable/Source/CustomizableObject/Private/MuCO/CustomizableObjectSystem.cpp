// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/CustomizableObjectSystem.h"

#include "Animation/Skeleton.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAsset.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Engine/SkeletalMeshLODSettings.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/ITargetPlatform.h"
#include "MuCO/CustomizableInstanceLODManagement.h"
#include "MuCO/CustomizableInstancePrivateData.h"
#include "MuCO/CustomizableObjectPrivate.h"
#include "MuCO/DefaultImageProvider.h"
#include "MuCO/CustomizableSkeletalComponent.h"
#include "MuCO/ICustomizableObjectModule.h"
#include "MuCO/LogBenchmarkUtil.h"
#include "MuCO/LogInformationUtil.h"
#include "MuCO/UnrealExtensionDataStreamer.h"
#include "MuCO/UnrealMutableImageProvider.h"
#include "MuCO/UnrealMutableModelDiskStreamer.h"
#include "MuCO/UnrealPortabilityHelpers.h"
#include "MuR/Model.h"
#include "MuR/Settings.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "ContentStreaming.h"
#include "MuCO/EditorImageProvider.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Logging/MessageLog.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/MessageDialog.h"
#include "Engine/World.h"
#else
#include "Engine/Engine.h"
#endif



#include UE_INLINE_GENERATED_CPP_BY_NAME(CustomizableObjectSystem)

class AActor;
class UAnimInstance;

DECLARE_CYCLE_STAT(TEXT("MutablePendingRelease Time"), STAT_MutablePendingRelease, STATGROUP_Game);
DECLARE_CYCLE_STAT(TEXT("MutableTask"), STAT_MutableTask, STATGROUP_Game);

UCustomizableObjectSystem* FCustomizableObjectSystemPrivate::SSystem = nullptr;

static TAutoConsoleVariable<int32> CVarWorkingMemory(
	TEXT("mutable.WorkingMemory"),
#if !PLATFORM_DESKTOP
	(10 * 1024),
#else
	(50 * 1024),
#endif
	TEXT("Limit the amount of memory (in KB) to use as working memory when building characters. More memory reduces the object construction time. 0 means no restriction. Defaults: Desktop = 50,000 KB, Others = 10,000 KB"),
	ECVF_Scalability);


TAutoConsoleVariable<bool> CVarClearWorkingMemoryOnUpdateEnd(
	TEXT("mutable.ClearWorkingMemoryOnUpdateEnd"),
	false,
	TEXT("Clear the working memory and cache after every Mutable operation."),
	ECVF_Scalability);


TAutoConsoleVariable<bool> CVarReuseImagesBetweenInstances(
	TEXT("mutable.ReuseImagesBetweenInstances"),
	true,
	TEXT("Enables or disables the reuse of images between instances."),
	ECVF_Scalability);

static TAutoConsoleVariable<int32> CVarGeneratedResourcesCacheSize(
	TEXT("mutable.GeneratedResourcesCacheSize"),
	512,
	TEXT("Limit the number of resources (images and meshes) that will be tracked for reusal. Each tracked resource uses a small amout of memory for its key."),
	ECVF_Scalability);

int32 FCustomizableObjectSystemPrivate::SkeletalMeshMinLodQualityLevel = -1;
static void CVarMutableSinkFunction()
{
	if (UCustomizableObjectSystem::IsCreated())
	{
		FCustomizableObjectSystemPrivate* PrivateSystem = UCustomizableObjectSystem::GetInstance()->GetPrivate();

		static const IConsoleVariable* CVarSkeletalMeshMinLodQualityLevelCVarName = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SkeletalMesh.MinLodQualityLevel"));
		PrivateSystem->SkeletalMeshMinLodQualityLevel = CVarSkeletalMeshMinLodQualityLevelCVarName ? CVarSkeletalMeshMinLodQualityLevelCVarName->GetInt() : INDEX_NONE;
	}
}

static FAutoConsoleVariableSink CVarMutableSink(FConsoleCommandDelegate::CreateStatic(&CVarMutableSinkFunction));


bool FMutablePendingInstanceWork::ArePendingUpdatesEmpty() const
{
	return PendingInstanceUpdates.Num() == 0;
}


int32 FMutablePendingInstanceWork::Num() const
{
	return PendingInstanceUpdates.Num() + PendingInstanceDiscards.Num() + PendingIDsToRelease.Num() + NumLODUpdatesLastTick;
}


void FMutablePendingInstanceWork::SetLODUpdatesLastTick(int32 NumLODUpdates)
{
	NumLODUpdatesLastTick = NumLODUpdates;
}


void FMutablePendingInstanceWork::AddUpdate(const FMutablePendingInstanceUpdate& UpdateToAdd)
{
	if (const FMutablePendingInstanceUpdate* ExistingUpdate = PendingInstanceUpdates.Find(UpdateToAdd.CustomizableObjectInstance))
	{
		FMutablePendingInstanceUpdate TaskToEnqueue = UpdateToAdd;

		FInstanceUpdateDelegate* Callback = ExistingUpdate->Callback;
		FinishUpdateGlobal(ExistingUpdate->CustomizableObjectInstance.Get(), EUpdateResult::ErrorReplaced, Callback);

		TaskToEnqueue.PriorityType = FMath::Min(ExistingUpdate->PriorityType, UpdateToAdd.PriorityType);
		TaskToEnqueue.SecondsAtUpdate = FMath::Min(ExistingUpdate->SecondsAtUpdate, UpdateToAdd.SecondsAtUpdate);
		
		PendingInstanceUpdates.Remove(ExistingUpdate->CustomizableObjectInstance);
		PendingInstanceUpdates.Add(TaskToEnqueue);
	}
	else
	{
		PendingInstanceUpdates.Add(UpdateToAdd);
	}

	if (const FMutablePendingInstanceDiscard* ExistingDiscard = PendingInstanceDiscards.Find(UpdateToAdd.CustomizableObjectInstance))
	{
		FinishUpdateGlobal(ExistingDiscard->CustomizableObjectInstance.Get(), EUpdateResult::ErrorReplaced, nullptr);

		PendingInstanceDiscards.Remove(ExistingDiscard->CustomizableObjectInstance);
	}
}


void FMutablePendingInstanceWork::RemoveUpdate(const TWeakObjectPtr<UCustomizableObjectInstance>& Instance)
{
	PendingInstanceUpdates.Remove(Instance);
}


const FMutablePendingInstanceUpdate* FMutablePendingInstanceWork::GetUpdate(const TWeakObjectPtr<UCustomizableObjectInstance>& Instance) const
{
	return PendingInstanceUpdates.Find(Instance);
}


void FMutablePendingInstanceWork::AddDiscard(const FMutablePendingInstanceDiscard& TaskToEnqueue)
{
	if (const FMutablePendingInstanceUpdate* ExistingUpdate = PendingInstanceUpdates.Find(TaskToEnqueue.CustomizableObjectInstance.Get()))
	{
		FinishUpdateGlobal(ExistingUpdate->CustomizableObjectInstance.Get(), EUpdateResult::ErrorReplaced, ExistingUpdate->Callback);
		PendingInstanceUpdates.Remove(ExistingUpdate->CustomizableObjectInstance);
	}

	PendingInstanceDiscards.Add(TaskToEnqueue);

	TaskToEnqueue.CustomizableObjectInstance->GetPrivate()->SetCOInstanceFlags(Updating);
}


void FMutablePendingInstanceWork::AddIDRelease(mu::Instance::ID IDToRelease)
{
	PendingIDsToRelease.Add(IDToRelease);
}


void FMutablePendingInstanceWork::RemoveAllUpdatesAndDiscardsAndReleases()
{
	PendingInstanceUpdates.Empty();
	PendingInstanceDiscards.Empty();
	PendingIDsToRelease.Empty();
}


UCustomizableObjectSystem* UCustomizableObjectSystem::GetInstance()
{
	if (!FCustomizableObjectSystemPrivate::SSystem)
	{
		UE_LOG(LogMutable, Log, TEXT("Creating Mutable Customizable Object System."));

		check(IsInGameThread());

		FCustomizableObjectSystemPrivate::SSystem = NewObject<UCustomizableObjectSystem>(UCustomizableObjectSystem::StaticClass());
		check(FCustomizableObjectSystemPrivate::SSystem != nullptr);
		checkf(!GUObjectArray.IsDisregardForGC(FCustomizableObjectSystemPrivate::SSystem), TEXT("Mutable was initialized too early in the UE4 init process, for instance, in the constructor of a default UObject."));
		FCustomizableObjectSystemPrivate::SSystem->AddToRoot();
		checkf(!GUObjectArray.IsDisregardForGC(FCustomizableObjectSystemPrivate::SSystem), TEXT("Mutable was initialized too early in the UE4 init process, for instance, in the constructor of a default UObject."));
		FCustomizableObjectSystemPrivate::SSystem->InitSystem();

		//FCoreUObjectDelegates::PurgePendingReleaseSkeletalMesh.AddUObject(FCustomizableObjectSystemPrivate::SSystem, &UCustomizableObjectSystem::PurgePendingReleaseSkeletalMesh);
	}

	return FCustomizableObjectSystemPrivate::SSystem;
}


UCustomizableObjectSystem* UCustomizableObjectSystem::GetInstanceChecked()
{
	UCustomizableObjectSystem* System = GetInstance();
	check(System);
	
	return System;
}


UCustomizableInstanceLODManagementBase* UCustomizableObjectSystem::GetInstanceLODManagement() const
{
	return CurrentInstanceLODManagement.Get();
}


void UCustomizableObjectSystem::SetInstanceLODManagement(UCustomizableInstanceLODManagementBase* NewInstanceLODManagement)
{
	CurrentInstanceLODManagement = NewInstanceLODManagement ? NewInstanceLODManagement : ToRawPtr(DefaultInstanceLODManagement);
}


FString UCustomizableObjectSystem::GetPluginVersion() const
{
	// Bridge the call from the module. This implementation is available from blueprint.
	return ICustomizableObjectModule::Get().GetPluginVersion();
}


void UCustomizableObjectSystem::LogShowData(bool bFullInfo, bool ShowMaterialInfo) const
{
	LogInformationUtil::ResetCounters();

	TArray<UCustomizableObjectInstance*> ArrayData;

	for (TObjectIterator<UCustomizableSkeletalComponent> It; It; ++It)
	{
		const UCustomizableSkeletalComponent* CustomizableSkeletalComponent = *It;

#if WITH_EDITOR
		if (CustomizableSkeletalComponent && CustomizableSkeletalComponent->IsNetMode(NM_DedicatedServer))
		{
			continue;
		}
#endif

		if (CustomizableSkeletalComponent && CustomizableSkeletalComponent->CustomizableObjectInstance)
		{
			const AActor* ParentActor = CustomizableSkeletalComponent->GetAttachmentRootActor();

			if (ParentActor != nullptr)
			{
				ArrayData.AddUnique(CustomizableSkeletalComponent->CustomizableObjectInstance);
			}
		}
	}

	ArrayData.Sort([](UCustomizableObjectInstance& A, UCustomizableObjectInstance& B)
	{
		check(A.GetPrivate() != nullptr);
		check(B.GetPrivate() != nullptr);
		return (A.GetPrivate()->LastMinSquareDistFromComponentToPlayer < B.GetPrivate()->LastMinSquareDistFromComponentToPlayer);
	});

	int32 i;
	const int32 Max = ArrayData.Num();

	if (bFullInfo)
	{
		for (i = 0; i < Max; ++i)
		{
			LogInformationUtil::LogShowInstanceDataFull(ArrayData[i], ShowMaterialInfo);
		}
	}
	else
	{
		FString LogData = "\n\n";
		for (i = 0; i < Max; ++i)
		{
			LogInformationUtil::LogShowInstanceData(ArrayData[i], LogData);
		}
		UE_LOG(LogMutable, Log, TEXT("%s"), *LogData);

		UWorld* World = GWorld;

		if (World)
		{
			APlayerController* PlayerController = World->GetFirstPlayerController();
			if (PlayerController)
			{
				PlayerController->ClientMessage(LogData);
			}
		}
	}
}


FCustomizableObjectSystemPrivate* UCustomizableObjectSystem::GetPrivate()
{
	return Private.Get();
}


const FCustomizableObjectSystemPrivate* UCustomizableObjectSystem::GetPrivate() const
{
	return Private.Get();
}


FCustomizableObjectSystemPrivate* UCustomizableObjectSystem::GetPrivateChecked()
{
	check(Private)
	return Private.Get();
}


const FCustomizableObjectSystemPrivate* UCustomizableObjectSystem::GetPrivateChecked() const
{
	check(Private)
	return Private.Get();
}


FStreamableManager& UCustomizableObjectSystem::GetStreamableManager()
{
	return StreamableManager;
}


bool UCustomizableObjectSystem::IsCreated()
{
	return FCustomizableObjectSystemPrivate::SSystem != 0;
}


void UCustomizableObjectSystem::InitSystem()
{
	// Everything initialized in Init() instead of constructor to prevent the default UCustomizableObjectSystem from registering a tick function
	Private = MakeShareable(new FCustomizableObjectSystemPrivate());
	check(Private != nullptr);
	Private->NewCompilerFunc = nullptr;

	Private->bReplaceDiscardedWithReferenceMesh = false;

	const IConsoleVariable* CVarSupport16BitBoneIndex = IConsoleManager::Get().FindConsoleVariable(TEXT("r.GPUSkin.Support16BitBoneIndex"));
	Private->bSupport16BitBoneIndex = CVarSupport16BitBoneIndex ? CVarSupport16BitBoneIndex->GetBool() : false;

	CVarMutableSinkFunction();

	Private->CurrentMutableOperation = nullptr;
	Private->CurrentInstanceBeingUpdated = nullptr;

#if !UE_SERVER
	Private->TickDelegate = FTickerDelegate::CreateUObject(this, &UCustomizableObjectSystem::Tick);
	Private->TickDelegateHandle = FTSTicker::GetCoreTicker().AddTicker(Private->TickDelegate, 0.f);
#endif // !UE_SERVER

	Private->MutableStats.TotalBuildMs = 0;
	Private->MutableStats.TotalBuiltInstances = 0;
	Private->MutableStats.NumInstances = 0;
	Private->MutableStats.TextureMemoryUsed = 0;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	SET_DWORD_STAT(STAT_MutableNumSkeletalMeshes, 0);
	SET_DWORD_STAT(STAT_MutableNumTextures, 0);
	SET_DWORD_STAT(STAT_MutableInstanceBuildTime, 0);
	SET_DWORD_STAT(STAT_MutableInstanceBuildTimeAvrg, 0);
#endif

	Private->LastWorkingMemoryBytes = CVarWorkingMemory.GetValueOnGameThread() * 1024;
	Private->LastGeneratedResourceCacheSize = CVarGeneratedResourcesCacheSize.GetValueOnGameThread();

	const mu::Ptr<mu::Settings> pSettings = new mu::Settings;
	check(pSettings);
	pSettings->SetProfile(false);
	pSettings->SetWorkingMemoryBytes(Private->LastWorkingMemoryBytes);
	Private->ExtensionDataStreamer = MakeShared<FUnrealExtensionDataStreamer>(Private.ToSharedRef());
	Private->MutableSystem = new mu::System(pSettings, Private->ExtensionDataStreamer);
	check(Private->MutableSystem);

	Private->Streamer = MakeShared<FUnrealMutableModelBulkReader>();
	check(Private->Streamer != nullptr);
	Private->MutableSystem->SetStreamingInterface(Private->Streamer);

	// Set up the external image provider, for image parameters.
	TSharedPtr<FUnrealMutableImageProvider> Provider = MakeShared<FUnrealMutableImageProvider>();
	check(Provider != nullptr);
	Private->ImageProvider = Provider;
	Private->MutableSystem->SetImageParameterGenerator(Provider);

#if WITH_EDITORONLY_DATA
	Private->EditorImageProvider = NewObject<UEditorImageProvider>();
	check(Private->EditorImageProvider);
	RegisterImageProvider(Private->EditorImageProvider);
#endif
	
#if WITH_EDITOR
	if (!IsRunningGame())
	{
		FEditorDelegates::PreBeginPIE.AddUObject(this, &UCustomizableObjectSystem::OnPreBeginPIE);
	}
#endif

	DefaultInstanceLODManagement = NewObject<UCustomizableInstanceLODManagement>();
	check(DefaultInstanceLODManagement != nullptr);
	CurrentInstanceLODManagement = DefaultInstanceLODManagement;
}


void UCustomizableObjectSystem::BeginDestroy()
{
#if WITH_EDITOR
	if (RecompileCustomizableObjectsCompiler)
	{
		RecompileCustomizableObjectsCompiler->ForceFinishCompilation();
		delete RecompileCustomizableObjectsCompiler;
	}

	if (!IsRunningGame())
	{
		FEditorDelegates::PreBeginPIE.RemoveAll(this);
	}

#endif

	// It could be null, for the default object.
	if (Private.IsValid())
	{

#if !UE_SERVER
		FTSTicker::GetCoreTicker().RemoveTicker(Private->TickDelegateHandle);
#endif // !UE_SERVER

		// Discard pending game thread tasks
		Private->PendingTasks.Empty();

		// Complete pending taskgraph tasks
		Private->MutableTaskGraph.WaitForMutableTasks();

		// Clear the ongoing operation
		Private->CurrentMutableOperation = nullptr;

		// Deallocate streaming
		check(Private->Streamer != nullptr);
		Private->Streamer->EndStreaming();

		Private->CurrentInstanceBeingUpdated = nullptr;

		Private->MutablePendingInstanceWork.RemoveAllUpdatesAndDiscardsAndReleases();

		FCustomizableObjectSystemPrivate::SSystem = nullptr;

		Private = nullptr;
	}

	Super::BeginDestroy();
}


FString UCustomizableObjectSystem::GetDesc()
{
	return TEXT("Customizable Object System Singleton");
}


FCustomizableObjectCompilerBase* (*FCustomizableObjectSystemPrivate::NewCompilerFunc)() = nullptr;


FCustomizableObjectCompilerBase* UCustomizableObjectSystem::GetNewCompiler()
{
	check(Private != nullptr);
	if (Private->NewCompilerFunc != nullptr)
	{
		return Private->NewCompilerFunc();
	}
	else
	{
		return nullptr;
	}
}


void UCustomizableObjectSystem::SetNewCompilerFunc(FCustomizableObjectCompilerBase* (*InNewCompilerFunc)())
{
	GetPrivateChecked()->NewCompilerFunc = InNewCompilerFunc;
}


void FCustomizableObjectSystemPrivate::CreatedTexture(UTexture2D* Texture) const
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	const bool bLogEnabled = true;
#else
	const bool bLogEnabled = LogBenchmarkUtil::IsLoggingActive();
#endif
	if (bLogEnabled)
	{
		MutableStats.TextureTrackerArray.Add(Texture);

		for (auto Iterator = MutableStats.TextureTrackerArray.CreateIterator(); Iterator; ++Iterator)
		{
			if (Iterator->IsStale())
			{
				Iterator.RemoveCurrent();
			}
		}
	}

	INC_DWORD_STAT(STAT_MutableNumTextures);
}


int32 FCustomizableObjectSystemPrivate::EnableMutableAnimInfoDebugging = 0;

static FAutoConsoleVariableRef CVarEnableMutableAnimInfoDebugging(
	TEXT("mutable.EnableMutableAnimInfoDebugging"), FCustomizableObjectSystemPrivate::EnableMutableAnimInfoDebugging,
	TEXT("If set to 1 or greater print on screen the animation info of the pawn's Customizable Object Instance. Anim BPs, slots and tags will be displayed."
	"If the root Customizable Object is recompiled after this command is run, the used skeletal meshes will also be displayed."),
	ECVF_Default);


void FCustomizableObjectSystemPrivate::AddGameThreadTask(const FMutableTask& Task)
{
	check(IsInGameThread())
	PendingTasks.Enqueue(Task);
}


void FCustomizableObjectSystemPrivate::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (CurrentInstanceBeingUpdated)
	{
		Collector.AddReferencedObject(CurrentInstanceBeingUpdated);
	}

#if WITH_EDITORONLY_DATA
	Collector.AddReferencedObject(EditorImageProvider);
#endif
}


FString FCustomizableObjectSystemPrivate::GetReferencerName() const
{
	return TEXT("FCustomizableObjectSystemPrivate");
}


TAutoConsoleVariable<bool> CVarCleanupTextureCache(
	TEXT("mutable.EnableCleanupCache"),
	true,
	TEXT("If enabled stale textures and meshes in mutable's cache will be removed."),
	ECVF_Scalability);


void FCustomizableObjectSystemPrivate::CleanupCache()
{
	check(IsInGameThread());

	const bool bCleanupEnabled = CVarCleanupTextureCache.GetValueOnGameThread();

	for (int32 ModelIndex = 0; ModelIndex < ModelResourcesCache.Num();)
	{
		if (!ModelResourcesCache[ModelIndex].Object.IsValid(false, true))
		{
			// The whole object has been destroyed. Remove everything.
			ModelResourcesCache.RemoveAtSwap(ModelIndex);
		}
		else
		{
			if (bCleanupEnabled)
			{
				// Remove stale textures
				for (auto Iterator = ModelResourcesCache[ModelIndex].Images.CreateIterator(); Iterator; ++Iterator)
				{
					if (Iterator->Value.IsStale())
					{
						Iterator.RemoveCurrent();
					}
				}

				// Remove stale meshes
				for (auto Iterator = ModelResourcesCache[ModelIndex].Meshes.CreateIterator(); Iterator; ++Iterator)
				{
					if (Iterator->Value.IsStale())
					{
						Iterator.RemoveCurrent();
					}
				}
			}

			++ModelIndex;
		}
	}
}


FMutableResourceCache& FCustomizableObjectSystemPrivate::GetObjectCache(const UCustomizableObject* Object)
{
	check(IsInGameThread());

	// Not mandatory, but a good place for a cleanup
	CleanupCache();

	for (int ModelIndex = 0; ModelIndex < ModelResourcesCache.Num(); ++ModelIndex)
	{
		if (ModelResourcesCache[ModelIndex].Object==Object)
		{
			return ModelResourcesCache[ModelIndex];
		}
	}
		
	// Not found, create and add it.
	ModelResourcesCache.Push(FMutableResourceCache());
	ModelResourcesCache.Last().Object = Object;
	return ModelResourcesCache.Last();
}


int32 FCustomizableObjectSystemPrivate::EnableMutableProgressiveMipStreaming = 1;

// Warning! If this is enabled, do not get references to the textures generated by Mutable! They are owned by Mutable and could become invalid at any moment
static FAutoConsoleVariableRef CVarEnableMutableProgressiveMipStreaming(
	TEXT("mutable.EnableMutableProgressiveMipStreaming"), FCustomizableObjectSystemPrivate::EnableMutableProgressiveMipStreaming,
	TEXT("If set to 1 or greater use progressive Mutable Mip streaming for Mutable textures. If disabled, all mips will always be generated and spending memory. In that case, on Desktop platforms they will be stored in CPU memory, on other platforms textures will be non-streaming."),
	ECVF_Default);


int32 FCustomizableObjectSystemPrivate::EnableMutableLiveUpdate = 1;

static FAutoConsoleVariableRef CVarEnableMutableLiveUpdate(
	TEXT("mutable.EnableMutableLiveUpdate"), FCustomizableObjectSystemPrivate::EnableMutableLiveUpdate,
	TEXT("If set to 1 or greater Mutable can use the live update mode if set in the current Mutable state. If disabled, it will never use live update mode even if set in the current Mutable state."),
	ECVF_Default);


int32 FCustomizableObjectSystemPrivate::EnableReuseInstanceTextures = 1;

static FAutoConsoleVariableRef CVarEnableMutableReuseInstanceTextures(
	TEXT("mutable.EnableReuseInstanceTextures"), FCustomizableObjectSystemPrivate::EnableReuseInstanceTextures,
	TEXT("If set to 1 or greater and set in the corresponding setting in the current Mutable state, Mutable can reuse instance UTextures (only uncompressed and not streaming, so set the options in the state) and their resources between updates when they are modified. If geometry or state is changed they cannot be reused."),
	ECVF_Default);


bool FCustomizableObjectSystemPrivate::bEnableMutableReusePreviousUpdateData = false;
static FAutoConsoleVariableRef CVarEnableMutableReusePreviousUpdateData(
	TEXT("mutable.EnableMutableReusePreviousUpdateData"), FCustomizableObjectSystemPrivate::bEnableMutableReusePreviousUpdateData,
	TEXT("If true, Mutable will try to reuse render sections from previous SkeletalMeshes. If false, all SkeletalMeshes will be build from scratch."),
	ECVF_Default);


int32 FCustomizableObjectSystemPrivate::EnableOnlyGenerateRequestedLODs = 1;

static FAutoConsoleVariableRef CVarEnableOnlyGenerateRequestedLODs(
	TEXT("mutable.EnableOnlyGenerateRequestedLODs"), FCustomizableObjectSystemPrivate::EnableOnlyGenerateRequestedLODs,
	TEXT("If 1 or greater, Only the RequestedLODLevels will be generated. If 0, all LODs will be build."),
	ECVF_Default);

int32 FCustomizableObjectSystemPrivate::EnableSkipGenerateResidentMips = 1;

static FAutoConsoleVariableRef CVarSkipGenerateResidentMips(
	TEXT("mutable.EnableSkipGenerateResidentMips"), FCustomizableObjectSystemPrivate::EnableSkipGenerateResidentMips,
	TEXT("If 1 or greater, resident mip generation will be optional. If 0, resident mips will be always generated"),
	ECVF_Default);

int32 FCustomizableObjectSystemPrivate::MaxTextureSizeToGenerate = 0;

FAutoConsoleVariableRef CVarMaxTextureSizeToGenerate(
	TEXT("Mutable.MaxTextureSizeToGenerate"),
	FCustomizableObjectSystemPrivate::MaxTextureSizeToGenerate,
	TEXT("Max texture size on Mutable textures. Mip 0 will be the first mip with max size equal or less than MaxTextureSizeToGenerate."
		"If a texture doesn't have small enough mips, mip 0 will be the last mip available."));

static bool bApplyFixPrepareSkeletons = true;

static FAutoConsoleVariableRef CVarApplyFixPrepareSkeletons(
	TEXT("mutable.ApplyFixPrepareSkeletons"), bApplyFixPrepareSkeletons,
	TEXT("If true, Fix missing SkeletonsData when FirstLODToGenerate is greater than 0. If false, There may be a crash when generating meshes on platform that skip LODs."),
	ECVF_Default);

void FinishUpdateGlobal(UCustomizableObjectInstance* Instance, EUpdateResult UpdateResult, const FInstanceUpdateDelegate* UpdateCallback, const FDescriptorRuntimeHash InUpdatedHash)
{
	check(IsInGameThread())
	
	// Callbacks. Must be done at the end.
	if (Instance)
	{
		Instance->FinishUpdate(UpdateResult, InUpdatedHash);			
	}

	if (UpdateResult == EUpdateResult::Success)
	{
		// Call Customizable Skeletal Components updated callbacks.
		for (TObjectIterator<UCustomizableSkeletalComponent> It; It; ++It) // Since iterating objects is expensive, for now CustomizableSkeletalComponent does not have a FinishUpdate function.
		{
#if WITH_EDITOR
			if (It && It->IsNetMode(NM_DedicatedServer))
			{
				continue;
			}
#endif

			if (const UCustomizableSkeletalComponent* CustomizableSkeletalComponent = *It;
				CustomizableSkeletalComponent &&
				CustomizableSkeletalComponent->CustomizableObjectInstance == Instance)
			{
				CustomizableSkeletalComponent->Callbacks();
			}
		}
	}

	if (UpdateCallback)
	{
		FUpdateContext Context;
		Context.UpdateResult = UpdateResult;
		
		UpdateCallback->ExecuteIfBound(Context);
	}

	UCustomizableObjectSystem::GetInstance()->GetPrivate()->MutableTaskGraph.AllowLaunchingMutableTaskLowPriority(true, false);

	const uint32 InstanceId = Instance ? Instance->GetUniqueID() : 0;
	UE_LOG(LogMutable, Log, TEXT("Finished UpdateSkeletalMesh Async. of Instance %d , frame=%d"), InstanceId, GFrameNumber);
}


/** Update the given Instance Skeletal Meshes and call its callbacks. */
void UpdateSkeletalMesh(UCustomizableObjectInstance& CustomizableObjectInstance, const FDescriptorRuntimeHash& UpdatedDescriptorRuntimeHash, EUpdateResult UpdateResult, FInstanceUpdateDelegate* UpdateCallback)
{
	MUTABLE_CPUPROFILER_SCOPE(UpdateSkeletalMesh);

	check(IsInGameThread());

	for (int32 ComponentIndex = 0; ComponentIndex < CustomizableObjectInstance.SkeletalMeshes.Num(); ++ComponentIndex)
	{
		if (TObjectPtr<USkeletalMesh> SkeletalMesh = CustomizableObjectInstance.SkeletalMeshes[ComponentIndex])
		{
#if WITH_EDITOR
			UCustomizableInstancePrivateData::RegenerateImportedModel(SkeletalMesh);
#else
			SkeletalMesh->RebuildSocketMap();
#endif
		}
	}

	UCustomizableInstancePrivateData* CustomizableObjectInstancePrivateData = CustomizableObjectInstance.GetPrivate();
	check(CustomizableObjectInstancePrivateData != nullptr);
	for (TObjectIterator<UCustomizableSkeletalComponent> It; It; ++It)
	{
		UCustomizableSkeletalComponent* CustomizableSkeletalComponent = *It;

#if WITH_EDITOR
		if (CustomizableSkeletalComponent && CustomizableSkeletalComponent->IsNetMode(NM_DedicatedServer))
		{
			continue;
		}
#endif

		if (CustomizableSkeletalComponent &&
			(CustomizableSkeletalComponent->CustomizableObjectInstance == &CustomizableObjectInstance) &&
			CustomizableObjectInstance.SkeletalMeshes.IsValidIndex(CustomizableSkeletalComponent->ComponentIndex)
		   )
		{
			MUTABLE_CPUPROFILER_SCOPE(UpdateSkeletalMesh_SetSkeletalMesh);

			const bool bIsCreatingSkeletalMesh = CustomizableObjectInstancePrivateData->HasCOInstanceFlags(CreatingSkeletalMesh); //TODO MTBL-391: Review
			CustomizableSkeletalComponent->SetSkeletalMesh(CustomizableObjectInstance.SkeletalMeshes[CustomizableSkeletalComponent->ComponentIndex], false, bIsCreatingSkeletalMesh);

			if (CustomizableObjectInstancePrivateData->HasCOInstanceFlags(ReplacePhysicsAssets))
			{
				CustomizableSkeletalComponent->SetPhysicsAsset(
					CustomizableObjectInstance.SkeletalMeshes[CustomizableSkeletalComponent->ComponentIndex] ? 
					CustomizableObjectInstance.SkeletalMeshes[CustomizableSkeletalComponent->ComponentIndex]->GetPhysicsAsset() : nullptr);
			}
		}
	}

	CustomizableObjectInstancePrivateData->SetCOInstanceFlags(Generated);
	CustomizableObjectInstancePrivateData->ClearCOInstanceFlags(CreatingSkeletalMesh);

	CustomizableObjectInstance.bEditorPropertyChanged = false;

	FinishUpdateGlobal(&CustomizableObjectInstance, UpdateResult, UpdateCallback, UpdatedDescriptorRuntimeHash);
}


void FCustomizableObjectSystemPrivate::GetMipStreamingConfig(const UCustomizableObjectInstance& Instance, bool& bOutNeverStream, int32& OutMipsToSkip) const
{
	const FString CurrentState = Instance.GetCurrentState();
	const FParameterUIData* State = Instance.GetCustomizableObject()->StateUIDataMap.Find(CurrentState);

	// \TODO: This should be controllable independently
	bOutNeverStream = State ? State->TextureCompressionStrategy != ETextureCompressionStrategy::None : false;
	bool bUseMipmapStreaming = !bOutNeverStream;
	OutMipsToSkip = 0; // 0 means all mips

#if PLATFORM_SUPPORTS_TEXTURE_STREAMING
	if (!IStreamingManager::Get().IsTextureStreamingEnabled())
	{
		bUseMipmapStreaming = false;
	}
#else
	bUseMipmapStreaming = false;
#endif

	if (bUseMipmapStreaming && EnableMutableProgressiveMipStreaming)
	{
		OutMipsToSkip = 255; // This means skip all possible mips until only UTexture::GetStaticMinTextureResidentMipCount() are left
	}
}


bool FCustomizableObjectSystemPrivate::IsReplaceDiscardedWithReferenceMeshEnabled() const
{
	return bReplaceDiscardedWithReferenceMesh;
}


void FCustomizableObjectSystemPrivate::SetReplaceDiscardedWithReferenceMeshEnabled(bool bIsEnabled)
{
	bReplaceDiscardedWithReferenceMesh = bIsEnabled;
}


int32 FCustomizableObjectSystemPrivate::GetCountAllocatedSkeletalMesh() const
{
	return MutableStats.CountAllocatedSkeletalMesh;
}


void FCustomizableObjectSystemPrivate::AddTextureReference(const FMutableImageCacheKey& TextureId)
{
	uint32& CountRef = TextureReferenceCount.FindOrAdd(TextureId);

	CountRef++;
}


bool FCustomizableObjectSystemPrivate::RemoveTextureReference(const FMutableImageCacheKey& TextureId)
{
	uint32* CountPtr = TextureReferenceCount.Find(TextureId);

	if (CountPtr && *CountPtr > 0)
	{
		(*CountPtr)--;

		if (*CountPtr == 0)
		{
			TextureReferenceCount.Remove(TextureId);

			return true;
		}
	}
	else
	{
		ensure(false); // Mutable texture reference count is incorrect
		TextureReferenceCount.Remove(TextureId);
	}

	return false;
}


bool FCustomizableObjectSystemPrivate::TextureHasReferences(const FMutableImageCacheKey& TextureId) const
{
	const uint32* CountPtr = TextureReferenceCount.Find(TextureId);

	if (CountPtr && *CountPtr > 0)
	{
		return true;
	}

	return false;
}


EUpdateRequired FCustomizableObjectSystemPrivate::IsUpdateRequired(const UCustomizableObjectInstance& Instance, bool bOnlyUpdateIfNotGenerated, bool bIgnoreCloseDist) const
{
	UCustomizableObjectSystem* System = UCustomizableObjectSystem::GetInstance();
	const UCustomizableInstancePrivateData* const Private = Instance.GetPrivate();
	
	const UCustomizableObject* CustomizableObject = Instance.GetCustomizableObject();
	if (!Instance.CanUpdateInstance() || CustomizableObject->IsLocked())
	{
		return EUpdateRequired::NoUpdate;
	}

	const bool bIsGenerated = Private->HasCOInstanceFlags(Generated);
	const int32 NumGeneratedInstancesLimit = System->GetInstanceLODManagement()->GetNumGeneratedInstancesLimitFullLODs();
	const int32 NumGeneratedInstancesLimitLOD1 = System->GetInstanceLODManagement()->GetNumGeneratedInstancesLimitLOD1();
	const int32 NumGeneratedInstancesLimitLOD2 = System->GetInstanceLODManagement()->GetNumGeneratedInstancesLimitLOD2();

	if (!bIsGenerated && // Prevent generating more instances than the limit, but let updates to existing instances run normally
		NumGeneratedInstancesLimit > 0 &&
		System->GetPrivate()->GetCountAllocatedSkeletalMesh() > NumGeneratedInstancesLimit + NumGeneratedInstancesLimitLOD1 + NumGeneratedInstancesLimitLOD2)
	{
		return EUpdateRequired::NoUpdate;
	}

	const bool bShouldUpdateLODs = Private->HasCOInstanceFlags(PendingLODsUpdate);

	const bool bDiscardByDistance = Private->LastMinSquareDistFromComponentToPlayer > FMath::Square(System->GetInstanceLODManagement()->GetOnlyUpdateCloseCustomizableObjectsDist());
	const bool bLODManagementDiscard = System->GetInstanceLODManagement()->IsOnlyUpdateCloseCustomizableObjectsEnabled() &&
			bDiscardByDistance &&
			!bIgnoreCloseDist;
	
	if (Private->HasCOInstanceFlags(DiscardedByNumInstancesLimit) ||
		bLODManagementDiscard)
	{
		if (bIsGenerated && !Private->HasCOInstanceFlags(Updating))
		{
			return EUpdateRequired::Discard;		
		}
		else
		{
			return EUpdateRequired::NoUpdate;
		}
	}

	if (bIsGenerated &&
		!bShouldUpdateLODs &&
		bOnlyUpdateIfNotGenerated)
	{
		return EUpdateRequired::NoUpdate;
	}

	return EUpdateRequired::Update;
}


EQueuePriorityType FCustomizableObjectSystemPrivate::GetUpdatePriority(const UCustomizableObjectInstance& Instance,	bool bForceHighPriority) const
{
	const UCustomizableInstancePrivateData* InstancePrivate = Instance.GetPrivate();
		
	const bool bIsGenerated = InstancePrivate->HasCOInstanceFlags(Generated);
	const bool bShouldUpdateLODs = InstancePrivate->HasCOInstanceFlags(PendingLODsUpdate);
	const bool bIsDowngradeLODUpdate = InstancePrivate->HasCOInstanceFlags(PendingLODsDowngrade);
	const bool bIsPlayerOrNearIt = InstancePrivate->HasCOInstanceFlags(UsedByPlayerOrNearIt);

	EQueuePriorityType Priority = EQueuePriorityType::Low;
	if (bForceHighPriority)
	{
		Priority = EQueuePriorityType::High;
	}
	else if (!bIsGenerated || !Instance.HasAnySkeletalMesh())
	{
		Priority = EQueuePriorityType::Med;
	}
	else if (bShouldUpdateLODs && bIsDowngradeLODUpdate)
	{
		Priority = EQueuePriorityType::Med_Low;
	}
	else if (bIsPlayerOrNearIt && bShouldUpdateLODs && !bIsDowngradeLODUpdate)
	{
		Priority = EQueuePriorityType::High;
	}
	else if (bShouldUpdateLODs && !bIsDowngradeLODUpdate)
	{
		Priority = EQueuePriorityType::Med;
	}
	else if (bIsPlayerOrNearIt)
	{
		Priority = EQueuePriorityType::High;
	}

	return Priority;
}


void FCustomizableObjectSystemPrivate::EnqueueUpdateSkeletalMesh(UCustomizableObjectInstance& Instance, bool bOnlyUpdateIfNotGenerated, bool bIgnoreCloseDist, bool bForceHighPriority, const EUpdateRequired* OptionalUpdateRequired, FInstanceUpdateDelegate* UpdateCallback)
{
	MUTABLE_CPUPROFILER_SCOPE(FCustomizableObjectSystemPrivate::EnqueueUpdateSkeletalMesh);
	check(IsInGameThread());

	if (!Instance.CanUpdateInstance())
	{
		FinishUpdateGlobal(&Instance, EUpdateResult::ErrorDiscarded, UpdateCallback);
		return;
	}

	UCustomizableObjectSystem* System = UCustomizableObjectSystem::GetInstance();

	UCustomizableInstancePrivateData* InstancePrivate = Instance.GetPrivate();
	
	const EUpdateRequired UpdateRequired = IsUpdateRequired(Instance, bOnlyUpdateIfNotGenerated, bIgnoreCloseDist);
	switch (UpdateRequired)
	{
	case EUpdateRequired::NoUpdate:
	{	
		FinishUpdateGlobal(&Instance, EUpdateResult::ErrorDiscarded, UpdateCallback);
		break;
	}		
	case EUpdateRequired::Update:
	{
		EQueuePriorityType Priority = GetUpdatePriority(Instance, bForceHighPriority);

		const uint32 InstanceId = Instance.GetUniqueID();
		const float Distance = FMath::Sqrt(InstancePrivate->LastMinSquareDistFromComponentToPlayer);
		const bool bIsPlayerOrNearIt = InstancePrivate->HasCOInstanceFlags(UsedByPlayerOrNearIt);
		UE_LOG(LogMutable, Log, TEXT("Enqueued UpdateSkeletalMesh Async. of Instance %d with priority %d at dist %f bIsPlayerOrNearIt=%d, frame=%d"), InstanceId, static_cast<int32>(Priority), Distance, bIsPlayerOrNearIt, GFrameNumber);				

#if WITH_EDITOR
		if (!UCustomizableObjectSystem::GetInstance()->IsCompilationDisabled())
		{
			if (Instance.SkeletalMeshStatus != ESkeletalMeshState::AsyncUpdatePending)
			{
				Instance.PreUpdateSkeletalMeshStatus = Instance.SkeletalMeshStatus;
			}
		}
#endif

		if (InstancePrivate->HasCOInstanceFlags(PendingLODsUpdate))
		{
			UE_LOG(LogMutable, Verbose, TEXT("LOD change: %d, %d -> %d, %d"), Instance.GetCurrentMinLOD(), Instance.GetCurrentMaxLOD(), Instance.GetMinLODToLoad(), Instance.GetMaxLODToLoad());
		}

		InstancePrivate->UpdateDescriptorRuntimeHash = FDescriptorRuntimeHash(Instance.GetDescriptor());

		if (const FMutablePendingInstanceUpdate* QueueElem = MutablePendingInstanceWork.GetUpdate(&Instance))
		{
			if (InstancePrivate->UpdateDescriptorRuntimeHash.IsSubset(FDescriptorRuntimeHash(QueueElem->InstanceDescriptor)))
			{
				FinishUpdateGlobal(&Instance, EUpdateResult::ErrorOptimized, UpdateCallback);			
				return; // The the requested update is equal to the last enqueued update.
			}
		}	

		if (CurrentMutableOperation &&
			&Instance == CurrentMutableOperation->CustomizableObjectInstance &&
			InstancePrivate->UpdateDescriptorRuntimeHash.IsSubset(CurrentMutableOperation->InstanceDescriptorRuntimeHash))
		{
			FinishUpdateGlobal(&Instance, EUpdateResult::ErrorOptimized, UpdateCallback);			
			return; // The requested update is equal to the running update.
		}
	
		if (InstancePrivate->UpdateDescriptorRuntimeHash.IsSubset(InstancePrivate->DescriptorRuntimeHash) &&
			!(CurrentMutableOperation &&
			&Instance == CurrentMutableOperation->CustomizableObjectInstance)) // This condition is necessary because even if the descriptor is a subset, it will be replaced by the CurrentMutableOperation
		{
			Instance.SkeletalMeshStatus = ESkeletalMeshState::Correct; // TODO FutureGMT MTBL-1033 should not be here. Move to UCustomizableObjectInstance::Updated
			UpdateSkeletalMesh(Instance, Instance.GetDescriptorRuntimeHash(), EUpdateResult::Success, UpdateCallback);
		}
		else
		{
			// Cache Texture Parameters being used during the update:
			check(ImageProvider);

			// Cache new Texture Parameters
			for (const FCustomizableObjectTextureParameterValue& TextureParameters : Instance.GetDescriptor().GetTextureParameters())
			{
				ImageProvider->CacheImage(TextureParameters.ParameterValue, false);

				for (const FName& TextureParameter : TextureParameters.ParameterRangeValues)
				{
					ImageProvider->CacheImage(TextureParameter, false);
				}
			}

			// Uncache old Texture Parameters
			for (const FName& TextureParameter : InstancePrivate->UpdateTextureParameters)
			{
				ImageProvider->UnCacheImage(TextureParameter, false);
			}

			// Update which ones are currently are being used
			InstancePrivate->UpdateTextureParameters.Reset();
			for (const FCustomizableObjectTextureParameterValue& TextureParameters : Instance.GetDescriptor().GetTextureParameters())
			{
				InstancePrivate->UpdateTextureParameters.Add(TextureParameters.ParameterValue);

				for (const FName& TextureParameter : TextureParameters.ParameterRangeValues)
				{
					InstancePrivate->UpdateTextureParameters.Add(TextureParameter);
				}
			}

			Instance.SkeletalMeshStatus = ESkeletalMeshState::AsyncUpdatePending;

			const FMutablePendingInstanceUpdate InstanceUpdate(&Instance, Priority, UpdateCallback);
			MutablePendingInstanceWork.AddUpdate(InstanceUpdate);
		}

		break;
	}

	case EUpdateRequired::Discard:
	{
		System->GetPrivate()->InitDiscardResourcesSkeletalMesh(&Instance);
		
		FinishUpdateGlobal(&Instance, EUpdateResult::ErrorDiscarded, UpdateCallback);
		break;
	}

	default:
		unimplemented();
	}
}


void FCustomizableObjectSystemPrivate::InitDiscardResourcesSkeletalMesh(UCustomizableObjectInstance* InCustomizableObjectInstance)
{
	check(IsInGameThread());

	if (InCustomizableObjectInstance && InCustomizableObjectInstance->IsValidLowLevel())
	{
		check(InCustomizableObjectInstance->GetPrivate() != nullptr);
		MutablePendingInstanceWork.AddDiscard(FMutablePendingInstanceDiscard(InCustomizableObjectInstance));
	}
}


void FCustomizableObjectSystemPrivate::InitInstanceIDRelease(mu::Instance::ID IDToRelease)
{
	check(IsInGameThread());

	MutablePendingInstanceWork.AddIDRelease(IDToRelease);
}


bool UCustomizableObjectSystem::IsReplaceDiscardedWithReferenceMeshEnabled() const
{
	if (Private.IsValid())
	{
		return Private->IsReplaceDiscardedWithReferenceMeshEnabled();
	}

	return false;
}


void UCustomizableObjectSystem::SetReplaceDiscardedWithReferenceMeshEnabled(bool bIsEnabled)
{
	if (Private.IsValid())
	{
		Private->SetReplaceDiscardedWithReferenceMeshEnabled(bIsEnabled);
	}
}


void UCustomizableObjectSystem::ClearResourceCacheProtected()
{
	check(IsInGameThread());

	ProtectedCachedTextures.Reset(0);
	check(GetPrivate() != nullptr);
	GetPrivate()->ProtectedObjectCachedImages.Reset(0);
}


#if WITH_EDITOR
bool UCustomizableObjectSystem::LockObject(const class UCustomizableObject* InObject)
{
	check(InObject != nullptr);
	check(!InObject->GetPrivate()->bLocked);
	check(IsInGameThread() && !IsInParallelGameThread());

	if (InObject && InObject->GetPrivate() && Private)
	{
		// If the current instance is for this object, make the lock fail by returning false
		if (Private->CurrentInstanceBeingUpdated &&
			Private->CurrentInstanceBeingUpdated->GetCustomizableObject() == InObject)
		{
			UE_LOG(LogMutable, Warning, TEXT("---- failed to lock object %s"), *InObject->GetName());

			return false;
		}

		FString Message = FString::Printf(TEXT("Customizable Object %s has pending texture streaming operations. Please wait a few seconds and try again."),
			*InObject->GetName());

		// Pre-check pending operations before locking. This check is redundant and incomplete because it's checked again after locking 
		// and some operations may start between here and the actual lock. But in the CO Editor preview it will prevent some 
		// textures getting stuck at low resolution when they try to update mips and are cancelled when the user presses 
		// the compile button but the compilation quits anyway because there are pending operations
		if (CheckIfDiskOrMipUpdateOperationsPending(*InObject))
		{
			UE_LOG(LogMutable, Warning, TEXT("%s"), *Message);

			return false;
		}

		// Lock the object, no new file or mip streaming operations should start from this point
		InObject->GetPrivate()->bLocked = true;

		// But some could have started between the first CheckIfDiskOrMipUpdateOperationsPending and the lock a few lines back, so check again
		if (CheckIfDiskOrMipUpdateOperationsPending(*InObject))
		{
			UE_LOG(LogMutable, Warning, TEXT("%s"), *Message);

			// Unlock and return because the pending operations cannot be easily stopped now, the compilation hasn't started and the CO
			// hasn't changed state yet. It's simpler to quit the compilation, unlock and let the user try to compile again
			InObject->GetPrivate()->bLocked = false;

			return false;
		}

		// Ensure that we don't try to handle any further streaming operations for this object
		check(GetPrivate() != nullptr);
		if (GetPrivate()->Streamer)
		{
			GetPrivate()->Streamer->CancelStreamingForObject(InObject);
		}

		// Clear the cache for the instance, since we will remake it
		FMutableResourceCache& Cache = GetPrivate()->GetObjectCache(InObject);
		Cache.Clear();

		check(InObject->GetPrivate()->bLocked);

		return true;
	}
	else
	{
		FString ObjectName = InObject ? InObject->GetName() : FString("null");
		UE_LOG(LogMutable, Warning, TEXT("Failed to lock the object [%s] because it was null or the system was null or partially destroyed."), *ObjectName);

		return false;
	}
}


void UCustomizableObjectSystem::UnlockObject(const class UCustomizableObject* Obj)
{
	check(Obj != nullptr);
	check(Obj->GetPrivate()->bLocked);
	check(IsInGameThread() && !IsInParallelGameThread());
	
	if (Obj && Obj->GetPrivate())
	{
		Obj->GetPrivate()->bLocked = false;
	}
}


bool UCustomizableObjectSystem::CheckIfDiskOrMipUpdateOperationsPending(const UCustomizableObject& Object) const
{
	for (TObjectIterator<UCustomizableObjectInstance> CustomizableObjectInstance; CustomizableObjectInstance; ++CustomizableObjectInstance)
	{
		if (CustomizableObjectInstance->GetCustomizableObject() == &Object)
		{
			for (const FGeneratedTexture& GeneratedTexture : CustomizableObjectInstance->GetPrivate()->GeneratedTextures)
			{
				if (GeneratedTexture.Texture->HasPendingInitOrStreaming())
				{
					return true;
				}
			}
		}
	}

	// Ensure that we don't try to handle any further streaming operations for this object
	check(GetPrivate());
	if (const FUnrealMutableModelBulkReader* Streamer = GetPrivate()->Streamer.Get())
	{
		if (Streamer->AreTherePendingStreamingOperationsForObject(&Object))
		{
			return true;
		}
	}

	return false;
}


void UCustomizableObjectSystem::EditorSettingsChanged(const FEditorCompileSettings& InEditorSettings)
{
	EditorSettings = InEditorSettings;
}


bool UCustomizableObjectSystem::IsCompilationDisabled() const
{
	return EditorSettings.bDisableCompilation;
}


bool UCustomizableObjectSystem::IsAutoCompileEnabled() const
{
	return EditorSettings.bEnableAutomaticCompilation;
}


bool UCustomizableObjectSystem::IsAutoCompilationSync() const
{
	return EditorSettings.bCompileObjectsSynchronously;
}

#endif


void UCustomizableObjectSystem::PurgePendingReleaseSkeletalMesh()
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableObjectSystem::PurgePendingReleaseSkeletalMesh);

	const double CurTime = FPlatformTime::Seconds();
	static double TimeToDelete = 1.0;

	for (int32 InfoIndex = PendingReleaseSkeletalMesh.Num() - 1; InfoIndex >= 0; --InfoIndex)
	{
		FPendingReleaseSkeletalMeshInfo& Info = PendingReleaseSkeletalMesh[InfoIndex];
		
		if (Info.SkeletalMesh != nullptr)
		{
			if ((CurTime - Info.TimeStamp) >= TimeToDelete)
			{
				if (Info.SkeletalMesh->GetSkeleton())
				{
					Info.SkeletalMesh->GetSkeleton()->ClearCacheData();
				}
				Info.SkeletalMesh->GetRefSkeleton().Empty();
				Info.SkeletalMesh->GetMaterials().Empty();
				Info.SkeletalMesh->GetRefBasesInvMatrix().Empty();
				Info.SkeletalMesh->ReleaseResources();
				Info.SkeletalMesh->ReleaseResourcesFence.Wait();

				PendingReleaseSkeletalMesh.RemoveAt(InfoIndex);
			}
		}
	}
}

static int32 TicksUntilMaterialRelease = 4;

static FAutoConsoleVariableRef CVarTicksUntilMaterialRelease(
	TEXT("mutable.TicksUntilMaterialRelease"), TicksUntilMaterialRelease,
	TEXT("If higher than zero, the number of ticks to keep discarded materials guarded from being GCed."),
	ECVF_Default);

void UCustomizableObjectSystem::AddPendingReleaseMaterials(TArray<TObjectPtr<UMaterialInterface>>& InMaterials)
{
	if (TicksUntilMaterialRelease > 0)
	{
		FPendingReleaseMaterialsInfo& PendingMaterials = PendingReleaseMaterials.AddDefaulted_GetRef();
		PendingMaterials.Materials = InMaterials;
		PendingMaterials.TicksUntilRelease = TicksUntilMaterialRelease;
	}
}


void UCustomizableObjectSystem::TickPendingReleaseMaterials()
{
	for (TArray<FPendingReleaseMaterialsInfo>::TIterator PendingRelease = PendingReleaseMaterials.CreateIterator(); PendingRelease; ++PendingRelease)
	{
		if (--PendingRelease->TicksUntilRelease <= 0)
		{
			PendingRelease.RemoveCurrent();
		}
	}
}


void UCustomizableObjectSystem::AddPendingReleaseSkeletalMesh(USkeletalMesh* SkeletalMesh)
{
	check(SkeletalMesh != nullptr);

	FPendingReleaseSkeletalMeshInfo	Info;
	Info.SkeletalMesh = SkeletalMesh;
	Info.TimeStamp = FPlatformTime::Seconds();

	PendingReleaseSkeletalMesh.Add(Info);
}


void UCustomizableObjectSystem::ClearCurrentMutableOperation()
{
	check(Private != nullptr);
	Private->CurrentInstanceBeingUpdated = nullptr;
	Private->CurrentMutableOperation = nullptr;
	ClearResourceCacheProtected();
}


void FCustomizableObjectSystemPrivate::UpdateMemoryLimit()
{
	// This must run on game thread, and when the mutable thread is not running
	check(IsInGameThread());

	const uint64 MemoryBytes = CVarWorkingMemory.GetValueOnGameThread() * 1024;
	if (MemoryBytes != LastWorkingMemoryBytes)
	{
		LastWorkingMemoryBytes = MemoryBytes;
		check(MutableSystem);
		MutableSystem->SetWorkingMemoryBytes(MemoryBytes);
	}

	const uint32 GeneratedResourceCacheSize = CVarGeneratedResourcesCacheSize.GetValueOnGameThread();
	if (GeneratedResourceCacheSize != LastGeneratedResourceCacheSize)
	{
		LastGeneratedResourceCacheSize = GeneratedResourceCacheSize;
		check(MutableSystem);
		MutableSystem->SetGeneratedCacheSize(GeneratedResourceCacheSize);
	}
}


// Asynchronous tasks performed during the creation or update of a mutable instance. 
// Check the documentation before modifying and keep it up to date.
// https://docs.google.com/drawings/d/109NlsdKVxP59K5TuthJkleVG3AROkLJr6N03U4bNp4s
// When it says "mutable thread" it means any task pool thread, but with the guarantee that no other thread is using the mutable runtime.
// Naming: Task_<thread>_<description>
namespace impl
{

	void Subtask_Mutable_UpdateParameterRelevancy(const TSharedPtr<FMutableOperationData>& OperationData)
	{
		MUTABLE_CPUPROFILER_SCOPE(Subtask_Mutable_UpdateParameterRelevancy)

		check(OperationData);
		check(OperationData->MutableParameters);
		check(OperationData->InstanceID != 0);

		OperationData->RelevantParametersInProgress.Empty();

		// This must run in the mutable thread.
		check(UCustomizableObjectSystem::GetInstance() != nullptr);
		check(UCustomizableObjectSystem::GetInstance()->GetPrivate() != nullptr);
		const mu::Ptr<mu::System> MutableSystem = UCustomizableObjectSystem::GetInstance()->GetPrivate()->MutableSystem;

		// Update the parameter relevancy.
		{
			MUTABLE_CPUPROFILER_SCOPE(ParameterRelevancy)

			const int32 NumParameters = OperationData->MutableParameters->GetCount();

			TArray<bool> Relevant;
			Relevant.SetNumZeroed(NumParameters);
			MutableSystem->GetParameterRelevancy(OperationData->InstanceID, OperationData->MutableParameters, Relevant.GetData());

			for (int32 ParamIndex = 0; ParamIndex < NumParameters; ++ParamIndex)
			{
				if (Relevant[ParamIndex])
				{
					OperationData->RelevantParametersInProgress.Add(ParamIndex);
				}
			}
		}
	}


	// This runs in the mutable thread.
	void Subtask_Mutable_BeginUpdate_GetMesh(const TSharedPtr<FMutableOperationData>& OperationData, TSharedPtr<mu::Model> Model)
	{
		MUTABLE_CPUPROFILER_SCOPE(Subtask_Mutable_BeginUpdate_GetMesh)

		check(OperationData->MutableParameters);
		check(OperationData);
		OperationData->InstanceUpdateData.Clear();

		check(UCustomizableObjectSystem::GetInstance() != nullptr);
		check(UCustomizableObjectSystem::GetInstance()->GetPrivate() != nullptr);
		mu::System* System = UCustomizableObjectSystem::GetInstance()->GetPrivate()->MutableSystem.get();
		check(System != nullptr);

		const UCustomizableObject* CustomizableObject = OperationData->Instance->GetCustomizableObject();
		UCustomizableInstancePrivateData* CustomizableObjectInstancePrivateData = OperationData->Instance->GetPrivate();

		CustomizableObjectInstancePrivateData->PassThroughTexturesToLoad.Empty();

		if (OperationData->bLiveUpdateMode)
		{
			if (OperationData->InstanceID == 0)
			{
				// It's the first update since the instance was put in LiveUpdate Mode, this ID will be reused from now on
				OperationData->InstanceID = System->NewInstance(Model);
				UE_LOG(LogMutable, Verbose, TEXT("Creating Mutable instance with id [%d] for reuse "), OperationData->InstanceID);
			}
			else
			{
				// The instance was already in LiveUpdate Mode, the ID is reused
				check(OperationData->InstanceID);
				UE_LOG(LogMutable, Verbose, TEXT("Reusing Mutable instance with id [%d] "), OperationData->InstanceID);
			}
		}
		else
		{
			// In non-LiveUpdate mode, we are forcing the recreation of mutable-side instances with every update.
			check(OperationData->InstanceID == 0);
			OperationData->InstanceID = System->NewInstance(Model);
			UE_LOG(LogMutable, Verbose, TEXT("Creating Mutable instance with id [%d] "), OperationData->InstanceID);
		}

		check(OperationData->InstanceID != 0);

		if (OperationData->PixelFormatOverride)
		{
			System->SetImagePixelConversionOverride( OperationData->PixelFormatOverride );
		}

		// Main instance generation step
		// LOD mask, set to all ones to build  all LODs
		const mu::Instance* Instance = System->BeginUpdate(OperationData->InstanceID, OperationData->MutableParameters, OperationData->State, mu::System::AllLODs);
		if (!Instance)
		{
			UE_LOG(LogMutable, Warning, TEXT("An Instace update has failed."));
			return;
		}

		OperationData->NumLODsAvailable = Instance->GetLODCount();

		if (OperationData->CurrentMinLOD >= OperationData->NumLODsAvailable)
		{
			OperationData->CurrentMinLOD = OperationData->NumLODsAvailable - 1;
			OperationData->CurrentMaxLOD = OperationData->CurrentMinLOD;
		}
		else if (OperationData->CurrentMaxLOD >= OperationData->NumLODsAvailable)
		{
			OperationData->CurrentMaxLOD = OperationData->NumLODsAvailable - 1;
		}

		if(!OperationData->RequestedLODs.IsEmpty())
		{
			// Initialize RequestedLODs to zero if not set
			const int32 ComponentCount = Instance->GetComponentCount(OperationData->CurrentMinLOD);
			OperationData->RequestedLODs.SetNumZeroed(ComponentCount);
			
			for (int32 ComponentIndex = 0; ComponentIndex < ComponentCount; ++ComponentIndex)
			{
				// Ensure we're generating at least one LOD
				if (OperationData->RequestedLODs[ComponentIndex] == 0)
				{
					OperationData->RequestedLODs[ComponentIndex] |= (1 << OperationData->CurrentMaxLOD);
				}

				// Make sure we are not requesting a LOD that doesn't exist in this state (Essentially for states with bBuildOnlyFirstLOD 
				// and NumExtraLODsToBuildPerPlatform when the ExtraLOD is not needed)
				if ((OperationData->RequestedLODs[ComponentIndex] & (1 << OperationData->CurrentMaxLOD)) == 0)
				{
					OperationData->CurrentMaxLOD = OperationData->CurrentMinLOD;

					// Ensure the fallback LOD actually exists
					OperationData->RequestedLODs[ComponentIndex] |= (1 << OperationData->CurrentMaxLOD);
				}
			}
		}

		// Map SharedSurfaceId to surface index
		TArray<int32> SurfacesSharedId;

		// Generate the mesh and gather all the required resource Ids
		OperationData->InstanceUpdateData.LODs.SetNum(Instance->GetLODCount());
		for (int32 MutableLODIndex = 0; MutableLODIndex < Instance->GetLODCount(); ++MutableLODIndex)
		{
			// Skip LODs outside the range we want to generate
			if (MutableLODIndex < OperationData->CurrentMinLOD || MutableLODIndex > OperationData->CurrentMaxLOD)
			{
				continue;
			}

			FInstanceUpdateData::FLOD& LOD = OperationData->InstanceUpdateData.LODs[MutableLODIndex];
			LOD.FirstComponent = OperationData->InstanceUpdateData.Components.Num();
			LOD.ComponentCount = Instance->GetComponentCount(MutableLODIndex);

			const FInstanceGeneratedData::FLOD& PrevUpdateLODData = OperationData->LastUpdateData.LODs.IsValidIndex(MutableLODIndex) ?
				OperationData->LastUpdateData.LODs[MutableLODIndex] : FInstanceGeneratedData::FLOD();


			for (int32 ComponentIndex = 0; ComponentIndex < LOD.ComponentCount; ++ComponentIndex)
			{
				OperationData->InstanceUpdateData.Components.Push(FInstanceUpdateData::FComponent());
				FInstanceUpdateData::FComponent& Component = OperationData->InstanceUpdateData.Components.Last();
				Component.Id = Instance->GetComponentId(MutableLODIndex, ComponentIndex);
				Component.FirstSurface = OperationData->InstanceUpdateData.Surfaces.Num();
				Component.SurfaceCount = 0;

				const FInstanceGeneratedData::FComponent& PrevUpdateComponent = OperationData->LastUpdateData.Components.IsValidIndex(PrevUpdateLODData.FirstComponent + ComponentIndex) ?
					OperationData->LastUpdateData.Components[PrevUpdateLODData.FirstComponent +  ComponentIndex] : FInstanceGeneratedData::FComponent();

				const bool bGenerateLOD = OperationData->RequestedLODs.IsValidIndex(Component.Id) ? (OperationData->RequestedLODs[Component.Id] & (1 << MutableLODIndex)) != 0 : true;

				// Mesh
				if (Instance->GetMeshCount(MutableLODIndex, ComponentIndex) > 0)
				{
					MUTABLE_CPUPROFILER_SCOPE(GetMesh);

					Component.MeshID = Instance->GetMeshId(MutableLODIndex, ComponentIndex, 0);

					// Mesh cache is not enabled yet.
					//auto CachedMesh = Cache.Meshes.Find(meshId);
					//if (CachedMesh && CachedMesh->IsValid(false, true))
					//{
					//	UE_LOG(LogMutable, Verbose, TEXT("Mesh resource with id [%d] can be cached."), meshId);
					//	INC_DWORD_STAT(STAT_MutableNumCachedSkeletalMeshes);
					//}

					// Check if we can use data from the currently generated SkeletalMesh
					Component.bReuseMesh = OperationData->bCanReuseGeneratedData && PrevUpdateComponent.bGenerated ? PrevUpdateComponent.MeshID == Component.MeshID : false;

					if(bGenerateLOD && !Component.bReuseMesh)
					{
						Component.Mesh = System->GetMesh(OperationData->InstanceID, Component.MeshID);
					}
				}

				if (!Component.Mesh && !Component.bReuseMesh)
				{
					continue;
				}

				Component.bGenerated = true;

				const int32 SurfaceCount = Component.Mesh ? Component.Mesh->GetSurfaceCount() : PrevUpdateComponent.SurfaceCount;

				// Materials and images
				for (int32 MeshSurfaceIndex = 0; MeshSurfaceIndex < SurfaceCount; ++MeshSurfaceIndex)
				{
					const uint32 SurfaceId = Component.Mesh ? Component.Mesh->GetSurfaceId(MeshSurfaceIndex) : OperationData->LastUpdateData.SurfaceIds[PrevUpdateComponent.FirstSurface + MeshSurfaceIndex];
					const int32 InstanceSurfaceIndex = Instance->FindSurfaceById(MutableLODIndex, ComponentIndex, SurfaceId);
					check(Component.bReuseMesh || Component.Mesh->GetVertexCount() == 0 || InstanceSurfaceIndex >= 0);

					int32 BaseSurfaceIndex = InstanceSurfaceIndex;
					int32 BaseLODIndex = MutableLODIndex;

					if (InstanceSurfaceIndex >= 0)
					{
						OperationData->InstanceUpdateData.Surfaces.Push({});
						FInstanceUpdateData::FSurface& Surface = OperationData->InstanceUpdateData.Surfaces.Last();
						++Component.SurfaceCount;

						// Now Surface.MaterialIndex is decoded from a parameter at the end of this if()
						Surface.SurfaceId = SurfaceId;

						const int32 SharedSurfaceId = Instance->GetSharedSurfaceId(MutableLODIndex, ComponentIndex, InstanceSurfaceIndex);
						const int32 SharedSurfaceIndex = SurfacesSharedId.Find(SharedSurfaceId);

						SurfacesSharedId.Add(SharedSurfaceId);

						if (SharedSurfaceId != INDEX_NONE)
						{
							if (SharedSurfaceIndex >= 0)
							{
								Surface = OperationData->InstanceUpdateData.Surfaces[SharedSurfaceIndex];
								continue;
							}

							// Find the first LOD where this surface can be found
							Instance->FindBaseSurfaceBySharedId(ComponentIndex, SharedSurfaceId, BaseSurfaceIndex, BaseLODIndex);

							Surface.SurfaceId = Instance->GetSurfaceId(BaseLODIndex, ComponentIndex, BaseSurfaceIndex);
						}

						// Images
						Surface.FirstImage = OperationData->InstanceUpdateData.Images.Num();
						Surface.ImageCount = Instance->GetImageCount(BaseLODIndex, ComponentIndex, InstanceSurfaceIndex);
						for (int32 ImageIndex = 0; ImageIndex < Surface.ImageCount; ++ImageIndex)
						{
							MUTABLE_CPUPROFILER_SCOPE(GetImageId);

							OperationData->InstanceUpdateData.Images.Push({});
							FInstanceUpdateData::FImage& Image = OperationData->InstanceUpdateData.Images.Last();
							Image.Name = Instance->GetImageName(BaseLODIndex, ComponentIndex, InstanceSurfaceIndex, ImageIndex);
							Image.ImageID = Instance->GetImageId(BaseLODIndex, ComponentIndex, BaseSurfaceIndex, ImageIndex);
							Image.FullImageSizeX = 0;
							Image.FullImageSizeY = 0;
							Image.BaseLOD = BaseLODIndex;
							Image.BaseMip = 0;

							FString KeyName = Image.Name.ToString();
							int32 ImageKey = FCString::Atoi(*KeyName);

							if (ImageKey >= 0 && ImageKey < CustomizableObject->ImageProperties.Num())
							{
								const FMutableModelImageProperties& Props = CustomizableObject->ImageProperties[ImageKey];

								if (Props.IsPassThrough)
								{
									// Since it's known it's a pass-through texture, the GetImage will be inexpensive enough to be run in the game thread
									Image.Image = System->GetImage(OperationData->InstanceID, Image.ImageID, 0, 0);
									check(Image.Image->IsReference());

									uint32 ReferenceID = Image.Image->GetReferencedTexture();

									if (CustomizableObject->ReferencedPassThroughTextures.IsValidIndex(ReferenceID))
									{
										TSoftObjectPtr<UTexture> Ref = CustomizableObject->ReferencedPassThroughTextures[ReferenceID];
										CustomizableObjectInstancePrivateData->PassThroughTexturesToLoad.Add(Ref);
										Image.bIsPassThrough = true;
									}
									else
									{
										// internal error.
										UE_LOG(LogMutable, Error, TEXT("Referenced image [%d] was not stored in the resource array."), ReferenceID);
									}
								}
							}
							else
							{
								// This means the compiled model (maybe coming from derived data) has images that the asset doesn't know about.
								UE_LOG(LogMutable, Error, TEXT("CustomizableObject derived data out of sync with asset for [%s]. Try recompiling it."), *CustomizableObject->GetName());
							}
						}

						// Vectors
						Surface.FirstVector = OperationData->InstanceUpdateData.Vectors.Num();
						Surface.VectorCount = Instance->GetVectorCount(BaseLODIndex, ComponentIndex, InstanceSurfaceIndex);
						for (int32 VectorIndex = 0; VectorIndex < Surface.VectorCount; ++VectorIndex)
						{
							MUTABLE_CPUPROFILER_SCOPE(GetVector);
							OperationData->InstanceUpdateData.Vectors.Push({});
							FInstanceUpdateData::FVector& Vector = OperationData->InstanceUpdateData.Vectors.Last();
							Vector.Name = Instance->GetVectorName(BaseLODIndex, ComponentIndex, InstanceSurfaceIndex, VectorIndex);
							Vector.Vector = Instance->GetVector(BaseLODIndex, ComponentIndex, InstanceSurfaceIndex, VectorIndex);
						}

						// Scalars
						Surface.FirstScalar = OperationData->InstanceUpdateData.Scalars.Num();
						Surface.ScalarCount = Instance->GetScalarCount(BaseLODIndex, ComponentIndex, InstanceSurfaceIndex);
						for (int32 ScalarIndex = 0; ScalarIndex < Surface.ScalarCount; ++ScalarIndex)
						{
							MUTABLE_CPUPROFILER_SCOPE(GetScalar)

							const FName ScalarName = Instance->GetScalarName(BaseLODIndex, ComponentIndex, InstanceSurfaceIndex, ScalarIndex);
							const float ScalarValue = Instance->GetScalar(BaseLODIndex, ComponentIndex, InstanceSurfaceIndex, ScalarIndex);
							
							FString EncodingMaterialIdString = "__MutableMaterialId";
							
							// Decoding Material Switch from Mutable parameter name
							if (ScalarName.ToString().Equals(EncodingMaterialIdString))
							{
								Surface.MaterialIndex = static_cast<uint32>(ScalarValue);
							
								// This parameter is not needed in the final material instance
								Surface.ScalarCount -= 1;
							}
							else
							{
								OperationData->InstanceUpdateData.Scalars.Push({ ScalarName, ScalarValue });
							}
						}
					}
				}
			}
		}

		// Copy ExtensionData Object node input from the Instance to the InstanceUpdateData
		for (int32 ExtensionDataIndex = 0; ExtensionDataIndex < Instance->GetExtensionDataCount(); ExtensionDataIndex++)
		{
			mu::Ptr<const mu::ExtensionData> ExtensionData;
			FName Name;
			Instance->GetExtensionData(ExtensionDataIndex, ExtensionData, Name);

			check(ExtensionData);

			FInstanceUpdateData::FNamedExtensionData& NewEntry = OperationData->InstanceUpdateData.ExtendedInputPins.AddDefaulted_GetRef();
			NewEntry.Data = ExtensionData;
			NewEntry.Name = Name;
			check(NewEntry.Name != NAME_None);
		}
	}


	// This runs in the mutable thread.
	void Subtask_Mutable_GetImages(const TSharedPtr<FMutableOperationData>& OperationData)
	{
		MUTABLE_CPUPROFILER_SCOPE(Subtask_Mutable_GetImages)

		check(OperationData);

		const FCustomizableObjectSystemPrivate* CustomizableObjectSystemPrivateData = UCustomizableObjectSystem::GetInstanceChecked()->GetPrivateChecked();
		mu::System* System = CustomizableObjectSystemPrivateData->MutableSystem.get();
		check(System != nullptr);

		// Generate all the required resources, that are not cached
		TArray<mu::FResourceID> ImagesInThisInstance;
		for (FInstanceUpdateData::FImage& Image : OperationData->InstanceUpdateData.Images)
		{
			MUTABLE_CPUPROFILER_SCOPE(GetImage);

			// If the image is a reference to an engine texture, we are done.
			if (Image.bIsPassThrough)
			{
				continue;
			}

			mu::FImageDesc ImageDesc;

			// This should only be done when using progressive images, since GetImageDesc does some actual processing.
			{
				System->GetImageDesc(OperationData->InstanceID, Image.ImageID, ImageDesc);

				const uint16 MaxTextureSizeToGenerate = static_cast<uint16>(CustomizableObjectSystemPrivateData->MaxTextureSizeToGenerate);
				const uint16 MaxSize = FMath::Max(ImageDesc.m_size[0], ImageDesc.m_size[1]);
				uint16 Reduction = 1;

				if (MaxTextureSizeToGenerate > 0 && MaxSize > MaxTextureSizeToGenerate)
				{
					// Find the reduction factor, and the BaseMip of the texture.
					const uint32 NextPowerOfTwo = FMath::RoundUpToPowerOfTwo(FMath::DivideAndRoundUp(MaxSize, MaxTextureSizeToGenerate));
					Reduction = FMath::Max(NextPowerOfTwo, 2U); // At least divide the texture by a factor of two
					Image.BaseMip = FMath::FloorLog2(Reduction);
				}

				Image.FullImageSizeX = ImageDesc.m_size[0] / Reduction;
				Image.FullImageSizeY = ImageDesc.m_size[1] / Reduction;
			}

			const bool bCached = ImagesInThisInstance.Contains(Image.ImageID) || // See if it is cached from this same instance (can happen with LODs)
				(CVarReuseImagesBetweenInstances.GetValueOnAnyThread() && CustomizableObjectSystemPrivateData->ProtectedObjectCachedImages.Contains(Image.ImageID)); // See if it is cached from another instance

			if (bCached)
			{
				UE_LOG(LogMutable, VeryVerbose, TEXT("Texture resource with id [%llu] is cached."), Image.ImageID);
				INC_DWORD_STAT(STAT_MutableNumCachedTextures);
			}
			else
			{
				const int32 MaxSize = FMath::Max(Image.FullImageSizeX, Image.FullImageSizeY);
				const int32 FullLODCount = FMath::CeilLogTwo(MaxSize) + 1;
				const int32 MinMipsInImage = FMath::Min(FullLODCount, UTexture::GetStaticMinTextureResidentMipCount());
				const int32 MaxMipsToSkip = FullLODCount - MinMipsInImage;
				int32 MipsToSkip = FMath::Min(MaxMipsToSkip, OperationData->MipsToSkip);

				if (!FMath::IsPowerOfTwo(Image.FullImageSizeX) || !FMath::IsPowerOfTwo(Image.FullImageSizeY))
				{
					// It doesn't make sense to skip mips as non-power-of-two size textures cannot be streamed anyway
					MipsToSkip = 0;
				}

				const int32 MipSizeX = FMath::Max(Image.FullImageSizeX >> MipsToSkip, 1);
				const int32 MipSizeY = FMath::Max(Image.FullImageSizeY >> MipsToSkip, 1);
				if (MipsToSkip > 0 && CustomizableObjectSystemPrivateData->EnableSkipGenerateResidentMips != 0 && OperationData->LowPriorityTextures.Find(Image.Name.ToString()) != INDEX_NONE)
				{
					Image.Image = new mu::Image(MipSizeX, MipSizeY, FullLODCount - MipsToSkip, ImageDesc.m_format, mu::EInitializationType::Black);
				}
				else
				{
					Image.Image = System->GetImage(OperationData->InstanceID, Image.ImageID, Image.BaseMip + MipsToSkip, Image.BaseLOD);
				}

				check(Image.Image);

				// We should have generated exactly this size.
				const bool bSizeMissmatch = Image.Image->GetSizeX() != MipSizeX || Image.Image->GetSizeY() != MipSizeY;
				if (bSizeMissmatch)
				{
					// Generate a correctly-sized but empty image instead, to avoid crashes.
					UE_LOG(LogMutable, Warning, TEXT("Mutable generated a wrongly-sized image %llu."), Image.ImageID);
					Image.Image = new mu::Image(MipSizeX, MipSizeY, FullLODCount - MipsToSkip, Image.Image->GetFormat(), mu::EInitializationType::Black);
				}

				// We need one mip or the complete chain. Otherwise there was a bug.
				const int32 FullMipCount = Image.Image->GetMipmapCount(Image.Image->GetSizeX(), Image.Image->GetSizeY());
				const int32 RealMipCount = Image.Image->GetLODCount();

				bool bForceMipchain = 
					// Did we fail to generate the entire mipchain (if we have mips at all)?
					(RealMipCount != 1) && (RealMipCount != FullMipCount);

				if (bForceMipchain)
				{
					MUTABLE_CPUPROFILER_SCOPE(GetImage_MipFix);

					UE_LOG(LogMutable, Warning, TEXT("Mutable generated an incomplete mip chain for image %llu."), Image.ImageID);

					// Force the right number of mips. The missing data will be black.
					const mu::Ptr<mu::Image> NewImage = new mu::Image(Image.Image->GetSizeX(), Image.Image->GetSizeY(), FullMipCount, Image.Image->GetFormat(), mu::EInitializationType::Black);
					check(NewImage);
					if (NewImage->GetDataSize() >= Image.Image->GetDataSize())
					{
						FMemory::Memcpy(NewImage->GetData(), Image.Image->GetData(), Image.Image->GetDataSize());
					}
					Image.Image = NewImage;
				}

				ImagesInThisInstance.Add(Image.ImageID);
			}
		}
	}
	

	// This runs in a worker thread
	void Subtask_Mutable_PrepareTextures(const TSharedPtr<FMutableOperationData>& OperationData)
	{
		MUTABLE_CPUPROFILER_SCOPE(Subtask_Mutable_PrepareTextures)

		check(OperationData);
		for (const FInstanceUpdateData::FSurface& Surface : OperationData->InstanceUpdateData.Surfaces)
		{
			for (int32 ImageIndex = 0; ImageIndex<Surface.ImageCount; ++ImageIndex)
			{
				const FInstanceUpdateData::FImage& Image = OperationData->InstanceUpdateData.Images[Surface.FirstImage+ImageIndex];

				const FName KeyName = Image.Name;
				mu::ImagePtrConst MutableImage = Image.Image;

				// If the image is null, it must be in the cache (or repeated in this instance), and we don't need to do anything here.
				if (MutableImage)
				{
					// Image references are just references to texture assets and require no work at all
					if (!MutableImage->IsReference())
					{
						if (!OperationData->ImageToPlatformDataMap.Contains(Image.ImageID))
						{
							FTexturePlatformData* PlatformData = UCustomizableInstancePrivateData::MutableCreateImagePlatformData(MutableImage, -1, Image.FullImageSizeX, Image.FullImageSizeY);
							OperationData->ImageToPlatformDataMap.Add(Image.ImageID, PlatformData);
							OperationData->PendingTextureCoverageQueries.Add({ KeyName.ToString(), Surface.MaterialIndex, PlatformData});
						}
						else
						{
							// The ImageID already exists in the ImageToPlatformDataMap, that means the equivalent surface in a lower
							// LOD already created the PlatformData for that ImageID and added it to the ImageToPlatformDataMap.
						}
					}
				}
			}
		}
	}
	

	// This runs in a worker thread
	void Subtask_Mutable_PrepareSkeletonData(const TSharedPtr<FMutableOperationData>& OperationData)
	{
		MUTABLE_CPUPROFILER_SCOPE(Subtask_Mutable_PrepareSkeletonData)

		if (bApplyFixPrepareSkeletons)
		{
			for (FInstanceUpdateData::FComponent& Component : OperationData->InstanceUpdateData.Components)
			{
				if (!OperationData->InstanceUpdateData.Skeletons.IsValidIndex(Component.Id))
				{
					OperationData->InstanceUpdateData.Skeletons.SetNum(Component.Id + 1);
				}

				FInstanceUpdateData::FSkeletonData& SkeletonData = OperationData->InstanceUpdateData.Skeletons[Component.Id];
				SkeletonData.ComponentIndex = Component.Id;

				mu::MeshPtrConst Mesh = Component.Mesh;
				if (!Mesh)
				{
					continue;
				}

				// Add SkeletonIds 
				const int32 SkeletonIDsCount = Mesh->GetSkeletonIDsCount();
				for (int32 SkeletonIndex = 0; SkeletonIndex < SkeletonIDsCount; ++SkeletonIndex)
				{
					SkeletonData.SkeletonIds.AddUnique(Mesh->GetSkeletonID(SkeletonIndex));
				}

				// Append BoneMap to the array of BoneMaps
				const TArray<uint16>& BoneMap = Mesh->GetBoneMap();
				Component.FirstBoneMap = OperationData->InstanceUpdateData.BoneMaps.Num();
				Component.BoneMapCount = BoneMap.Num();
				OperationData->InstanceUpdateData.BoneMaps.Append(BoneMap);

				// Add active bone indices and poses
				const int32 MaxBoneIndex = Mesh->GetBonePoseCount();
				Component.ActiveBones.Reserve(MaxBoneIndex);
				for (int32 BonePoseIndex = 0; BonePoseIndex < MaxBoneIndex; ++BonePoseIndex)
				{
					const uint16 BoneId = Mesh->GetBonePoseBoneId(BonePoseIndex);

					Component.ActiveBones.Add(BoneId);

					if (SkeletonData.BoneIds.Find(BoneId) == INDEX_NONE)
					{
						SkeletonData.BoneIds.Add(BoneId);

						FTransform3f Transform;
						Mesh->GetBoneTransform(BonePoseIndex, Transform);
						SkeletonData.BoneMatricesWithScale.Emplace(Transform.Inverse().ToMatrixWithScale());
					}
				}
			}
		
			return;
		}


		check(OperationData);
		const int32 LODCount = OperationData->InstanceUpdateData.LODs.Num();
		const FInstanceUpdateData::FLOD& MinLOD = OperationData->InstanceUpdateData.LODs[OperationData->CurrentMinLOD];
		const int32 ComponentCount = MinLOD.ComponentCount;

		// Add SkeletonData for each component
		OperationData->InstanceUpdateData.Skeletons.AddDefaulted(ComponentCount);

		for (int32 ComponentIndex = 0; ComponentIndex < ComponentCount; ++ComponentIndex)
		{
			FInstanceUpdateData::FComponent& MinLODComponent = OperationData->InstanceUpdateData.Components[MinLOD.FirstComponent + ComponentIndex];

			// Set the ComponentIndex
			OperationData->InstanceUpdateData.Skeletons[ComponentIndex].ComponentIndex = MinLODComponent.Id;

			// Fill the data used to generate the RefSkeletalMesh
			TArray<uint16>& SkeletonIds = OperationData->InstanceUpdateData.Skeletons[ComponentIndex].SkeletonIds;
			TArray<uint16>& BoneIds = OperationData->InstanceUpdateData.Skeletons[ComponentIndex].BoneIds;
			TArray<FMatrix44f>& BoneMatricesWithScale = OperationData->InstanceUpdateData.Skeletons[ComponentIndex].BoneMatricesWithScale;

			// Use first valid LOD bone count as a potential total number of bones, used for pre-allocating data arrays
			if (MinLODComponent.Mesh && MinLODComponent.Mesh->GetSkeleton())
			{
				const int32 TotalPossibleBones = MinLODComponent.Mesh->GetSkeleton()->GetBoneCount();

				// Out Data
				BoneIds.Reserve(TotalPossibleBones);
				BoneMatricesWithScale.Reserve(TotalPossibleBones);
			}

			for (int32 LODIndex = OperationData->CurrentMinLOD; LODIndex <= OperationData->CurrentMaxLOD && LODIndex < LODCount; ++LODIndex)
			{
				MUTABLE_CPUPROFILER_SCOPE(PrepareSkeletonData_LODs);

				const FInstanceUpdateData::FLOD& CurrentLOD = OperationData->InstanceUpdateData.LODs[LODIndex];
				FInstanceUpdateData::FComponent& CurrentLODComponent = OperationData->InstanceUpdateData.Components[CurrentLOD.FirstComponent + ComponentIndex];
				mu::MeshPtrConst Mesh = CurrentLODComponent.Mesh;

				if (!Mesh)
				{
					continue;
				}

				// Add SkeletonIds 
				const int32 SkeletonIDsCount = Mesh->GetSkeletonIDsCount();
				for (int32 SkeletonIndex = 0; SkeletonIndex < SkeletonIDsCount; ++SkeletonIndex)
				{
					SkeletonIds.AddUnique(Mesh->GetSkeletonID(SkeletonIndex));
				}

				// Append BoneMap to the array of BoneMaps
				const TArray<uint16>& BoneMap = Mesh->GetBoneMap();
				CurrentLODComponent.FirstBoneMap = OperationData->InstanceUpdateData.BoneMaps.Num();
				CurrentLODComponent.BoneMapCount = BoneMap.Num();
				OperationData->InstanceUpdateData.BoneMaps.Append(BoneMap);

				// Add active bone indices and poses
				const int32 MaxBoneIndex = Mesh->GetBonePoseCount();
				CurrentLODComponent.ActiveBones.Reserve(MaxBoneIndex);
				for (int32 BonePoseIndex = 0; BonePoseIndex < MaxBoneIndex; ++BonePoseIndex)
				{
					const uint16 BoneId = Mesh->GetBonePoseBoneId(BonePoseIndex);

					CurrentLODComponent.ActiveBones.Add(BoneId);

					if(BoneIds.Find(BoneId) == INDEX_NONE)
					{
						BoneIds.Add(BoneId);

						FTransform3f Transform;
						Mesh->GetBoneTransform(BonePoseIndex, Transform);
						BoneMatricesWithScale.Emplace(Transform.Inverse().ToMatrixWithScale());
					}
				}
			}
		}
	}


	// This runs in a worker thread.
	void Task_Mutable_Update_GetMesh(const TSharedPtr<FMutableOperationData>& OperationData, const TSharedPtr<mu::Model>& Model)
	{
		MUTABLE_CPUPROFILER_SCOPE(Task_Mutable_Update_GetMesh)

		check(OperationData);

#if WITH_EDITOR
		const uint32 StartCycles = FPlatformTime::Cycles();
#endif

		Subtask_Mutable_BeginUpdate_GetMesh(OperationData, Model);

		// TODO: Not strictly mutable: move to another worker thread task to free mutable access?
		Subtask_Mutable_PrepareSkeletonData(OperationData);

		if (OperationData->bBuildParameterRelevancy)
		{
			Subtask_Mutable_UpdateParameterRelevancy(OperationData);
		}
		else
		{
			OperationData->RelevantParametersInProgress.Reset();
		}

#if WITH_EDITOR
		const uint32 EndCycles = FPlatformTime::Cycles();
		OperationData->MutableRuntimeCycles = EndCycles - StartCycles;
#endif
	}

	
	// This runs in a worker thread.
	void Task_Mutable_Update_GetImages(const TSharedPtr<FMutableOperationData>& OperationData)
	{
		MUTABLE_CPUPROFILER_SCOPE(Task_Mutable_GetImages)

		check(OperationData);

#if WITH_EDITOR
		uint32 StartCycles = FPlatformTime::Cycles();
#endif

		Subtask_Mutable_GetImages(OperationData);

		// TODO: Not strictly mutable: move to another worker thread task to free mutable access?
		Subtask_Mutable_PrepareTextures(OperationData);

#if WITH_EDITOR
		const uint32 EndCycles = FPlatformTime::Cycles();
		OperationData->MutableRuntimeCycles += EndCycles - StartCycles;
#endif
	}


	// This runs in a worker thread.
	void Task_Mutable_ReleaseInstance(const TSharedPtr<FMutableOperationData>& OperationData, mu::Ptr<mu::System> MutableSystem)
	{
		MUTABLE_CPUPROFILER_SCOPE(Task_Mutable_ReleaseInstance)

		check(OperationData);
		check(MutableSystem);

		if (OperationData->InstanceID > 0)
		{
			MUTABLE_CPUPROFILER_SCOPE(EndUpdate);
			MutableSystem->EndUpdate(OperationData->InstanceID);
			OperationData->InstanceUpdateData.Clear();

			if (!OperationData->bLiveUpdateMode)
			{
				MutableSystem->ReleaseInstance(OperationData->InstanceID);
				OperationData->InstanceID = 0;
			}
		}

		MutableSystem->SetImagePixelConversionOverride(nullptr);

		if (CVarClearWorkingMemoryOnUpdateEnd.GetValueOnAnyThread())
		{
			MutableSystem->ClearWorkingMemory();
		}

		UCustomizableObjectSystem::GetInstance()->GetPrivate()->MutableTaskGraph.AllowLaunchingMutableTaskLowPriority(true, true);
	}


	// This runs in a worker thread.
	void Task_Mutable_ReleaseInstanceID(const TSharedPtr<FMutableOperationData>& OperationData, const mu::Ptr<mu::System>& MutableSystem)
	{
		MUTABLE_CPUPROFILER_SCOPE(Task_Mutable_ReleaseInstanceID)

		check(OperationData);

		if (OperationData->InstanceID > 0)
		{
			MutableSystem->ReleaseInstance(OperationData->InstanceID);
			OperationData->InstanceID = 0;
		}

		if (CVarClearWorkingMemoryOnUpdateEnd.GetValueOnAnyThread())
		{
			MutableSystem->ClearWorkingMemory();
		}
	}


	void Task_Game_ReleasePlatformData(const TSharedPtr<FMutableReleasePlatformOperationData>& OperationData)
	{
		MUTABLE_CPUPROFILER_SCOPE(Task_Game_ReleasePlatformData)

		check(OperationData);
		
		TMap<uint32, FTexturePlatformData*>& ImageToPlatformDataMap = OperationData->ImageToPlatformDataMap;
		for (const TPair<uint32, FTexturePlatformData*>& Pair : ImageToPlatformDataMap)
		{
			delete Pair.Value; // If this is not null then it must mean it hasn't been used, otherwise they would have taken ownership and nulled it
		}
		ImageToPlatformDataMap.Reset();
	}

	
	void Task_Game_Callbacks(const TSharedPtr<FMutableOperationData>& OperationData)
	{
		MUTABLE_CPUPROFILER_SCOPE(Task_Game_Callbacks)

		check(IsInGameThread());
		check(OperationData);

		UCustomizableObjectSystem* System = UCustomizableObjectSystem::GetInstance();
		if (!System || !System->IsValidLowLevel() || System->HasAnyFlags(RF_BeginDestroyed))
		{
			FinishUpdateGlobal(OperationData->Instance.Get(), EUpdateResult::Error, &OperationData->UpdateCallback);
			return;
		}

		UCustomizableObjectInstance* CustomizableObjectInstance = OperationData->Instance.Get();

		// TODO: Review checks.
		if (!CustomizableObjectInstance || !CustomizableObjectInstance->IsValidLowLevel() )
		{
			System->ClearCurrentMutableOperation();
			FinishUpdateGlobal(CustomizableObjectInstance, EUpdateResult::Error, &OperationData->UpdateCallback);
			return;
		}

		FCustomizableObjectSystemPrivate * CustomizableObjectSystemPrivateData = System->GetPrivateChecked();

		// Actual work
		// TODO MTBL-391: Review This hotfix
		UpdateSkeletalMesh(*CustomizableObjectInstance, CustomizableObjectSystemPrivateData->CurrentMutableOperation->InstanceDescriptorRuntimeHash, OperationData->UpdateResult, &OperationData->UpdateCallback);

		// All work is done, release unused textures.
		if (CustomizableObjectSystemPrivateData->bReleaseTexturesImmediately)
		{
			FMutableResourceCache& Cache = CustomizableObjectSystemPrivateData->GetObjectCache(CustomizableObjectInstance->GetCustomizableObject());

			UCustomizableInstancePrivateData* CustomizableObjectInstancePrivateData = CustomizableObjectInstance->GetPrivate();
			for (FGeneratedTexture& GeneratedTexture : CustomizableObjectInstancePrivateData->TexturesToRelease)
			{
				UCustomizableInstancePrivateData::ReleaseMutableTexture(GeneratedTexture.Key, Cast<UTexture2D>(GeneratedTexture.Texture), Cache);
			}

			CustomizableObjectInstancePrivateData->TexturesToRelease.Empty();
		}

		LogBenchmarkUtil::UpdateBuildTimeStats(CustomizableObjectSystemPrivateData->MutableStats, 
												CustomizableObjectSystemPrivateData->CurrentMutableOperation->StartUpdateTime);

		// End Update
		System->ClearCurrentMutableOperation();
	}


	void Task_Game_ConvertResources(TSharedPtr<FMutableOperationData> OperationData)
	{
		MUTABLE_CPUPROFILER_SCOPE(Task_Game_ConvertResources)

		check(IsInGameThread());
		check(OperationData);

		UCustomizableObjectSystem* System = UCustomizableObjectSystem::GetInstance();
		if (!System || !System->IsValidLowLevel() || System->HasAnyFlags(RF_BeginDestroyed))
		{
			FinishUpdateGlobal(OperationData->Instance.Get(), EUpdateResult::Error, &OperationData->UpdateCallback);
			return;
		}

		UCustomizableObjectInstance* CustomizableObjectInstance = OperationData->Instance.Get();

		// Actual work
		// TODO: Review checks.
		const bool bInstanceInvalid = !CustomizableObjectInstance || !CustomizableObjectInstance->IsValidLowLevel();
		if (!bInstanceInvalid)
		{
			UCustomizableInstancePrivateData* CustomizableInstancePrivateData = CustomizableObjectInstance->GetPrivate();

			// Process the pending texture coverage queries
			{
				MUTABLE_CPUPROFILER_SCOPE(GameTextureQueries);
				for (const FPendingTextureCoverageQuery& Query : OperationData->PendingTextureCoverageQueries)
				{
					UMaterialInterface* Material = nullptr;
					const uint32* InstanceIndex = CustomizableInstancePrivateData->ObjectToInstanceIndexMap.Find(Query.MaterialIndex);
					if (InstanceIndex && CustomizableInstancePrivateData->ReferencedMaterials.IsValidIndex(*InstanceIndex))
					{
						Material = CustomizableInstancePrivateData->ReferencedMaterials[*InstanceIndex];
					}

					UCustomizableInstancePrivateData::ProcessTextureCoverageQueries(OperationData, CustomizableObjectInstance->GetCustomizableObject(), Query.KeyName, Query.PlatformData, Material);
				}
				OperationData->PendingTextureCoverageQueries.Empty();
			}

			// Process texture coverage queries because it's safe to do now that the Mutable thread is stopped
			{
				if (OperationData->TextureCoverageQueries_MutableThreadResults.Num() > 0)
				{
					for (auto& Result : OperationData->TextureCoverageQueries_MutableThreadResults)
					{
						FTextureCoverageQueryData* FinalResultData = CustomizableInstancePrivateData->TextureCoverageQueries.Find(Result.Key);
						*FinalResultData = Result.Value;
					}

					OperationData->TextureCoverageQueries_MutableThreadResults.Empty();
				}

#if WITH_EDITOR
				CustomizableObjectInstance->LastUpdateMutableRuntimeCycles = OperationData->MutableRuntimeCycles;
#endif

				// Convert Step
				//-------------------------------------------------------------

				// \TODO: Bring that code here instead of keeping it in the UCustomizableObjectInstance
				if (CustomizableInstancePrivateData->UpdateSkeletalMesh_PostBeginUpdate0(CustomizableObjectInstance, OperationData))
				{
					// This used to be CustomizableObjectInstance::UpdateSkeletalMesh_PostBeginUpdate1
					{
						MUTABLE_CPUPROFILER_SCOPE(UpdateSkeletalMesh_PostBeginUpdate1);

						// \TODO: Bring here
						CustomizableInstancePrivateData->BuildMaterials(OperationData, CustomizableObjectInstance);
					}

					// This used to be CustomizableObjectInstance::UpdateSkeletalMesh_PostBeginUpdate2
					{
						MUTABLE_CPUPROFILER_SCOPE(UpdateSkeletalMesh_PostBeginUpdate2);

						for (int32 Component = 0; Component < CustomizableObjectInstance->SkeletalMeshes.Num(); ++Component)
						{
							if (CustomizableObjectInstance->SkeletalMeshes[Component] && CustomizableObjectInstance->SkeletalMeshes[Component]->GetLODInfoArray().Num())
							{
								MUTABLE_CPUPROFILER_SCOPE(UpdateSkeletalMesh_PostEditChangeProperty);

								CustomizableInstancePrivateData->PostEditChangePropertyWithoutEditor(CustomizableObjectInstance->SkeletalMeshes[Component]);
							}
						}
					}
				}
			} // END - Process texture coverage queries
		} // if (!bInstanceValid)

		FCustomizableObjectSystemPrivate* CustomizableObjectSystemPrivateData = System->GetPrivateChecked();

		for (int32 ComponentIndex = 0; ComponentIndex < CustomizableObjectInstance->GetPrivate()->ComponentsData.Num(); ++ComponentIndex)
		{
			ensure(CustomizableObjectInstance->SkeletalMeshes.IsValidIndex(ComponentIndex));

			FCustomizableInstanceComponentData& ComponentData = CustomizableObjectInstance->GetPrivate()->ComponentsData[ComponentIndex];
			
			for (TObjectPtr<UAssetUserData> AssetUserData : ComponentData.AssetUserDataArray)
			{
				CustomizableObjectInstance->SkeletalMeshes[ComponentIndex]->AddAssetUserData(AssetUserData);
			}
		}

		// Next Task: Release Mutable. We need this regardless if we cancel or not
		//-------------------------------------------------------------		
		{
			const mu::Ptr<mu::System> MutableSystem = CustomizableObjectSystemPrivateData->MutableSystem;
			CustomizableObjectSystemPrivateData->MutableTaskGraph.AddMutableThreadTask(
				TEXT("Task_Mutable_ReleaseInstance"),
				[OperationData, MutableSystem]() {Task_Mutable_ReleaseInstance(OperationData, MutableSystem); });
		}


		// Next Task: Release Platform Data
		//-------------------------------------------------------------
		if (!bInstanceInvalid)
		{
			TSharedPtr<FMutableReleasePlatformOperationData> ReleaseOperationData = MakeShared<FMutableReleasePlatformOperationData>();
			check(ReleaseOperationData);
			ReleaseOperationData->ImageToPlatformDataMap = MoveTemp(OperationData->ImageToPlatformDataMap);
			CustomizableObjectSystemPrivateData->MutableTaskGraph.AddAnyThreadTask(
				TEXT("Mutable_ReleasePlatformData"),
				[ReleaseOperationData]()
				{
					Task_Game_ReleasePlatformData(ReleaseOperationData);
				}
			);


			// Unlock step
			//-------------------------------------------------------------
			if (CustomizableObjectInstance->GetCustomizableObject())
			{
				// Unlock the resource cache for the object used by this instance to avoid
				// the destruction of resources that we may want to reuse.
				System->ClearResourceCacheProtected();
			}

			// Next Task: Callbacks
			//-------------------------------------------------------------
			CustomizableObjectSystemPrivateData->AddGameThreadTask(
				{
				FMutableTaskDelegate::CreateLambda(
					[OperationData]()
					{
						Task_Game_Callbacks(OperationData);
					}),
					{}
				});
		}
		else
		{
			FinishUpdateGlobal(CustomizableObjectInstance, EUpdateResult::Error, &OperationData->UpdateCallback);
		}
	}


	/** Lock Cached Resources. */
	void Task_Game_LockCache(TSharedPtr<FMutableOperationData> OperationData)
	{
		MUTABLE_CPUPROFILER_SCOPE(Task_Game_LockCache)

		check(IsInGameThread());
		check(OperationData);

		UCustomizableObjectSystem* System = UCustomizableObjectSystem::GetInstance();
		if (!System)
		{
			return;
		}

		UCustomizableObjectInstance* ObjectInstance = OperationData->Instance.Get();
		if (!ObjectInstance)
		{
			System->ClearCurrentMutableOperation();
			FinishUpdateGlobal(ObjectInstance, EUpdateResult::Error, &OperationData->UpdateCallback);
			return;
		}

		UCustomizableInstancePrivateData* ObjectInstancePrivateData = ObjectInstance->GetPrivate();
		check(ObjectInstancePrivateData != nullptr);

		if (OperationData->bLiveUpdateMode)
		{
			check(OperationData->InstanceID != 0);

			if (ObjectInstancePrivateData->LiveUpdateModeInstanceID == 0)
			{
				// From now this instance will reuse this InstanceID until it gets out of LiveUpdateMode
				ObjectInstancePrivateData->LiveUpdateModeInstanceID = OperationData->InstanceID;
			}
		}

		const UCustomizableObject* CustomizableObject = ObjectInstance->GetCustomizableObject(); 
		if (!CustomizableObject)
		{
			System->ClearCurrentMutableOperation();
			FinishUpdateGlobal(ObjectInstance, EUpdateResult::Error, &OperationData->UpdateCallback);
			return;
		}
		
		if (OperationData->bBuildParameterRelevancy)
		{
			// Relevancy
			ObjectInstancePrivateData->RelevantParameters = OperationData->RelevantParametersInProgress;
		}

		
		// Selectively lock the resource cache for the object used by this instance to avoid the destruction of resources that we may want to reuse.
		// When protecting textures there mustn't be any left from a previous update
		check(System->ProtectedCachedTextures.Num() == 0);

		FCustomizableObjectSystemPrivate* SystemPrivateData = System->GetPrivateChecked();

		// TODO: If this is the first code that runs after the CO program has finished AND if it's
		// guaranteed that the next CO program hasn't started yet, we need to call ClearActiveObject
		// and CancelPendingLoads on SystemPrivateData->ExtensionDataStreamer.
		//
		// ExtensionDataStreamer->AreAnyLoadsPending should return false if the program succeeded.
		//
		// If the program aborted, AreAnyLoadsPending may return true, as the program doesn't cancel
		// its own loads on exit (maybe it should?)

		FMutableResourceCache& Cache = SystemPrivateData->GetObjectCache(CustomizableObject);

		System->ProtectedCachedTextures.Reset(Cache.Images.Num());
		SystemPrivateData->ProtectedObjectCachedImages.Reset(Cache.Images.Num());

		for (const FInstanceUpdateData::FImage& Image : OperationData->InstanceUpdateData.Images)
		{
			FMutableImageCacheKey Key(Image.ImageID, OperationData->MipsToSkip);
			const TWeakObjectPtr<UTexture2D>* TexturePtr = Cache.Images.Find(Key);

			if (TexturePtr && TexturePtr->Get() && SystemPrivateData->TextureHasReferences(Key))
			{
				System->ProtectedCachedTextures.Add(TexturePtr->Get());
				SystemPrivateData->ProtectedObjectCachedImages.Add(Image.ImageID);
			}
		}

		// Any external texture that may be needed for this update will be requested from Mutable Core's GetImage
		// which will safely access the GlobalExternalImages map, and then just get the cached image or issue a disk read

		// Copy data generated in the mutable thread over to the instance
		ObjectInstancePrivateData->PrepareForUpdate(OperationData);

		// Task: Mutable GetImages
		//-------------------------------------------------------------
#ifdef MUTABLE_USE_NEW_TASKGRAPH
		UE::Tasks::FTask Mutable_GetImagesTask;
#else
		FGraphEventRef Mutable_GetImagesTask;
#endif
		{
			// Task inputs
			Mutable_GetImagesTask = SystemPrivateData->MutableTaskGraph.AddMutableThreadTask(
				TEXT("Task_Mutable_GetImages"),
				[OperationData]()
				{
					impl::Task_Mutable_Update_GetImages(OperationData);
				});
		}


		// Next Task: Load Unreal Assets
		//-------------------------------------------------------------
		FGraphEventRef Game_LoadUnrealAssets = ObjectInstancePrivateData->LoadAdditionalAssetsAsync(OperationData, ObjectInstance, UCustomizableObjectSystem::GetInstance()->GetStreamableManager());
		if (Game_LoadUnrealAssets)
		{
			Game_LoadUnrealAssets->SetDebugName(TEXT("LoadAdditionalAssetsAsync"));
		}

		// Next-next Task: Convert Resources
		//-------------------------------------------------------------
		SystemPrivateData->AddGameThreadTask(
			{
			FMutableTaskDelegate::CreateLambda(
				[OperationData]()
				{
					Task_Game_ConvertResources(OperationData);
				}),
#ifdef MUTABLE_USE_NEW_TASKGRAPH
				{},
#endif
				Game_LoadUnrealAssets,
				Mutable_GetImagesTask
			});
	}


	/** Enqueue the release ID operation in the Mutable queue */
	void Task_Game_ReleaseInstanceID(const mu::Instance::ID IDToRelease)
	{
		MUTABLE_CPUPROFILER_SCOPE(Task_Game_ReleaseInstanceID)

		UCustomizableObjectSystem* System = UCustomizableObjectSystem::GetInstanceChecked();
		FCustomizableObjectSystemPrivate* SystemPrivateData = System->GetPrivateChecked();

		const mu::Ptr<mu::System> MutableSystem = SystemPrivateData->MutableSystem;

		// Task: Release Instance ID
		//-------------------------------------------------------------
		TSharedPtr<FMutableOperationData> CurrentOperationData = MakeShared<FMutableOperationData>();
		check(CurrentOperationData);
		CurrentOperationData->InstanceID = IDToRelease;

		{
			// Task inputs
			SystemPrivateData->MutableTaskGraph.AddMutableThreadTask(
				TEXT("Task_Mutable_ReleaseInstanceID"),
				[CurrentOperationData, MutableSystem]()
				{
					impl::Task_Mutable_ReleaseInstanceID(CurrentOperationData, MutableSystem);
				});
		}
	}


	/** "Start Update" */
	void Task_Game_StartUpdate(TSharedPtr<FMutableOperation> Operation)
	{
		MUTABLE_CPUPROFILER_SCOPE(Task_Game_StartUpdate)

		check(Operation);
		
		UCustomizableObjectSystem::GetInstance()->GetPrivate()->MutableTaskGraph.AllowLaunchingMutableTaskLowPriority(false, false);
		
		UCustomizableObjectSystem* System = UCustomizableObjectSystem::GetInstance();
		check(System != nullptr);

		if (!Operation->CustomizableObjectInstance.IsValid() || !Operation->CustomizableObjectInstance->IsValidLowLevel()) // Only start if it hasn't been already destroyed (i.e. GC after finish PIE)
		{
			System->ClearCurrentMutableOperation();
			FinishUpdateGlobal(nullptr, EUpdateResult::Error, &Operation->UpdateCallback);
			return;
		}

		UCustomizableObjectInstance* CandidateInstance = Operation->CustomizableObjectInstance.Get();
		
		UCustomizableInstancePrivateData* CandidateInstancePrivateData = CandidateInstance->GetPrivate();
		if (!CandidateInstancePrivateData)
		{
			System->ClearCurrentMutableOperation();
			FinishUpdateGlobal(nullptr, EUpdateResult::Error, &Operation->UpdateCallback);
			return;
		}
		
		CandidateInstancePrivateData->InstanceUpdateFlags(*CandidateInstance);
		
		if (CandidateInstancePrivateData->HasCOInstanceFlags(PendingLODsUpdate))
		{
			CandidateInstancePrivateData->ClearCOInstanceFlags(PendingLODsUpdate);
			// TODO: Is anything needed for this now?
			//Operation->CustomizableObjectInstance->ReleaseMutableInstanceId(); // To make mutable regenerate the LODs even if the instance parameters have not changed
		}

		// Skip update, the requested update is equal to the running update.
		if (Operation->InstanceDescriptorRuntimeHash.IsSubset(CandidateInstance->GetDescriptorRuntimeHash()))
		{
			CandidateInstance->SkeletalMeshStatus = ESkeletalMeshState::Correct;

			CandidateInstancePrivateData->ClearCOInstanceFlags(Updating);
			System->ClearCurrentMutableOperation();
			UpdateSkeletalMesh(*CandidateInstance, CandidateInstance->GetDescriptorRuntimeHash(), EUpdateResult::ErrorOptimized, &Operation->UpdateCallback);
			return;
		}

		bool bCancel = false;

		// If the object is locked (for instance, compiling) we skip any instance update.
		TObjectPtr<UCustomizableObject> CustomizableObject = CandidateInstance->GetCustomizableObject();
		if (!CustomizableObject)
		{
			bCancel = true;
		}
		else
		{
			if (CustomizableObject->GetPrivate()->bLocked)
			{
				bCancel = true;
			}
		}

		// Only update resources if the instance is in range (it could have got far from the player since the task was queued)
		check(System->CurrentInstanceLODManagement != nullptr);
		if (System->CurrentInstanceLODManagement->IsOnlyUpdateCloseCustomizableObjectsEnabled()
			&& CandidateInstancePrivateData
			&& CandidateInstancePrivateData->LastMinSquareDistFromComponentToPlayer > FMath::Square(System->CurrentInstanceLODManagement->GetOnlyUpdateCloseCustomizableObjectsDist())
			&& CandidateInstancePrivateData->LastMinSquareDistFromComponentToPlayer != FLT_MAX // This means it is the first frame so it has to be updated
		   )
		{
			bCancel = true;
		}

		mu::Ptr<const mu::Parameters> Parameters = Operation->GetParameters();
		if (!Parameters)
		{
			bCancel = true;
		}

		if (bCancel)
		{
			CandidateInstancePrivateData->ClearCOInstanceFlags(Updating);

			System->ClearCurrentMutableOperation();

			FinishUpdateGlobal(CandidateInstance, EUpdateResult::Error, &Operation->UpdateCallback);
			return;
		}

		if (LogBenchmarkUtil::IsLoggingActive())
		{
			Operation->StartUpdateTime = FPlatformTime::Seconds();
		}

		FCustomizableObjectSystemPrivate* SystemPrivateData = System->GetPrivateChecked();

		SystemPrivateData->CurrentInstanceBeingUpdated = CandidateInstance;

		// Prepare streaming for the current customizable object
		check(SystemPrivateData->Streamer != nullptr);
		SystemPrivateData->Streamer->PrepareStreamingForObject(CustomizableObject);

		check(SystemPrivateData->ExtensionDataStreamer != nullptr);
		SystemPrivateData->ExtensionDataStreamer->SetActiveObject(CustomizableObject);

		CandidateInstance->CommitMinMaxLOD();

		FString StateName = CandidateInstance->GetCustomizableObject()->GetStateName(CandidateInstance->GetState());
		const FParameterUIData* StateData = CandidateInstance->GetCustomizableObject()->StateUIDataMap.Find(StateName);

		bool bLiveUpdateMode = false;

		if (SystemPrivateData->EnableMutableLiveUpdate)
		{
			bLiveUpdateMode = StateData ? StateData->bLiveUpdateMode : false;
		}

		bool bNeverStream = false;
		int32 MipsToSkip = 0;

		SystemPrivateData->GetMipStreamingConfig(*CandidateInstance, bNeverStream, MipsToSkip);
		
		if (bLiveUpdateMode && (!bNeverStream || MipsToSkip > 0))
		{
			UE_LOG(LogMutable, Warning, TEXT("Instance LiveUpdateMode does not yet support progressive streaming of Mutable textures. Disabling LiveUpdateMode for this update."));
			bLiveUpdateMode = false;
		}

		bool bReuseInstanceTextures = false;

		if (SystemPrivateData->EnableReuseInstanceTextures)
		{
			bReuseInstanceTextures = StateData ? StateData->bReuseInstanceTextures : false;
			bReuseInstanceTextures |= CandidateInstancePrivateData->HasCOInstanceFlags(ReuseTextures);
			
			if (bReuseInstanceTextures && !bNeverStream)
			{
				UE_LOG(LogMutable, Warning, TEXT("Instance texture reuse requires that the current Mutable state is in non-streaming mode. Change it in the Mutable graph base node in the state definition."));
				bReuseInstanceTextures = false;
			}
		}

		if (!bLiveUpdateMode && CandidateInstancePrivateData->LiveUpdateModeInstanceID != 0)
		{
			// The instance was in live update mode last update, but now it's not. So the Id and resources have to be released.
			// Enqueue a new mutable task to release them
			Task_Game_ReleaseInstanceID(CandidateInstancePrivateData->LiveUpdateModeInstanceID);
			CandidateInstancePrivateData->LiveUpdateModeInstanceID = 0;
		}

		
		// Task: Mutable Update and GetMesh
		//-------------------------------------------------------------
		TSharedPtr<FMutableOperationData> CurrentOperationData = MakeShared<FMutableOperationData>();
		check(CurrentOperationData);
		CurrentOperationData->Instance = Operation->CustomizableObjectInstance;
		CurrentOperationData->TextureCoverageQueries_MutableThreadParams = CandidateInstancePrivateData->TextureCoverageQueries;
		CurrentOperationData->TextureCoverageQueries_MutableThreadResults.Empty();
		CurrentOperationData->bCanReuseGeneratedData = SystemPrivateData->bEnableMutableReusePreviousUpdateData;
		CurrentOperationData->LastUpdateData = SystemPrivateData->bEnableMutableReusePreviousUpdateData ? CandidateInstancePrivateData->LastUpdateData : FInstanceGeneratedData();
		CurrentOperationData->CurrentMinLOD = Operation->InstanceDescriptorRuntimeHash.GetMinLOD();
		CurrentOperationData->CurrentMaxLOD = Operation->InstanceDescriptorRuntimeHash.GetMaxLOD();
		CurrentOperationData->bNeverStream = bNeverStream;
		CurrentOperationData->bLiveUpdateMode = bLiveUpdateMode;
		CurrentOperationData->bReuseInstanceTextures = bReuseInstanceTextures;
		CurrentOperationData->InstanceID = bLiveUpdateMode ? CandidateInstancePrivateData->LiveUpdateModeInstanceID : 0;
		CurrentOperationData->MipsToSkip = MipsToSkip;
		CurrentOperationData->MutableParameters = Parameters;
		CurrentOperationData->State = Operation->GetState();
		CurrentOperationData->UpdateResult = EUpdateResult::Success;
		CurrentOperationData->UpdateCallback = Operation->UpdateCallback;
		CurrentOperationData->bBuildParameterRelevancy = Operation->IsBuildParameterRelevancy();
#if WITH_EDITOR
		CurrentOperationData->PixelFormatOverride = SystemPrivateData->ImageFormatOverrideFunc;
#endif

		if (!CandidateInstancePrivateData->HasCOInstanceFlags(ForceGenerateMipTail))
		{
			CustomizableObject->GetLowPriorityTextureNames(CurrentOperationData->LowPriorityTextures);
		}

		bool bIsInEditorViewport = false;

#if WITH_EDITOR
		for (TObjectIterator<UCustomizableSkeletalComponent> CustomizableSkeletalComponent; CustomizableSkeletalComponent && !bIsInEditorViewport; ++CustomizableSkeletalComponent)
		{
			if (CustomizableSkeletalComponent && CustomizableSkeletalComponent->IsNetMode(NM_DedicatedServer))
			{
				continue;
			}

			if (CustomizableSkeletalComponent &&
				CustomizableSkeletalComponent->CustomizableObjectInstance == CandidateInstance)
			{
				EWorldType::Type WorldType = EWorldType::Type::None;

				if (CustomizableSkeletalComponent->GetWorld())
				{
					WorldType = CustomizableSkeletalComponent->GetWorld()->WorldType;
				}

				switch (WorldType)
				{
					// Editor preview instances
					case EWorldType::EditorPreview:
					case EWorldType::None:
						bIsInEditorViewport = true;
					default: ;
				}
			}
		}
#endif // WITH_EDITOR
		
		if (System->IsOnlyGenerateRequestedLODsEnabled() && System->CurrentInstanceLODManagement->IsOnlyGenerateRequestedLODLevelsEnabled() && 
			!bIsInEditorViewport)
		{
			CurrentOperationData->RequestedLODs = Operation->InstanceDescriptorRuntimeHash.GetRequestedLODs();
		}

#ifdef MUTABLE_USE_NEW_TASKGRAPH
		UE::Tasks::FTask Mutable_GetMeshTask;
#else
		FGraphEventRef Mutable_GetMeshTask;
#endif
		{
			// Task inputs
			TSharedPtr<mu::Model> Model = CustomizableObject->GetPrivate()->GetModel();

			Mutable_GetMeshTask = SystemPrivateData->MutableTaskGraph.AddMutableThreadTask(
				TEXT("Task_Mutable_Update_GetMesh"),
				[CurrentOperationData, Model]()
				{
					impl::Task_Mutable_Update_GetMesh(CurrentOperationData, Model);
				});
		}


		// Task: Lock cache
		//-------------------------------------------------------------
		{
			// Task inputs
			SystemPrivateData->AddGameThreadTask(
				{
				FMutableTaskDelegate::CreateLambda(
					[CurrentOperationData]()
					{
						impl::Task_Game_LockCache(CurrentOperationData);
					}),
				Mutable_GetMeshTask
				});
		}
	}
} // namespace impl


void UCustomizableObjectSystem::AdvanceCurrentOperation() 
{
	MUTABLE_CPUPROFILER_SCOPE(AdvanceCurrentOperation);

	check(Private != nullptr);

	// See if we have a game-thread task to process
	FMutableTask* PendingTask = Private->PendingTasks.Peek();
	if (PendingTask)
	{
		if (PendingTask->AreDependenciesComplete())
		{
			PendingTask->ClearDependencies();
			PendingTask->Function.Execute();
			Private->PendingTasks.Pop();
		}

		// Don't do anything else until the pending work is completed.
		return;
	}

	// It is safe to do this now.
	Private->UpdateMemoryLimit();

	// If we don't have an ongoing operation, don't do anything.
	if (!Private->CurrentMutableOperation.IsValid())
	{
		return;
	}

	// If we reach here it means:
	// - we have an ongoing operations
	// - we have no pending work for the ongoing operation
	// - so we are starting it.
	{
		MUTABLE_CPUPROFILER_SCOPE(OperationUpdate);

		// Start the first task of the update process. See namespace impl comments above.
		impl::Task_Game_StartUpdate(Private->CurrentMutableOperation);
	}
}


bool UCustomizableObjectSystem::Tick(float DeltaTime)
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableObjectSystem::Tick)
	
	// Building instances is not enabled in servers. If at some point relevant collision or animation data is necessary for server logic this will need to be changed.
#if UE_SERVER
	return true;
#endif

	if (!Private.IsValid())
	{
		return true;
	}

	if (GWorld)
	{
		const EWorldType::Type WorldType = GWorld->WorldType;

		if (WorldType != EWorldType::PIE && WorldType != EWorldType::Game && WorldType != EWorldType::Editor && WorldType != EWorldType::GamePreview)
		{
			return true;
		}
	}

	// \TODO: Review: We should never compile an object from this tick, so this could be removed
#if WITH_EDITOR
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	if (AssetRegistryModule.Get().IsLoadingAssets())
	{
		return true; // Assets are still being loaded, so subobjects won't be found, compiled objects incomplete and thus updates wrong
	}

	// Do not tick if the CookCommandlet is running.
	if (IsRunningCookCommandlet())
	{
		return true;
	}
#endif
	
	TickPendingReleaseMaterials();

	// Get a new operation if we aren't working on one
	if (!Private->CurrentMutableOperation)
	{
		// Reset the instance relevancy
		// The RequestedUpdates only refer to LOD changes. User Customization and discards are handled separately
		FMutableInstanceUpdateMap RequestedLODUpdates;
		
		CurrentInstanceLODManagement->UpdateInstanceDistsAndLODs(RequestedLODUpdates);

		for (TObjectIterator<UCustomizableObjectInstance> CustomizableObjectInstance; CustomizableObjectInstance; ++CustomizableObjectInstance)
		{
			if (IsValidChecked(*CustomizableObjectInstance) && CustomizableObjectInstance->GetPrivate())
			{
				UCustomizableInstancePrivateData* ObjectInstancePrivateData = CustomizableObjectInstance->GetPrivate();

				if (ObjectInstancePrivateData->HasCOInstanceFlags(UsedByComponentInPlay))
				{
					ObjectInstancePrivateData->TickUpdateCloseCustomizableObjects(**CustomizableObjectInstance, RequestedLODUpdates);
				}
				else if (ObjectInstancePrivateData->HasCOInstanceFlags(UsedByComponent))
				{
					ensure(!RequestedLODUpdates.Contains(*CustomizableObjectInstance));
					ObjectInstancePrivateData->UpdateInstanceIfNotGenerated(**CustomizableObjectInstance, RequestedLODUpdates);
				}
				else
				{
					ensure(!RequestedLODUpdates.Contains(*CustomizableObjectInstance));
				}

				ObjectInstancePrivateData->ClearCOInstanceFlags((ECOInstanceFlags)(UsedByComponent | UsedByComponentInPlay | PendingLODsUpdate)); // TODO MTBL-391: Makes no sense to clear it here, what if an update is requested before we set it back to true
			}
			else
			{
				ensure(!RequestedLODUpdates.Contains(*CustomizableObjectInstance));
			}
		}

		{
			// Look for the highest priority update between the pending updates and the LOD Requested Updates
			EQueuePriorityType MaxPriorityFound = EQueuePriorityType::Low;
			double MaxSquareDistanceFound = TNumericLimits<double>::Max();
			double MinTimeFound = TNumericLimits<double>::Max();
			const FMutablePendingInstanceUpdate* PendingInstanceUpdateFound = nullptr;
			FMutableUpdateCandidate* LODUpdateCandidateFound = nullptr;

			// Look for the highest priority Pending Update
			for (auto Iterator = Private->MutablePendingInstanceWork.GetUpdateIterator(); Iterator; ++Iterator)
			{
				FMutablePendingInstanceUpdate& PendingUpdate = *Iterator;

				if (PendingUpdate.CustomizableObjectInstance.IsValid())
				{
					const EQueuePriorityType PriorityType = Private->GetUpdatePriority(*PendingUpdate.CustomizableObjectInstance, false);
					
					if (PendingUpdate.PriorityType <= MaxPriorityFound)
					{
						const double MinSquareDistFromComponentToPlayer = PendingUpdate.CustomizableObjectInstance->GetPrivate()->MinSquareDistFromComponentToPlayer;
						
						if (MinSquareDistFromComponentToPlayer < MaxSquareDistanceFound ||
							(MinSquareDistFromComponentToPlayer == MaxSquareDistanceFound && PendingUpdate.SecondsAtUpdate < MinTimeFound))
						{
							MaxPriorityFound = PriorityType;
							MaxSquareDistanceFound = MinSquareDistFromComponentToPlayer;
							MinTimeFound = PendingUpdate.SecondsAtUpdate;
							PendingInstanceUpdateFound = &PendingUpdate;
							LODUpdateCandidateFound = nullptr;
						}
					}
				}
				else
				{
					Iterator.RemoveCurrent();
				}
			}

			// Look for a higher priority LOD update
			for (TPair<const UCustomizableObjectInstance*, FMutableUpdateCandidate>& LODUpdateTuple : RequestedLODUpdates)
			{
				const UCustomizableObjectInstance* Instance = LODUpdateTuple.Key;

				if (Instance)
				{
					FMutableUpdateCandidate& LODUpdateCandidate = LODUpdateTuple.Value;
					ensure(LODUpdateCandidate.HasBeenIssued());

					if (LODUpdateCandidate.Priority <= MaxPriorityFound)
					{
						if (LODUpdateCandidate.CustomizableObjectInstance->GetPrivate()->MinSquareDistFromComponentToPlayer < MaxSquareDistanceFound)
						{
							MaxPriorityFound = LODUpdateCandidate.Priority;
							MaxSquareDistanceFound = LODUpdateCandidate.CustomizableObjectInstance->GetPrivate()->MinSquareDistFromComponentToPlayer;
							PendingInstanceUpdateFound = nullptr;
							LODUpdateCandidateFound = &LODUpdateCandidate;
						}
					}
				}
			}

			Private->MutablePendingInstanceWork.SetLODUpdatesLastTick(RequestedLODUpdates.Num());

			// If the chosen LODUpdate has the same instance as a PendingUpdate, choose the PendingUpdate to apply both the LOD update
			// and customization change
			if (LODUpdateCandidateFound)
			{
				if (const FMutablePendingInstanceUpdate *PendingUpdateWithSameInstance = Private->MutablePendingInstanceWork.GetUpdate(LODUpdateCandidateFound->CustomizableObjectInstance))
				{
					PendingInstanceUpdateFound = PendingUpdateWithSameInstance;
					LODUpdateCandidateFound = nullptr;

					// In the processing of the PendingUpdate just below, it will add the LODUpdate's LOD params
				}
			}

			if (PendingInstanceUpdateFound)
			{
				check(!LODUpdateCandidateFound);

				UCustomizableObjectInstance* PendingInstance = PendingInstanceUpdateFound->CustomizableObjectInstance.Get();
				check(PendingInstance);

				// Maybe there's a LODUpdate that has the same instance, merge both updates as an optimization
				FMutableUpdateCandidate* LODUpdateWithSameInstance = RequestedLODUpdates.Find(PendingInstance);
				
				if (LODUpdateWithSameInstance)
				{
					LODUpdateWithSameInstance->ApplyLODUpdateParamsToInstance();
				}

				Private->CurrentMutableOperation = MakeShared<FMutableOperation>(FMutableOperation::CreateInstanceUpdate(*PendingInstance, PendingInstanceUpdateFound->Callback));

				Private->MutablePendingInstanceWork.RemoveUpdate(PendingInstanceUpdateFound->CustomizableObjectInstance);
			}
			else if (LODUpdateCandidateFound)
			{
				// Commit the LOD changes
				LODUpdateCandidateFound->ApplyLODUpdateParamsToInstance();

				Private->CurrentMutableOperation = MakeShared<FMutableOperation>(FMutableOperation::CreateInstanceUpdate(*LODUpdateCandidateFound->CustomizableObjectInstance, nullptr));
			}
		}

		{
			for (TObjectIterator<UCustomizableObjectInstance> CustomizableObjectInstance; CustomizableObjectInstance; ++CustomizableObjectInstance)
			{
				if (IsValidChecked(*CustomizableObjectInstance) && CustomizableObjectInstance->GetPrivate())
				{
					CustomizableObjectInstance->GetPrivate()->LastMinSquareDistFromComponentToPlayer = CustomizableObjectInstance->GetPrivate()->MinSquareDistFromComponentToPlayer;
					CustomizableObjectInstance->GetPrivate()->MinSquareDistFromComponentToPlayer = FLT_MAX;
				}
			}
		}

		// Update the streaming limit if it has changed. It is safe to do this now.
		Private->UpdateMemoryLimit();

		// Free memory before starting the new update
		DiscardInstances();
		ReleaseInstanceIDs();
	}
	
	// Advance the current operation
	if (Private->CurrentMutableOperation)
	{
		AdvanceCurrentOperation();
	}

	Private->MutableStats.MutablePendingInstanceWorkCount = Private->MutablePendingInstanceWork.Num();
	LogBenchmarkUtil::UpdateStats(Private->MutableStats, ProtectedCachedTextures);

#if WITH_EDITOR
	TickRecompileCustomizableObjects();
#endif

	Private->MutableTaskGraph.Tick();

	return true;
}


TAutoConsoleVariable<int32> CVarMaxNumInstancesToDiscardPerTick(
	TEXT("mutable.MaxNumInstancesToDiscardPerTick"),
	30,
	TEXT("The maximum number of stale instances that will be discarded per tick by Mutable."),
	ECVF_Scalability);


void UCustomizableObjectSystem::DiscardInstances()
{
	// Handle instance discards
	int32 NumInstancesDiscarded = 0;
	const int32 DiscardLimitPerTick = CVarMaxNumInstancesToDiscardPerTick.GetValueOnGameThread();

	for (TSet<FMutablePendingInstanceDiscard, FPendingInstanceDiscardKeyFuncs>::TIterator Iterator = Private->MutablePendingInstanceWork.GetDiscardIterator();
		Iterator && NumInstancesDiscarded < DiscardLimitPerTick;
		++Iterator)
	{
		MUTABLE_CPUPROFILER_SCOPE(OperationDiscard);

		UCustomizableObjectInstance* COI = Iterator->CustomizableObjectInstance.Get();

		if (COI)
		{
			UCustomizableInstancePrivateData* COIPrivateData = COI ? COI->GetPrivate() : nullptr;

			// Only discard resources if the instance is still out range (it could have got closer to the player since the task was queued)
			if (!CurrentInstanceLODManagement->IsOnlyUpdateCloseCustomizableObjectsEnabled() ||
				!COI ||
				((COIPrivateData != nullptr) &&
					(COIPrivateData->LastMinSquareDistFromComponentToPlayer > FMath::Square(CurrentInstanceLODManagement->GetOnlyUpdateCloseCustomizableObjectsDist()))
					)
				)
			{
				if (COI && COI->IsValidLowLevel())
				{
					check(COIPrivateData != nullptr);
					COIPrivateData->DiscardResourcesAndSetReferenceSkeletalMesh(COI);
					COIPrivateData->ClearCOInstanceFlags(Updating);
					COI->SkeletalMeshStatus = ESkeletalMeshState::Correct;
				}
			}
			else
			{
				check(COIPrivateData != nullptr);
				COIPrivateData->ClearCOInstanceFlags(Updating);
			}

			if (COI && !COI->HasAnySkeletalMesh())
			{
				// To solve the problem in the Mutable demo where PIE just after editor start made all instances appear as reference mesh until editor restart
				check(COIPrivateData != nullptr);
				COIPrivateData->ClearCOInstanceFlags(Generated);
			}
		}

		Iterator.RemoveCurrent();
		NumInstancesDiscarded++;
	}
}


TAutoConsoleVariable<int32> CVarMaxNumInstanceIDsToReleasePerTick(
	TEXT("mutable.MaxNumInstanceIDsToReleasePerTick"),
	30,
	TEXT("The maximum number of stale instances IDs that will be released per tick by Mutable."),
	ECVF_Scalability);


void UCustomizableObjectSystem::ReleaseInstanceIDs()
{
	// Handle ID discards
	int32 NumIDsReleased = 0;
	const int32 IDReleaseLimitPerTick = CVarMaxNumInstanceIDsToReleasePerTick.GetValueOnGameThread();

	for (auto Iterator = Private->MutablePendingInstanceWork.GetIDsToReleaseIterator();
		Iterator && NumIDsReleased < IDReleaseLimitPerTick; ++Iterator)
	{
		impl::Task_Game_ReleaseInstanceID(*Iterator);

		Iterator.RemoveCurrent();
		NumIDsReleased++;
	}
}


TArray<FCustomizableObjectExternalTexture> UCustomizableObjectSystem::GetTextureParameterValues()
{
	TArray<FCustomizableObjectExternalTexture> Result;

	for (const TWeakObjectPtr<UCustomizableSystemImageProvider> Provider : GetPrivateChecked()->GetImageProviderChecked()->ImageProviders)
	{
		if (Provider.IsValid())
		{
			Provider->GetTextureParameterValues(Result);
		}
	}

	return Result;
}


void UCustomizableObjectSystem::RegisterImageProvider(UCustomizableSystemImageProvider* Provider)
{
	GetPrivateChecked()->GetImageProviderChecked()->ImageProviders.Add(Provider);
}


void UCustomizableObjectSystem::UnregisterImageProvider(UCustomizableSystemImageProvider* Provider)
{
	GetPrivateChecked()->GetImageProviderChecked()->ImageProviders.Remove(Provider);
}


static bool bRevertCacheTextureParameters = false;
static FAutoConsoleVariableRef CVarRevertCacheTextureParameters(
	TEXT("mutable.RevertCacheTextureParameters"), bRevertCacheTextureParameters,
	TEXT("If true, FMutableOperation will not cache/uncache texture parameters. If false, FMutableOperation will add an additional reference to the TextureParameters being used in the update."));

void CacheTexturesParameters(const TArray<FName>& TextureParameters)
{
	if (bRevertCacheTextureParameters)
	{
		return;
	}

	if (!TextureParameters.IsEmpty() && UCustomizableObjectSystem::IsCreated())
	{
		FUnrealMutableImageProvider* ImageProvider = UCustomizableObjectSystem::GetInstance()->GetPrivateChecked()->GetImageProviderChecked();
		check(ImageProvider);

		for (const FName& TextureParameter : TextureParameters)
		{
			ImageProvider->CacheImage(TextureParameter, false);
		}
	}
}


void UnCacheTexturesParameters(const TArray<FName>& TextureParameters)
{
	if (bRevertCacheTextureParameters)
	{
		return;
	}

	if (!TextureParameters.IsEmpty() && UCustomizableObjectSystem::IsCreated())
	{
		FUnrealMutableImageProvider* ImageProvider = UCustomizableObjectSystem::GetInstance()->GetPrivateChecked()->GetImageProviderChecked();
		check(ImageProvider);

		for (const FName& TextureParameter : TextureParameters)
		{
			ImageProvider->UnCacheImage(TextureParameter, false);
		}
	}
}


FMutableOperation::FMutableOperation(const FMutableOperation& Other)
{
	CustomizableObjectInstance = Other.CustomizableObjectInstance;
	InstanceDescriptorRuntimeHash = Other.InstanceDescriptorRuntimeHash;
	bBuildParameterRelevancy = Other.bBuildParameterRelevancy;
	Parameters = Other.Parameters;
	TextureParameters = Other.TextureParameters;
	UpdateCallback = Other.UpdateCallback;
	State = Other.State;

	CacheTexturesParameters(TextureParameters);
}


FMutableOperation& FMutableOperation::operator=(const FMutableOperation& Other)
{
	CustomizableObjectInstance = Other.CustomizableObjectInstance;
	InstanceDescriptorRuntimeHash = Other.InstanceDescriptorRuntimeHash;
	bBuildParameterRelevancy = Other.bBuildParameterRelevancy;
	Parameters = Other.Parameters;
	TextureParameters = Other.TextureParameters;
	UpdateCallback = Other.UpdateCallback;

	CacheTexturesParameters(TextureParameters);

	return *this;
}


FMutableOperation::~FMutableOperation()
{
	// Uncache Texture Parameters
	UnCacheTexturesParameters(TextureParameters);
}


FMutableOperation FMutableOperation::CreateInstanceUpdate(UCustomizableObjectInstance& InCustomizableObjectInstance, const FInstanceUpdateDelegate* UpdateCallback)
{
	check(InCustomizableObjectInstance.GetPrivate() != nullptr);
	check(InCustomizableObjectInstance.GetCustomizableObject() != nullptr);

	FMutableOperation Op;
	Op.CustomizableObjectInstance = &InCustomizableObjectInstance;
	Op.InstanceDescriptorRuntimeHash = InCustomizableObjectInstance.GetUpdateDescriptorRuntimeHash();
	Op.State = InCustomizableObjectInstance.GetState();
	Op.bBuildParameterRelevancy = InCustomizableObjectInstance.GetBuildParameterRelevancy();
	Op.Parameters = InCustomizableObjectInstance.GetDescriptor().GetParameters();
	Op.TextureParameters = InCustomizableObjectInstance.GetPrivate()->UpdateTextureParameters;

	if (UpdateCallback)
	{
		Op.UpdateCallback = *UpdateCallback;		
	}
	
	InCustomizableObjectInstance.GetCustomizableObject()->ApplyStateForcedValuesToParameters(Op.State, Op.Parameters.get());

	if (!Op.Parameters)
	{
		// Cancel the update because the parameters aren't valid, probably because the object is not compiled
		Op.CustomizableObjectInstance = nullptr;
	}

	CacheTexturesParameters(Op.TextureParameters);

	return Op;
}


int32 UCustomizableObjectSystem::GetNumInstances() const
{
	return GetPrivateChecked()->MutableStats.NumInstances;
}

int32 UCustomizableObjectSystem::GetNumPendingInstances() const
{
	return GetPrivateChecked()->MutableStats.NumPendingInstances;
}

int32 UCustomizableObjectSystem::GetTotalInstances() const
{
	return GetPrivateChecked()->MutableStats.TotalInstances;
}

int32 UCustomizableObjectSystem::GetTextureMemoryUsed() const
{
	return static_cast<int32>(GetPrivateChecked()->MutableStats.TextureMemoryUsed);
}

int32 UCustomizableObjectSystem::GetAverageBuildTime() const
{
	return GetPrivateChecked()->MutableStats.TotalBuiltInstances == 0 ? 0 : Private->MutableStats.TotalBuildMs / Private->MutableStats.TotalBuiltInstances;
}


int32 UCustomizableObjectSystem::GetSkeletalMeshMinLODQualityLevel() const
{
	return GetPrivateChecked()->SkeletalMeshMinLodQualityLevel;
}


bool UCustomizableObjectSystem::IsSupport16BitBoneIndexEnabled() const
{
	return GetPrivateChecked()->bSupport16BitBoneIndex;
}


bool UCustomizableObjectSystem::IsProgressiveMipStreamingEnabled() const
{
	return GetPrivateChecked()->EnableMutableProgressiveMipStreaming != 0;
}


void UCustomizableObjectSystem::SetProgressiveMipStreamingEnabled(bool bIsEnabled)
{
	GetPrivateChecked()->EnableMutableProgressiveMipStreaming = bIsEnabled ? 1 : 0;
}


bool UCustomizableObjectSystem::IsOnlyGenerateRequestedLODsEnabled() const
{
	return GetPrivateChecked()->EnableOnlyGenerateRequestedLODs != 0;
}


void UCustomizableObjectSystem::SetOnlyGenerateRequestedLODsEnabled(bool bIsEnabled)
{
	GetPrivateChecked()->EnableOnlyGenerateRequestedLODs = bIsEnabled ? 1 : 0;
}


#if WITH_EDITOR
void UCustomizableObjectSystem::SetImagePixelFormatOverride(const mu::FImageOperator::FImagePixelFormatFunc& InFunc)
{
	if (Private != nullptr)
	{
		Private->ImageFormatOverrideFunc = InFunc;
	}
}
#endif


void UCustomizableObjectSystem::AddUncompiledCOWarning(const UCustomizableObject& InObject, FString const* OptionalLogInfo)
{
	FString Msg;
	Msg += FString::Printf(TEXT("Warning: Customizable Object [%s] not compiled."), *InObject.GetName());
	GEngine->AddOnScreenDebugMessage((uint64)((PTRINT)&InObject), 10.0f, FColor::Red, Msg);

#if WITH_EDITOR
	// Mutable will spam these warnings constantly due to the tick and LOD manager checking for instances to update with every tick. Send only one message per CO in the editor.
	if (UncompiledCustomizableObjectIds.Find(InObject.GetVersionId()) != INDEX_NONE)
	{
		return;
	}
	
	// Add notification
	UncompiledCustomizableObjectIds.Add(InObject.GetVersionId());

	FMessageLog MessageLog("Mutable");
	MessageLog.Warning(FText::FromString(Msg));

	if (!UncompiledCustomizableObjectsNotificationPtr.IsValid())
	{
		FNotificationInfo Info(FText::FromString("Uncompiled Customizable Object/s found. Please, check the Message Log - Mutable for more information."));
		Info.bFireAndForget = true;
		Info.bUseThrobber = true;
		Info.FadeOutDuration = 1.0f;
		Info.ExpireDuration = 5.0f;

		UncompiledCustomizableObjectsNotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
	}

	const FString ErrorString = FString::Printf(
		TEXT("Customizable Object [%s] not compiled.  Compile via the editor or via code before instancing.  %s"),
		*InObject.GetName(), OptionalLogInfo ? **OptionalLogInfo : TEXT(""));

#else // !WITH_EDITOR
	const FString ErrorString = FString::Printf(
		TEXT("Customizable Object [%s] not compiled.  This is not an Editor build, so this is an unrecoverable bad state; could be due to code or a cook failure.  %s"),
		*InObject.GetName(), OptionalLogInfo ? **OptionalLogInfo : TEXT(""));
#endif

	// Also log an error so if this happens as part of a bug report we'll have this info.
	UE_LOG(LogMutable, Error, TEXT("%s"), *ErrorString);
}


void UCustomizableObjectSystem::EnableBenchmark()
{
	LogBenchmarkUtil::StartLogging();
}


void UCustomizableObjectSystem::EndBenchmark()
{
	LogBenchmarkUtil::ShutdownAndSaveResults();
}


void UCustomizableObjectSystem::SetReleaseMutableTexturesImmediately(bool bReleaseTextures)
{
	GetPrivateChecked()->bReleaseTexturesImmediately = bReleaseTextures;
}


#if WITH_EDITOR

void UCustomizableObjectSystem::OnPreBeginPIE(const bool bIsSimulatingInEditor)
{
	if (!EditorSettings.bCompileRootObjectsOnStartPIE || IsRunningGame() || IsCompilationDisabled())
	{
		return;
	}
	
	// Find root customizable objects
	FARFilter AssetRegistryFilter;
	UE_MUTABLE_GET_CLASSPATHS(AssetRegistryFilter).Add(UE_MUTABLE_TOPLEVELASSETPATH(TEXT("/Script/CustomizableObject"), TEXT("CustomizableObject")));
	AssetRegistryFilter.TagsAndValues.Add(FName("IsRoot"), FString::FromInt(1));

	TArray<FAssetData> OutAssets;
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	AssetRegistryModule.Get().GetAssets(AssetRegistryFilter, OutAssets);

	TArray<FAssetData> TempObjectsToRecompile;
	for (const FAssetData& Asset : OutAssets)
	{
		// If it is referenced by PIE it should be loaded
		if (!Asset.IsAssetLoaded())
		{
			continue;
		}
		
		const UCustomizableObject* Object = Cast<UCustomizableObject>(Asset.GetAsset());
		if (!Object || Object->IsCompiled() || Object->IsLocked() || Object->bIsChildObject)
		{
			continue;
		}
		
		// Add uncompiled objects to the objects to cook list
		TempObjectsToRecompile.Add(Asset);
	}

	if (!TempObjectsToRecompile.IsEmpty())
	{
		const FText Msg = FText::FromString(TEXT("Warning: one or more Customizable Objects used in PIE are uncompiled.\n\nDo you want to compile them?"));
		if (FMessageDialog::Open(EAppMsgType::OkCancel, Msg) == EAppReturnType::Ok)
		{
			ObjectsToRecompile.Empty(TempObjectsToRecompile.Num());
			RecompileCustomizableObjects(TempObjectsToRecompile);
		}
	}
}

void UCustomizableObjectSystem::StartNextRecompile()
{
	if (GEngine)
	{
		GEngine->ForceGarbageCollection();
	}

	FAssetData Itr = ObjectsToRecompile.Pop();

	if (UCustomizableObject* CustomizableObject = Cast<UCustomizableObject>(Itr.GetAsset()))
	{
		const FText UpdateMsg = FText::FromString(FString::Printf(TEXT("Compiling Customizable Objects:\n%s"), *CustomizableObject->GetName()));
		FSlateNotificationManager::Get().UpdateProgressNotification(RecompileNotificationHandle, NumObjectsCompiled, TotalNumObjectsToRecompile, UpdateMsg);

		// Use default options
		FCompilationOptions Options = CustomizableObject->CompileOptions;
		Options.bSilentCompilation = true;
		check(RecompileCustomizableObjectsCompiler != nullptr);
		RecompileCustomizableObjectsCompiler->Compile(*CustomizableObject, Options, true);
	}
}

void UCustomizableObjectSystem::RecompileCustomizableObjectAsync(const FAssetData& InAssetData,
	const UCustomizableObject* InObject)
{
	if (IsRunningGame() || IsCompilationDisabled())
	{
		return;
	}
	
	if ((InObject && InObject->IsLocked()) || ObjectsToRecompile.Find((InAssetData)) != INDEX_NONE)
	{
		return;
	}
	
	if (!ObjectsToRecompile.IsEmpty())
	{
		ObjectsToRecompile.Add(InAssetData);
	}
	else
	{
		RecompileCustomizableObjects({InAssetData});
	}
}

void UCustomizableObjectSystem::RecompileCustomizableObjects(const TArray<FAssetData>& InObjects)
{
	if (IsRunningGame() || IsCompilationDisabled())
	{
		return;
	}

	if (InObjects.Num())
	{
		if (!RecompileCustomizableObjectsCompiler)
		{
			RecompileCustomizableObjectsCompiler = GetNewCompiler();

			if (!RecompileCustomizableObjectsCompiler)
			{
				return;
			}
		}

		ObjectsToRecompile.Append(InObjects);

		TotalNumObjectsToRecompile = ObjectsToRecompile.Num();
		NumObjectsCompiled = 0;

		if (RecompileNotificationHandle.IsValid())
		{
			++TotalNumObjectsToRecompile;
			FSlateNotificationManager::Get().UpdateProgressNotification(RecompileNotificationHandle, NumObjectsCompiled, TotalNumObjectsToRecompile);
		}
		else
		{
			RecompileNotificationHandle = FSlateNotificationManager::Get().StartProgressNotification(FText::FromString(TEXT("Compiling Customizable Objects")), TotalNumObjectsToRecompile);
			StartNextRecompile();
		}
	}
}


void UCustomizableObjectSystem::TickRecompileCustomizableObjects()
{
	bool bUpdated = false;
	
	if (RecompileCustomizableObjectsCompiler)
	{
		bUpdated = RecompileCustomizableObjectsCompiler->Tick() || RecompileCustomizableObjectsCompiler->GetCompilationState() == ECustomizableObjectCompilationState::Failed;
	}

	if (bUpdated)
	{
		NumObjectsCompiled++;

		if (!ObjectsToRecompile.IsEmpty())
		{
			StartNextRecompile();
		}
		else // All objects compiled, clean up
		{
			// Delete compiler
			delete RecompileCustomizableObjectsCompiler;
			RecompileCustomizableObjectsCompiler = nullptr;

			// Remove progress bar
			FSlateNotificationManager::Get().UpdateProgressNotification(RecompileNotificationHandle, NumObjectsCompiled, TotalNumObjectsToRecompile);
			FSlateNotificationManager::Get().CancelProgressNotification(RecompileNotificationHandle);
			RecompileNotificationHandle.Reset();

			if (GEngine)
			{
				GEngine->ForceGarbageCollection();
			}
		}
	}
}


uint64 UCustomizableObjectSystem::GetMaxChunkSizeForPlatform(const ITargetPlatform* TargetPlatform)
{
	const FString& PlatformName = TargetPlatform ? TargetPlatform->IniPlatformName() : FPlatformProperties::IniPlatformName();

	if (const int64* CachedMaxChunkSize = PlatformMaxChunkSize.Find(PlatformName))
	{
		return *CachedMaxChunkSize;
	}

	int64 MaxChunkSize = -1;

	if (!FParse::Value(FCommandLine::Get(), TEXT("ExtraFlavorChunkSize="), MaxChunkSize) || MaxChunkSize < 0)
	{
		FConfigFile PlatformIniFile;
		FConfigCacheIni::LoadLocalIniFile(PlatformIniFile, TEXT("Game"), true, *PlatformName);
		FString ConfigString;
		if (PlatformIniFile.GetString(TEXT("/Script/UnrealEd.ProjectPackagingSettings"), TEXT("MaxChunkSize"), ConfigString))
		{
			MaxChunkSize = FCString::Atoi64(*ConfigString);
		}
	}

	// If no limit is specified default it to MUTABLE_STREAMED_DATA_MAXCHUNKSIZE 
	if (MaxChunkSize <= 0)
	{
		MaxChunkSize = MUTABLE_STREAMED_DATA_MAXCHUNKSIZE;
	}

	PlatformMaxChunkSize.Add(PlatformName, MaxChunkSize);

	return MaxChunkSize;
}

#endif // WITH_EDITOR


void UCustomizableObjectSystem::CacheImage(FName ImageId)
{
	GetPrivateChecked()->GetImageProviderChecked()->CacheImage(ImageId, true);
}


void UCustomizableObjectSystem::UnCacheImage(FName ImageId)
{
	GetPrivateChecked()->GetImageProviderChecked()->UnCacheImage(ImageId, true);
}


void UCustomizableObjectSystem::ClearImageCache()
{
	GetPrivateChecked()->GetImageProviderChecked()->ClearCache(true);
}


bool FCustomizableObjectSystemPrivate::IsMutableAnimInfoDebuggingEnabled() const
{ 
#if WITH_EDITORONLY_DATA
	return EnableMutableAnimInfoDebugging > 0;
#else
	return false;
#endif
}


FUnrealMutableImageProvider* FCustomizableObjectSystemPrivate::GetImageProviderChecked() const
{
	check(ImageProvider)
	return ImageProvider.Get();
}


bool UCustomizableObjectSystem::IsMutableAnimInfoDebuggingEnabled() const
{
#if WITH_EDITOR
	return GetPrivateChecked()->IsMutableAnimInfoDebuggingEnabled();
#else
	return false;
#endif
}
