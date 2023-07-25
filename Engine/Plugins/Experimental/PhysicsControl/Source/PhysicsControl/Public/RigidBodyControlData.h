// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Physics/ImmediatePhysics/ImmediatePhysicsDeclares.h"
#include "Animation/AnimTypes.h"
#include "RigidBodyControlData.generated.h"

/**
 * Setup data that is used to create the representation of a single limb. A limb is an array of 
 * contiguous bones (e.g. left arm, or the spine etc). We can define it has the set of bones that
 * are children of a start bone (plus the start bone itself), plus optionally the parent of that start 
 * bone (this is useful when defining the spine, since you will want to include the pelvis, but you 
 * don't to include all the children of the pelvis since that would include the legs), but excluding 
 * any bones that are already part of another limb. This implies limbs should be constructed in order
 * from leaf to root.
 */
USTRUCT(BlueprintType)
struct PHYSICSCONTROL_API FRigidBodyLimbSetupData
{
	GENERATED_BODY();

	FRigidBodyLimbSetupData()
		: bIncludeParentBone(false), bCreateWorldSpaceControls(true)
		, bCreateParentSpaceControls(true), bCreateBodyModifiers(true)
	{}

	/** The name of the limb that this will be used to create */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RigidBodyControl)
	FName LimbName;

	/* 
	 * Normally the root-most bone of the limb (e.g. left clavicle when defining the left arm) - so the
	 * limb will contain children of this bone (plus this bone itself).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RigidBodyControl)
	FName StartBone;

	/* 
	 * Whether or not to include the parent of the start bone. This is intended to be used for limbs like 
	 * the spine, where you would set StartBone = spine_01 but also expect to include the pelvis (parent 
	 * of spine_01) in the spine limb.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RigidBodyControl)
	uint8 bIncludeParentBone : 1;

	/** Whether to create-world space controls for this limb */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RigidBodyControl)
	uint8 bCreateWorldSpaceControls : 1;

	/** Whether to create parent-space controls for this limb */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RigidBodyControl)
	uint8 bCreateParentSpaceControls : 1;

	/** Whether to create body modifiers for this limb */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RigidBodyControl)
	uint8 bCreateBodyModifiers : 1;
};

/**
 * Helper structure when creating the limb setup
 */
struct FRigidBodyLimbBones
{
	FRigidBodyLimbBones() 
		: bFirstBoneIsAdditional(false), bCreateWorldSpaceControls(true)
		, bCreateParentSpaceControls(true), bCreateBodyModifiers(true)
	{}

	/** The names of the bones in the limb */
	TArray<FName> BoneNames;

	/** Indicates if the first bone in this limb was included due to "IncludeParentBone". */
	uint8 bFirstBoneIsAdditional : 1;

	uint8 bCreateWorldSpaceControls : 1;
	uint8 bCreateParentSpaceControls : 1;
	uint8 bCreateBodyModifiers : 1;
};

/**
 * Used by Body Modifiers to specify how the physical bodies should move.
 */
UENUM(BlueprintType)
enum class ERigidBodyMovementType : uint8
{
	// Kinematic means that the object won't be simulated, but will be moved according to the
	// kinematic target set in the Body Modifier.
	Kinematic,
	// Simulated means that the object will be controlled by the physics solver
	Simulated
};
FName GetControlTypeName(const ERigidBodyMovementType MovementType);

/**
 * Analogous to the ControlData, this indicates how an individual controlled body should move, with flags indicating
 * whether each element should get used.
 */
USTRUCT(BlueprintType)
struct PHYSICSCONTROL_API FRigidBodyModifierSparseData
{
	GENERATED_BODY();

	FRigidBodyModifierSparseData(
		const ERigidBodyMovementType InMovementType = ERigidBodyMovementType::Simulated,
		const float                  InGravityMultiplier = 1.0f)
		: MovementType(InMovementType)
		, GravityMultiplier(InGravityMultiplier)
		, bEnableMovementType(1)
		, bEnableGravityMultiplier(1)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (editcondition = "bEnableMovementType"))
	ERigidBodyMovementType MovementType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (editcondition = "bEnableGravityMultiplier"))
	float GravityMultiplier;

	UPROPERTY(EditAnywhere, Category = PhysicsControl, meta = (InlineEditConditionToggle))
	uint8 bEnableMovementType : 1;

	UPROPERTY(EditAnywhere, Category = PhysicsControl, meta = (InlineEditConditionToggle))
	uint8 bEnableGravityMultiplier : 1;
};

