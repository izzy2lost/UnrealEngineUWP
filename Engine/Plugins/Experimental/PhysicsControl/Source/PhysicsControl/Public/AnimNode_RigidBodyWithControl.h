// Copyright Epic Games, Inc. All Rights Reserved.

/* 
 * This is essentially a copy + paste of the RigidBody file
 */

#pragma once

#include "CoreMinimal.h"
#include "RigidBodyPoseData.h"
#include "RigidBodyControlData.h"
#include "RigidBodyNameRecords.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Physics/ImmediatePhysics/ImmediatePhysicsDeclares.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsProxy/PerSolverFieldSystem.h"
#include "Tasks/Task.h"

 // Use the simulation space functions from the RBAN
#include "BoneControllers/AnimNode_RigidBody.h" 

#include "AnimNode_RigidBodyWithControl.generated.h"

struct FBodyInstance;
struct FConstraintInstance;
class FEvent;

extern TAutoConsoleVariable<int32> CVarEnableRigidBodyNodeWithControl;
extern TAutoConsoleVariable<int32> CVarEnableRigidBodyNodeWithControlSimulation;
extern TAutoConsoleVariable<int32> CVarRigidBodyNodeWithControlLODThreshold;

/**
 * Controller that simulates physics based on the physics asset of the skeletal mesh component
 */
USTRUCT()
struct PHYSICSCONTROL_API FAnimNode_RigidBodyWithControl : public FAnimNode_SkeletalControlBase
{
	GENERATED_BODY()

	FAnimNode_RigidBodyWithControl();
	~FAnimNode_RigidBodyWithControl();

	// FAnimNode_Base interface
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	// End of FAnimNode_Base interface

	// FAnimNode_SkeletalControlBase interface
	virtual void UpdateComponentPose_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void EvaluateComponentPose_AnyThread(FComponentSpacePoseContext& Output) override;
	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual void OnInitializeAnimInstance(const FAnimInstanceProxy* InProxy, const UAnimInstance* InAnimInstance) override;
	virtual bool NeedsOnInitializeAnimInstance() const override { return true; }
	virtual void PreUpdate(const UAnimInstance* InAnimInstance) override;
	virtual void UpdateInternal(const FAnimationUpdateContext& Context) override;
	virtual bool HasPreUpdate() const override { return true; }
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;
	virtual bool NeedsDynamicReset() const override;
	virtual void ResetDynamics(ETeleportType InTeleportType) override;
	virtual int32 GetLODThreshold() const override;
	// End of FAnimNode_SkeletalControlBase interface

	virtual void AddImpulseAtLocation(FVector Impulse, FVector Location, FName BoneName = NAME_None);

	// TEMP: Exposed for use in PhAt as a quick way to get drag handles working with Chaos
	virtual ImmediatePhysics::FSimulation* GetSimulation() { return PhysicsSimulation; }

	UPhysicsAsset* GetPhysicsAsset() const { return PhysicsAssetToUse; }

public:
	/** Physics asset to use. If empty use the skeletal mesh's default physics asset */
	UPROPERTY(EditAnywhere, Category = Settings)
	TObjectPtr<UPhysicsAsset> OverridePhysicsAsset;

private:
	FTransform PreviousCompWorldSpaceTM;
	FTransform CurrentTransform;
	FTransform PreviousTransform;

	UPhysicsAsset* PhysicsAssetToUse;
public:

	/** Override gravity*/
	UPROPERTY(EditAnywhere, Category = Settings, meta = (PinHiddenByDefault, editcondition = "bOverrideWorldGravity"))
	FVector OverrideWorldGravity;

	/** Applies a uniform external force in world space. This allows for easily faking inertia of movement while still simulating in component space for example */
	UPROPERTY(EditAnywhere, Category = Settings, meta = (PinShownByDefault))
	FVector ExternalForce;

	/** When using non-world-space sim, this controls how much of the components world-space acceleration is passed on to the local-space simulation. */
	UPROPERTY(EditAnywhere, Category = Settings, meta = (PinHiddenByDefault))
	FVector ComponentLinearAccScale;

