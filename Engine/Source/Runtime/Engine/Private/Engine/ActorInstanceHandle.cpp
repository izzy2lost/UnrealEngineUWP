// Copyright Epic Games, Inc. All Rights Reserved.

#include "Engine/ActorInstanceHandle.h"
#include "PhysicsPublic.h"
#include "Engine/World.h"
#include "Engine/ActorInstanceManagerInterface.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"
#include "UObject/FortniteValkyrieBranchObjectVersion.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ActorInstanceHandle)

struct FActorInstanceHandleInternalHelper
{
	inline static void SetUpAsInterface(FActorInstanceHandle& InstanceHandle, IActorInstanceManagerInterface& InManagerInterface, const UPrimitiveComponent* RelevantComponent, int32 CollisionInstanceIndex)
	{
		InstanceHandle.InstanceIndex = InManagerInterface.ConvertCollisionIndexToInstanceIndex(CollisionInstanceIndex, RelevantComponent);
		InstanceHandle.Actor = InManagerInterface.FindActor(InstanceHandle);
	}

	inline static void SetUpWithActor(FActorInstanceHandle& InstanceHandle, AActor* InActor, const UPrimitiveComponent* RelevantComponent, int32 CollisionInstanceIndex)
	{
		InstanceHandle.ManagerInterface = FActorInstanceManagerInterface(InActor);
		if (IActorInstanceManagerInterface* ManagerInterfacePtr = InstanceHandle.ManagerInterface.Get())
		{
			SetUpAsInterface(InstanceHandle, *ManagerInterfacePtr, RelevantComponent, CollisionInstanceIndex);
		}
		else
		{
			InstanceHandle.Actor = InActor;
		}
	}
};

//-----------------------------------------------------------------------------
// FActorInstanceHandle
//-----------------------------------------------------------------------------
FActorInstanceHandle::FActorInstanceHandle(AActor* InActor)
	: Actor(InActor)
{
}

FActorInstanceHandle::FActorInstanceHandle(const UPrimitiveComponent* RelevantComponent, int32 CollisionInstanceIndex)
{
	if (UNLIKELY(!ensureMsgf(RelevantComponent, TEXT("Calling FActorInstanceHandle(UPrimitiveComponent, int32) constructor is pointless with RelevantComponent == nullptr"))))
	{
		return;
	}

	if (AActor* OwnerActor = RelevantComponent->GetOwner())
	{
		FActorInstanceHandleInternalHelper::SetUpWithActor(*this, OwnerActor, RelevantComponent, CollisionInstanceIndex);
	}
}

FActorInstanceHandle::FActorInstanceHandle(AActor* InActor, const UPrimitiveComponent* RelevantComponent, int32 CollisionInstanceIndex)
{
	if (LIKELY(InActor))
	{
		FActorInstanceHandleInternalHelper::SetUpWithActor(*this, InActor, RelevantComponent, CollisionInstanceIndex);
	}
	else if (RelevantComponent)
	{
		FActorInstanceHandle(RelevantComponent, CollisionInstanceIndex);
	}
	else
	{
		Actor = InActor;
	}
}

FActorInstanceHandle::FActorInstanceHandle(FActorInstanceManagerInterface InManagerInterface, int32 CollisionInstanceIndex)
	: ManagerInterface(InManagerInterface)
{
	if (IActorInstanceManagerInterface* ManagerInterfacePtr = ManagerInterface.Get())
	{
		FActorInstanceHandleInternalHelper::SetUpAsInterface(*this, *ManagerInterfacePtr, /*RelevantComponent=*/nullptr, CollisionInstanceIndex);
	}
}

FActorInstanceHandle::FActorInstanceHandle(const FActorInstanceHandle& Other)
{
	Actor = Other.Actor;

	ManagerInterface = Other.ManagerInterface;
	InstanceIndex = Other.InstanceIndex;
}

