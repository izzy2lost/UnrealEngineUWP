// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SimulationSelfCollisionSpheresConfigNode.h"
#include "ChaosClothAsset/SimulationBaseConfigNodePrivate.h"
#include "Chaos/CollectionPropertyFacade.h"

#include "ChaosClothAsset/CollectionClothFacade.h"
#include "ChaosClothAsset/CollectionClothSelectionFacade.h"
#include "ChaosClothAsset/ClothCollectionGroup.h"
#include "ChaosClothAsset/ClothGeometryTools.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimulationSelfCollisionSpheresConfigNode)

FChaosClothAssetSimulationSelfCollisionSpheresConfigNode::FChaosClothAssetSimulationSelfCollisionSpheresConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FChaosClothAssetSimulationBaseConfigNode(InParam, InGuid)
{
	RegisterCollectionConnections();
}

void FChaosClothAssetSimulationSelfCollisionSpheresConfigNode::AddProperties(Dataflow::FContext& Context, ::Chaos::Softs::FCollectionPropertyMutableFacade& Properties) const
{
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTY(SelfCollisionSphereRadius);
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTY(SelfCollisionSphereRadiusCullMultiplier);
	UE_CHAOS_CLOTHASSET_SIMULATIONCONFIG_SETPROPERTY(SelfCollisionSphereStiffness); 
}

void FChaosClothAssetSimulationSelfCollisionSpheresConfigNode::EvaluateClothCollection(Dataflow::FContext& Context, const TSharedRef<FManagedArrayCollection>& ClothCollection) const
{
	using namespace UE::Chaos::ClothAsset;
	FCollectionClothConstFacade Cloth(ClothCollection);
	const float CullDiameterSq = FMath::Square(SelfCollisionSphereRadius * SelfCollisionSphereRadiusCullMultiplier * 2.f);
	if (Cloth.IsValid() && CullDiameterSq > 0.f)
	{
		TConstArrayView<FVector3f> SimPositions = Cloth.GetSimPosition3D();
		TSet<int32> VertexSet;

		FClothGeometryTools::SampleVertices(SimPositions, CullDiameterSq, VertexSet);

		FCollectionClothSelectionFacade Selection(ClothCollection);
		Selection.DefineSchema();

		static const FName SelectionSetName(TEXT("_SelfCollisionSpheres"));
		Selection.FindOrAddSelectionSet(SelectionSetName, ClothCollectionGroup::SimVertices3D) = VertexSet;
	}

}
