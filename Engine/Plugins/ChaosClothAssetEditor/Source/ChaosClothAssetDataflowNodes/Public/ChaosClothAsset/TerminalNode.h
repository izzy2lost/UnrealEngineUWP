// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Dataflow/DataflowTerminalNode.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "ChaosClothAsset/ClothLodTransitionDataCache.h"
#include "TerminalNode.generated.h"

/** Refresh structure for push buton customization. */
USTRUCT()
struct FChaosClothAssetTerminalNodeRefreshAsset
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "Terminal Node Refresh Asset")
	bool bRefreshAsset = false;
};

/** Cloth terminal node to generate a cloth asset from a cloth collection. */
USTRUCT(Meta = (DataflowCloth, DataflowTerminal))
struct FChaosClothAssetTerminalNode_v2 : public FDataflowTerminalNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FChaosClothAssetTerminalNode_v2, "ClothAssetTerminal", "Cloth", "Cloth Terminal")  // TODO: Should the category be Terminal instead like all other terminal nodes
	DATAFLOW_NODE_RENDER_TYPE("SurfaceRender", FName("FClothCollection"), "Collection")

public:
	UPROPERTY()
	TArray<FManagedArrayCollection> CollectionLods;

	/**
	 * Refresh the asset even if the ClothCollection hasn't changed.
	 * Note that it is not required to manually refresh the cloth asset, this is done automatically when there is a change in the Dataflow.
	 * This function is a developper utility used for debugging.
	 */
	UPROPERTY(EditAnywhere, Category = "Cloth Asset Terminal")
	mutable FChaosClothAssetTerminalNodeRefreshAsset RefreshAsset;

	FChaosClothAssetTerminalNode_v2(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:
	//~ Begin FDataflowNode interface
	virtual void SetAssetValue(TObjectPtr<UObject> Asset, Dataflow::FContext& Context) const override;
	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override {}
	virtual TArray<Dataflow::FPin> AddPins() override;
	virtual bool CanAddPin() const override { return true; }
	virtual bool CanRemovePin() const override { return CollectionLods.Num() > NumInitialCollectionLods; }
	virtual TArray<Dataflow::FPin> GetPinsToRemove() const override;
	virtual void OnPinRemoved(const Dataflow::FPin& Pin) override;
	virtual void Serialize(FArchive& Ar) override;
	//~ End FDataflowNode interface

	TArray<TSharedRef<FManagedArrayCollection>> GetCleanedCollectionLodValues(Dataflow::FContext& Context) const;
	Dataflow::TConnectionReference<FManagedArrayCollection> GetConnectionReference(int32 Index) const;

	UPROPERTY()
	mutable TArray<FChaosClothAssetLodTransitionDataCache> LODTransitionDataCache;

	// This is for runtime only--used to determine if only properties need to be updated.
	mutable bool bClothCollectionChecksumValid = false;
	mutable uint32 ClothColllectionChecksum = 0;
	static constexpr int32 NumRequiredInputs = 0;
	static constexpr int32 NumInitialCollectionLods = 1;
};



/** Cloth terminal node to generate a cloth asset from a cloth collection. */
USTRUCT(Meta = (DataflowCloth, DataflowTerminal, Deprecated = 5.5))
struct FChaosClothAssetTerminalNode : public FDataflowTerminalNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FChaosClothAssetTerminalNode, "ClothAssetTerminal", "Cloth", "Cloth Terminal")  // TODO: Should the category be Terminal instead like all other terminal nodes
	DATAFLOW_NODE_RENDER_TYPE("SurfaceRender", FName("FClothCollection"), "Collection")

public:
	static constexpr int32 MaxLods = 6;  // Hardcoded number of LODs since it is currently not possible to use arrays for optional inputs

	/** LOD 0 input, right click on the node and add pins to add more LODs. */
	UPROPERTY(meta = (DataflowInput, DisplayName = "Collection LOD 0"))
	FManagedArrayCollection CollectionLod0;
	/** LOD 1 input, right click on the node and add pins to add more LODs. */
	UPROPERTY(meta = (DisplayName = "Collection LOD 1"))
	FManagedArrayCollection CollectionLod1;
	/** LOD 2 input, right click on the node and add pins to add more LODs. */
	UPROPERTY(meta = (DisplayName = "Collection LOD 2"))
	FManagedArrayCollection CollectionLod2;
	/** LOD 3 input, right click on the node and add pins to add more LODs. */
	UPROPERTY(meta = (DisplayName = "Collection LOD 3"))
	FManagedArrayCollection CollectionLod3;
	/** LOD 4 input, right click on the node and add pins to add more LODs. */
	UPROPERTY(meta = (DisplayName = "Collection LOD 4"))
	FManagedArrayCollection CollectionLod4;
	/** LOD 5 input, right click on the node and add pins to add more LODs. */
	UPROPERTY(meta = (DisplayName = "Collection LOD 5"))
	FManagedArrayCollection CollectionLod5;
	/** The number of LODs currently exposed to the node UI. */
	UPROPERTY()
	int32 NumLods = NumInitialCollectionLods;
	/**
	 * Refresh the asset even if the ClothCollection hasn't changed.
	 * Note that it is not required to manually refresh the cloth asset, this is done automatically when there is a change in the Dataflow.
	 * This function is a developper utility used for debugging.
	 */
	UPROPERTY(EditAnywhere, Category = "Cloth Asset Terminal")
	mutable FChaosClothAssetTerminalNodeRefreshAsset RefreshAsset;

	FChaosClothAssetTerminalNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

private:
	//~ Begin FDataflowNode interface
	virtual void SetAssetValue(TObjectPtr<UObject> Asset, Dataflow::FContext& Context) const override;
	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override {}
	virtual TArray<Dataflow::FPin> AddPins() override;
	virtual bool CanAddPin() const override { return NumLods < MaxLods; }
	virtual bool CanRemovePin() const override { return NumLods > 1; }
	virtual TArray<Dataflow::FPin> GetPinsToRemove() const override;
	virtual void OnPinRemoved(const Dataflow::FPin& Pin) override;
	virtual void Serialize(FArchive& Ar) override;
	//~ End FDataflowNode interface

	TArray<const FManagedArrayCollection*> GetCollectionLods() const;
	TArray<TSharedRef<FManagedArrayCollection>> GetCleanedCollectionLodValues(Dataflow::FContext& Context) const;
	const FManagedArrayCollection* GetCollectionLod(int32 LodIndex) const;

	UPROPERTY()
	mutable TArray<FChaosClothAssetLodTransitionDataCache> LODTransitionDataCache;

	// This is for runtime only--used to determine if only properties need to be updated.
	mutable bool bClothCollectionChecksumValid = false;
	mutable uint32 ClothColllectionChecksum = 0;

	static constexpr int32 NumRequiredInputs = 0;
	static constexpr int32 NumInitialCollectionLods = 1;
};
