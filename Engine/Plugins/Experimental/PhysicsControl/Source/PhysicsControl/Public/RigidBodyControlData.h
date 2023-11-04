// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "PhysicsControlLimbData.h"
#include "Physics/ImmediatePhysics/ImmediatePhysicsDeclares.h"
#include "Animation/AnimTypes.h"
#include "RigidBodyControlData.generated.h"

/**
 * Note that this file defines structures that are only used by the RigidBodyWithControl node. The RBWC node will
 * also use structures defined in PhysicsControlData.h Some structures defined here may end up being shared with 
 * PhysicsControlComponent, so have a PhysicsControl prefix in anticipation of that.
 */

/**
 * Setup data for all the bodies controlled by the node. Contains info to split the skeleton up into limbs, 
 * and default control and modifier settings for each of them.
 */
USTRUCT(BlueprintType)
struct FRigidBodySetupData
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
 * Specifies a single/individual control between parent/child bodies, with the control data
 */
USTRUCT(BlueprintType)
struct PHYSICSCONTROL_API FRigidBodyControl
{
	GENERATED_BODY();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FName ParentBoneName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FName ChildBoneName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FPhysicsControlData ControlData;

	bool IsEnabled() const { return ControlData.bEnabled; }
};

/**
 * Used on creation, to allow requesting the control to be in certain sets
 */
USTRUCT(BlueprintType)
struct PHYSICSCONTROL_API FRigidBodyControlCreation
{
	GENERATED_BODY();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FRigidBodyControl Control;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	TArray<FName> Sets;
};

/**
 * Used to create a single/individual body modifier, with the modifier data,
 * and what sets it should be added to.
 */
USTRUCT(BlueprintType)
struct PHYSICSCONTROL_API FRigidBodyModifier
{
	GENERATED_BODY();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FName BoneName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FPhysicsControlModifierData ModifierData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	TArray<FName> Sets;
};

/**
 * Used on creation, to allow requesting the modifier to be in certain sets
 */
USTRUCT(BlueprintType)
struct PHYSICSCONTROL_API FRigidBodyModifierCreation
{
	GENERATED_BODY();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FRigidBodyModifier Modifier;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	TArray<FName> Sets;
};

/**
 * Collection of controls and body modifiers, used for creation
 */
USTRUCT(BlueprintType)
struct PHYSICSCONTROL_API FRigidBodyControlAndBodyModifierCreations
{
	GENERATED_BODY();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	TArray<FRigidBodyControlCreation> Controls;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	TArray<FRigidBodyModifierCreation> Modifiers;
};

PHYSICSCONTROL_API FPhysicsControlData Interpolate(
	const FPhysicsControlData& A, const FPhysicsControlData& B, const float Weight);
PHYSICSCONTROL_API FPhysicsControlSparseData Interpolate(
	const FPhysicsControlSparseData& A, const FPhysicsControlSparseData& B, const float Weight);

/**
 * Data that can be used to parameterize (modify/update) a control 
 */
USTRUCT(BlueprintType)
struct PHYSICSCONTROL_API FPhysicsControlNamedControlParameters
{
	GENERATED_BODY();

	FPhysicsControlNamedControlParameters() {}

	FPhysicsControlNamedControlParameters(FName InName, const FPhysicsControlSparseData& InData)
		: Name(InName), Data(InData) {}

	// The name of the control (or set of controls) to update
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FPhysicsControlSparseData Data;
};

PHYSICSCONTROL_API FPhysicsControlModifierData Interpolate(
	const FPhysicsControlModifierData& A, const FPhysicsControlModifierData& B, const float Weight);
PHYSICSCONTROL_API FPhysicsControlModifierSparseData Interpolate(
	const FPhysicsControlModifierSparseData& A, const FPhysicsControlModifierSparseData& B, const float Weight);

/**
 * Data that can be used to parameterize(modify / update) a control
 */
USTRUCT(BlueprintType)
struct PHYSICSCONTROL_API FPhysicsControlNamedModifierParameters
{
	GENERATED_BODY();

	FPhysicsControlNamedModifierParameters() {}

	FPhysicsControlNamedModifierParameters(FName InName, const FPhysicsControlModifierSparseData& InData)
		: Name(InName), Data(InData) {}

	// The name of the modifier (or set of modifiers) to update
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FPhysicsControlModifierSparseData Data;
};

/**
 * These apply temporary/ephemeral changes to the controls that only persist for one tick.
 */
USTRUCT(BlueprintType)
struct PHYSICSCONTROL_API FPhysicsControlControlAndModifierParameters
{
	GENERATED_BODY();

	/**
	 * Parameters for existing controls. Each name can be the name of a control, or the name of a 
	 * set of controls. They will only apply for one tick/update. They will be applied in order (so 
	 * subsequent entries will override earlier ones if they apply to the same control).
	 */ 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	TArray<FPhysicsControlNamedControlParameters> ControlParameters;
	