	/** When using non-world-space sim, this applies a 'drag' to the bodies in the local space simulation, based on the components world-space velocity. */
	UPROPERTY(EditAnywhere, Category = Settings, meta = (PinHiddenByDefault))
	FVector ComponentLinearVelScale;

	/** When using non-world-space sim, this is an overall clamp on acceleration derived from ComponentLinearAccScale and ComponentLinearVelScale, to ensure it is not too large. */
	UPROPERTY(EditAnywhere, Category = Settings)
	FVector	ComponentAppliedLinearAccClamp;

	/**
	 * Settings for the system which passes motion of the simulation's space
	 * into the simulation. This allows the simulation to pass a
	 * fraction of the world space motion onto the bodies which allows Bone-Space
	 * and Component-Space simulations to react to world-space movement in a
	 * controllable way.
	 * This system is a superset of the functionality provided by ComponentLinearAccScale,
	 * ComponentLinearVelScale, and ComponentAppliedLinearAccClamp. In general
	 * you should not have both systems enabled.
	 */
	UPROPERTY(EditAnywhere, Category = Settings, meta = (PinHiddenByDefault))
	FSimSpaceSettings SimSpaceSettings;


	/**
	 * Scale of cached bounds (vs. actual bounds).
	 * Increasing this may improve performance, but overlaps may not work as well.
	 * (A value of 1.0 effectively disables cached bounds).
	 */
	UPROPERTY(EditAnywhere, Category = Settings, meta = (ClampMin="1.0", ClampMax="2.0"))
	float CachedBoundsScale;

	/** Matters if SimulationSpace is BaseBone */
	UPROPERTY(EditAnywhere, Category = Settings)
	FBoneReference BaseBoneRef;

	/** The channel we use to find static geometry to collide with */
	UPROPERTY(EditAnywhere, Category = Settings, meta = (editcondition = "bEnableWorldGeometry"))
	TEnumAsByte<ECollisionChannel> OverlapChannel;

	/** What space to simulate the bodies in. This affects how velocities are generated */
	UPROPERTY(EditAnywhere, Category = Settings)
	ESimulationSpace SimulationSpace;

	/** Whether to allow collisions between two bodies joined by a constraint  */
	UPROPERTY(EditAnywhere, Category = Settings)
	bool bForceDisableCollisionBetweenConstraintBodies;

