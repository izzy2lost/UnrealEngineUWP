// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/ActorDescContainer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ActorDescContainer)

#if WITH_EDITOR
#include "Editor.h"
#include "Algo/Transform.h"
#include "Engine/Level.h"
#include "ExternalPackageHelper.h"
#include "UObject/ObjectSaveContext.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Modules/ModuleManager.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionActorDescUtils.h"
#include "WorldPartition/WorldPartitionClassDescRegistry.h"
#include "WorldPartition/DataLayer/DataLayerManager.h"

UActorDescContainer::FActorDescContainerInitializeDelegate UActorDescContainer::OnActorDescContainerInitialized;
#endif

UActorDescContainer::UActorDescContainer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
#if WITH_EDITOR
	, bContainerInitialized(false)
#endif
{}

#if WITH_EDITOR
void UActorDescContainer::Initialize(const FInitializeParams& InitParams)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UActorDescContainer::Initialize);

	// @todo_ow: We need to pass the world context to RegisterActorDescriptor for FLevelInstanceActorDesc::RegisterContainerInstance to resolve the ULevelInstanceSubsystem.
	// A better solution would be for ActorDescContainers to always be outered to an OwningWorldPartition (with the downside of not sharing between 2 instanced WorldPartition).
	// With this, we could always find the owning WorldPartition and of course the owning world (GetOwningWorldPartition()->GetWorld()).
	UWorld* OwningWorld = InitParams.World;

	check(!bContainerInitialized);
	ContainerPackageName = InitParams.PackageName;
	TArray<FAssetData> Assets;

	if (!ContainerPackageName.IsNone())
	{
		const FString ContainerExternalActorsPath = GetExternalActorPath();

		// Do a synchronous scan of the level external actors path.					
		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(ScanSynchronous);
			AssetRegistry.ScanSynchronous({ ContainerExternalActorsPath }, TArray<FString>());
		}

		FARFilter Filter;
		Filter.bRecursivePaths = true;
		Filter.bIncludeOnlyOnDiskAssets = true;
		Filter.PackagePaths.Add(*ContainerExternalActorsPath);

		TRACE_CPUPROFILER_EVENT_SCOPE(GetAssets);
		FExternalPackageHelper::GetSortedAssets(Filter, Assets);
	}

	FWorldPartitionClassDescRegistry& ClassDescRegistry = FWorldPartitionClassDescRegistry::Get();
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(GatherDescriptorsClass);
		
		TSet<FTopLevelAssetPath> ClassPaths;
		for (const FAssetData& Asset : Assets)
		{
			ClassPaths.Add(Asset.AssetClassPath);
		}

		ClassDescRegistry.PrefetchClassDescs(ClassPaths.Array());
	}

	TMap<FGuid, TUniquePtr<FWorldPartitionActorDesc>> ValidActorDescs;
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(CreateDescriptors);

		TMap<FName, FWorldPartitionActorDesc*> ActorDescsByPackage;
		for (const FAssetData& Asset : Assets)
		{
			TUniquePtr<FWorldPartitionActorDesc> ActorDesc = FWorldPartitionActorDescUtils::GetActorDescriptorFromAssetData(Asset);
								
			if (!ActorDesc.IsValid())
			{
				UE_LOG(LogWorldPartition, Warning, TEXT("Invalid actor descriptor for actor '%s' from package '%s'"), *Asset.GetObjectPathString(), *Asset.PackageName.ToString());
				InvalidActors.Emplace(Asset);
			} 
			else if (!ActorDesc->GetNativeClass().IsValid())
			{
				UE_LOG(LogWorldPartition, Warning, TEXT("Invalid actor native class: Actor: '%s' (guid '%s') from package '%s'"),
					*ActorDesc->GetActorName().ToString(),
					*ActorDesc->GetGuid().ToString(),
					*ActorDesc->GetActorPackage().ToString());
				InvalidActors.Emplace(Asset);
			}
			else if (ActorDesc->GetBaseClass().IsValid() && !ClassDescRegistry.IsRegisteredClass(ActorDesc->GetBaseClass()))
			{
				UE_LOG(LogWorldPartition, Warning, TEXT("Unknown actor base class `%s`: Actor: '%s' (guid '%s') from package '%s'"),
					*ActorDesc->GetBaseClass().ToString(),
					*ActorDesc->GetActorName().ToString(),
					*ActorDesc->GetGuid().ToString(),
					*ActorDesc->GetActorPackage().ToString());
				InvalidActors.Emplace(Asset);
			}
			else if (InitParams.FilterActorDesc && !InitParams.FilterActorDesc(ActorDesc.Get()))
			{
				InvalidActors.Emplace(Asset);
			}
			// At this point, the actor descriptor is well formed and valid on its own. We now make validations based on the already registered
			// actor descriptors, such as duplicated actor GUIDs or multiple actors in the same package, etc.
			else if (FWorldPartitionActorDesc* ExistingDescPackage = ActorDescsByPackage.FindRef(ActorDesc->GetActorPackage()))
			{
				UE_LOG(LogWorldPartition, Warning, TEXT("Duplicate actor descriptor in package `%s`: Actor: '%s' -> Existing actor '%s'"), 
					*ActorDesc->GetActorPackage().ToString(), 
					*ActorDesc->GetActorName().ToString(), 
					*ExistingDescPackage->GetActorName().ToString());

				// No need to add all actors in the same package several times as we only want to open the package for delete when repairing
				if (ValidActorDescs.Contains(ActorDesc->GetGuid()))
				{
					InvalidActors.Emplace(Asset);
					ValidActorDescs.Remove(ActorDesc->GetGuid());
				}
			}
			else if (TUniquePtr<FWorldPartitionActorDesc>* ExistingActorDescPtr = ValidActorDescs.Find(ActorDesc->GetGuid()))
			{
				const FWorldPartitionActorDesc* ExistingActorDesc = ExistingActorDescPtr->Get();
				check(ExistingActorDesc->GetGuid() == ActorDesc->GetGuid());
				UE_LOG(LogWorldPartition, Warning, TEXT("Duplicate actor descriptor guid `%s`: Actor: '%s' from package '%s' -> Existing actor '%s' from package '%s'"), 
					*ActorDesc->GetGuid().ToString(), 
					*ActorDesc->GetActorName().ToString(), 
					*ActorDesc->GetActorPackage().ToString(),
					*ExistingActorDesc->GetActorName().ToString(),
					*ExistingActorDesc->GetActorPackage().ToString());
				InvalidActors.Emplace(Asset);
			}
			else
			{
				ActorDescsByPackage.Add(ActorDesc->GetActorPackage(), ActorDesc.Get());
				ValidActorDescs.Add(ActorDesc->GetGuid(), MoveTemp(ActorDesc));
			}
		}
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(RegisterDescriptors);
		for (auto& [ActorGuid, ActorDesc] : ValidActorDescs)
		{
			RegisterActorDescriptor(ActorDesc.Release(), OwningWorld);
		}
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(OnActorDescContainerInitialized)
		OnActorDescContainerInitialized.Broadcast(this);
	}

	RegisterEditorDelegates();

	bContainerInitialized = true;
}

