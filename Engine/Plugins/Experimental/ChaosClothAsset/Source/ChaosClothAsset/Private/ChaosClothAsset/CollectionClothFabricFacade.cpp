// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/CollectionClothFabricFacade.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "ChaosClothAsset/ClothCollection.h"
#include "ChaosClothAsset/ClothCollectionGroup.h"

namespace UE::Chaos::ClothAsset
{
	FCollectionClothFabricConstFacade::FAnisotropicData::FAnisotropicData(const float WeftValue, const float WarpValue, const float BiasValue) :
		Weft(WeftValue), Warp(WarpValue), Bias(BiasValue)
	{}
			
	FCollectionClothFabricConstFacade::FAnisotropicData::FAnisotropicData(const FVector3f& VectorDatas) :
		Weft(VectorDatas.X), Warp(VectorDatas.Y), Bias(VectorDatas.Z) 
	{}

	FCollectionClothFabricConstFacade::FAnisotropicData::FAnisotropicData(const float& FloatDatas) :
		Weft(FloatDatas), Warp(FloatDatas), Bias(FloatDatas)
	{}
			
	FVector3f FCollectionClothFabricConstFacade::FAnisotropicData::GetVectorDatas() const
	{
		return FVector3f(Weft, Warp, Bias);
	}
	
	FCollectionClothFabricConstFacade::FAnisotropicData FCollectionClothFabricConstFacade::GetXPBDAnisoBendingStiffness() const
	{
		return FCollectionClothFabricConstFacade::FAnisotropicData(
			ClothCollection->GetElements(ClothCollection->GetFabricBendingStiffness())[GetElementIndex()]);
	}
	
	float FCollectionClothFabricConstFacade::GetXPBDAnisoBucklingRatio() const
	{
		return ClothCollection->GetElements(ClothCollection->GetFabricBucklingRatio())[GetElementIndex()];
	}
	
	FCollectionClothFabricConstFacade::FAnisotropicData FCollectionClothFabricConstFacade::GetXPBDAnisoBucklingStiffness() const
	{
		return FCollectionClothFabricConstFacade::FAnisotropicData(
			ClothCollection->GetElements(ClothCollection->GetFabricBucklingStiffness())[GetElementIndex()]);
	}
	
	FCollectionClothFabricConstFacade::FAnisotropicData FCollectionClothFabricConstFacade::GetXPBDAnisoSpringStiffness() const
	{
		return FCollectionClothFabricConstFacade::FAnisotropicData(
			ClothCollection->GetElements(ClothCollection->GetFabricStretchStiffness())[GetElementIndex()]);
	}
	
	float FCollectionClothFabricConstFacade::GetDensityWeighted() const
	{
		return ClothCollection->GetElements(ClothCollection->GetFabricDensity())[GetElementIndex()];
	}
	
	float FCollectionClothFabricConstFacade::GetCollisionThickness() const
	{
		return ClothCollection->GetElements(ClothCollection->GetFabricThickness())[GetElementIndex()];
	}
	
	float FCollectionClothFabricConstFacade::GetXPBDAnisoDamping() const
	{
		return ClothCollection->GetElements(ClothCollection->GetFabricDamping())[GetElementIndex()];
	}
	
	float FCollectionClothFabricConstFacade::GetFrictionCoefficient() const
	{
		return ClothCollection->GetElements(ClothCollection->GetFabricFriction())[GetElementIndex()];
	}
	
	FCollectionClothFabricConstFacade::FCollectionClothFabricConstFacade(const TSharedRef<const FClothCollection>& ClothCollection, int32 InFabricIndex)
		: ClothCollection(ClothCollection)
		, FabricIndex(InFabricIndex)
	{
		check(ClothCollection->IsValid());
		check(FabricIndex >= 0 && FabricIndex < ClothCollection->GetNumElements(ClothCollectionGroup::Fabrics));
	}

	void FCollectionClothFabricFacade::Initialize(const FAnisotropicData& BendingStiffness, const float BucklingRatio,
			const FAnisotropicData& BucklingStiffness, const FAnisotropicData& StretchStiffness,
			const float ClothDensity, const float ClothFriction, const float ClothDamping, const float ClothThickness)
	{
		const int32 ElementIndex = GetElementIndex();
		check((ElementIndex >= 0) && (ElementIndex < ClothCollection->GetNumElements(ClothCollectionGroup::Fabrics)));
		
		const TSharedRef<class FClothCollection> ClothCol = GetClothCollection();
		
		ClothCol->GetElements(ClothCol->GetFabricBendingStiffness())[ElementIndex] = BendingStiffness.GetVectorDatas();
		ClothCol->GetElements(ClothCol->GetFabricBucklingRatio())[ElementIndex] = BucklingRatio;
		ClothCol->GetElements(ClothCol->GetFabricStretchStiffness())[ElementIndex] = StretchStiffness.GetVectorDatas();
		ClothCol->GetElements(ClothCol->GetFabricBucklingStiffness())[ElementIndex] = BucklingStiffness.GetVectorDatas();
		ClothCol->GetElements(ClothCol->GetFabricFriction())[ElementIndex] = ClothFriction;
		ClothCol->GetElements(ClothCol->GetFabricDensity())[ElementIndex] = ClothDensity;
		ClothCol->GetElements(ClothCol->GetFabricThickness())[ElementIndex] = ClothThickness;
		ClothCol->GetElements(ClothCol->GetFabricDamping())[ElementIndex] = ClothDamping;
	}
	
	void FCollectionClothFabricFacade::Initialize(const FCollectionClothFabricConstFacade& OtherFabricFacade)
	{
		Initialize( OtherFabricFacade.GetXPBDAnisoBendingStiffness(), OtherFabricFacade.GetXPBDAnisoBucklingRatio(), OtherFabricFacade.GetXPBDAnisoBucklingStiffness(),
			OtherFabricFacade.GetXPBDAnisoSpringStiffness(), OtherFabricFacade.GetDensityWeighted(),
			OtherFabricFacade.GetFrictionCoefficient(), OtherFabricFacade.GetXPBDAnisoDamping(), OtherFabricFacade.GetCollisionThickness());
	}

	FCollectionClothFabricFacade::FCollectionClothFabricFacade(const TSharedRef<FClothCollection>& ClothCollection, int32 InFabricIndex)
		: FCollectionClothFabricConstFacade(ClothCollection, InFabricIndex)
	{
	}

	void FCollectionClothFabricFacade::Reset()
	{
		SetDefaults();
	}

	void FCollectionClothFabricFacade::SetDefaults()
	{
		Initialize(100.0f, 0.5f, 50.0f, 100.0f, 0.35f, 0.1f, 0.01f, 0.8f);
	}
}  // End namespace UE::Chaos::ClothAsset