/**
 * Analogous to the ControlData, this indicates how an individual controlled body should move
 */
USTRUCT(BlueprintType)
struct PHYSICSCONTROL_API FRigidBodyModifierData
{
	GENERATED_BODY();

	FRigidBodyModifierData(
		const ERigidBodyMovementType InMovementType = ERigidBodyMovementType::Simulated,
		const float                  InGravityMultiplier = 1.0f)
		: MovementType(InMovementType)
		, GravityMultiplier(InGravityMultiplier)
	{
	}

	void UpdateFromSparseData(const FRigidBodyModifierSparseData& SparseData);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	ERigidBodyMovementType MovementType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	float GravityMultiplier;
};

/**
 * Specifies the type of control that is created when making controls from a skeleton or a set of limbs.
 * Note that if controls are made individually then other options are available - i.e. in a character,
 * any body part can be controlled relative to any other part, or indeed any other object.
 */
UENUM(BlueprintType)
enum class ERigidBodyControlType : uint8
{
	/** Control is done in world space, so each object/part is driven independently */
	WorldSpace,
	/** Control is done in the space of the parent of each object */
	ParentSpace,
};
FName GetControlTypeName(const ERigidBodyControlType ControlType);

/**
 * Strength and damping etc parameters that will affect a control, with flags indicating
 * whether each element should get used.
 */
USTRUCT(BlueprintType)
struct PHYSICSCONTROL_API FRigidBodyControlSparseData
{
	GENERATED_BODY();

	FRigidBodyControlSparseData(
		float InLinearStrength = 0.0f, float InLinearDampingRatio = 1.0f, float InLinearExtraDamping = 0.0f,
		float InAngularStrength = 0.0f, float InAngularDampingRatio = 1.0f, float InAngularExtraDamping = 0.0f,
		float InLinearTargetVelocityMultiplier = 1.0f, float InAngularTargetVelocityMultiplier = 1.0f,
		bool bInEnabled = true)
		: LinearStrength(InLinearStrength)
		, LinearDampingRatio(InLinearDampingRatio)
		, LinearExtraDamping(InLinearExtraDamping)
		, AngularStrength(InAngularStrength)
		, AngularDampingRatio(InAngularDampingRatio)
		, AngularExtraDamping(InAngularExtraDamping)
		, LinearTargetVelocityMultiplier(InLinearTargetVelocityMultiplier)
		, AngularTargetVelocityMultiplier(InAngularTargetVelocityMultiplier)
		, bEnabled(bInEnabled)
		, bEnableLinearStrength(true)
		, bEnableLinearDampingRatio(true)
		, bEnableLinearExtraDamping(true)
		, bEnableAngularStrength(true)
		, bEnableAngularDampingRatio(true)
		, bEnableAngularExtraDamping(true)
		, bEnableLinearTargetVelocityMultiplier(true)
		, bEnableAngularTargetVelocityMultiplier(true)
		, bEnablebEnabled(true)
	{
	}

