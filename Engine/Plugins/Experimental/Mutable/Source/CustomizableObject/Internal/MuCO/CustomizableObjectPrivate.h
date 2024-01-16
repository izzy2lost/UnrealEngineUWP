// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCO/StateMachine.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"
#include "MuR/Types.h"

#if WITH_EDITOR
#include "Misc/Guid.h"
#endif

#include "CustomizableObjectPrivate.generated.h"

namespace mu { class Model; }
class UCustomizableObject;
class USkeletalMesh;
class USkeleton;


class FMeshCache
{
public:
	USkeletalMesh* Get(const TArray<mu::FResourceID>& Key);

	void Add(const TArray<mu::FResourceID>& Key, USkeletalMesh* Value);

private:
	TMap<TArray<mu::FResourceID>, TWeakObjectPtr<USkeletalMesh>> GeneratedMeshes;
};

struct FCustomizableObjectStatusTypes
{
	enum class EState : uint8
	{
		Loading = 0, // Waiting for PostLoad and Asset Registry to finish.
		ModelLoaded, // Model loaded correctly.
		NoModel, // No model (due to no model not found and automatic compilations disabled).
		// Compiling, // Compiling the CO.

		Count,
	};
	
	static constexpr EState StartState = EState::Loading;

	static constexpr bool ValidTransitions[3][3] =
	{
		// TO
		// Loading, ModelLoaded, NoModel // FROM
		{false,   true,        true},  // Loading
		{false,   true,        true},  // ModelLoaded
		{false,   true,        true},  // NoModel
	};
};


using FCustomizableObjectStatus = FStateMachine<FCustomizableObjectStatusTypes>;


class FSkeletonCache
{
public:
	USkeleton* Get(const TArray<uint16>& Key);

	void Add(const TArray<uint16>& Key, USkeleton* Value);

private:
	TMap<TArray<uint16>, TWeakObjectPtr<USkeleton>> MergedSkeletons;
};


UCLASS()
class UCustomizableObjectPrivate : public UObject
{
	GENERATED_BODY()
	
	TSharedPtr<mu::Model, ESPMode::ThreadSafe> MutableModel;

public:
	void SetModel(const TSharedPtr<mu::Model, ESPMode::ThreadSafe>& Model, const FGuid Identifier);
	const TSharedPtr<mu::Model, ESPMode::ThreadSafe>& GetModel();
	TSharedPtr<const mu::Model, ESPMode::ThreadSafe> GetModel() const;

#if WITH_EDITORONLY_DATA
	/** See UCustomizableObject::ParticipatingObjects. */
	CUSTOMIZABLEOBJECT_API TMap<TObjectPtr<const UObject>, FGuid>& GetParticipatingObjects(UCustomizableObject& Public);
#endif
	
	// See UCustomizableObjectSystem::LockObject. Must only be modified from the game thread
	bool bLocked = false;

#if WITH_EDITOR
	FGuid Identifier;

	TArray<FString> CachedPlatformNames;

	/** List of external packages that if changed, a compilation is required.
	 * Key is the package name. Value is the the UPackage::Guid, which is regenerated each time the packages is saved.
	 *
	 * Updated each time the CO is compiled and saved in the Derived Data. */
	TMap<FName, FGuid> ParticipatingObjects;
#endif

	/** Cache of generated SkeletalMeshes */
	FMeshCache MeshCache;

	/** Cache of merged Skeletons */
	FSkeletonCache SkeletonCache;

	FCustomizableObjectStatus Status;
};

