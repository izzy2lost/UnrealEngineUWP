// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchDebuggerViewModel.h"
#include "Animation/AnimComposite.h"
#include "Animation/AnimSequence.h"
#include "Animation/MirrorDataTable.h"
#include "Engine/SkeletalMesh.h"
#include "IAnimationProvider.h"
#include "IGameplayProvider.h"
#include "InstancedStruct.h"
#include "IRewindDebugger.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchDerivedData.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "Trace/PoseSearchTraceProvider.h"

namespace UE::PoseSearch
{

FDebuggerViewModel::FDebuggerViewModel(uint64 InAnimInstanceId)
	: AnimInstanceId(InAnimInstanceId)
{
}

FDebuggerViewModel::~FDebuggerViewModel()
{
	if (DebugDrawActor != nullptr)
	{
		DebugDrawActor->Destroy();
	}
}

const FTraceMotionMatchingStateMessage* FDebuggerViewModel::GetMotionMatchingState() const
{
	if (MotionMatchingStates.IsValidIndex(ActiveMotionMatchingStateIdx))
	{
		return &MotionMatchingStates[ActiveMotionMatchingStateIdx];
	}
	return nullptr;
}

const UPoseSearchDatabase* FDebuggerViewModel::GetCurrentDatabase() const
{
	if (const FTraceMotionMatchingStateMessage* State = GetMotionMatchingState())
	{
		return State->GetCurrentDatabase();
	}
	return nullptr;
}

const TArray<int32>* FDebuggerViewModel::GetNodeIds() const
{
	return &NodeIds;
}

int32 FDebuggerViewModel::GetNodesNum() const
{
	return MotionMatchingStates.Num();
}

const FTransform& FDebuggerViewModel::GetRootBoneTransform() const
{
	return RootBoneWorldTransform;
}

void FDebuggerViewModel::OnUpdate()
{
	if (!bSkeletonInitialized)
	{
		UWorld* World = RewindDebugger.Get()->GetWorldToVisualize();
		FActorSpawnParameters ActorSpawnParameters;
		ActorSpawnParameters.bHideFromSceneOutliner = false;
		ActorSpawnParameters.ObjectFlags |= RF_Transient;
		DebugDrawActor = World->SpawnActor<AActor>(ActorSpawnParameters);
		DebugDrawActor->SetActorLabel(TEXT("PoseSearch"));
		DebugDrawMeshComponent = NewObject<UPoseSearchMeshComponent>(DebugDrawActor.Get());
		DebugDrawActor->AddInstanceComponent(DebugDrawMeshComponent.Get());
		DebugDrawMeshComponent->RegisterComponentWithWorld(World);
	
		FWorldDelegates::OnWorldCleanup.AddRaw(this, &FDebuggerViewModel::OnWorldCleanup);
		bSkeletonInitialized = true;
	}

	UpdateFromTimeline();
}

void FDebuggerViewModel::OnUpdateNodeSelection(int32 InNodeId)
{
	if (InNodeId == INDEX_NONE)
	{
		return;
	}

	// Find node in all motion matching states this frame
	ActiveMotionMatchingStateIdx = INDEX_NONE;
	const int32 NodesNum = NodeIds.Num();
	for (int32 i = 0; i < NodesNum; ++i)
	{
		if (NodeIds[i] == InNodeId)
		{
			ActiveMotionMatchingStateIdx = i;
			break;
		}
	}
}

void FDebuggerViewModel::UpdateFromTimeline()
{
	NodeIds.Empty();
	MotionMatchingStates.Empty();
	SkeletalMeshComponentId = 0;

	// Get provider and validate
	const TraceServices::IAnalysisSession* Session = RewindDebugger.Get()->GetAnalysisSession();
	TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session);

