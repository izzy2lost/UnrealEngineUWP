// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PhysicsControlData.h"
#include "PhysicsControlLimbData.h"

#include "Interfaces/Interface_PreviewMeshProvider.h"

#include "PhysicsControlProfileAsset.generated.h"

class USkeletalMesh;


USTRUCT(BlueprintType)
struct PHYSICSCONTROL_API FPhysicsControlProfileInitialControl
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FName ParentBoneName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FName ChildBoneName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FPhysicsControlData ControlData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	TArray<FName> Sets;
};


USTRUCT(BlueprintType)
struct PHYSICSCONTROL_API FPhysicsControlProfileInitialBodyModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FName BoneName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FPhysicsControlModifierData BodyModifierData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	TArray<FName> Sets;
};

USTRUCT(BlueprintType)
struct FPhysicsControlProfileCharacterSetup
{
	GENERATED_BODY();

	UPROPERTY(EditAnywhere, Category = ControlSetup)
	TArray<FPhysicsControlLimbSetupData> LimbSetupData;

	UPROPERTY(EditAnywhere, Category = ControlSetup)
	FPhysicsControlData DefaultWorldSpaceControlData;

	UPROPERTY(EditAnywhere, Category = ControlSetup)
	FPhysicsControlData DefaultParentSpaceControlData;

	UPROPERTY(EditAnywhere, Category = ControlSetup)
	FPhysicsControlModifierData DefaultBodyModifierData;
};

/**
 * Asset for storing Physics Control Profiles. These will contain data that define:
 * - Controls and body modifiers to be created on a mesh
 * - Sets referencing those controls and body modifiers
 * - Full profiles containing settings for all the controls/modifiers
 * - Sparse profiles containing partial sets of settings for specific controls/modifiers
 * 
 * It will also be desirable to support "inheritance" - so a generic profile can be made, and then 
 * customized for certain characters or scenarios.
 */
UCLASS(BlueprintType)
class PHYSICSCONTROL_API UPhysicsControlProfileAsset : public UObject, public IInterface_PreviewMeshProvider
{
	GENERATED_BODY()
public:
	UPhysicsControlProfileAsset();

	/** Shows all the controls etc that would be made */
	UFUNCTION(CallInEditor, Category = Development)
	void Log();

	/** Sets the initial controls/modifiers based on character setup data */
	UFUNCTION(CallInEditor, Category = Development)
	void MakeControlsAndModifiersFromCharacterSetupData();

	/**
	 * We can define controls in the form of limbs etc here
	 */
	UPROPERTY(EditAnywhere, Category = Development)
	FPhysicsControlProfileCharacterSetup CharacterSetupData;

public:
	/** A profile asset to inherit from (can be null). If set, we will just add/modify data in that */
	UPROPERTY(EditAnywhere, Category = Inheritance)
	TObjectPtr<UPhysicsControlProfileAsset> ParentAsset;

	/** Additional profile assets to use for the profiles */
	UPROPERTY(EditAnywhere, Category = Inheritance)
	TArray<TObjectPtr<UPhysicsControlProfileAsset>> AdditionalProfileAssets;

public:
	/** 
	* The skeletal mesh to use for generating controls and previewing. If it turns out this 
	* doesn't need to be stored in the asset, it will get moved out of here.
	*/
	UPROPERTY(AssetRegistrySearchable, EditAnywhere, Category = PreviewMesh)
	TSoftObjectPtr<USkeletalMesh> PreviewSkeletalMesh;

	/**
	 * Additional controls. If these have the same name as one that's already created, they'll just override it.
	 */
	UPROPERTY(EditAnywhere, Category = ControlsAndBodyModifiers)
	TMap<FName, FPhysicsControlProfileInitialControl> InitialControls;

	/**
	 * Additional body modifiers. If these have the same name as one that's already created, they'll just override it.
	 */
	UPROPERTY(EditAnywhere, Category = ControlsAndBodyModifiers)
	TMap<FName, FPhysicsControlProfileInitialBodyModifier> InitialBodyModifiers;

	/**
	 * The named profiles, which are essentially control and modifier updates
	 */
	UPROPERTY(EditAnywhere, Category = Profiles)
	TMap<FName, FPhysicsControlControlAndModifierUpdates> Profiles;

public:
	/** IInterface_PreviewMeshProvider interface */
	virtual void SetPreviewMesh(USkeletalMesh* PreviewMesh, bool bMarkAsDirty = true) override;
	virtual USkeletalMesh* GetPreviewMesh() const override;
	/** END IInterface_PreviewMeshProvider interface */

#if WITH_EDITOR
/* Get name of Preview Mesh property */
	static const FName GetPreviewMeshPropertyName();
#endif

};