void UActorDescContainer::Uninitialize()
{
	if (bContainerInitialized)
	{
		UnregisterEditorDelegates();
		bContainerInitialized = false;
	}

	for (TUniquePtr<FWorldPartitionActorDesc>& ActorDescPtr : ActorDescList)
	{
		if (FWorldPartitionActorDesc* ActorDesc = ActorDescPtr.Get())
		{
			UnregisterActorDescriptor(ActorDesc);
		}
		ActorDescPtr.Reset();
	}
}

void UActorDescContainer::BeginDestroy()
{
	Super::BeginDestroy();

	Uninitialize();
}

FString UActorDescContainer::GetExternalActorPath() const
{
	return ULevel::GetExternalActorsPath(ContainerPackageName.ToString());
}

bool UActorDescContainer::IsActorDescHandled(const AActor* Actor) const
{
	if (Actor->GetContentBundleGuid() == GetContentBundleGuid())
	{
		const FString ActorPackageName = Actor->GetPackage()->GetName();
		const FString ExternalActorPath = GetExternalActorPath() / TEXT("");
		return ActorPackageName.StartsWith(ExternalActorPath);
	}
	return false;
}

bool UActorDescContainer::IsMainPartitionContainer() const
{
	return GetWorldPartition() && GetWorldPartition()->GetActorDescContainer() == this;
}

