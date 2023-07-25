// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "BonePose.h"

// Use the simulation space functions from the RBAN
#include "BoneControllers/AnimNode_RigidBody.h" 

struct FComponentSpacePoseContext;

namespace RigidBodyWithControl
{

//======================================================================================================================
struct FOutputBoneData
{
	FOutputBoneData()
		: CompactPoseBoneIndex(INDEX_NONE), CompactPoseParentBoneIndex(INDEX_NONE)
	{}

	TArray<FCompactPoseBoneIndex> BoneIndicesToParentBody;
	FCompactPoseBoneIndex CompactPoseBoneIndex;
	FCompactPoseBoneIndex CompactPoseParentBoneIndex;
	int32 BodyIndex; // Index into Bodies - will be the same index as into the joints
	int32 ParentBodyIndex;
};

//======================================================================================================================
struct FBoneData
{
	FBoneData()
		: Position(FVector::ZeroVector), Orientation(FQuat::Identity) 
	{}
	FBoneData(const FVector& InPosition, const FQuat& InOrientation)
		: Position(InPosition)
		, Orientation(InOrientation)
	{}

	/**
	 * Sets position/orientation in both current and previous (i.e. implying zero velocity)
	 */
	void Set(const FVector& InPosition, const FQuat& InOrientation) { 
		Position = InPosition; Orientation = InOrientation; }

	FTransform GetTM() const { return FTransform(Orientation, Position); }

	FVector Position;
	FQuat   Orientation;
};

//======================================================================================================================
struct FRigidBodyPoseData
{
	void Update(
		FComponentSpacePoseContext&   ComponentSpacePoseContext,
		const TArray<FOutputBoneData> OutputBoneData,
		const ESimulationSpace        SimulationSpace,
		const FBoneReference&         BaseBoneRef,
		const FGraphTraversalCounter& InUpdateCounter);

	FTransform GetTM(int32 Index) const { return BoneData[Index].GetTM(); }
	bool IsValidIndex(const int32 Index) const { return BoneData.IsValidIndex(Index); }
	bool IsEmpty() const { return BoneData.IsEmpty(); }

	/**
	 * The cached skeletal data, updated at the start of each tick
	 */
	TArray<FBoneData> BoneData;

	/**
	 * The origin (in world space). BoneData transforms are relative to this.
	 */
	FTransform WorldCoordinateFrame;

	// Track when we were currently/last updated so the user can detect missing updates if calculating
	// velocity etc
	FGraphTraversalCounter UpdateCounter;
	// When the update is called we'll take the current counter, increment it, and store here so it
	// can be compared.
	FGraphTraversalCounter ExpectedUpdateCounter;
};

}