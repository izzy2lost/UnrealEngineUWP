// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ChaosVDCollisionDataProviderInterface.h"
#include "ChaosVDGeometryDataComponent.h"
#include "ChaosVDSceneObjectBase.h"
#include "Chaos/Core.h"
#include "DataWrappers/ChaosVDParticleDataWrapper.h"
#include "DataWrappers/ChaosVDCollisionDataWrappers.h"
#include "GameFramework/Actor.h"
#include "Visualizers/ChaosVDDataVisualizerBase.h"
#include "Visualizers/IChaosVDParticleVisualizationDataProvider.h"

#include "ChaosVDParticleActor.generated.h"

class FChaosVDParticleActorCustomization;
enum class EChaosVDParticleDataVisualizationFlags : uint32;
class FChaosVDScene;
struct FChaosVDParticleDebugData;
class UMeshComponent;
class USceneComponent;
class UStaticMeshComponent;
class UStaticMesh;

namespace Chaos
{
	class FImplicitObject;
}

/** Options flags to control how geometry is updated in a ChaosVDActor */
UENUM()
enum class EChaosVDActorGeometryUpdateFlags : int32
{
	None = 0,
	ForceUpdate = 1 << 0
};
ENUM_CLASS_FLAGS(EChaosVDActorGeometryUpdateFlags)

DECLARE_DELEGATE(FChaosVDParticleDataUpdatedDelegate)

/** Actor used to represent a Chaos Particle in the Visual Debugger's world */
UCLASS(HideCategories=(Transform))
class AChaosVDParticleActor : public AActor, public IChaosVDParticleVisualizationDataProvider, public IChaosVDVisualizerContainerInterface,
								public FChaosVDSceneObjectBase, public IChaosVDCollisionDataProviderInterface
{
	GENERATED_BODY()

public:
	AChaosVDParticleActor(const FObjectInitializer& ObjectInitializer);

	void UpdateFromRecordedParticleData(const TSharedPtr<FChaosVDParticleDataWrapper>& InRecordedData, const Chaos::FRigidTransform3& SimulationTransform);

	void UpdateGeometry(const Chaos::FConstImplicitObjectPtr& InImplicitObject, EChaosVDActorGeometryUpdateFlags OptionsFlags = EChaosVDActorGeometryUpdateFlags::None);

	void UpdateGeometry(uint32 NewGeometryHash, EChaosVDActorGeometryUpdateFlags OptionsFlags = EChaosVDActorGeometryUpdateFlags::None);

	virtual void SetScene(TWeakPtr<FChaosVDScene> InScene) override;

	virtual void BeginDestroy() override;

	virtual const FChaosVDParticleDataWrapper* GetParticleData() override { return ParticleDataPtr.Get(); }
	virtual void GetVisualizationContext(FChaosVDVisualizationContext& OutVisualizationContext) override;

	void CreateVisualizers();

	virtual void DrawVisualization(const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	
#if WITH_EDITOR
	virtual bool IsSelectedInEditor() const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void SetIsTemporarilyHiddenInEditor(bool bIsHidden) override;
#endif

	void UpdateGeometryComponentsVisibility();
	void UpdateGeometryColors();

	/** Changes the active state of this CVD Particle Actor */
	void SetIsActive(bool bNewActive);

	/** Returns true if this particle actor is active - Inactive Particle actors are still in the world but with outdated data
	 * and hidden from the viewport and outliner. They represent particles that were destroyed.
	 */
	bool IsActive() const { return bIsActive; }

	virtual FBox GetComponentsBoundingBox(bool bNonColliding, bool bIncludeFromChildActors) const override;

	//BEGIN IChaosVDCollisionDataProvider Interface
	virtual void GetCollisionData(TArray<TSharedPtr<FChaosVDCollisionDataFinder>>& OutCollisionDataFound) override;
	virtual bool HasCollisionData() override;
	virtual FName GetProviderName() override;
	//END IChaosVDCollisionDataProvider Interface

	void SetIsServerParticle(bool bNewIsServer) { bIsServer = bNewIsServer; }
	bool GetIsServerParticle() const { return bIsServer; }

	virtual void PushSelectionToProxies() override;

	FChaosVDParticleDataUpdatedDelegate& OnParticleDataUpdated() { return ParticleDataUpdatedDelegate; };

	TConstArrayView<TSharedPtr<FChaosVDMeshDataInstanceHandle>> GetMeshInstances() const { return MeshDataHandles; }

protected:

	void ProcessUpdatedAndRemovedHandles(TArray<TSharedPtr<FChaosVDExtractedGeometryDataHandle>>& OutExtractedGeometryDataHandles);

	const TArray<TSharedPtr<FChaosVDParticlePairMidPhase>>* GetCollisionMidPhasesArray() const;

	void UpdateShapeDataComponents();

	template<typename TTaskCallback>
	void PerformTaskOnGeometry(const TTaskCallback& TaskToPerform);
	
	UPROPERTY(EditAnywhere, Category = "Viewport Visualization Flags", meta = (Bitmask, BitmaskEnum = "/Script/ChaosVD.EChaosVDParticleDataVisualizationFlags"))
	uint8 LocalParticleDataVisualizationFlags;

	UPROPERTY(EditAnywhere, Category = "Viewport Visualization Flags")
	bool bShowDebugText = false;

	TSharedPtr<FChaosVDParticleDataWrapper> ParticleDataPtr;

	FTransform CachedSimulationTransform;

	bool bIsGeometryDataGenerationStarted = false;

	FDelegateHandle GeometryUpdatedDelegate;

	TMap<FStringView, TUniquePtr<FChaosVDDataVisualizerBase>> CVDVisualizers;

	TArray<TSharedPtr<FChaosVDMeshDataInstanceHandle>> MeshDataHandles;

	FChaosVDParticleDataUpdatedDelegate ParticleDataUpdatedDelegate;

	bool bIsActive = false;

	bool bIsServer = false;

	friend FChaosVDParticleActorCustomization;
};

template <typename TTaskCallback>
void AChaosVDParticleActor::PerformTaskOnGeometry(const TTaskCallback& TaskToPerform)
{
	for (TSharedPtr<FChaosVDMeshDataInstanceHandle>& MeshDataHandle : MeshDataHandles)
	{
		if (MeshDataHandle)
		{
			TaskToPerform(MeshDataHandle);
		}
	}
}
