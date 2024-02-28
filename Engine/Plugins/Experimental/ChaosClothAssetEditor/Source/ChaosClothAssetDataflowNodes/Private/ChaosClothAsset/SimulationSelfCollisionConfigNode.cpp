// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SimulationSelfCollisionConfigNode.h"
#include "Dataflow/DataflowInputOutput.h"
#include "Chaos/CollectionPropertyFacade.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimulationSelfCollisionConfigNode)

FChaosClothAssetSimulationSelfCollisionConfigNode::FChaosClothAssetSimulationSelfCollisionConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FChaosClothAssetSimulationBaseConfigNode(InParam, InGuid)
{
	RegisterCollectionConnections();
	RegisterInputConnection(&SelfCollisionLayers.StringValue, GET_MEMBER_NAME_CHECKED(FChaosClothAssetConnectableIStringValue, StringValue));
	RegisterInputConnection(&SelfCollisionDisabledFaces.StringValue, GET_MEMBER_NAME_CHECKED(FChaosClothAssetConnectableIStringValue, StringValue));
	RegisterInputConnection(&SelfCollisionEnabledKinematicFaces.StringValue, GET_MEMBER_NAME_CHECKED(FChaosClothAssetConnectableIStringValue, StringValue));
	RegisterInputConnection(&SelfCollisionKinematicColliderFrictionWeighted.WeightMap);
}

void FChaosClothAssetSimulationSelfCollisionConfigNode::AddProperties(FPropertyHelper& PropertyHelper) const
{
	PropertyHelper.SetPropertyBool(FName("UseSelfCollisions"), true);
	PropertyHelper.SetProperty(this, &SelfCollisionThickness);
	PropertyHelper.SetProperty(this, &SelfCollisionStiffness);
	PropertyHelper.SetProperty(this, &SelfCollisionFriction);
	PropertyHelper.SetProperty(this, &SelfCollisionDisableNeighborDistance, {}, ECollectionPropertyFlags::None); // Non animatable
	PropertyHelper.SetPropertyString(this, &SelfCollisionLayers);
	PropertyHelper.SetPropertyString(this, &SelfCollisionDisabledFaces);
	PropertyHelper.SetPropertyBool(this, &bSelfCollideAgainstKinematicCollidersOnly);
	PropertyHelper.SetPropertyBool(this, &bSelfCollideAgainstAllKinematicVertices);
	PropertyHelper.SetPropertyString(this, &SelfCollisionEnabledKinematicFaces);
	PropertyHelper.SetProperty(this, &SelfCollisionKinematicColliderThickness);
	PropertyHelper.SetProperty(this, &SelfCollisionKinematicColliderStiffness);
	PropertyHelper.SetPropertyWeighted(TEXT("SelfCollisionKinematicColliderFriction"), SelfCollisionKinematicColliderFrictionWeighted);

	PropertyHelper.SetPropertyBool(this, &bUseSelfIntersections);
	PropertyHelper.SetPropertyBool(this, &bUseGlobalIntersectionAnalysis);
	PropertyHelper.SetPropertyBool(this, &bUseContourMinimization);
	PropertyHelper.SetProperty(this, &NumContourMinimizationPostSteps);
	PropertyHelper.SetPropertyBool(this, &bUseGlobalPostStepContours);
	PropertyHelper.SetProperty(this, &SelfCollisionProximityStiffness);
}

void FChaosClothAssetSimulationSelfCollisionConfigNode::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	if (Ar.IsLoading())
	{
#if WITH_EDITORONLY_DATA
		if (SelfCollisionKinematicColliderFriction_DEPRECATED != FrictionDeprecatedValue)
		{
			SelfCollisionKinematicColliderFrictionWeighted.Low = SelfCollisionKinematicColliderFrictionWeighted.High = SelfCollisionKinematicColliderFriction_DEPRECATED;
			SelfCollisionKinematicColliderFriction_DEPRECATED = FrictionDeprecatedValue;
		}
#endif
	}
}