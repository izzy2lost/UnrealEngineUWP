// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/WeakInterfacePtr.h"
#include "ActorInstanceHandle.generated.h"

class USceneComponent;
class AActor;
class UActorInstanceManager;
class IActorInstanceManagerInterface;
class ULevel;

using FActorInstanceManagerInterface = TWeakInterfacePtr<IActorInstanceManagerInterface>;

// Handle to a unique object. This may specify a full weigh actor or it may only specify the actor instance that represents the same object.
USTRUCT(BlueprintType)
struct FActorInstanceHandle
{
	GENERATED_BODY()

	ENGINE_API FActorInstanceHandle() = default;

	ENGINE_API explicit FActorInstanceHandle(AActor* InActor);
	ENGINE_API FActorInstanceHandle(const UPrimitiveComponent* RelevantComponent, int32 CollisionInstanceIndex);
	ENGINE_API FActorInstanceHandle(AActor* InActor, const UPrimitiveComponent* RelevantComponent, int32 CollisionInstanceIndex);
	ENGINE_API FActorInstanceHandle(FActorInstanceManagerInterface InManagerInterface, int32 InstanceIndex);
	ENGINE_API FActorInstanceHandle(const FActorInstanceHandle& Other);

	/** 
	 * A path dedicated to creation of handles while converting actor to a dehydrated representation. This path ensures
	 * an actor won't be spawned as a side effect of looking for the actor given Manager/Index represents
	 */
	static FActorInstanceHandle MakeDehydratedActorHandle(UObject& Manager, int32 InInstanceIndex);

	ENGINE_API bool IsValid() const;

	ENGINE_API bool DoesRepresentClass(const UClass* OtherClass) const;

	template<typename T>
	bool DoesRepresent() const;

	ENGINE_API UClass* GetRepresentedClass() const;
	ENGINE_API ULevel* GetLevel() const;
	ENGINE_API FVector GetLocation() const;
	ENGINE_API FRotator GetRotation() const;
	ENGINE_API FTransform GetTransform() const;

	ENGINE_API FName GetFName() const;
	ENGINE_API FString GetName() const;

	/** If this handle has a valid actor, return it; otherwise return the actor responsible for managing the instances. */
	ENGINE_API AActor* GetManagingActor() const;

	/** Returns either the actor's root component or the root component for the manager associated with the handle */
	ENGINE_API USceneComponent* GetRootComponent() const;

	/** Returns the actor specified by this handle. This may require loading and creating the actor object. */
	ENGINE_API AActor* FetchActor() const;

	template <typename T>
	T* FetchActor() const;

	AActor* GetCachedActor() const { return Actor.Get(); }
	ENGINE_API void SetCachedActor(AActor* InActor) const;

	/* Returns the index used internally by the manager */
	FORCEINLINE int32 GetInstanceIndex() const { return InstanceIndex; }

	FActorInstanceHandle& operator=(const FActorInstanceHandle& Other) = default;
	FActorInstanceHandle& operator=(FActorInstanceHandle&& Other) = default;
	ENGINE_API FActorInstanceHandle& operator=(AActor* OtherActor);

	ENGINE_API bool operator==(const FActorInstanceHandle& Other) const;
	ENGINE_API bool operator!=(const FActorInstanceHandle& Other) const;

	ENGINE_API bool operator==(const AActor* OtherActor) const;
	ENGINE_API bool operator!=(const AActor* OtherActor) const;

	friend ENGINE_API uint32 GetTypeHash(const FActorInstanceHandle& Handle);

	friend ENGINE_API FArchive& operator<<(FArchive& Ar, FActorInstanceHandle& Handle);

	FActorInstanceManagerInterface GetManagerInterface() const { return ManagerInterface; }

	template<typename T>
	T* GetManager() const 
	{
		return Cast<T>(ManagerInterface.GetObject());
	}

private:
	friend struct FActorInstanceHandleInternalHelper;

	/**
	 * helper functions that let us treat the actor pointer as a UObject in templated functions
	 * these do NOT fetch the actor so they will return nullptr if we don't have a full actor representation
	 */
	ENGINE_API UObject* GetActorAsUObject();
	ENGINE_API const UObject* GetActorAsUObject() const;

	/** Returns true if Actor is not null and not pending kill */
	ENGINE_API bool IsActorValid() const;

	/** this is cached here for convenience */
	UPROPERTY()
	mutable TWeakObjectPtr<AActor> Actor;

	/** Identifies the actor instance manager to use */
	FActorInstanceManagerInterface ManagerInterface;

	/** Identifies the instance within the manager */
	int32 InstanceIndex = INDEX_NONE;
};

template<typename T>
bool FActorInstanceHandle::DoesRepresent() const
{
	if (const UClass* RepresentedClass = GetRepresentedClass())
	{
		if constexpr (TIsIInterface<T>::Value)
		{
			return RepresentedClass->ImplementsInterface(T::UClassType::StaticClass());
		}
		else
		{
			return RepresentedClass->IsChildOf(T::StaticClass());
		}
	}
	return false;
}

template <typename T>
T* FActorInstanceHandle::FetchActor() const
{
	if (DoesRepresent<T>())
	{
		return CastChecked<T>(FetchActor(), ECastCheckedType::NullAllowed);
	}
	return nullptr;
}