bool UActorDescContainer::IsTemplateContainer() const
{
	bool bIsTemplate = GetOuter() == GetTransientPackage();
	check(bIsTemplate || GetWorldPartition());
	return bIsTemplate;
}

UWorldPartition* UActorDescContainer::GetWorldPartition() const
{
	UWorldPartition* OuterWorldPartition = GetTypedOuter<UWorldPartition>();
	check(OuterWorldPartition || IsTemplateContainer());
	return OuterWorldPartition;
}

void UActorDescContainer::RegisterActorDescriptor(FWorldPartitionActorDesc* ActorDesc, UWorld* InWorldContext)
{
	FActorDescList::AddActorDescriptor(ActorDesc);

	ActorDesc->SetContainer(this, InWorldContext);
		
	//@odo_ow get rid of this with a delegate
	if (const UDataLayerManager* DataLayerManager = UDataLayerManager::GetDataLayerManager(InWorldContext))
	{
		DataLayerManager->ResolveActorDescDataLayers(ActorDesc);
	}

	ActorsByName.Add(ActorDesc->GetActorName(), ActorsByGuid.FindChecked(ActorDesc->GetGuid()));
}

void UActorDescContainer::UnregisterActorDescriptor(FWorldPartitionActorDesc* ActorDesc)
{
	FActorDescList::RemoveActorDescriptor(ActorDesc);
	ActorDesc->SetContainer(nullptr, nullptr);
	verifyf(ActorsByName.Remove(ActorDesc->GetActorName()), TEXT("Missing actor '%s' from container '%s'"), *ActorDesc->GetActorName().ToString(), *ContainerPackageName.ToString());
}

bool UActorDescContainer::ShouldHandleActorEvent(const AActor* Actor)
{
	return Actor && IsActorDescHandled(Actor) && Actor->IsMainPackageActor() && Actor->GetLevel();
}

