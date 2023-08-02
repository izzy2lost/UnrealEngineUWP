// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "MoviePipelineRenderLayerSubsystem.generated.h"

/**
 * EXPERIMENTAL
 */

/**
 * Generates a collection of actors via provided queries.
 */
UCLASS(BlueprintType)
class UMoviePipelineCollection : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Settings")
	TArray<AActor*> GetMatchingActors(const UWorld* World, const bool bInvertResult = false) const;

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void AddQuery(UMoviePipelineCollectionQuery* Query);

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void SetCollectionName(const FString& InName) { CollectionName = InName; }

	UFUNCTION(BlueprintCallable, Category = "Settings")
	FString GetCollectionName() const { return CollectionName; }

private:
	UPROPERTY()
	FString CollectionName;

	UPROPERTY()
	TArray<TObjectPtr<UMoviePipelineCollectionQuery>> Queries;
};

/**
 * Base class for providing actor modification functionality via collections.
 */
UCLASS(Abstract, EditInlineNew)
class UMoviePipelineCollectionModifier : public UObject
{
	GENERATED_BODY()

public:
	/** Adds a collection to the existing set of collections in this modifier. */
	UFUNCTION(BlueprintCallable, Category = "Settings")
	void AddCollection(UMoviePipelineCollection* Collection);

	/** Overwrites the existing collections with the provided array of collections. */
	UFUNCTION(BlueprintCallable, Category = "Settings")
	void SetCollections(const TArray<UMoviePipelineCollection*> InCollections) { Collections = InCollections; }
	
	UFUNCTION(BlueprintCallable, Category = "Settings")
	TArray<UMoviePipelineCollection*> GetCollections() const { return Collections; }

	virtual void ApplyModifier(const UWorld* World) PURE_VIRTUAL(UMoviePipelineCollectionModifier::ApplyModifier, );
	virtual void UndoModifier() PURE_VIRTUAL(UMoviePipelineCollectionModifier::UndoModifier, );

public:
	/** Whether an inverted collection of actors should be used instead. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	bool bUseInvertedActors = false;

protected:
	/** The collections which this modifier will operate on. */
	UPROPERTY()
	TArray<TObjectPtr<UMoviePipelineCollection>> Collections;
};

/**
 * Modifies actor materials.
 */
UCLASS(BlueprintType)
class UMoviePipelineMaterialModifier : public UMoviePipelineCollectionModifier
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Settings")
	void SetMaterial(TSoftObjectPtr<UMaterialInterface> InMaterial) { MaterialToApply = InMaterial; }

	UFUNCTION(BlueprintCallable, Category = "Settings")
	virtual void ApplyModifier(const UWorld* World) override;

	UFUNCTION(BlueprintCallable, Category = "Settings")
	virtual void UndoModifier() override;

private:
	typedef TTuple<int32, TSoftObjectPtr<UMaterialInterface>> FMaterialSlotAssignment;
	typedef TMap<TSoftObjectPtr<UPrimitiveComponent>, TArray<FMaterialSlotAssignment>> FComponentToMaterialMap;
	
	/** Maps a component to its original material assignments (per index). */
	FComponentToMaterialMap ModifiedComponents;

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	TSoftObjectPtr<UMaterialInterface> MaterialToApply;
};

/**
 * Modifies actor visibility.
 */
UCLASS(BlueprintType)
class UMoviePipelineVisibilityModifier : public UMoviePipelineCollectionModifier
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Settings")
	void SetHidden(const bool bInIsHidden) { bIsHidden = bInIsHidden; }

	UFUNCTION(BlueprintCallable, Category = "Settings")
	bool IsHidden() const { return bIsHidden; }

	UFUNCTION(BlueprintCallable, Category = "Settings")
	virtual void ApplyModifier(const UWorld* World) override;

	UFUNCTION(BlueprintCallable, Category = "Settings")
	virtual void UndoModifier() override;

private:
	void SetActorHiddenState(AActor* Actor, const bool bInIsHidden) const;