FActorInstanceHandle FActorInstanceHandle::MakeDehydratedActorHandle(UObject& Manager, int32 InInstanceIndex)
{
	FActorInstanceHandle ReturnHandle;
	ReturnHandle.ManagerInterface = FActorInstanceManagerInterface(&Manager);
	ReturnHandle.InstanceIndex = InInstanceIndex;

	return ReturnHandle;
}

bool FActorInstanceHandle::IsValid() const
{
	return (ManagerInterface.IsValid() && InstanceIndex != INDEX_NONE) || IsActorValid();
}

bool FActorInstanceHandle::DoesRepresentClass(const UClass* OtherClass) const
{
	if (const UClass* RepresentedClass = GetRepresentedClass())
	{
		return RepresentedClass->IsChildOf(OtherClass);
	}
	return false;
}

UClass* FActorInstanceHandle::GetRepresentedClass() const
{
	if (IsActorValid())
	{
		return Actor->GetClass();
	}

	IActorInstanceManagerInterface* ManagerInterfacePtr = ManagerInterface.Get();
	return ManagerInterfacePtr && (InstanceIndex != INDEX_NONE)
		? ManagerInterfacePtr->GetRepresentedClass(InstanceIndex)
		: nullptr;
}

ULevel* FActorInstanceHandle::GetLevel() const
{
	if (IsActorValid())
	{
		return Actor->GetLevel();
	}

	IActorInstanceManagerInterface* ManagerInterfacePtr = ManagerInterface.Get();
	return ManagerInterfacePtr && (InstanceIndex != INDEX_NONE)
		? ManagerInterfacePtr->GetLevelForInstance(InstanceIndex)
		: nullptr;
}

FVector FActorInstanceHandle::GetLocation() const
{
	if (IsActorValid())
	{
		return Actor->GetActorLocation();
	}

	IActorInstanceManagerInterface* ManagerInterfacePtr = ManagerInterface.Get();
	return ManagerInterfacePtr && (InstanceIndex != INDEX_NONE)
		? ManagerInterfacePtr->GetTransform(*this).GetLocation()
		: FVector();
}

FRotator FActorInstanceHandle::GetRotation() const
{
	if (IsActorValid())
	{
		return Actor->GetActorRotation();
	}

	IActorInstanceManagerInterface* ManagerInterfacePtr = ManagerInterface.Get();
	return ManagerInterfacePtr && (InstanceIndex != INDEX_NONE)
		? ManagerInterfacePtr->GetTransform(*this).GetRotation().Rotator()
		: FRotator();
}

FTransform FActorInstanceHandle::GetTransform() const
{
	if (IsActorValid())
	{
		return Actor->GetActorTransform();
	}

	IActorInstanceManagerInterface* ManagerInterfacePtr = ManagerInterface.Get();
	return ManagerInterfacePtr && (InstanceIndex != INDEX_NONE)
		? ManagerInterfacePtr->GetTransform(*this)
		: FTransform();
}

FName FActorInstanceHandle::GetFName() const
{
	if (IsActorValid())
	{
		return Actor->GetFName();
	}

	return NAME_None;
}

FString FActorInstanceHandle::GetName() const
{
	if (IsActorValid())
	{
		return Actor->GetName();
	}

	if (ManagerInterface.IsValid())
	{
		return FString::Printf(TEXT("%s:d"), *GetNameSafe(ManagerInterface.GetObject()), InstanceIndex);
	}

	return TEXT("Invalid");
}

AActor* FActorInstanceHandle::GetManagingActor() const
{
	if (IsActorValid())
	{
		return Actor.Get();
	}

	return Cast<AActor>(ManagerInterface.GetObject());
}

USceneComponent* FActorInstanceHandle::GetRootComponent() const
{
	if (IsActorValid())
	{
		return Actor->GetRootComponent();
	}

	AActor* AsActor = Cast<AActor>(ManagerInterface.GetObject());
	return AsActor ? AsActor->GetRootComponent() : nullptr;
}

AActor* FActorInstanceHandle::FetchActor() const
{
	if (IsActorValid())
	{
		return Actor.Get();
	}

	return ManagerInterface.IsValid() ? ManagerInterface->FindOrCreateActor(*this) : nullptr;
}

