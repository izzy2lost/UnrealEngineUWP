// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigidBodyPoseData.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimInstanceProxy.h"

namespace UE
{
namespace PhysicsControl
{

//======================================================================================================================
void FRigidBodyPoseData::Update(
	FComponentSpacePoseContext&    ComponentSpacePoseContext,
	const TArray<FOutputBoneData>& OutputBoneData,
	const ESimulationSpace         SimulationSpace,
	const FBoneReference&          BaseBoneRef,
	const FGraphTraversalCounter&  InUpdateCounter)
{
	ExpectedUpdateCounter = UpdateCounter;
	ExpectedUpdateCounter.Increment();
	UpdateCounter = InUpdateCounter;

	const FTransform CompWorldSpaceTM = ComponentSpacePoseContext.AnimInstanceProxy->GetComponentTransform();
	const FBoneContainer& BoneContainer = ComponentSpacePoseContext.Pose.GetPose().GetBoneContainer();
	const FTransform BaseBoneTM = ComponentSpacePoseContext.Pose.GetComponentSpaceTransform(
		BaseBoneRef.GetCompactPoseIndex(BoneContainer));

	for (const FOutputBoneData& OutputData : OutputBoneData)
	{
		const int32 BodyIndex = OutputData.BodyIndex;
		if (ensure(BoneTMs.IsValidIndex(BodyIndex)))
		{
			const FTransform& ComponentSpaceTM = ComponentSpacePoseContext.Pose.GetComponentSpaceTransform(OutputData.CompactPoseBoneIndex);
			const FTransform BodyTM = ConvertCSTransformToSimSpace(SimulationSpace, ComponentSpaceTM, CompWorldSpaceTM, BaseBoneTM);
			BoneTMs[BodyIndex] = BodyTM;
		}
	}
}

} // namespace PhysicsControl
} // namespace UE