	/** The strength used to drive linear motion */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (ClampMin = "0.0", editcondition = "bEnableLinearStrength"))
	float LinearStrength;

	/** 
	 * The amount of damping associated with the linear strength. A value of 1 Results in critically 
	 * damped motion where the control drives as quickly as possible to the target without overshooting. 
	 * Values > 1 result in more damped motion, and values below 1 result in faster, but more "wobbly" motion.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (ClampMin = "0.0", editcondition = "bEnableLinearDampingRatio"))
	float LinearDampingRatio;

	/** 
	 * The amount of additional linear damping. This is added to the damping that comes from LinearDampingRatio
	 * and can be useful when you want damping even when LinearStrength is zero.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (ClampMin = "0.0", editcondition = "bEnableLinearExtraDamping"))
	float LinearExtraDamping;

	/** The strength used to drive angular motion */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (ClampMin = "0.0", editcondition = "bEnableAngularStrength"))
	float AngularStrength;

	/** 
	 * The amount of damping associated with the angular strength. A value of 1 Results in critically 
	 * damped motion where the control drives as quickly as possible to the target without overshooting. 
	 * Values > 1 result in more damped motion, and values below 1 result in faster, but more "wobbly" motion.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (ClampMin = "0.0", editcondition = "bEnableAngularDampingRatio"))
	float AngularDampingRatio;

	/** 
	 * The amount of additional angular damping. This is added to the damping that comes from AngularDampingRatio
	 * and can be useful when you want damping even when AngularStrength is zero.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (ClampMin = "0.0", editcondition = "bEnableAngularExtraDamping"))
	float AngularExtraDamping;

	/**
	 * Multiplier on the velocity, which gets applied to the damping. A value of 1 means the animation target
	 * velocity is used, which helps it track the animation. A value of 0 means damping happens in "world space" 
	 * - so damping acts like drag on the movement.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (ClampMin = "0.0", editcondition = "bEnableLinearTargetVelocityMultiplier"))
	float LinearTargetVelocityMultiplier;

	/**
	 * Multiplier on the angular velocity, which gets applied to the damping. A value of 1 means the animation target
	 * velocity is used, which helps it track the animation. A value of 0 means damping happens in "world space" 
	 * - so damping acts like drag on the movement.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (ClampMin = "0.0", editcondition = "bEnableAngularTargetVelocityMultiplier"))
	float AngularTargetVelocityMultiplier;

	/**
	 * Whether this control should be enabled
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (editcondition = "bEnablebEnabled"))
	uint8 bEnabled : 1;

	UPROPERTY(EditAnywhere, Category = PhysicsControl, meta = (InlineEditConditionToggle))
	uint8 bEnableLinearStrength : 1;

	UPROPERTY(EditAnywhere, Category = PhysicsControl, meta = (InlineEditConditionToggle))
	uint8 bEnableLinearDampingRatio : 1;

	UPROPERTY(EditAnywhere, Category = PhysicsControl, meta = (InlineEditConditionToggle))
	uint8 bEnableLinearExtraDamping : 1;

	UPROPERTY(EditAnywhere, Category = PhysicsControl, meta = (InlineEditConditionToggle))
	uint8 bEnableAngularStrength : 1;

	UPROPERTY(EditAnywhere, Category = PhysicsControl, meta = (InlineEditConditionToggle))
	uint8 bEnableAngularDampingRatio : 1;

	UPROPERTY(EditAnywhere, Category = PhysicsControl, meta = (InlineEditConditionToggle))
	uint8 bEnableAngularExtraDamping : 1;

	UPROPERTY(EditAnywhere, Category = PhysicsControl, meta = (InlineEditConditionToggle))
	uint8 bEnableLinearTargetVelocityMultiplier : 1;

	UPROPERTY(EditAnywhere, Category = PhysicsControl, meta = (InlineEditConditionToggle))
	uint8 bEnableAngularTargetVelocityMultiplier : 1;

	UPROPERTY(EditAnywhere, Category = PhysicsControl, meta = (InlineEditConditionToggle))
	uint8 bEnablebEnabled : 1;
};



/**
 * Strength and damping etc parameters that will affect a control.
 */
USTRUCT(BlueprintType)
struct PHYSICSCONTROL_API FRigidBodyControlData
{
	GENERATED_BODY();

	FRigidBodyControlData(
		float InLinearStrength = 0.0f, float InLinearDampingRatio = 1.0f, float InLinearExtraDamping = 0.0f,
		float InAngularStrength = 0.0f, float InAngularDampingRatio = 1.0f, float InAngularExtraDamping = 0.0f,
		float InLinearTargetVelocityMultiplier = 1.0f, float InAngularTargetVelocityMultiplier = 1.0f,
		bool bInEnabled = true)
		: LinearStrength(InLinearStrength)
		, LinearDampingRatio(InLinearDampingRatio)
		, LinearExtraDamping(InLinearExtraDamping)
		, AngularStrength(InAngularStrength)
		, AngularDampingRatio(InAngularDampingRatio)
		, AngularExtraDamping(InAngularExtraDamping)
		, LinearTargetVelocityMultiplier(InLinearTargetVelocityMultiplier)
		, AngularTargetVelocityMultiplier(InAngularTargetVelocityMultiplier)
		, bEnabled(bInEnabled)
	{
	}

