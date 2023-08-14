// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"
#include "Components/ActorComponent.h"
#include "EngineDefines.h"
#include "PhysicsControlData.generated.h"

/**
 * Note that this file defines structures that mostly are, or could be, shared between the PhysicsControlComponent
 * and the RigidBodyWithControl node.
 */

class UMeshComponent;

/**
 * Used by Body Modifiers to specify how the physical bodies should move.  
 */
UENUM(BlueprintType)
enum class EPhysicsMovementType : uint8
{
	// Static means that the object won't be simulated, and it won't be moved according to the
	// kinematic target set in the Body Modifier (though something else might move it)
	Static,
	// Kinematic means that the object won't be simulated, but will be moved according to the
	// kinematic target set in the Body Modifier.
	Kinematic,
	// Simulated means that the object will be controlled by the physics solver
	Simulated
};

inline FName GetPhysicsMovementTypeName(const EPhysicsMovementType MovementType)
{
	switch (MovementType)
	{
	case EPhysicsMovementType::Static:
		return "Static";
	case EPhysicsMovementType::Kinematic:
		return "Kinematic";
	case EPhysicsMovementType::Simulated:
		return "Simulated";
	}
	return "None";
}

/**
 * Specifies the type of control that is created when making controls from a skeleton or a set of limbs. 
 * Note that if controls are made individually then other options are available - i.e. in a character, 
 * any body part can be controlled relative to any other part, or indeed any other object.
 */
UENUM(BlueprintType)
enum class EPhysicsControlType : uint8
{
	/** Control is done in world space, so each object/part is driven independently */
	WorldSpace,
	/** Control is done in the space of the parent of each object */
	ParentSpace,
};

inline FName GetPhysicsControlTypeName(const EPhysicsControlType ControlType)
{
	switch (ControlType)
	{
	case EPhysicsControlType::WorldSpace:
		return "WorldSpace";
	case EPhysicsControlType::ParentSpace:
		return "ParentSpace";
	}
	return "None";
}

/**
 * Update an existing set, or add to it
 */
USTRUCT(BlueprintType)
struct PHYSICSCONTROL_API FPhysicsControlSetUpdate
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
struct PHYSICSCONTROL_API FPhysicsControlSetUpdates
{
	GENERATED_BODY();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	TArray<FPhysicsControlSetUpdate> ControlSetUpdates;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	TArray<FPhysicsControlSetUpdate> ModifierSetUpdates;
};

/**
 * Analogous to the ControlData, this indicates how an individual controlled body should move, with flags indicating
 * whether each element should get used.
 */
USTRUCT(BlueprintType)
struct PHYSICSCONTROL_API FPhysicsControlModifierSparseData
{
	GENERATED_BODY();

	FPhysicsControlModifierSparseData(
		const EPhysicsMovementType InMovementType = EPhysicsMovementType::Simulated,
		const float                InGravityMultiplier = 1.0f)
		: MovementType(InMovementType)
		, GravityMultiplier(InGravityMultiplier)
		, bEnableMovementType(1)
		, bEnableGravityMultiplier(1)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (editcondition = "bEnableMovementType"))
	EPhysicsMovementType MovementType;

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
struct PHYSICSCONTROL_API FPhysicsControlModifierData
{
	GENERATED_BODY();

	FPhysicsControlModifierData(
		const EPhysicsMovementType InMovementType = EPhysicsMovementType::Simulated,
		const float                InGravityMultiplier = 1.0f)
		: MovementType(InMovementType)
		, GravityMultiplier(InGravityMultiplier)
	{
	}

	void UpdateFromSparseData(const FPhysicsControlModifierSparseData& SparseData);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	EPhysicsMovementType MovementType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	float GravityMultiplier;
};


/**
 * Strength and damping etc parameters that will affect a control, with flags indicating
 * whether each element should get used.
 */
USTRUCT(BlueprintType)
struct PHYSICSCONTROL_API FPhysicsControlSparseData
{
	GENERATED_BODY();

