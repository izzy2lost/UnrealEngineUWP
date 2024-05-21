// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Dataflow/DataflowEditorPreviewSceneBase.h"
#include "Dataflow/DataflowSimulationGenerator.h"
#include "Chaos/CacheCollection.h"

#include "DataflowSimulationScene.generated.h"

class UDataflowEditor;

DECLARE_EVENT(UDataflowSimulationSceneDescription, FDataflowSimulationSceneDescriptionChanged)

UCLASS()
class DATAFLOWEDITOR_API UDataflowSimulationSceneDescription : public UObject
{
public:
	GENERATED_BODY()

	FDataflowSimulationSceneDescriptionChanged DataflowSimulationSceneDescriptionChanged;

	UDataflowSimulationSceneDescription()
	{
		SetFlags(RF_Transactional);
	}

	/** Set the simulation scene */
	void SetSimulationScene(class FDataflowSimulationScene* SimulationScene);

	/** Caching asset to be used to record the simulation  */
	UPROPERTY(EditAnywhere, Category="Caching")
	TObjectPtr<UChaosCacheCollection> CacheAsset = nullptr;

	/** Number of sampling frames per second for caching*/
	UPROPERTY(EditAnywhere, Category="Caching")
	int32 SamplingRate = 30;

private:

	//~ UObject Interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostTransacted(const FTransactionObjectEvent& TransactionEvent) override;

	/** Simulation scene linked to that descriptor */
	class FDataflowSimulationScene* SimulationScene;
};


/**
 * Dataflow simulation scene holding all the dataflow content components
 */
class DATAFLOWEDITOR_API FDataflowSimulationScene : public FDataflowPreviewSceneBase
{
public:

	FDataflowSimulationScene(FPreviewScene::ConstructionValues ConstructionValues, UDataflowEditor* Editor);
	
	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	
	virtual ~FDataflowSimulationScene();

	/** Tick data flow scene */
	virtual void TickDataflowScene(const float DeltaSeconds) override;
	
	/** Check if the preview scene can run simulation */
	virtual bool CanRunSimulation() const { return true; }

	/** Get the scene description used in the preview scene widget */
	UDataflowSimulationSceneDescription* GetPreviewSceneDescription() const { return SimulationSceneDescription; }

	/** Create all the simulation world components and instances */
	void CreateSimulationWorld();

	/** Reset all the simulation world components and instances */
	void ResetSimulationWorld();

	/** Reset the simulation scene */
	void ResetSimulationScene() {}

	/** Check if there is something to render */
	bool HasRenderableGeometry() { return true; }

	/** Update Scene in response to the SceneDescription changing */
	void SceneDescriptionPropertyChanged(const FName& PropertyName);

	/** Update the simulation cache */
	void UpdateSimulationCache();

	/** Get the simulation time range */
	const FVector2f& GetTimeRange() const {return TimeRange;}

	/** Get the number of frames */
	const int32& GetNumFrames() const {return NumFrames;}

	/** Simulation time used to drive the cache loading */
	float SimulationTime;
	
private:

	/** Bind the scene selection to the components */
	void BindSceneSelection();

	/** Unbind the scene selection from the components */
	void UnbindSceneSelection();
	
	/** Simulation scene description */
	TObjectPtr<UDataflowSimulationSceneDescription> SimulationSceneDescription;

	/** Simulation generator to record the simulation result */
	TSharedPtr<Dataflow::FDataflowSimulationGenerator> SimulationGenerator;

	/** Simulation context used for the simulation graph */
	TSharedPtr<Dataflow::FSimulationContext> SimulationContext;

	/** Cache time range in seconds */
	FVector2f TimeRange;

	/** Number of cache frames */
	int32 NumFrames;

	/** Last context time stamp for which we regenerated the world */
	Dataflow::FTimestamp LastTimeStamp = Dataflow::FTimestamp::Invalid;
};