	const FTraceProvider* PoseSearchProvider = Session->ReadProvider<FTraceProvider>(FTraceProvider::ProviderName);
	const IAnimationProvider* AnimationProvider = Session->ReadProvider<IAnimationProvider>("AnimationProvider");
	const IGameplayProvider* GameplayProvider = Session->ReadProvider<IGameplayProvider>("GameplayProvider");
	if (!(PoseSearchProvider && AnimationProvider && GameplayProvider))
	{
		return;
	}
	const double TraceTime = RewindDebugger.Get()->CurrentTraceTime();
	TraceServices::FFrame Frame;
	ReadFrameProvider(*Session).GetFrameFromTime(TraceFrameType_Game, TraceTime, Frame);
	PoseSearchProvider->EnumerateMotionMatchingStateTimelines(AnimInstanceId, [&](const FTraceProvider::FMotionMatchingStateTimeline& InTimeline)
	{
		const FTraceMotionMatchingStateMessage* Message = nullptr;

		InTimeline.EnumerateEvents(Frame.StartTime, Frame.EndTime, [&Message](double InStartTime, double InEndTime, const FTraceMotionMatchingStateMessage& InMessage)
		{
			Message = &InMessage;
			return TraceServices::EEventEnumerate::Stop;
		});
		if (Message)
		{
			NodeIds.Add(Message->NodeId);

			// @todo: figure out if we can avoid this copy
			MotionMatchingStates.Add(*Message);
			SkeletalMeshComponentId = Message->SkeletalMeshComponentId;
		}
	});
	/** No active motion matching state as no messages were read */
	if (SkeletalMeshComponentId == 0)
	{
		return;
	}
	AnimationProvider->ReadSkeletalMeshPoseTimeline(SkeletalMeshComponentId, [&](const IAnimationProvider::SkeletalMeshPoseTimeline& TimelineData, bool bHasCurves)
	{
		TimelineData.EnumerateEvents(Frame.StartTime, Frame.EndTime, [&](double InStartTime, double InEndTime, uint32 InDepth, const FSkeletalMeshPoseMessage& PoseMessage) -> TraceServices::EEventEnumerate
		{
			const FSkeletalMeshInfo* SkeletalMeshInfo = AnimationProvider->FindSkeletalMeshInfo(PoseMessage.MeshId);
			const FObjectInfo* SkeletalMeshObjectInfo = GameplayProvider->FindObjectInfo(PoseMessage.MeshId);
			if (!SkeletalMeshInfo || !SkeletalMeshObjectInfo)
			{
				return TraceServices::EEventEnumerate::Stop;
			}

			UPoseSearchMeshComponent* PoseSearchMeshComponent = DebugDrawMeshComponent.Get();
			USkeletalMesh* SkeletalMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(SkeletalMeshObjectInfo->PathName)).LoadSynchronous();
			if (SkeletalMesh)
			{
				PoseSearchMeshComponent->SetSkinnedAssetAndUpdate(SkeletalMesh, true);
			}
			FTransform ComponentWorldTransform;
			// Active skeleton is simply the traced bone transforms
			TArray<FTransform>& ComponentSpaceTransforms = PoseSearchMeshComponent->GetEditableComponentSpaceTransforms();
			AnimationProvider->GetSkeletalMeshComponentSpacePose(PoseMessage, *SkeletalMeshInfo, ComponentWorldTransform, ComponentSpaceTransforms);

			check(ComponentWorldTransform.Equals(PoseMessage.ComponentToWorld));

			if (!ComponentSpaceTransforms.IsEmpty())
			{
				RootBoneWorldTransform = ComponentSpaceTransforms[RootBoneIndexType] * ComponentWorldTransform;
			}
			else
			{
				RootBoneWorldTransform = ComponentWorldTransform;
			}

			PoseSearchMeshComponent->Initialize(ComponentWorldTransform);

			return TraceServices::EEventEnumerate::Stop;
		});
	});
}

const USkinnedMeshComponent* FDebuggerViewModel::GetMeshComponent() const
{
	return DebugDrawMeshComponent.Get();
}

void FDebuggerViewModel::OnWorldCleanup(UWorld* InWorld, bool bSessionEnded, bool bCleanupResources)
{
	bSkeletonInitialized = false;
}

} // namespace UE::PoseSearch
