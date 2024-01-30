// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/WorldPartitionActorDescInstance.h"

#if WITH_EDITOR

#include "WorldPartition/ActorDescContainerInstance.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionActorDesc.h"
#include "UObject/MetaData.h"

#define LOCTEXT_NAMESPACE "FWorldPartitionActorDescInstance"

FWorldPartitionActorDescInstance::FWorldPartitionActorDescInstance()
	: ContainerInstance(nullptr)
	, SoftRefCount(0)
	, HardRefCount(0)
	, bIsForcedNonSpatiallyLoaded(false)
	, UnloadedReason(nullptr)
	, ActorDesc(nullptr)
	, ChildContainerInstance(nullptr)
{
}

FWorldPartitionActorDescInstance::FWorldPartitionActorDescInstance(UActorDescContainerInstance* InContainerInstance, FWorldPartitionActorDesc* InActorDesc)
	: FWorldPartitionActorDescInstance()
{
	check(InContainerInstance);
	ContainerInstance = InContainerInstance;

	check(InActorDesc);
	ActorDesc = InActorDesc;
}

void FWorldPartitionActorDescInstance::UpdateActorDesc(FWorldPartitionActorDesc* InActorDesc)
{
	ActorDesc = InActorDesc;
}

bool FWorldPartitionActorDescInstance::IsLoaded(bool bEvenIfPendingKill) const
{
	if (ActorPtr.IsExplicitlyNull() || ActorPtr.IsStale())
	{
		ActorPtr = FindObject<AActor>(nullptr, *GetActorSoftPath().ToString());
	}

	return ActorPtr.IsValid(bEvenIfPendingKill);
}

AActor* FWorldPartitionActorDescInstance::GetActor(bool bEvenIfPendingKill, bool bEvenIfUnreachable) const
{
	if (ActorPtr.IsExplicitlyNull() || ActorPtr.IsStale())
	{
		ActorPtr = FindObject<AActor>(nullptr, *GetActorSoftPath().ToString());
	}

	return bEvenIfUnreachable ? ActorPtr.GetEvenIfUnreachable() : ActorPtr.Get(bEvenIfPendingKill);
}

TWeakObjectPtr<AActor>* FWorldPartitionActorDescInstance::GetActorPtr(bool bEvenIfPendingKill, bool bEvenIfUnreachable) const
{
	return GetActor(bEvenIfPendingKill, bEvenIfUnreachable) ? &ActorPtr : nullptr;
}

FSoftObjectPath FWorldPartitionActorDescInstance::GetActorSoftPath() const
{
	return ActorPath.IsSet() ? ActorPath.GetValue() : GetActorDesc()->GetActorSoftPath();
}

FName FWorldPartitionActorDescInstance::GetActorName() const
{
	return *FPaths::GetExtension(GetActorSoftPath().ToString());
}

bool FWorldPartitionActorDescInstance::IsValid() const
{
	return !!GetActorDesc();
}

bool FWorldPartitionActorDescInstance::IsEditorRelevant() const
{
	return GetActorDesc()->IsEditorRelevant(this);
}

bool FWorldPartitionActorDescInstance::IsRuntimeRelevant() const
{
	return GetActorDesc()->IsRuntimeRelevant(this);
}

AActor* FWorldPartitionActorDescInstance::Load()
{
	static FText FailedToLoad(LOCTEXT("FailedToLoadReason", "Failed to load"));
	UnloadedReason = nullptr;

	if (ActorPtr.IsExplicitlyNull() || ActorPtr.IsStale())
	{
		// First, try to find the existing actor which could have been loaded by another actor (through standard serialization)
		ActorPtr = FindObject<AActor>(nullptr, *GetActorSoftPath().ToString());
	}

	// Then, if the actor isn't loaded, load it
	if (ActorPtr.IsExplicitlyNull())
	{
		const FLinkerInstancingContext* InstancingContext = GetContainerInstance()->GetInstancingContext();
		FSoftObjectPath LocalActorPath = GetActorSoftPath();

		UPackage* Package = nullptr;

		if (InstancingContext)
		{
			FName RemappedPackageName = InstancingContext->RemapPackage(GetActorPackage());
			check(RemappedPackageName != LocalActorPath.GetLongPackageFName());

			Package = CreatePackage(*RemappedPackageName.ToString());
		}

		Package = LoadPackage(Package, *GetActorPackage().ToString(), LOAD_None, nullptr, InstancingContext);

		if (Package)
		{
			ActorPtr = FindObject<AActor>(nullptr, *LocalActorPath.ToString());
			if (!ActorPtr.IsValid())
			{
				UE_LOG(LogWorldPartition, Warning, TEXT("Can't load actor guid `%s` ('%s') from package '%s'"), *GetGuid().ToString(), *GetActorName().ToString(), *GetActorPackage().ToString());
				UnloadedReason = &FailedToLoad;
			}
		}
	}

	return ActorPtr.Get();
}