	FPhysicsControlSparseData(
		float InLinearStrength = 0.0f, float InLinearDampingRatio = 1.0f, 
		float InLinearExtraDamping = 0.0f, float InMaxForce = 0.0f,
		float InAngularStrength = 0.0f, float InAngularDampingRatio = 1.0f, 
		float InAngularExtraDamping = 0.0f, float InMaxTorque = 0.0f,
		float InLinearTargetVelocityMultiplier = 1.0f, float InAngularTargetVelocityMultiplier = 1.0f,
		bool bInEnabled = true)
		: LinearStrength(InLinearStrength)
		, LinearDampingRatio(InLinearDampingRatio)
		, LinearExtraDamping(InLinearExtraDamping)
		, MaxForce(InMaxForce)
		, AngularStrength(InAngularStrength)
		, AngularDampingRatio(InAngularDampingRatio)
		, AngularExtraDamping(InAngularExtraDamping)
		, MaxTorque(InMaxTorque)
		, LinearTargetVelocityMultiplier(InLinearTargetVelocityMultiplier)
		, AngularTargetVelocityMultiplier(InAngularTargetVelocityMultiplier)
		, bEnabled(bInEnabled)
		, bEnableLinearStrength(true)
		, bEnableLinearDampingRatio(true)
		, bEnableLinearExtraDamping(true)
		, bEnableMaxForce(true)
		, bEnableAngularStrength(true)
		, bEnableAngularDampingRatio(true)
		, bEnableAngularExtraDamping(true)
		, bEnableMaxTorque(true)
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

	/** 
	 * The maximum force used to drive the linear motion. Zero indicates no limit. 
	 * Note - not yet implemented for RigidBodyWithControl 
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (ClampMin = "0.0", editcondition = "bEnableMaxForce"))
	float MaxForce;

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
	 * The maximum torque used to drive the angular motion. Zero indicates no limit. 
 	 * Note - not yet implemented for RigidBodyWithControl 
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (ClampMin = "0.0", editcondition = "bEnableMaxTorque"))
	float MaxTorque;

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
	uint8 bEnableMaxForce : 1;

	UPROPERTY(EditAnywhere, Category = PhysicsControl, meta = (InlineEditConditionToggle))
	uint8 bEnableAngularStrength : 1;

	UPROPERTY(EditAnywhere, Category = PhysicsControl, meta = (InlineEditConditionToggle))
	uint8 bEnableAngularDampingRatio : 1;

	UPROPERTY(EditAnywhere, Category = PhysicsControl, meta = (InlineEditConditionToggle))
	uint8 bEnableAngularExtraDamping : 1;

	UPROPERTY(EditAnywhere, Category = PhysicsControl, meta = (InlineEditConditionToggle))
	uint8 bEnableMaxTorque : 1;

	UPROPERTY(EditAnywhere, Category = PhysicsControl, meta = (InlineEditConditionToggle))
	uint8 bEnableLinearTargetVelocityMultiplier : 1;

	UPROPERTY(EditAnywhere, Category = PhysicsControl, meta = (InlineEditConditionToggle))
	uint8 bEnableAngularTargetVelocityMultiplier : 1;

	UPROPERTY(EditAnywhere, Category = PhysicsControl, meta = (InlineEditConditionToggle))
	uint8 bEnablebEnabled : 1;
};

/**
 * Contains data associated with how physical bodies should be controlled/directed towards their targets. 
 * The underlying control is done through damped springs, so the parameters here relate to that.
 */
USTRUCT(BlueprintType)
struct PHYSICSCONTROL_API FPhysicsControlData
{
	GENERATED_BODY()

	FPhysicsControlData()
		: LinearStrength(0.0f)
		, LinearDampingRatio(1.0f)
		, LinearExtraDamping(0.0f)
		, MaxForce(0.0f)
		, AngularStrength(0.0f)
		, AngularDampingRatio(1.0f)
		, AngularExtraDamping(0.0f)
		, MaxTorque(0.0f)
		, LinearTargetVelocityMultiplier(1.0f)
		, AngularTargetVelocityMultiplier(1.0f)
		, bEnabled(true)
	{
	}