	/** If true, kinematic objects will be added to the simulation at runtime to represent any cloth colliders defined for the parent object. */
	UPROPERTY(EditAnywhere, Category = Settings)
	bool bUseExternalClothCollision;

private:
	ETeleportType ResetSimulatedTeleportType;

public:
	UPROPERTY(EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle))
	uint8 bEnableWorldGeometry : 1;

	UPROPERTY(EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle))
	uint8 bOverrideWorldGravity : 1;

	/** 
		When simulation starts, transfer previous bone velocities (from animation)
		to make transition into simulation seamless.
	*/
	UPROPERTY(EditAnywhere, Category = Settings, meta=(PinHiddenByDefault))
	uint8 bTransferBoneVelocities : 1;

	/**
		When simulation starts, freeze incoming pose.
		This is useful for ragdolls, when we want the simulation to take over.
		It prevents non simulated bones from animating.
	*/
	UPROPERTY(EditAnywhere, Category = Settings)
	uint8 bFreezeIncomingPoseOnStart : 1;

	/**
		Correct for linear tearing on bodies with all axes Locked.
		This only works if all axes linear translation are locked
	*/
	UPROPERTY(EditAnywhere, Category = Settings)
	uint8 bClampLinearTranslationLimitToRefPose : 1;

	/**
		For world-space simulations, if the magnitude of the component's 3D scale is less than WorldSpaceMinimumScale, do not update the node.
	*/
	UPROPERTY(EditAnywhere, Category = Settings)
	float WorldSpaceMinimumScale;

	/**
		If the node is not evaluated for this amount of time (seconds), either because a lower LOD was in use for a while or the component was
		not visible, reset the simulation to the default pose on the next evaluation. Set to 0 to disable time-based reset.
	*/
	UPROPERTY(EditAnywhere, Category = Settings)
	float EvaluationResetTime;

	/**
	 * If false, then controls will not be created. Note that this can be exposed as a pin/bound, and then control 
	 * creation can be deferred to when the value is set to true as the node runs.
	 */
	UPROPERTY(EditAnywhere, Category = ControlSetup, meta = (PinHiddenByDefault))
	bool bEnableControls;

	/**
	 * A map of bone names to "body" names, the latter being used to assign names to controls/modifiers. 
	 * This is optional - so if there is no mapping for a bone, then its name will be used directly when 
	 * creating controls. The two main benefits of this are (1) to generate consistently named controls 
	 * even on different skeletons and (2) to make it easier to refer to individual controls, without needing 
	 * to refer to the skeleton.
	 */
	UPROPERTY(EditAnywhere, Category = ControlSetup, meta = (PinHiddenByDefault))
	TMap<FName, FName> BoneToBodyNameMap;

	/**
	 * Setup data for creating the main controls (world- and parent-space) and modifiers, based on splitting the 
	 * skeleton up into limbs.
	 */
	UPROPERTY(EditAnywhere, Category = ControlSetup, meta = (PinHiddenByDefault))
	FRigidBodySetupData SetupData;

	/**
	 * Controls and modifiers that should be created, in addition to those made as part of the limb setup.
	 */
	UPROPERTY(EditAnywhere, Category = ControlSetup, meta = (PinHiddenByDefault))
	FRigidBodyControlAndBodyModifierCreations AdditionalControlsAndBodyModifiers;

	/**
	 * Allows additional sets of controls or modifiers to be created, and existing sets to be modified
	 */
	UPROPERTY(EditAnywhere, Category = ControlSetup, meta = (PinHiddenByDefault))
	FRigidBodySetUpdates AdditionalSets;

	/**
	 * An initial set of controls that should be applied immediately after setup. This allows individual or 
	 * sets of controls/modifiers etc to be adjusted. Note that these will then be "baked" into the 
	 * controls.
	 */
	UPROPERTY(EditAnywhere, Category = ControlSetup, meta = (PinHiddenByDefault))
	FRigidBodyControlAndModifierParameters InitialControlAndBodyModifierUpdates;

	/**
	 * Controls that should be applied each frame, and can be expected to change. Note that if these
	 * stop being passed in then the controls and modifiers will return to their normal/original state.
	 */
	UPROPERTY(EditAnywhere, Category = Controls, meta = (PinShownByDefault))
	FRigidBodyControlAndModifierParameters ControlAndModifierParameters;

	/**
	 * Updates to controls that can be applied. Note that these update the normal/original state.
	 */
	UPROPERTY(EditAnywhere, Category = Controls, meta = (PinShownByDefault))
	FRigidBodyControlAndModifierUpdates ControlAndModifierUpdates;

	/**
	 * Targets that should be applied to the controls. 
	 */
	UPROPERTY(EditAnywhere, Category = Controls, meta = (PinShownByDefault))
	FRigidBodyControlTargets ControlTargets;

	/**
	 * Targets that should be applied to kinematic bodies that are under the influence of a body modifier. 
	 */
	UPROPERTY(EditAnywhere, Category = Controls, meta = (PinShownByDefault))
	FRigidBodyKinematicTargets KinematicTargets;

	/**
	 * The constraint profile to use on all the joints in the physics asset
	 */
	UPROPERTY(EditAnywhere, Category = Controls, meta = (PinHiddenByDefault))
	FName ConstraintProfile;

private:
	uint8 bEnabled : 1;
	uint8 bSimulationStarted : 1;
	uint8 bCheckForBodyTransformInit : 1;
	uint8 bHaveRunControlSeup : 1;

public:
	void PostSerialize(const FArchive& Ar);

	const int32 GetNumBodies() const;

	const FRigidBodyNameRecords& GetNameRecords() const { return NameRecords; }

private:

	// FAnimNode_SkeletalControlBase interface
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;
	// End of FAnimNode_SkeletalControlBase interface

	void InitPhysics(const UAnimInstance* InAnimInstance);
	void UpdateWorldGeometry(const UWorld& World, const USkeletalMeshComponent& SKC);
	void UpdateWorldForces(const FTransform& ComponentToWorld, const FTransform& RootBoneTM, const float DeltaSeconds);