void FWorldPartitionActorDescInstance::Unload()
{
	// Notify Desc as it can have some custom code to run on the actor depending on type
	GetActorDesc()->OnUnloadingInstance(this);

	if (AActor* Actor = GetActor())
	{
		// At this point, it can happen that an actor isn't in an external package:
		//
		// PIE travel: 
		//		in this case, actors referenced by the world package (an example is the level script) will be duplicated as part of the PIE world duplication and will end up
		//		not being using an external package, which is fine because in that case they are considered as always loaded.
		//
		// FWorldPartitionCookPackageSplitter:
		//		should mark each FWorldPartitionActorDesc as moved, and the splitter should take responsbility for calling ClearFlags on every object in 
		//		the package when it does the move

		if (Actor->IsPackageExternal())
		{
			ForEachObjectWithPackage(Actor->GetPackage(), [](UObject* Object)
				{
					if (Object->HasAnyFlags(RF_Public | RF_Standalone))
					{
						CastChecked<UMetaData>(Object)->ClearFlags(RF_Public | RF_Standalone);
					}
					return true;
				}, false);
		}

		ActorPtr = nullptr;
	}
}

void FWorldPartitionActorDescInstance::Invalidate()
{
	check(!ChildContainerInstance);
	ContainerInstance = nullptr;
}

const FDataLayerInstanceNames& FWorldPartitionActorDescInstance::GetDataLayerInstanceNames() const
{
	static FDataLayerInstanceNames EmptyDataLayers;
	if (ensure(HasResolvedDataLayerInstanceNames()))
	{
		return ResolvedDataLayerInstanceNames.GetValue();
	}
	return EmptyDataLayers;
}

const FText& FWorldPartitionActorDescInstance::GetUnloadedReason() const
{
	static FText Unloaded(LOCTEXT("UnloadedReason", "Unloaded"));
	return UnloadedReason ? *UnloadedReason : Unloaded;
}

FString FWorldPartitionActorDescInstance::ToString(FWorldPartitionActorDesc::EToStringMode Mode) const
{
	return GetActorDesc()->ToString(Mode);
}

void FWorldPartitionActorDescInstance::RegisterChildContainerInstance()
{
	check(IsChildContainerInstance());
	check(!ChildContainerInstance);

	ChildContainerInstance = GetActorDesc()->CreateChildContainerInstance(this);
	check(ChildContainerInstance);
	ContainerInstance->OnRegisterChildContainerInstance(GetGuid(), ChildContainerInstance);
}

void FWorldPartitionActorDescInstance::UnregisterChildContainerInstance()
{
	check(ChildContainerInstance);
	ContainerInstance->OnUnregisterChildContainerInstance(GetGuid());
	ChildContainerInstance->Uninitialize();
	ChildContainerInstance = nullptr;
}

void FWorldPartitionActorDescInstance::UpdateChildContainerInstance()
{
	// Create before unregistering so that we benefit from shared containers (use GetActorDesc->IsChildContainerInstance as we want to know if our updated desc should be a Container instance or not)
	// ChildContainerInstance member might be non null and we don't want IsChildContainerInstance() to return true in this case if the ActorDesc isn't a Container anymore
	UActorDescContainerInstance* NewChildContainerInstance = GetActorDesc()->IsChildContainerInstance() ? GetActorDesc()->CreateChildContainerInstance(this) : nullptr;
	
	// Unregister previous
	if (ChildContainerInstance)
	{
		ContainerInstance->OnUnregisterChildContainerInstance(GetGuid());
		ChildContainerInstance->Uninitialize();
		ChildContainerInstance = nullptr;
	}
	
	// Register new if it is valid
	if (NewChildContainerInstance)
	{
		ChildContainerInstance = NewChildContainerInstance;
		ContainerInstance->OnRegisterChildContainerInstance(GetGuid(), ChildContainerInstance);
	}
}

#undef LOCTEXT_NAMESPACE

#endif