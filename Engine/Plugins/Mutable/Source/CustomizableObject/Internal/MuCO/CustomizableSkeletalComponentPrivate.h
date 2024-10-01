// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CustomizableSkeletalComponentPrivate.generated.h"

class UCustomizableSkeletalComponent;
class UCustomizableObjectInstanceUsage;
class UPhysicsAsset;
class USkeletalMesh;


UCLASS(Blueprintable, BlueprintType, ClassGroup = (CustomizableObject), meta = (BlueprintSpawnableComponent))
class CUSTOMIZABLEOBJECT_API UCustomizableSkeletalComponentPrivate : public UObject
{
	GENERATED_BODY()

public:
	void CreateCustomizableObjectInstanceUsage();

	/** Common end point of all updates. Even those which failed. */
	void Callbacks() const;
	
	USkeletalMesh* GetSkeletalMesh() const;

	USkeletalMesh* GetAttachedSkeletalMesh() const;
	
	void SetSkeletalMesh(USkeletalMesh* SkeletalMesh);
	
	void SetPhysicsAsset(UPhysicsAsset* PhysicsAsset);

#if WITH_EDITOR
	void EditorUpdateComponent();
#endif

	UCustomizableSkeletalComponent* GetPublic();
	
	const UCustomizableSkeletalComponent* GetPublic() const;
	
	// Used to replace the SkeletalMesh of the parent component by the ReferenceSkeletalMesh or the generated SkeletalMesh 
	bool bPendingSetSkeletalMesh = false;

	UPROPERTY()
	bool bSkipSkipSetSkeletalMeshOnAttach = false;

	UPROPERTY(Transient)
	TObjectPtr<UCustomizableObjectInstanceUsage> CustomizableObjectInstanceUsage;
};