const FWorldPartitionActorDesc* UActorDescContainer::GetActorDescByPath(const FString& ActorPath) const
{
	FString ActorName;
	FString ActorContext;
	if (!ActorPath.Split(TEXT("."), &ActorContext, &ActorName, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
	{
		ActorName = ActorPath;
	}

	return GetActorDescByName(FName(*ActorName));
}

const FWorldPartitionActorDesc* UActorDescContainer::GetActorDescByPath(const FSoftObjectPath& ActorPath) const
{
	return GetActorDescByPath(ActorPath.ToString());
}

const FWorldPartitionActorDesc* UActorDescContainer::GetActorDescByName(FName ActorName) const
{
	if (const TUniquePtr<FWorldPartitionActorDesc>* const* ActorDesc = ActorsByName.Find(ActorName))
	{
		return (*ActorDesc)->Get();
	}

	return nullptr;
}

void UActorDescContainer::OnObjectPreSave(UObject* Object, FObjectPreSaveContext SaveContext)
{
	if (!SaveContext.IsProceduralSave() && !(SaveContext.GetSaveFlags() & SAVE_FromAutosave))
	{
		if (const AActor* Actor = Cast<AActor>(Object))
		{
			if (ShouldHandleActorEvent(Actor))
			{
				check(IsValidChecked(Actor));
				if (TUniquePtr<FWorldPartitionActorDesc>* ExistingActorDesc = GetActorDescriptor(Actor->GetActorGuid()))
				{
					// Existing actor
					OnActorDescUpdating(ExistingActorDesc->Get());
					FWorldPartitionActorDescUtils::UpdateActorDescriptorFromActor(Actor, *ExistingActorDesc);
					OnActorDescUpdated(ExistingActorDesc->Get());
				}
				else
				{
					// New actor
					FWorldPartitionActorDesc* AddedActorDesc = Actor->CreateActorDesc().Release();
					RegisterActorDescriptor(AddedActorDesc, Actor->GetWorld());
					OnActorDescAdded(AddedActorDesc);
				}
			}
		}
	}
}

void UActorDescContainer::OnPackageDeleted(UPackage* Package)
{
	AActor* Actor = AActor::FindActorInPackage(Package);

	if (ShouldHandleActorEvent(Actor))
	{
		RemoveActor(Actor->GetActorGuid());
	}
}

void UActorDescContainer::OnObjectsReplaced(const TMap<UObject*, UObject*>& OldToNewObjectMap)
{
	// Patch up Actor pointers in ActorDescs
	for (auto [OldObject, NewObject] : OldToNewObjectMap)
	{
		if (AActor* OldActor = Cast<AActor>(OldObject))
		{
			if (ShouldHandleActorEvent(OldActor))
			{
				AActor* NewActor = Cast<AActor>(NewObject);
				if (FWorldPartitionActorDesc* ActorDesc = GetActorDesc(OldActor->GetActorGuid()))
				{
					FWorldPartitionActorDescUtils::ReplaceActorDescriptorPointerFromActor(OldActor, NewActor, ActorDesc);
				}
			}
		}
	}
}

void UActorDescContainer::OnClassDescriptorUpdated(const FWorldPartitionActorDesc* InClassDesc)
{
	FWorldPartitionClassDescRegistry& ClassDescRegistry = FWorldPartitionClassDescRegistry::Get();

	TArray<FString> ActorPackages;
	for (FActorDescList::TIterator<> ActorDescIterator(this); ActorDescIterator; ++ActorDescIterator)
	{
		if (ActorDescIterator->GetBaseClass().IsValid())
		{
			if (const FWorldPartitionActorDesc* ActorClassDesc = ClassDescRegistry.GetClassDescDefaultForActor(ActorDescIterator->GetBaseClass()))
			{
				if (ClassDescRegistry.IsDerivedFrom(ActorClassDesc, InClassDesc))
				{
					ActorPackages.Add(ActorDescIterator->GetActorPackage().ToString());
				}
			}
		}
	}

	if (ActorPackages.Num())
	{
		FARFilter Filter;
		Filter.bIncludeOnlyOnDiskAssets = true;
		Filter.PackageNames.Reserve(ActorPackages.Num());
		Algo::Transform(ActorPackages, Filter.PackageNames, [](const FString& ActorPath) { return *ActorPath; });

		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		AssetRegistry.ScanSynchronous(TArray<FString>(), ActorPackages);

		TArray<FAssetData> Assets;
		AssetRegistry.GetAssets(Filter, Assets);

		for (const FAssetData& Asset : Assets)
		{
			TUniquePtr<FWorldPartitionActorDesc> NewActorDesc = FWorldPartitionActorDescUtils::GetActorDescriptorFromAssetData(Asset);
								
			if (NewActorDesc.IsValid() && NewActorDesc->GetNativeClass().IsValid())
			{
				if (TUniquePtr<FWorldPartitionActorDesc>* ExistingActorDesc = GetActorDescriptor(NewActorDesc->GetGuid()))
				{
					OnActorDescUpdating(ExistingActorDesc->Get());
					FWorldPartitionActorDescUtils::UpdateActorDescriptorFromActorDescriptor(NewActorDesc, *ExistingActorDesc);
					OnActorDescUpdated(ExistingActorDesc->Get());
				}
			}
		}
	}
}

bool UActorDescContainer::RemoveActor(const FGuid& ActorGuid)
{
	if (TUniquePtr<FWorldPartitionActorDesc>* ExistingActorDesc = GetActorDescriptor(ActorGuid))
	{
		OnActorDescRemoved(ExistingActorDesc->Get());
		UnregisterActorDescriptor(ExistingActorDesc->Get());
		ExistingActorDesc->Reset();
		return true;
	}

	return false;
}

void UActorDescContainer::LoadAllActors(TArray<FWorldPartitionReference>& OutReferences)
{
	FWorldPartitionLoadingContext::FDeferred LoadingContext;
	OutReferences.Reserve(OutReferences.Num() + GetActorDescCount());
	for (FActorDescList::TIterator<> ActorDescIterator(this); ActorDescIterator; ++ActorDescIterator)
	{
		FWorldPartitionActorDesc* ActorDesc = *ActorDescIterator;
		OutReferences.Emplace(this, ActorDesc->GetGuid());
	}
}

bool UActorDescContainer::ShouldRegisterDelegates()
{
	UWorld* OuterWorld = GetTypedOuter<UWorld>();
	// No need to register delegates for level instances
	bool bIsInstance = OuterWorld && OuterWorld->IsInstanced() && !OuterWorld->GetPackage()->HasAnyPackageFlags(PKG_NewlyCreated);
	UWorld* OwningWorld = GetWorldPartition() ? GetWorldPartition()->GetWorld() : nullptr;
	// Template container will always register
	bool bShouldRegisterForWorld = (OwningWorld && !OwningWorld->IsGameWorld()) || (IsTemplateContainer());
	return GEditor && !IsTemplate() && !IsRunningCookCommandlet() && !bIsInstance && bShouldRegisterForWorld;
}

void UActorDescContainer::RegisterEditorDelegates()
{
	if (ShouldRegisterDelegates())
	{
		FCoreUObjectDelegates::OnObjectPreSave.AddUObject(this, &UActorDescContainer::OnObjectPreSave);
		FEditorDelegates::OnPackageDeleted.AddUObject(this, &UActorDescContainer::OnPackageDeleted);
		FCoreUObjectDelegates::OnObjectsReplaced.AddUObject(this, &UActorDescContainer::OnObjectsReplaced);
		
		FWorldPartitionClassDescRegistry& ClassDescRegistry = FWorldPartitionClassDescRegistry::Get();
		ClassDescRegistry.OnClassDescriptorUpdated().AddUObject(this, &UActorDescContainer::OnClassDescriptorUpdated);
	}
}

void UActorDescContainer::UnregisterEditorDelegates()
{
	if (ShouldRegisterDelegates())
	{
		FCoreUObjectDelegates::OnObjectPreSave.RemoveAll(this);
		FEditorDelegates::OnPackageDeleted.RemoveAll(this);
		FCoreUObjectDelegates::OnObjectsReplaced.RemoveAll(this);

		FWorldPartitionClassDescRegistry& ClassDescRegistry = FWorldPartitionClassDescRegistry::Get();
		ClassDescRegistry.OnClassDescriptorUpdated().RemoveAll(this);
	}
}

void UActorDescContainer::OnActorDescAdded(FWorldPartitionActorDesc* NewActorDesc)
{
	OnActorDescAddedEvent.Broadcast(NewActorDesc);

	if (UWorldPartition* WorldPartition = GetWorldPartition())
	{
		WorldPartition->OnActorDescAdded(NewActorDesc);
	}
}

void UActorDescContainer::OnActorDescRemoved(FWorldPartitionActorDesc* ActorDesc)
{
	OnActorDescRemovedEvent.Broadcast(ActorDesc);

	if (UWorldPartition* WorldPartition = GetWorldPartition())
	{
		WorldPartition->OnActorDescRemoved(ActorDesc);
	}
}

void UActorDescContainer::OnActorDescUpdating(FWorldPartitionActorDesc* ActorDesc)
{
	if (UWorldPartition* WorldPartition = GetWorldPartition())
	{
		WorldPartition->OnActorDescUpdating(ActorDesc);
	}
}

void UActorDescContainer::OnActorDescUpdated(FWorldPartitionActorDesc* ActorDesc)
{
	if (UWorldPartition* WorldPartition = GetWorldPartition())
	{
		if (const UDataLayerManager* DataLayerManager = UDataLayerManager::GetDataLayerManager(WorldPartition))
		{
			DataLayerManager->ResolveActorDescDataLayers(ActorDesc);
		}
		WorldPartition->OnActorDescUpdated(ActorDesc);
	}
}

const FLinkerInstancingContext* UActorDescContainer::GetInstancingContext() const
{
	const FLinkerInstancingContext* InstancingContext = nullptr;

	if (UWorldPartition* WorldPartition = GetWorldPartition())
	{
		WorldPartition->GetInstancingContext(InstancingContext);
	}

	return InstancingContext;
}

const FTransform& UActorDescContainer::GetInstanceTransform() const
{
	if (UWorldPartition* WorldPartition = GetWorldPartition())
	{
		return WorldPartition->GetInstanceTransform();
	}

	return FTransform::Identity;
}
#endif

