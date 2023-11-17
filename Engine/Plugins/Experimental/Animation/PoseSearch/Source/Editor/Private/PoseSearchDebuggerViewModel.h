// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "PoseSearchMeshComponent.h"
#include "PoseSearch/PoseSearchMirrorDataCache.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtrTemplates.h"

struct FInstancedStruct;
class IRewindDebugger;
class UPoseSearchDatabase;

namespace UE::PoseSearch
{

struct FTraceMotionMatchingStateMessage;

class FDebuggerViewModel : public TSharedFromThis<FDebuggerViewModel>
{
public:
	explicit FDebuggerViewModel(uint64 InAnimInstanceId);
	virtual ~FDebuggerViewModel();

	// Used for view callbacks
    const FTraceMotionMatchingStateMessage* GetMotionMatchingState() const;
	const UPoseSearchDatabase* GetCurrentDatabase() const;
	const TArray<int32>* GetNodeIds() const;
	int32 GetNodesNum() const;
	const FTransform& GetRootBoneTransform() const;

	/** Update motion matching states for frame */
	void OnUpdate();
	
	/** Updates active motion matching state based on node selection */
	void OnUpdateNodeSelection(int32 InNodeId);

	void SetVerbose(bool bVerbose) { bIsVerbose = bVerbose; }
	bool IsVerbose() const { return bIsVerbose; }

	void SetDrawQuery(bool bInDrawQuery) { bDrawQuery = bInDrawQuery; }
	bool GetDrawQuery() const { return bDrawQuery; }

	void SetDrawTrajectory(bool bInDrawTrajectory) { bDrawTrajectory = bInDrawTrajectory; }

	bool GetDrawTrajectory() const { return bDrawTrajectory; }

	/** Callback to reset debug skeletons for the active world */
	void OnWorldCleanup(UWorld* InWorld, bool bSessionEnded, bool bCleanupResources);

	const USkinnedMeshComponent* GetMeshComponent() const;

private:
	/** Update the list of states for this frame */
	void UpdateFromTimeline();

	/** List of all Node IDs associated with motion matching states */
	TArray<int32> NodeIds;
	
	/** List of all updated motion matching states per node */
	TArray<FTraceMotionMatchingStateMessage> MotionMatchingStates;
	
	/** Currently active motion matching state index based on node selection in the view */
	int32 ActiveMotionMatchingStateIdx = INDEX_NONE;

	/** Current Skeletal Mesh Component Id for the AnimInstance */
	uint64 SkeletalMeshComponentId = 0;

	/** Currently active root bone transform */
	FTransform RootBoneWorldTransform = FTransform::Identity;

	/** Pointer to the active rewind debugger in the scene */
	TAttribute<const IRewindDebugger*> RewindDebugger;

	/** Anim Instance associated with this debugger instance */
	uint64 AnimInstanceId = 0;

	/** Actor object for the skeleton */
	TWeakObjectPtr<AActor> DebugDrawActor;

	/** Derived skeletal mesh for setting the skeleton in the scene */
	TWeakObjectPtr<UPoseSearchMeshComponent> DebugDrawMeshComponent;

	/** Whether the skeleton have been initialized for this world */
	bool bSkeletonInitialized = false;
	
	bool bIsVerbose = false;

	bool bDrawQuery = true;	

	bool bDrawTrajectory = false;
	
	/** Limits some public API */
	friend class FDebugger;
};

} // namespace UE::PoseSearch