private:
	/** Maps an actor to its original hidden state. */
	UPROPERTY(Transient)
	TMap<TSoftObjectPtr<AActor>, bool> ModifiedActors;

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	bool bIsHidden = false;
};

/**
 * Base class for providing the ability to determine if an actor matches a query.
 */
UCLASS(Abstract, EditInlineNew)
class UMoviePipelineCollectionQuery : public UObject
{
	GENERATED_BODY()

public:
	virtual bool DoesActorMatchQuery(const AActor* Actor) const PURE_VIRTUAL(UMoviePipelineCollectionQuery::DoesActorMatchQuery, return false; );  
};

UENUM(BlueprintType)
enum class EMoviePipelineCollectionCommonQueryMode : uint8
{
	And UMETA(ToolTip = "All specifiers in the query must be true"),
	Or UMETA(ToolTip = "At least one specifier in the query must be true")
};

/**
 * Provides common actor querying functionality (names, tags, components, etc). These individual sub-queries can be
 * AND'd or OR'd together (eg, matches provided names OR provided tags, vs matches provided names AND provided tags).
 */
UCLASS(BlueprintType)
class UMoviePipelineCollectionCommonQuery : public UMoviePipelineCollectionQuery
{
	GENERATED_BODY()

public:
	// TODO: Add other common query operations (level, etc)
	UFUNCTION(BlueprintCallable, Category = "Settings")
	void SetActorNames(const TArray<FString>& InActorNames) { ActorNames = InActorNames; }

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void SetTags(const TArray<FName>& InTags) { Tags = InTags; }

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void SetComponentTypes(TArray<UClass*> InComponentTypes) { ComponentTypes = InComponentTypes; }

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void SetQueryMode(const EMoviePipelineCollectionCommonQueryMode InQueryMode) { QueryMode = InQueryMode; }

	UFUNCTION(BlueprintCallable, Category = "Settings")
	virtual bool DoesActorMatchQuery(const AActor* Actor) const override;

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General")
	TArray<TObjectPtr<UClass>> ComponentTypes;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General")
	EMoviePipelineCollectionCommonQueryMode QueryMode = EMoviePipelineCollectionCommonQueryMode::And;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General")
	TArray<FString> ActorNames;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General")
	TArray<FName> Tags;
};

/** Generates a query that matches all lighting components. */
UCLASS(BlueprintType)
class UMoviePipelineCollectionLightingQuery : public UMoviePipelineCollectionQuery
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Settings")
	virtual bool DoesActorMatchQuery(const AActor* Actor) const override;
};

/**
 * Provides a means of assembling modifiers together to generate a desired view of a scene. 
 */
UCLASS(BlueprintType)
class UMoviePipelineRenderLayer : public UObject
{
	GENERATED_BODY()

public:
	UMoviePipelineRenderLayer() = default;

	UFUNCTION(BlueprintCallable, Category = "Settings")
	FName GetRenderLayerName() const { return RenderLayerName; };

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void SetRenderLayerName(const FName& NewName) { RenderLayerName = NewName; }

	UFUNCTION(BlueprintCallable, Category = "Settings")
	UMoviePipelineCollection* GetCollectionByName(const FString& Name) const;

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void AddModifier(UMoviePipelineCollectionModifier* Modifier);

	UFUNCTION(BlueprintCallable, Category = "Settings")
	TArray<UMoviePipelineCollectionModifier*> GetModifiers() const { return Modifiers; };

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void RemoveModifier(UMoviePipelineCollectionModifier* Modifier);

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void Preview(const UWorld* World);

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void UndoPreview(const UWorld* World);

private:
	/** The name of this render layer. */
	UPROPERTY()
	FName RenderLayerName;

	/** The modifiers that are active when this render layer is active. */
	UPROPERTY()
	TArray<TObjectPtr<UMoviePipelineCollectionModifier>> Modifiers;
};

/**
 * The primary means of controlling render layers in MRQ. Render layers can be added/registered with the subsystem, then
 * made active in order to view them. Collections and modifiers can also be viewed, but they do not need to be added to
 * the subsystem ahead of time.
 */