	/** Applies the values that have been flagged as enabled from the sparse data */
	void UpdateFromSparseData(const FPhysicsControlSparseData& SparseData);

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

	/** 
	 * The maximum force used to drive the linear motion. Zero indicates no limit. 
	 * Note - not yet implemented for RigidBodyWithControl 
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (ClampMin = "0.0"))
	float MaxForce;

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
	 * The maximum torque used to drive the angular motion. Zero indicates no limit. 
 	 * Note - not yet implemented for RigidBodyWithControl 
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (ClampMin = "0.0"))
	float MaxTorque;

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
 * These parameters allow modification of the parameters in FPhysicsControlData for two reasons:
 * 1. They allow per-axis settings for the linear components (e.g. so you can drive an object 
 *    horizontally but still let it fall under gravity)
 * 2. They make it easy to create the controls with "default" strength/damping (e.g. taken from the
 *    physics asset) in FPhysicsControlData, and then the strength/damping etc can be scaled every 
 *    tick (typically between 0 and 1, though that is up to the user).
 */
USTRUCT(BlueprintType)
struct PHYSICSCONTROL_API FPhysicsControlMultiplier
{
	GENERATED_BODY()

	FPhysicsControlMultiplier()
		: LinearStrengthMultiplier(1.0)
		, LinearDampingRatioMultiplier(1.0)
		, LinearExtraDampingMultiplier(1.0)
		, MaxForceMultiplier(1.0)
		, AngularStrengthMultiplier(1.0)
		, AngularDampingRatioMultiplier(1.0)
		, AngularExtraDampingMultiplier(1.0)
		, MaxTorqueMultiplier(1.0)
	{
	}

	// Per-direction multiplier on the linear strength.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (ClampMin = "0.0"))
	FVector LinearStrengthMultiplier;

	// Per-direction multiplier on the linear damping ratio.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (ClampMin = "0.0"))
	FVector LinearDampingRatioMultiplier;

	// Per-direction multiplier on the linear extra damping.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (ClampMin = "0.0"))
	FVector LinearExtraDampingMultiplier;

	// Per-direction multiplier on the maximum force that can be applied. Note that zero means zero force.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (ClampMin = "0.0"))
	FVector MaxForceMultiplier;

	// Multiplier on the angular strength.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (ClampMin = "0.0"))
	float AngularStrengthMultiplier;

	// Multiplier on the angular damping ratio.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (ClampMin = "0.0"))
	float AngularDampingRatioMultiplier;

	// Multiplier on the angular extra damping.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (ClampMin = "0.0"))
	float AngularExtraDampingMultiplier;

	// Per-direction multiplier on the maximum torque that can be applied. Note that zero means zero torque.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl, meta = (ClampMin = "0.0"))
	float MaxTorqueMultiplier;
};

/**
 * Defines a target position and orientation, and also the target velocity and angular velocity.
 * In many cases the velocities will be calculated automatically (e.g. when setting the target position,
 * the component will optionally calculate an implied velocity. However, the user can also specify a 
 * target velocity directly. Note that the velocity influences the control through the damping parameters
 * in FPhysicsControlData
 */
USTRUCT(BlueprintType)
struct PHYSICSCONTROL_API FPhysicsControlTarget
{
	GENERATED_BODY()

	FPhysicsControlTarget()
		: TargetPosition(ForceInitToZero)
		, TargetVelocity(ForceInitToZero)
		, TargetOrientation(ForceInitToZero)
		, TargetAngularVelocity(ForceInitToZero)
		, bApplyControlPointToTarget(false)
	{
	}

	/** The target position of the child body, relative to the parent body */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FVector TargetPosition;

	/** The target velocity of the child body, relative to the parent body */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FVector TargetVelocity;