public:
	const FTransform GetBodyTransform(const int32 BodyIndex) const;
	const FTransform GetWorldSpaceControlRootTransform() const;

private:

	// Add a new physics body and update the body name to index map
	int32 AddBody(ImmediatePhysics::FActorHandle* const BodyHandle); 
	int32 FindBodyIndexFromBoneName(const FName BoneName) const;
	FName FindParentBoneNameFromBoneName(const FName BoneName) const;
	ImmediatePhysics::FActorHandle* FindBodyFromBoneName(const FName BoneName) const;

	ImmediatePhysics::FJointHandle* CreateConstraint(
		ImmediatePhysics::FActorHandle* const ChildBodyHandle, ImmediatePhysics::FActorHandle* const ParentBodyHandle);
	void CreateWorldSpaceControlRootBody(UPhysicsAsset* const PhysicsAsset);

	void ApplyControl(FControlRecord& ControlRecord, float DeltaTime);
	void ApplyModifier(const FBodyModifierRecord& BodyModifierRecord, const FVector& SimSpaceGravity);

	void ApplyControlsAndModifiers(const FVector& SimSpaceGravity, float DeltaTime);

	// This applies kinematic targets to any that have been set through the body modifiers
	void ApplyKinematicTargets();

	// This applies the desired constraint profile, if necessary
	void ApplyCurrentConstraintProfile();

	// Adjusts the spring drive settings to reflect the control data
	void UpdateDriveSpringDamperSettings(Chaos::FPBDJointSettings& Settings, const FRigidBodyControlData& ControlData);

	TMap<FName, int32> BodyNameToIndexMap;
	ImmediatePhysics::FActorHandle* WorldSpaceControlActorHandle;

	FName GetBodyFromBoneName(const FName BoneName) const;

	FName GetUniqueBodyModifierName(const FName BoneName);

	FName GetUniqueControlName(const FName ParentBoneName, const FName ChildBoneName);

	FName CreateControl(const FName ParentBoneName, const FName ChildBoneName, const FRigidBodyControlData& ControlData);

	FName CreateBodyModifier(const FName BoneName, const FRigidBodyModifierData& ModifierData);

	void CreateControlsFromLimbBones(
		const FName                  LimbName,
		const FRigidBodyLimbBones&   LimbBones,
		const ERigidBodyControlType  ControlType,
		const FRigidBodyControlData& ControlData);

	void CreateBodyModifiersFromLimbBones(
		const FName                   LimbName,
		const FRigidBodyLimbBones&    LimbBones,
		const FRigidBodyModifierData& DefaultModifierData);

	TMap<FName, FRigidBodyLimbBones> GetLimbBonesFromSkeletalMesh(
		const TArray<FRigidBodyLimbSetupData>& LimbSetupData,
		USkeletalMeshComponent* const SkeletalMeshComponent) const;

	// This will walk through the skeleton, create controls and body modifiers, and create the sets.
	void InitControlsAndBodyModifiers(USkeletalMeshComponent* const SkeletalMeshComponent);

	void DestroyControlsAndBodyModifiers();

	// Helper to log overything that has been created to help the user
	void LogControlsModifiersAndSets();

	// Creates any controls as set in AdditionalControls
	void CreateAdditionalControls();
	
	// Creates/updates sets - possibly making new ones, or adding to existing ones
	void CreateAdditionalSets();

	// Applies the overrides to the underlying controls and modifiers
	void ApplyControlAndBodyModifierDatas(
		const TArray<FRigidBodyNamedControlParameters>& ControlParameters,
		const TArray<FRigidBodyNamedModifierParameters>& ModifierParameters);

	// Applies the parameters to control and modifier records
	void ApplyControlAndModifierUpdatesAndParametersToRecords(
		const FRigidBodyControlAndModifierUpdates&    Updates,
		const FRigidBodyControlAndModifierParameters& Parameters);

	void InitializeNewBodyTransformsDuringSimulation(
		FComponentSpacePoseContext& Output, const FTransform& ComponentTransform, const FTransform& BaseBoneTM);

	void InitSimulationSpace(const FTransform& ComponentToWorld, const FTransform& BoneToComponent);

	// Calculate simulation space transform, velocity etc to pass into the solver
	void CalculateSimulationSpace(
		ESimulationSpace Space,
		const FTransform& ComponentToWorld,
		const FTransform& BoneToComponent,
		const float Dt,
		const FSimSpaceSettings& Settings,
		FTransform& SpaceTransform,
		FVector& SpaceLinearVel,
		FVector& SpaceAngularVel,
		FVector& SpaceLinearAcc,
		FVector& SpaceAngularAcc);

	// Gather cloth collision sources from the supplied Skeltal Mesh and add a kinematic actor representing each one of them to the sim.
	void CollectClothColliderObjects(const USkeletalMeshComponent* SkeletalMeshComp);
	
	// Remove all cloth collider objects from the sim.
	void RemoveClothColliderObjects();

	// Update the sim-space transforms of all cloth collider objects.
	void UpdateClothColliderObjects(const FTransform& SpaceTransform);

	// Gather nearby world objects and add them to the sim
	void CollectWorldObjects();

	// Flag invalid world objects to be removed from the sim
	void ExpireWorldObjects();

	// Remove simulation objects that are flagged as expired
	void PurgeExpiredWorldObjects();

	// Update sim-space transforms of world objects
	void UpdateWorldObjects(const FTransform& SpaceTransform);

	// Advances the simulation by a given timestep
	void RunPhysicsSimulation(float DeltaSeconds, const FVector& SimSpaceGravity);

	// Waits for the deferred simulation task to complete if it's not already finished
	void FlushDeferredSimulationTask();

	// Destroy the simulation and free related structures
	void DestroyPhysicsSimulation();