UCLASS()
class MOVIERENDERPIPELINECORE_API UMoviePipelineRenderLayerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UMoviePipelineRenderLayerSubsystem() = default;

	/* Get this subsystem for a specific world. Handy for use from Python. */
	UFUNCTION(BlueprintCallable, Category = "Settings")
	static UMoviePipelineRenderLayerSubsystem* GetFromWorld(const UWorld* World);

	//~ Begin USubsystem interface
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem interface

	/** Clear out all tracked render layers and collections. */
	UFUNCTION(BlueprintCallable, Category = "Settings")
	void Reset();

	/**
	 * Adds a render layer to the system, which can later be made active by SetActiveRenderLayer*(). Returns true
	 * if the layer was added successfully, else false.
	 */
	UFUNCTION(BlueprintCallable, Category = "Settings")
	bool AddRenderLayer(UMoviePipelineRenderLayer* RenderLayer);

	UFUNCTION(BlueprintCallable, Category = "Settings")
	const TArray<UMoviePipelineRenderLayer*>& GetRenderLayers() { return RenderLayers; }

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void RemoveRenderLayer(const FString& RenderLayerName);

	UFUNCTION(BlueprintCallable, Category = "Settings")
	UMoviePipelineRenderLayer* GetActiveRenderLayer() const { return ActiveRenderLayer; }

	/** Previews the layer with the given name. The layer needs to have been registered with AddRenderLayer(). */
	UFUNCTION(BlueprintCallable, Category = "Settings")
	void SetActiveRenderLayerByName(const FName& RenderLayerName);

	/** Previews the given layer. The layer does not need to have been registered with AddRenderLayer(). */
	UFUNCTION(BlueprintCallable, Category = "Settings")
	void SetActiveRenderLayerByObj(UMoviePipelineRenderLayer* RenderLayer);

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void ClearActiveRenderLayer();

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void PreviewCollection(UMoviePipelineCollection* Collection);

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void ClearCollectionPreview();

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void PreviewModifier(UMoviePipelineCollectionModifier* Modifier);

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void ClearModifierPreview();

private:
	/** Clears the render layer, collection, and modifier previews if any of them are active. */
	void ClearAllPreviews();

	/** Sets the active render layer and previews it. */
	void SetAndPreviewRenderLayer(UMoviePipelineRenderLayer* RenderLayer);

private:
	/** Render layers which have been added/registered with the subsystem. These can be found by name. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMoviePipelineRenderLayer>> RenderLayers;

	/** The render layer that is currently being viewed/previewed. */
	UPROPERTY(Transient)
	TObjectPtr<UMoviePipelineRenderLayer> ActiveRenderLayer = nullptr;

	/** The collection that is currently being viewed/previewed. */
	UPROPERTY(Transient)
	TObjectPtr<UMoviePipelineCollection> ActiveCollection = nullptr;

	/** The modifier that is currently being viewed/previewed. */
	UPROPERTY(Transient)
	TObjectPtr<UMoviePipelineCollectionModifier> ActiveModifier = nullptr;

	/** A render layer dedicated to visualizing collections and modifiers. */
	UPROPERTY(Transient)
	TObjectPtr<UMoviePipelineRenderLayer> VisualizationRenderLayer = nullptr;

	/** Empty collection used for visualization purposes (in conjunction w/ the viz render layer). */
	UPROPERTY(Transient)
	TObjectPtr<UMoviePipelineCollection> VisualizationEmptyCollection = nullptr;

	/** A modifier used for visualization purposes (to hide the entire world). */
	UPROPERTY(Transient)
	TObjectPtr<UMoviePipelineVisibilityModifier> VisualizationModifier_HideWorld = nullptr;

	/** A modifier used for visualization purposes (to show collections used with the modifier). */
	UPROPERTY(Transient)
	TObjectPtr<UMoviePipelineVisibilityModifier> VisualizationModifier_VisibleCollections = nullptr;
};
