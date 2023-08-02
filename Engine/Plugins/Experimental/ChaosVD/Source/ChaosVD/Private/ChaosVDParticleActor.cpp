// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDParticleActor.h"

#include "ChaosVDGeometryBuilder.h"
#include "ChaosVDScene.h"
#include "Components/ChaosVDInstancedStaticMeshComponent.h"
#include "Components/ChaosVDStaticMeshComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DataWrappers/ChaosVDParticleDataWrapper.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Visualizers/ChaosVDParticleDataVisualizer.h"

AChaosVDParticleActor::AChaosVDParticleActor(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneComponent0"));
	CreateVisualizers();
}

void AChaosVDParticleActor::UpdateFromRecordedParticleData(const FChaosVDParticleDataWrapper& InRecordedData, const Chaos::FRigidTransform3& SimulationTransform)
{
	//TODO: Make the simulation transform be cached on the CVD Scene, so we can query from it when needed
	// Copying it to each particle actor is not efficient
	CachedSimulationTransform = SimulationTransform;

	if (InRecordedData.ParticlePositionRotation.HasValidData())
	{
		// TODO: Only update if the transform (either the simulation or the particle) has changed;
		SetActorLocationAndRotation(SimulationTransform.TransformPosition(InRecordedData.ParticlePositionRotation.MX), SimulationTransform.GetRotation() * InRecordedData.ParticlePositionRotation.MR, false);
	}

	if (ParticleDataViewer.GeometryHash != InRecordedData.GeometryHash)
	{
		UpdateGeometry(InRecordedData.GeometryHash, EChaosVDActorGeometryUpdateFlags::ForceUpdate);
	}

	// TODO: We should store a ptr to the data and in our custom details panel draw it
	ParticleDataViewer = InRecordedData;

	// Now that we have updated particle data, update the Shape data and visibility as needed
	UpdateShapeDataComponents();
	UpdateGeometryComponentsVisibility();
}

void AChaosVDParticleActor::UpdateCollisionData(const TArray<TSharedPtr<FChaosVDParticlePairMidPhase>>& InRecordedMidPhases)
{
	// TODO: We should store a ptr to the data and in our custom details panel draw it
	ParticleDataViewer.ParticleMidPhases.Reserve(InRecordedMidPhases.Num());
	ParticleDataViewer.ParticleMidPhases.Reset(InRecordedMidPhases.Num());

	for (const TSharedPtr<FChaosVDParticlePairMidPhase>& MidPhase : InRecordedMidPhases)
	{
		ParticleDataViewer.ParticleMidPhases.Emplace(*MidPhase.Get());
	}
}

void AChaosVDParticleActor::UpdateCollisionData(const TArray<FChaosVDConstraint>& InRecordedConstraints)
{
	// TODO: We should store a ptr to the data and in our custom details panel draw it
	ParticleDataViewer.ParticleConstraints.Reserve(InRecordedConstraints.Num());
	ParticleDataViewer.ParticleConstraints.Reset(InRecordedConstraints.Num());

	for (const FChaosVDConstraint& Constraint : InRecordedConstraints)
	{
		ParticleDataViewer.ParticleConstraints.Emplace(Constraint);
	}
}