	void UpdateFromSparseData(const FRigidBodyControlSparseData& SparseData);

	/** The strength used to drive linear motion */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (ClampMin = "0.0"))
	float LinearStrength;

	/** 
	 * The amount of damping associated with the linear strength. A value of 1 Results in critically 
	 * damped motion where the control drives as quickly as possible to the target without overshooting. 
	 * Values > 1 result in more damped motion, and values below 1 result in faster, but more "wobbly" motion.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (ClampMin = "0.0"))
	float LinearDampingRatio;

	/** 
	 * The amount of additional linear damping. This is added to the damping that comes from LinearDampingRatio
	 * and can be useful when you want damping even when LinearStrength is zero.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (ClampMin = "0.0"))
	float LinearExtraDamping;

	/** The strength used to drive angular motion */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (ClampMin = "0.0"))
	float AngularStrength;

	/** 
	 * The amount of damping associated with the angular strength. A value of 1 Results in critically 
	 * damped motion where the control drives as quickly as possible to the target without overshooting. 
	 * Values > 1 result in more damped motion, and values below 1 result in faster, but more "wobbly" motion.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (ClampMin = "0.0"))
	float AngularDampingRatio;

	/** 
	 * The amount of additional angular damping. This is added to the damping that comes from AngularDampingRatio
	 * and can be useful when you want damping even when AngularStrength is zero.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (ClampMin = "0.0"))
	float AngularExtraDamping;

	/**
	 * Multiplier on the velocity, which gets applied to the damping. A value of 1 means the animation target
	 * velocity is used, which helps it track the animation. A value of 0 means damping happens in "world space" 
	 * - so damping acts like drag on the movement.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (ClampMin = "0.0"))
	float LinearTargetVelocityMultiplier;

	/**
	 * Multiplier on the angular velocity, which gets applied to the damping. A value of 1 means the animation target
	 * velocity is used, which helps it track the animation. A value of 0 means damping happens in "world space" 
	 * - so damping acts like drag on the movement.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (ClampMin = "0.0"))
	float AngularTargetVelocityMultiplier;

	/**
	 * Whether this control should be enabled
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	uint8 bEnabled : 1;
};

/**
 * Setup data for all the bodies controlled by the node. Contains info to split the skeleton up into limbs, 
 * and default control and modifier settings for each of them.
 */
USTRUCT(BlueprintType)
struct FRigidBodySetupData
{
	GENERATED_BODY();

	UPROPERTY(EditAnywhere, Category = ControlSetup)
	TArray<FRigidBodyLimbSetupData> LimbSetupData;

	UPROPERTY(EditAnywhere, Category = ControlSetup)
	FRigidBodyControlData DefaultWorldSpaceControlData;

	UPROPERTY(EditAnywhere, Category = ControlSetup)
	FRigidBodyControlData DefaultParentSpaceControlData;

	UPROPERTY(EditAnywhere, Category = ControlSetup)
	FRigidBodyModifierData DefaultBodyModifierData;
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
	FRigidBodyControlData ControlData;

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
	FRigidBodyModifierData ModifierData;

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

/**
 * Update an existing set, or add to it
 */
USTRUCT(BlueprintType)
struct PHYSICSCONTROL_API FSetUpdate
{
	GENERATED_BODY();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FName SetName;

	/** The names of either controls or body modifiers (depending on context), or sets of controls/body modifiers */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	TArray<FName> Names;
};

/**
 * Combines updates for control and modifier sets
 */
USTRUCT(BlueprintType)
struct PHYSICSCONTROL_API FRigidBodySetUpdates
{
	GENERATED_BODY();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	TArray<FSetUpdate> ControlSetUpdates;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	TArray<FSetUpdate> ModifierSetUpdates;
};

FRigidBodyControlData Interpolate(
	const FRigidBodyControlData& A, const FRigidBodyControlData& B, const float Weight);
FRigidBodyControlSparseData Interpolate(
	const FRigidBodyControlSparseData& A, const FRigidBodyControlSparseData& B, const float Weight);

/**
 * Data that can be used to parameterize (modify/update) a control 
 */
USTRUCT(BlueprintType)
struct PHYSICSCONTROL_API FRigidBodyNamedControlParameters
{
	GENERATED_BODY();

