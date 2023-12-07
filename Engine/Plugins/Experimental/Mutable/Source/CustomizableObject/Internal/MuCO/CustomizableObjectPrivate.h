// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

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


class FMeshCache
{
public:
	USkeletalMesh* Get(const TArray<mu::FResourceID>& Key);

	void Add(const TArray<mu::FResourceID>& Key, USkeletalMesh* Value);

private:
	TMap<TArray<mu::FResourceID>, TWeakObjectPtr<USkeletalMesh>> GeneratedMeshes;
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

	bool bModelCompiledForCook = false;
	TArray<FString> CachedPlatformNames;
#endif

	/** Cache of generated SkeletalMeshes */
	FMeshCache MeshCache;
};