void AChaosVDParticleActor::UpdateGeometry(const Chaos::FConstImplicitObjectPtr& InImplicitObject, EChaosVDActorGeometryUpdateFlags OptionsFlags)
{
	if (!InImplicitObject.IsValid())
	{
		return;
	}
	
	if (EnumHasAnyFlags(OptionsFlags, EChaosVDActorGeometryUpdateFlags::ForceUpdate))
	{
		bIsGeometryDataGenerationStarted = false;

		for (TWeakObjectPtr<UMeshComponent>& MeshComponent : MeshComponents)
		{
			if (MeshComponent.IsValid())
			{
				MeshComponent->DestroyComponent();
			}
		}

		MeshComponents.Reset();
	}

	if (bIsGeometryDataGenerationStarted)
	{
		return;
	}

	if (const TSharedPtr<FChaosVDScene>& ScenePtr = OwningScene.Pin())
	{
		if (const TSharedPtr<FChaosVDGeometryBuilder>& GeometryGenerator = ScenePtr->GetGeometryGenerator())
		{
			TArray<TWeakObjectPtr<UMeshComponent>> OutGeneratedMeshComponents;
			Chaos::FRigidTransform3 Transform;

			// Heightfields need to be created as Static meshes and use normal Static Mesh components because we need LODs for them due to their high triangle count
			if (FChaosVDGeometryBuilder::DoesImplicitContainType(InImplicitObject, Chaos::ImplicitObjectType::HeightField))
			{
				constexpr int32 LODsToGenerateNum = 3;
				constexpr int32 StartingMeshComponentIndex = 0;
				GeometryGenerator->CreateMeshComponentsFromImplicit<UStaticMesh, UChaosVDStaticMeshComponent>(InImplicitObject, this, OutGeneratedMeshComponents, Transform, StartingMeshComponentIndex, LODsToGenerateNum);
			}
			else
			{
				GeometryGenerator->CreateMeshComponentsFromImplicit<UStaticMesh, UChaosVDInstancedStaticMeshComponent>(InImplicitObject, this, OutGeneratedMeshComponents, Transform);
			}

			if (OutGeneratedMeshComponents.Num() > 0)
			{
				MeshComponents.Append(OutGeneratedMeshComponents);

				for (TWeakObjectPtr<UMeshComponent> MeshComponent : MeshComponents)
				{
					if (IChaosVDGeometryDataComponent* DataComponent = Cast<IChaosVDGeometryDataComponent>(MeshComponent.Get()))
					{
						DataComponent->SetRootImplicitObject(InImplicitObject);
					}
				}

				UpdateShapeDataComponents();
				UpdateGeometryComponentsVisibility();

				bIsGeometryDataGenerationStarted = true;
			}
		}
	}
}

void AChaosVDParticleActor::UpdateGeometry(uint32 NewGeometryHash, EChaosVDActorGeometryUpdateFlags OptionsFlags)
{
	if (const TSharedPtr<FChaosVDScene>& ScenePtr = OwningScene.Pin())
	{
		if (const Chaos::FConstImplicitObjectPtr& Geometry = ScenePtr->GetUpdatedGeometry(NewGeometryHash))
		{
			UpdateGeometry(Geometry, OptionsFlags);
		}
	}
}

void AChaosVDParticleActor::SetScene(const TSharedPtr<FChaosVDScene>& InScene)
{
	OwningScene = InScene;

	if (const TSharedPtr<FChaosVDScene>& ScenePtr = OwningScene.Pin())
	{
		GeometryUpdatedDelegate = ScenePtr->OnNewGeometryAvailable().AddWeakLambda(this, [this](const Chaos::FConstImplicitObjectPtr& ImplicitObject, const uint32 ID)
		{
			if (ParticleDataViewer.GeometryHash == ID)
			{
				UpdateGeometry(ImplicitObject);
			}
		});
	}
}

void AChaosVDParticleActor::BeginDestroy()
{
	if (const TSharedPtr<FChaosVDScene>& ScenePtr = OwningScene.Pin())
	{
		ScenePtr->OnNewGeometryAvailable().Remove(GeometryUpdatedDelegate);
	}

	Super::BeginDestroy();
}

void AChaosVDParticleActor::GetVisualizationContext(FChaosVDVisualizationContext& OutVisualizationContext)
{
	OutVisualizationContext.SpaceTransform = CachedSimulationTransform;
	OutVisualizationContext.CVDScene = OwningScene;
	OutVisualizationContext.SolverID = ParticleDataViewer.SolverID;
}

