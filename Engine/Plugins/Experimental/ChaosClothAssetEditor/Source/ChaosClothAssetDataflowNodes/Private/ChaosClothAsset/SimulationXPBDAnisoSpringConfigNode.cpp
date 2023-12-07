// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SimulationXPBDAnisoSpringConfigNode.h"
#include "ChaosClothAsset/SimulationBaseConfigNodePrivate.h"
#include "Chaos/CollectionPropertyFacade.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimulationXPBDAnisoSpringConfigNode)

FChaosClothAssetSimulationXPBDAnisoSpringConfigNode::FChaosClothAssetSimulationXPBDAnisoSpringConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FChaosClothAssetSimulationBaseConfigNode(InParam, InGuid)
{
	RegisterCollectionConnections();
	RegisterInputConnection(&XPBDAnisoSpringStiffnessWarp.WeightMap);
	RegisterInputConnection(&XPBDAnisoSpringStiffnessWeft.WeightMap);
	RegisterInputConnection(&XPBDAnisoSpringStiffnessBias.WeightMap);
	RegisterInputConnection(&XPBDAnisoSpringDamping.WeightMap);
	RegisterInputConnection(&XPBDAnisoSpringWarpScale.WeightMap);
	RegisterInputConnection(&XPBDAnisoSpringWeftScale.WeightMap);
}

void FChaosClothAssetSimulationXPBDAnisoSpringConfigNode::AddProperties(Dataflow::FContext& Context, ::Chaos::Softs::FCollectionPropertyMutableFacade& Properties) const
{
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTYBOOL(XPBDAnisoSpringUse3dRestLengths);
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTYWEIGHTED(XPBDAnisoSpringStiffnessWarp);
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTYWEIGHTED(XPBDAnisoSpringStiffnessWeft);
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTYWEIGHTED(XPBDAnisoSpringStiffnessBias);
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTYWEIGHTED(XPBDAnisoSpringDamping);
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTYWEIGHTED(XPBDAnisoSpringWarpScale);
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTYWEIGHTED(XPBDAnisoSpringWeftScale);
}