	/**
	 *  Parameters for existing modifiers. Each name can be the name of a modifier, or the name of a 
	 * set of modifiers. They will only apply for one tick/update.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	TArray<FPhysicsControlNamedModifierParameters> ModifierParameters;

	void Add(const FPhysicsControlNamedControlParameters& InParameters) { ControlParameters.Add(InParameters); }
	void Add(const FPhysicsControlNamedModifierParameters& InParameters) { ModifierParameters.Add(InParameters); }
};

/**
 * These apply permanent changes to the controls and modifiers, allowing all the settings to be changed
 * (apart from the actual bodies that are being controlled/affected)
 */
USTRUCT(BlueprintType)
struct PHYSICSCONTROL_API FPhysicsControlControlAndModifierUpdates
{
	GENERATED_BODY();

	/** Modifications to the underlying controls - these will persist */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	TArray<FPhysicsControlNamedControlParameters> ControlParameters;

	/** Modifications to the underlying modifiers - these will persist */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	TArray<FPhysicsControlNamedModifierParameters> ModifierParameters;
};

/**
 * A single target for a control, which may be defined as an offset from the (implicit) animation target.
 */
USTRUCT(BlueprintType)
struct PHYSICSCONTROL_API FRigidBodyControlTarget
{
	GENERATED_BODY();

	FRigidBodyControlTarget()
		: TargetPosition(ForceInitToZero)
		, TargetOrientation(ForceInitToZero)
		, TargetPoint(ForceInitToZero)
		, bUseSkeletalAnimation(true)
		, bUseTargetPoint(false)
	{
	}

	/** The target position of the child body, relative to the parent body */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FVector TargetPosition;

	/** The target orientation of the child body, relative to the parent body */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FRotator TargetOrientation;

	/** 
	 * The point on the controlled (child) object that should be driven towards the target position.
	 * Note that if this is not set (i.e. if UseTargetPoint is false) then the centre of mass will be used. 
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (editcondition = "bUseTargetPoint"))
	FVector TargetPoint;

	/** If true then the target will be applied on top of the skeletal animation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	uint8 bUseSkeletalAnimation : 1;

	/** If true then the target will be applied on top of the skeletal animation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	uint8 bUseTargetPoint : 1;
};

/**
 * A set of targets for controls
 */
USTRUCT(BlueprintType)
struct PHYSICSCONTROL_API FRigidBodyControlTargets
{
	GENERATED_BODY();

	/** Targets to apply to the named control */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	TMap<FName, FRigidBodyControlTarget> Targets;
};

/**
 * A single kinematic target, which may be defined as an offset from the (implicit) animation target.
 */
USTRUCT(BlueprintType)
struct PHYSICSCONTROL_API FRigidBodyKinematicTarget
{
	GENERATED_BODY();

	FRigidBodyKinematicTarget()
		: TargetPosition(ForceInitToZero)
		, TargetOrientation(ForceInitToZero)
		, bUseSkeletalAnimation(true)
	{
	}

	/** The target position of the body */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FVector TargetPosition;

	/** The target orientation of the body */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FRotator TargetOrientation;

	/** If true then the target will be applied on top of the skeletal animation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	uint8 bUseSkeletalAnimation : 1;
};

/**
 * A set of kinematic targets
 */
USTRUCT(BlueprintType)
struct PHYSICSCONTROL_API FRigidBodyKinematicTargets
{
	GENERATED_BODY();

	/** Targets to apply to the named body modifier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	TMap<FName, FRigidBodyKinematicTarget> Targets;
};

/**
 * This is the record for a control. It stores the "original" data, which will normally stay fixed.
 * In addition, it will also store some updates to these original data which will determine how the actual
 * controls get used.
 */
struct FRigidBodyControlRecord
{
	FRigidBodyControlRecord(const FRigidBodyControl& InControl, ImmediatePhysics::FJointHandle* InJointHandle);

	void ResetCurrent(bool bResetTarget);

	// Note that this is only correct when called during or after the update has been done
	bool IsEnabled() const { return CurrentData.bEnabled; }

	// TODO - might benefit from smaller - non-blueprint data members here ?
	FRigidBodyControl               Control;
	ImmediatePhysics::FJointHandle* JointHandle;

	// This contains the currently active control data. It will be updated just prior to
	// applying the controls, by setting it to the default, and then updating it with any parameters.
	FPhysicsControlData CurrentData;

	// Contains any control target that has been set
	FRigidBodyControlTarget ControlTarget;

	// The previous control target. This will have been set at the end of a previous update (but
	// only if the control was enabled etc), so to check if it is valid, check the update counter.
	FTransform PrevTargetTM;

	// Update counter set when the control was last updated.
	// TODO just store the count we're interested in rather than the whole structure
	FGraphTraversalCounter ExpectedUpdateCounter;
};

/**
 * This is the record for a modifier. It stores the "original" data, which will normally stay fixed.
 * In addition, it will also store some updates to these original data which will determine how the actual
 * body properties get set.
 */
struct FRigidBodyModifierRecord
{
	FRigidBodyModifierRecord(const FRigidBodyModifier& InModifier, ImmediatePhysics::FActorHandle* InActorHandle);

	void ResetCurrent();

	// TODO - might benefit from smaller - non-blueprint data members here ?
	FRigidBodyModifier              Modifier;
	ImmediatePhysics::FActorHandle* ActorHandle;

	// This contains the currently active modifier data. It will be updated just prior to
	// applying the controls, by setting it to the default, and then updating it with any parameters.
	FPhysicsControlModifierData CurrentData;
};