void AChaosVDParticleActor::CreateVisualizers()
{
	CVDVisualizers.Add(FChaosVDParticleDataVisualizer::VisualizerID,MakeUnique<FChaosVDParticleDataVisualizer>(*this));
	CVDVisualizers.Add(FChaosVDCollisionDataVisualizer::VisualizerID, MakeUnique<FChaosVDCollisionDataVisualizer>(*this));
}

#if WITH_EDITOR

bool AChaosVDParticleActor::IsSelectedInEditor() const
{
	// The implementation of this method in UObject, used a global edit callback,
	// but as we don't use the global editor selection system, we need to re-route it.
	if (TSharedPtr<FChaosVDScene> ScenePtr = OwningScene.Pin())
	{
		return ScenePtr->IsObjectSelected(this);
	}

	return false;
}

void AChaosVDParticleActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_STRING_CHECKED(AChaosVDParticleActor, ParticleDataViewer))
	{
		// Not particularly useful for now. This is a test code verifying we can react to changes in the data.
		// In the future this could be part of the Re-simulation feature. When data is changed here, it can be propagated to the evolution instance that will be re-simulated
		if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_STRING_CHECKED(FChaosVDParticleDataWrapper, GeometryHash))
		{
			UpdateGeometry(ParticleDataViewer.GeometryHash, EChaosVDActorGeometryUpdateFlags::ForceUpdate);
		}
	}

	if (PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_STRING_CHECKED(AChaosVDParticleActor, LocalCollisionDataVisualizationFlags))
	{
		if (TUniquePtr<FChaosVDDataVisualizerBase>* CollisionVisualizer = CVDVisualizers.Find(FChaosVDCollisionDataVisualizer::VisualizerID))
		{
			CollisionVisualizer->Get()->UpdateVisualizationFlags(LocalCollisionDataVisualizationFlags);
		}
	}

	if (PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_STRING_CHECKED(AChaosVDParticleActor, LocalParticleDataVisualizationFlags))
	{
		if (TUniquePtr<FChaosVDDataVisualizerBase>* ParticleDataVisualizer = CVDVisualizers.Find(FChaosVDParticleDataVisualizer::VisualizerID))
		{
			ParticleDataVisualizer->Get()->UpdateVisualizationFlags(LocalParticleDataVisualizationFlags);
		}
	}
}

void AChaosVDParticleActor::UpdateShapeDataComponents()
{
	for (TWeakObjectPtr<UMeshComponent> MeshComponent : MeshComponents)
	{
		if (IChaosVDGeometryDataComponent* DataComponent = Cast<IChaosVDGeometryDataComponent>(MeshComponent.Get()))
		{
			DataComponent->UpdateDataFromShapeArray(ParticleDataViewer.CollisionDataPerShape);
		}
	}			
}

void AChaosVDParticleActor::UpdateGeometryComponentsVisibility()
{
	for (TWeakObjectPtr<UMeshComponent> MeshComponent : MeshComponents)
	{
		if (IChaosVDGeometryDataComponent* DataComponent = Cast<IChaosVDGeometryDataComponent>(MeshComponent.Get()))
		{
			// We need wait until we have a valid mesh
			if (DataComponent->IsMeshReady())
			{
				DataComponent->UpdateVisibility();
			}
			else if (!DataComponent->OnMeshReady()->IsBound())
			{
				DataComponent->OnMeshReady()->BindWeakLambda(this, [](IChaosVDGeometryDataComponent& GeometryDataComponent)
				{
					GeometryDataComponent.UpdateVisibility();
				});
			}
		}
	}
}

void AChaosVDParticleActor::DrawVisualization(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	for (const TPair<FStringView, TUniquePtr<FChaosVDDataVisualizerBase>>& VisualizerWithID : CVDVisualizers)
	{
		if (VisualizerWithID.Value)
		{
			VisualizerWithID.Value->DrawVisualization(View, PDI);
		}
	}
}

#endif //WITH_EDITOR