	/** The target orientation of the child body, relative to the parent body */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FRotator TargetOrientation;

	/** The target angular velocity (revolutions per second) of the child body, relative to the parent body */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FVector TargetAngularVelocity;

	/** 
	 * Whether to use the ControlPoint as an offset for the target transform, as well as the 
	 * physical body. If true then the target TM is treated as a target transform for the actual 
	 * object, though the control is still applied through the control point (which is at the 
	 * center of mass by default). If false then it is treated as a target transform for the 
	 * control point on the object.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	bool bApplyControlPointToTarget;
};

/**
 * General settings for a control
 */
USTRUCT(BlueprintType)
struct PHYSICSCONTROL_API FPhysicsControlSettings
{
	GENERATED_BODY()

	FPhysicsControlSettings()
		: ControlPoint(ForceInitToZero)
		, bUseSkeletalAnimation(true)
		, SkeletalAnimationVelocityMultiplier(1.0f)
		, bDisableCollision(false)
	{
	}

	/**
	 * The position of the control point relative to the child mesh. Note that this can't be authored
	 * directly here/on creation - it needs to be set after creation in UPhysicsControlComponent::SetControlPoint
	 */
	UPROPERTY()
	FVector ControlPoint;

	/** If true then the target will be applied on top of the skeletal animation (if there is any) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	bool bUseSkeletalAnimation;

	/** The amount of skeletal animation velocity to use in the targets */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	float SkeletalAnimationVelocityMultiplier;

	/**
	 * Whether or not this control should disable collision between the parent and child bodies (only
	 * has an effect if there is a parent body)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	bool bDisableCollision;
};

/**
 * Structure that determines a "control" - this contains all the information needed to drive (with spring-dampers)
 * a child body relative to a parent body. These bodies will be associated with either a static or skeletal mesh.
 */
USTRUCT(BlueprintType)
struct PHYSICSCONTROL_API FPhysicsControl
{
	GENERATED_BODY()

	FPhysicsControl() {}

	FPhysicsControl(
		UMeshComponent*              InParentMeshComponent,
		const FName&                 InParentBoneName,
		UMeshComponent*              InChildMeshComponent,
		const FName&                 InChildBoneName,
		const FPhysicsControlData&   InControlData,
		const FPhysicsControlTarget& InControlTarget,
		const FPhysicsControlSettings& InControlSettings)
		: ParentMeshComponent(InParentMeshComponent)
		, ParentBoneName(InParentBoneName)
		, ChildMeshComponent(InChildMeshComponent)
		, ChildBoneName(InChildBoneName)
		, ControlData(InControlData)
		, ControlTarget(InControlTarget)
		, ControlSettings(InControlSettings)
	{
	}

	/**  The mesh that will be doing the driving. Blank/non-existent means it will happen in world space */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	TObjectPtr<UMeshComponent> ParentMeshComponent;

	/** The name of the skeletal mesh bone or the name of the static mesh body that will be doing the driving. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FName ParentBoneName;

	/** The mesh that the control will be driving. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	TObjectPtr<UMeshComponent> ChildMeshComponent;

	/** 
	 * The name of the skeletal mesh bone or the name of the static mesh body that the control 
	 * will be driving. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FName ChildBoneName;

	/** 
	 * Strength and damping parameters. Can be modified at any time, but will sometimes have 
	 * been set once during initialization 
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FPhysicsControlData ControlData;

	/**
	 * Multiplier for the ControlData. This will typically be modified dynamically, and also expose the ability
	 * to set directional strengths
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FPhysicsControlMultiplier ControlMultiplier;

	/**
	 * The position/orientation etc targets for the controls. These are procedural/explicit control targets -
	 * skeletal meshes have the option to use skeletal animation as well, in which case these targets are 
	 * expressed as relative to that animation.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FPhysicsControlTarget ControlTarget;

	/**
	 * More general settings for the control
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhysicsControl)
	FPhysicsControlSettings ControlSettings;
};

