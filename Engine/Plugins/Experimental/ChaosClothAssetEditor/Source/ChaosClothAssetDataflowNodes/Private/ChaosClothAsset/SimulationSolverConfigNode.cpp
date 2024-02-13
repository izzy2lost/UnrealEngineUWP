// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SimulationSolverConfigNode.h"
#include "Chaos/CollectionPropertyFacade.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimulationSolverConfigNode)

FChaosClothAssetSimulationSolverConfigNode::FChaosClothAssetSimulationSolverConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FChaosClothAssetSimulationBaseConfigNode(InParam, InGuid)
{
	RegisterCollectionConnections();
}

void FChaosClothAssetSimulationSolverConfigNode::AddProperties(FPropertyHelper& PropertyHelper) const
{
	PropertyHelper.SetProperty(this, &NumIterations);
	PropertyHelper.SetProperty(this, &MaxNumIterations);
	PropertyHelper.SetProperty(this, &NumSubsteps);
	const float DynamicSubstepDeltaTimeValue = bEnableDynamicSubstepping ? DynamicSubstepDeltaTime : 0.f;
	PropertyHelper.SetProperty(TEXT("DynamicSubstepDeltaTime"), DynamicSubstepDeltaTimeValue);
	PropertyHelper.SetPropertyBool(this, &bEnableNumSelfCollisionSubsteps);
	PropertyHelper.SetProperty(this, &NumSelfCollisionSubsteps);
	PropertyHelper.SetPropertyBool(this, &bEnableForceBasedSolver, {}, ECollectionPropertyFlags::Intrinsic);
	PropertyHelper.SetProperty(this, &NumNewtonIterations);
	PropertyHelper.SetProperty(this, &MaxNumCGIterations);
	PropertyHelper.SetProperty(this, &CGResidualTolerance);
	PropertyHelper.SetPropertyBool(this, &bDoQuasistatics);
}

void FChaosClothAssetSimulationSolverConfigNode::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar.UsingCustomVersion(FFortniteValkyrieBranchObjectVersion::GUID);
	if (Ar.IsLoading())
	{
		if (Ar.CustomVer(FFortniteValkyrieBranchObjectVersion::GUID) < FFortniteValkyrieBranchObjectVersion::ChaosClothAssetWeightedMassAndGravity)
		{
			if (!bEnableForceBasedSolver)
			{
				NumNewtonIterations = 0;
			}
		}
	}
}
