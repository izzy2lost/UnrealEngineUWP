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
}

void FChaosClothAssetSimulationSelfCollisionConfigNode::AddProperties(FPropertyHelper& PropertyHelper) const
{
	PropertyHelper.SetPropertyBool(FName("UseSelfCollisions"), true);
	PropertyHelper.SetProperty(this, &SelfCollisionThickness);
	PropertyHelper.SetProperty(this, &SelfCollisionStiffness);
	PropertyHelper.SetProperty(this, &SelfCollisionFriction);
	PropertyHelper.SetProperty(this, &SelfCollisionDisableNeighborDistance, {}, ECollectionPropertyFlags::None); // Non animatable
	PropertyHelper.SetPropertyString(this, &SelfCollisionLayers);

	PropertyHelper.SetPropertyBool(this, &bUseSelfIntersections);
	PropertyHelper.SetPropertyBool(this, &bUseGlobalIntersectionAnalysis);
	PropertyHelper.SetPropertyBool(this, &bUseContourMinimization);
	PropertyHelper.SetProperty(this, &NumContourMinimizationPostSteps);
	PropertyHelper.SetPropertyBool(this, &bUseGlobalPostStepContours);
	PropertyHelper.SetProperty(this, &SelfCollisionProximityStiffness);
}