public:

	/* Whether the physics simulation runs synchronously with the node's evaluation or is run in the background until the next frame. */
	UPROPERTY(EditAnywhere, Category=Settings, AdvancedDisplay)
	ESimulationTiming SimulationTiming;

private:

	float WorldTimeSeconds;
	float LastEvalTimeSeconds;

	float AccumulatedDeltaTime;
	float AnimPhysicsMinDeltaTime;
	bool bSimulateAnimPhysicsAfterReset;
	/** This should only be used for removing the delegate during termination. Do NOT use this for any per frame work */
	TWeakObjectPtr<USkeletalMeshComponent> SkelMeshCompWeakPtr;

	ImmediatePhysics::FSimulation* PhysicsSimulation;
	FPhysicsAssetSolverSettings SolverSettings;
	FSolverIterations SolverIterations;	// to be deprecated

	friend class FRigidBodyNodeWithControlSimulationTask;
	UE::Tasks::FTask SimulationTask;

	struct FBodyAnimData
	{
		FBodyAnimData()
			: TransferedBoneAngularVelocity(ForceInit)
			, TransferedBoneLinearVelocity(ForceInitToZero)
			, LinearXMotion(ELinearConstraintMotion::LCM_Locked)
			, LinearYMotion(ELinearConstraintMotion::LCM_Locked)
			, LinearZMotion(ELinearConstraintMotion::LCM_Locked)
			, LinearLimit(0.0f)
			, RefPoseLength (0.f)
			, bIsSimulated(false)
			, bBodyTransformInitialized(false)
		{}

		FQuat TransferedBoneAngularVelocity;
		FVector TransferedBoneLinearVelocity;

		ELinearConstraintMotion LinearXMotion;
		ELinearConstraintMotion LinearYMotion;
		ELinearConstraintMotion LinearZMotion;
		float LinearLimit;
		// we don't use linear limit but use default length to limit the bodies
		// linear limits are defined per constraint - it can be any two joints that can limit
		// this is just default length of the local space from parent, and we use that info to limit
		// the translation
		float RefPoseLength;

		bool bIsSimulated : 1;
		bool bBodyTransformInitialized : 1;
	};

	struct FWorldObject
	{
		FWorldObject() : ActorHandle(nullptr), LastSeenTick(0), bExpired(false) {}
		FWorldObject(ImmediatePhysics::FActorHandle* InActorHandle, int32 InLastSeenTick) : ActorHandle(InActorHandle), LastSeenTick(InLastSeenTick), bExpired(false) {}

		ImmediatePhysics::FActorHandle* ActorHandle;
		int32 LastSeenTick;
		bool bExpired;
	};

	TArray<RigidBodyWithControl::FOutputBoneData> OutputBoneData;
	// Note that the Bodies and Joints arrays will be the same size - i.e. there will be a
	// correspondence between every joint and body. The joint will correspond to the parent of the
	// body. Note that some joints will not be set.
	TArray<ImmediatePhysics::FActorHandle*> Bodies;
	TArray<ImmediatePhysics::FJointHandle*> Joints;
	TArray<int32> SkeletonBoneIndexToBodyIndex;
	TArray<FBodyAnimData> BodyAnimData;

	friend class UAnimGraphNode_RigidBodyWithControl;

	// Each update we cache the incoming pose transforms in whatever space the simulation is running in
	RigidBodyWithControl::FRigidBodyPoseData PoseData;

	// Map of control records - they will be referenced by name
	TMap<FName, FControlRecord> ControlRecords;

	// Map of body modifier records - they will be referenced by name
	TMap<FName, FBodyModifierRecord> ModifierRecords;

	// Details about sets etc
	FRigidBodyNameRecords NameRecords;

	FName CurrentConstraintProfile;

	TArray<USkeletalMeshComponent::FPendingRadialForces> PendingRadialForces;

	FPerSolverFieldSystem PerSolverField;

	// Information required to identify and update a kinematic object representing a cloth collision source in the sim.
	struct FClothCollider
	{
		FClothCollider(ImmediatePhysics::FActorHandle* const InActorHandle, const USkeletalMeshComponent* const InSkeletalMeshComponent, const uint32 InBoneIndex)
			: ActorHandle(InActorHandle)
			, SkeletalMeshComponent(InSkeletalMeshComponent)
			, BoneIndex(InBoneIndex)
		{}

		ImmediatePhysics::FActorHandle* ActorHandle; // Identifies the physics actor in the sim.
		const USkeletalMeshComponent* SkeletalMeshComponent; // Parent skeleton.
		uint32 BoneIndex; // Bone within parent skeleton that drives physics actors transform.
	};

	// List of actors in the sim that represent objects collected from other parts of this character.
	TArray<FClothCollider> ClothColliders; 
	
	TMap<const UPrimitiveComponent*, FWorldObject> ComponentsInSim;
	int32 ComponentsInSimTick;

	FVector WorldSpaceGravity;

	float TotalMass;

	// Bounds used to gather world objects copied into the simulation
	FSphere CachedBounds;

	FCollisionQueryParams QueryParams;

	FPhysScene* PhysScene;

	// Used by CollectWorldObjects and UpdateWorldGeometry in Task Thread
	// Typically, World should never be accessed off the Game Thread.
	// However, since we're just doing overlaps this should be OK.
	const UWorld* UnsafeWorld;

	// Used by CollectWorldObjects and UpdateWorldGeometry in Task Thread
	// Only used for a pointer comparison.
	const AActor* UnsafeOwner;

	FBoneContainer CapturedBoneVelocityBoneContainer;
	FCSPose<FCompactHeapPose> CapturedBoneVelocityPose;
	FCSPose<FCompactHeapPose> CapturedFrozenPose;
	FBlendedHeapCurve CapturedFrozenCurves;

	FVector PreviousComponentLinearVelocity;

	// Used by the world-space to simulation-space motion transfer system in Component- or Bone-Space sims
	FTransform SimSpacePreviousComponentToWorld;
	FTransform SimSpacePreviousBoneToComponent;
	FVector SimSpacePreviousComponentLinearVelocity;
	FVector SimSpacePreviousComponentAngularVelocity;
	FVector SimSpacePreviousBoneLinearVelocity;
	FVector SimSpacePreviousBoneAngularVelocity;
};

#if WITH_EDITORONLY_DATA
template<>
struct TStructOpsTypeTraits<FAnimNode_RigidBodyWithControl> : public TStructOpsTypeTraitsBase2<FAnimNode_RigidBodyWithControl>
{
	enum
	{
		WithPostSerialize = true,
	};
};
#endif

