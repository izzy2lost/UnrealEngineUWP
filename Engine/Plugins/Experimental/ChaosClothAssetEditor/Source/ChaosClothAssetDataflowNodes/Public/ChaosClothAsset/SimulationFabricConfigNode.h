// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "ChaosClothAsset/SimulationBaseConfigNode.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "SimulationFabricConfigNode.generated.h"

/** Cloth configuration base node to import properties from fabrics . */
USTRUCT(Meta = (Abstract))
struct FChaosClothAssetSimulationFabricConfigNode : public FChaosClothAssetSimulationBaseConfigNode
{
	GENERATED_USTRUCT_BODY()

public:
	
	/** Import properties to boolean the  fabric override */
	UPROPERTY(EditAnywhere, Category = "Fabric Properties")
	bool bImportProperties = false;
	
	FChaosClothAssetSimulationFabricConfigNode() = default;
	
	FChaosClothAssetSimulationFabricConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());
	
protected :

	/** We can only use fabrics to set property values if the import settings is true and if the cloth facade has fabrics */
	bool CanUseFabrics(const UE::Chaos::ClothAsset::FCollectionClothFacade& ClothFacade) const
	{
		return bImportProperties && ClothFacade.IsValid() && (ClothFacade.GetNumFabrics() > 0);
	}
	
	/** Set an imported fabric value onto the weighted value property (animatable or not) */
	template<typename PropertyType>
	static void SetFabricPropertyWeighted(const FName& PropertyName, const PropertyType& PropertyValue, UE::Chaos::ClothAsset::FCollectionClothFacade& ClothFacade,
		FPropertyHelper& PropertyHelper, const TFunction<float(const UE::Chaos::ClothAsset::FCollectionClothFabricFacade&)>& FabricValueFunction,
		const TArray<FName>& SimilarPropertyNames, ECollectionPropertyFlags PropertyFlags = ECollectionPropertyFlags::None);
	
	/** Set an imported fabric value onto the float property */
	static void SetFabricProperty(const FName& PropertyName, UE::Chaos::ClothAsset::FCollectionClothFacade& ClothFacade,
		FPropertyHelper& PropertyHelper, const TFunction<float(const UE::Chaos::ClothAsset::FCollectionClothFabricFacade&)>& FabricValueFunction,
		const TArray<FName>& SimilarPropertyNames, ECollectionPropertyFlags PropertyFlags = ECollectionPropertyFlags::None);
};