	FRigidBodyNamedControlParameters() {}

	FRigidBodyNamedControlParameters(FName InName, const FRigidBodyControlSparseData& InData) 
		: Name(InName), Data(InData) {}

	// The name of the control (or set of controls) to update
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FRigidBodyControlSparseData Data;
};

FRigidBodyModifierData Interpolate(
	const FRigidBodyModifierData& A, const FRigidBodyModifierData& B, const float Weight);
FRigidBodyModifierSparseData Interpolate(
	const FRigidBodyModifierSparseData& A, const FRigidBodyModifierSparseData& B, const float Weight);

/**
 * Data that can be used to parameterize(modify / update) a control
 */
USTRUCT(BlueprintType)
struct PHYSICSCONTROL_API FRigidBodyNamedModifierParameters
{
	GENERATED_BODY();

	FRigidBodyNamedModifierParameters() {}

	FRigidBodyNamedModifierParameters(FName InName, const FRigidBodyModifierSparseData& InData)
		: Name(InName), Data(InData) {}

	// The name of the modifier (or set of modifiers) to update
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FRigidBodyModifierSparseData Data;
};

/**
 * These apply temporary/ephemeral changes to the controls that only persist for one tick.
 */
USTRUCT(BlueprintType)
struct PHYSICSCONTROL_API FRigidBodyControlAndModifierParameters
{
	GENERATED_BODY();

	/**
	 * Parameters for existing controls. Each name can be the name of a control, or the name of a 
	 * set of controls. They will only apply for one tick/update. They will be applied in order (so 
	 * subsequent entries will override earlier ones if they apply to the same control).
	 */ 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	TArray<FRigidBodyNamedControlParameters> ControlParameters;
	
	/**
	 *  Parameters for existing modifiers. Each name can be the name of a modifier, or the name of a 
	 * set of modifiers. They will only apply for one tick/update.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	TArray<FRigidBodyNamedModifierParameters> ModifierParameters;

	void Add(const FRigidBodyNamedControlParameters& InParameters) { ControlParameters.Add(InParameters); }
	void Add(const FRigidBodyNamedModifierParameters& InParameters) { ModifierParameters.Add(InParameters); }
};

/**
 * These apply permanent changes to the controls and modifiers, allowing all the settings to be changed
 * (apart from the actual bodies that are being controlled/affected)
 */
USTRUCT(BlueprintType)
struct PHYSICSCONTROL_API FRigidBodyControlAndModifierUpdates
{
	GENERATED_BODY();

	/** Modifications to the underlying controls - these will persist */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	TArray<FRigidBodyNamedControlParameters> ControlParameters;

	/** Modifications to the underlying modifiers - these will persist */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	TArray<FRigidBodyNamedModifierParameters> ModifierParameters;
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
struct FControlRecord
{
	FControlRecord(const FRigidBodyControl& InControl, ImmediatePhysics::FJointHandle* InJointHandle);

	void ResetCurrent(bool bResetTarget);

	// Note that this is only correct when called during or after the update has been done
	bool IsEnabled() const { return CurrentData.bEnabled; }

	// TODO - might benefit from smaller - non-blueprint data members here ?
	FRigidBodyControl               Control;
	ImmediatePhysics::FJointHandle* JointHandle;

	// This contains the currently active control data. It will be updated just prior to
	// applying the controls, by setting it to the default, and then updating it with any parameters.
	FRigidBodyControlData CurrentData;

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
struct FBodyModifierRecord
{
	FBodyModifierRecord(const FRigidBodyModifier& InModifier, ImmediatePhysics::FActorHandle* InActorHandle);

	void ResetCurrent();

	// TODO - might benefit from smaller - non-blueprint data members here ?
	FRigidBodyModifier              Modifier;
	ImmediatePhysics::FActorHandle* ActorHandle;

	// This contains the currently active modifier data. It will be updated just prior to
	// applying the controls, by setting it to the default, and then updating it with any parameters.
	FRigidBodyModifierData CurrentData;
};