UObject* FActorInstanceHandle::GetActorAsUObject()
{
	// 
	return Cast<UObject>(Actor.Get());
}

const UObject* FActorInstanceHandle::GetActorAsUObject() const
{
	if (IsActorValid())
	{
		return Cast<UObject>(Actor.Get());
	}

	return nullptr;
}

bool FActorInstanceHandle::IsActorValid() const
{
	return Actor.IsValid();
}

void FActorInstanceHandle::SetCachedActor(AActor* InActor) const
{
	check(Actor.IsValid() == false);
	Actor = InActor;
}

FActorInstanceHandle& FActorInstanceHandle::operator=(AActor* OtherActor)
{
	Actor = OtherActor;
	ManagerInterface.Reset();
	InstanceIndex = INDEX_NONE;

	return *this;
}

bool FActorInstanceHandle::operator==(const FActorInstanceHandle& Other) const
{
	// try to compare managers and indices first if we have them
	if (ManagerInterface.IsValid() && Other.ManagerInterface.IsValid() && InstanceIndex != INDEX_NONE && Other.InstanceIndex != INDEX_NONE)
	{
		return ManagerInterface == Other.ManagerInterface && InstanceIndex == Other.InstanceIndex;
	}

	// try to compare the actors
	const AActor* MyActor = FetchActor();
	const AActor* OtherActor = Other.FetchActor();

	return MyActor == OtherActor;
}

bool FActorInstanceHandle::operator!=(const FActorInstanceHandle& Other) const
{
	return !(*this == Other);
}

bool FActorInstanceHandle::operator==(const AActor* OtherActor) const
{
	// if we have an actor, compare the two actors
	if (AActor* AsActor = Actor.Get())
	{
		return AsActor == OtherActor;
	}

	// if OtherActor is null then we're only equal if this doesn't refer to a valid instance
	if (OtherActor == nullptr)
	{
		return !ManagerInterface.IsValid() && InstanceIndex == INDEX_NONE;
	}

	IActorInstanceManagerInterface* ManagerInterfacePtr = ManagerInterface.Get();
	return ManagerInterfacePtr && (ManagerInterfacePtr->FindActor(*this) == OtherActor);
}

bool FActorInstanceHandle::operator!=(const AActor* OtherActor) const
{
	return !(*this == OtherActor);
}

uint32 GetTypeHash(const FActorInstanceHandle& Handle)
{
	uint32 Hash = 0;
	if (AActor* Actor = Handle.Actor.Get())
	{
		FCrc::StrCrc32(*(Actor->GetPathName()), Hash);
	}
	if (UObject* ManagerInterfaceObject = Handle.ManagerInterface.GetObject())
	{
		Hash = HashCombine(Hash, GetTypeHash(ManagerInterfaceObject));
	}
	Hash = HashCombine(Hash, Handle.InstanceIndex);

	return Hash;
}

FArchive& operator<<(FArchive& Ar, FActorInstanceHandle& Handle)
{
	Ar.UsingCustomVersion(FFortniteValkyrieBranchObjectVersion::GUID);
	if (Ar.CustomVer(FFortniteValkyrieBranchObjectVersion::GUID) < FFortniteValkyrieBranchObjectVersion::ActorInstanceHandleSwitchedToInterfaces)
	{
		Ar << Handle.Actor;
		TWeakObjectPtr<AActor> Manager;
		Ar << Manager;
		ensureMsgf(Ar.IsLoading(), TEXT("We expect this piece of code to be running only while loading data."));
		Handle.ManagerInterface = FActorInstanceManagerInterface(Manager.Get());
		Ar << Handle.InstanceIndex;
		return Ar;
	}
	
	Ar << Handle.Actor;
	Ar << Handle.InstanceIndex;

	TSoftObjectPtr<UObject> SoftObject(Handle.ManagerInterface.GetObject());
	Ar << SoftObject;

	if (Ar.IsLoading())
	{
		Handle.ManagerInterface = FActorInstanceManagerInterface(SoftObject.Get());
	}

	return Ar;
}
