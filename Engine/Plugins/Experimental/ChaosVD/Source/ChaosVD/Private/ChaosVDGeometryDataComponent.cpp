// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDGeometryDataComponent.h"

#include "ChaosVDEditorSettings.h"
#include "ChaosVDGeometryBuilder.h"
#include "ChaosVDModule.h"
#include "ChaosVDParticleActor.h"
#include "ChaosVDScene.h"
#include "Components/MeshComponent.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

void FChaosVDGeometryDataComponentBase::UpdateVisibility_Internal(const FChaosVDShapeCollisionData& InCollisionData, UMeshComponent* MeshComponent)
{
	if (!InCollisionData.bIsValid)
	{
		return;
	}

	if (MeshComponent)
	{
		if (const UChaosVDEditorSettings* EditorSettings = GetDefault<UChaosVDEditorSettings>())
		{
			const EChaosVDGeometryVisibilityFlags CurrentVisibilityFlags = static_cast<EChaosVDGeometryVisibilityFlags>(EditorSettings->GeometryVisibilityFlags);
			
			bool bShouldGeometryBeVisible = false;

			if (!EnumHasAnyFlags(CurrentVisibilityFlags, EChaosVDGeometryVisibilityFlags::ShowDisabledParticles))
			{
				//TODO: We should use IChaosVDParticleVisualizationDataProvider instead, which AChaosVDParticleActor implements already
				// but it is not an uinterface
				if (AChaosVDParticleActor* ParticleActor = Cast<AChaosVDParticleActor>(MeshComponent->GetOwner()))
				{
					if (const FChaosVDParticleDataWrapper* ParticleData = ParticleActor->GetParticleData())
					{
						if (ParticleData->ParticleDynamicsMisc.HasValidData() && ParticleData->ParticleDynamicsMisc.bDisabled)
						{
							MeshComponent->SetVisibility(bShouldGeometryBeVisible);
							return;
						}
					}
				}
			}

			// TODO: Re-visit the way we determine visibility of the meshes.
			// Now that the options have grown and they will continue to do so, these checks are becoming hard to read and extend

			const bool bIsHeightfield = GetImplicitObject() && Chaos::GetInnerType(GetImplicitObject()->GetType()) == Chaos::ImplicitObjectType::HeightField;

			if (bIsHeightfield && EnumHasAnyFlags(CurrentVisibilityFlags, EChaosVDGeometryVisibilityFlags::ShowHeightfields))
			{
				bShouldGeometryBeVisible = true;
			}
			else
			{
				// Complex vs Simple takes priority although this is subject to change
				const bool bShouldBeVisibleIfComplex = InCollisionData.bIsComplex && EnumHasAnyFlags(CurrentVisibilityFlags, EChaosVDGeometryVisibilityFlags::Complex);
				const bool bShouldBeVisibleIfSimple = !InCollisionData.bIsComplex && EnumHasAnyFlags(CurrentVisibilityFlags, EChaosVDGeometryVisibilityFlags::Simple);
			
				if (bShouldBeVisibleIfComplex || bShouldBeVisibleIfSimple)
				{
					bShouldGeometryBeVisible = (InCollisionData.bSimCollision && EnumHasAnyFlags(CurrentVisibilityFlags, EChaosVDGeometryVisibilityFlags::Simulated))
					|| (InCollisionData.bQueryCollision && EnumHasAnyFlags(CurrentVisibilityFlags, EChaosVDGeometryVisibilityFlags::Query));
				}
			}

			MeshComponent->SetVisibility(bShouldGeometryBeVisible);
		}
	}
}

void FChaosVDGeometryDataComponentBase::UpdateDataFromShapeArray_Internal(const TArray<FChaosVDShapeCollisionData>& InShapeArray, FChaosVDShapeCollisionData& CollisionDataToUpdate, UMeshComponent* MeshComponent)
{
	if (!ensureMsgf(RootImplicitObject.IsValid(), TEXT("Tried to Update Collision Data without a valid Implicit Object")))
	{
		return;
	}

	RootImplicitObject.GetReference()->VisitObjects([this, &CollisionDataToUpdate, &InShapeArray] (const Chaos::FImplicitObject* ImplicitA, const Chaos::FRigidTransform3& RelativeTransformA, const int32 RootObjectIndexA, const int32 ObjectIndex, const int32 LeafObjectIndexA)
	{
		if (!InShapeArray.IsValidIndex(LeafObjectIndexA))
		{
			return true;
		}

		if (ImplicitA == ImplicitObject)
		{
			CollisionDataToUpdate = InShapeArray[LeafObjectIndexA];
			CollisionDataToUpdate.bIsComplex = FChaosVDGeometryBuilder::DoesImplicitContainType(ImplicitA, Chaos::ImplicitObjectType::HeightField) || FChaosVDGeometryBuilder::DoesImplicitContainType(ImplicitA, Chaos::ImplicitObjectType::TriangleMesh);
			CollisionDataToUpdate.bIsValid = true;
		}

		return true;
	});

	// If our collision data was successfully updated and we have a valid mesh component, set the correct material
	if (CollisionDataToUpdate.bIsValid && MeshComponent)
	{
		if (const UChaosVDEditorSettings* EditorSettings = GetDefault<UChaosVDEditorSettings>())
		{
			const bool bIsQueryOnly = CollisionDataToUpdate.bQueryCollision && !CollisionDataToUpdate.bSimCollision;

			UMaterialInterface* MaterialToApply = bIsQueryOnly ? GetCachedMaterialInstance(EChaosVDMaterialType::QueryOnlyMaterial) : GetCachedMaterialInstance(EChaosVDMaterialType::SimOnlyMaterial);

			if (MaterialToApply)
			{
				MeshComponent->SetMaterial(0, MaterialToApply);
			}
			else
			{
				ensure(false);
				UE_LOG(LogChaosVDEditor, Error, TEXT("[%s] Failed to get Query Only material for, applying the default mesh to all geometry"), ANSI_TO_TCHAR(__FUNCTION__));
				
				MeshComponent->SetMaterial(0, UMaterial::GetDefaultMaterial(EMaterialDomain::MD_Surface));
			}
		}
	}
}

void FChaosVDGeometryDataComponentBase::UpdateColors_Internal(UMeshComponent* MeshComponent)
{
	if (!MeshComponent)
	{
		return;
	}

	//TODO: Remove this direct dependency to AChaosVDParticleACtor
	AChaosVDParticleActor* ParticleActor = Cast<AChaosVDParticleActor>(MeshComponent->GetOwner());
	if (!ParticleActor)
	{
		return;
	}

	const FChaosVDParticleDataWrapper* ParticleData = ParticleActor->GetParticleData();
	if (!ParticleData)
	{
		return;
	}

	const UChaosVDEditorSettings* EditorSettings = GetDefault<UChaosVDEditorSettings>();
	if (!EditorSettings)
	{
		return;
	}

	constexpr FLinearColor DefaultColor(0.088542f, 0.088542f, 0.088542f);
	FLinearColor ColorToApply = DefaultColor;

	switch(EditorSettings->ParticleColorMode)
	{
		case EChaosVDParticleDebugColorMode::ShapeType:
			{
				ColorToApply = GetImplicitObject() ? EditorSettings->ColorsByShapeType.GetColorFromShapeType(Chaos::GetInnerType(GetImplicitObject()->GetType())) : DefaultColor;
				break;
			}
		case EChaosVDParticleDebugColorMode::State:
			{
				if (ParticleData->Type == EChaosVDParticleType::Static)
				{
					ColorToApply = EditorSettings->ColorsByParticleState.GetColorFromState(EChaosVDObjectStateType::Static);
				}
				else
				{
					ColorToApply = EditorSettings->ColorsByParticleState.GetColorFromState(ParticleData->ParticleDynamicsMisc.MObjectState);
				}
				break;
			}
		case EChaosVDParticleDebugColorMode::ClientServer:
			{
				const TSharedPtr<FChaosVDScene> Scene = ParticleActor->GetScene().Pin();
				const bool bIsServer = (Scene.IsValid()) ? Scene->IsSolverForServer(ParticleData->SolverID) : false;
				if (ParticleData->Type == EChaosVDParticleType::Static)
				{
					ColorToApply = EditorSettings->ColorsByClientServer.GetColorFromState(bIsServer, EChaosVDObjectStateType::Static);
				}
				else
				{
					ColorToApply = EditorSettings->ColorsByClientServer.GetColorFromState(bIsServer, ParticleData->ParticleDynamicsMisc.MObjectState);
				}
				break;
			}

		case EChaosVDParticleDebugColorMode::None:
		default:
			// Nothing to do here. Color to apply is already set to the default
			break;
	}

	if (CurrentGeometryColor == ColorToApply)
	{
		return;
	}
	
	if (UMaterialInstanceDynamic* DynamicMaterial = Cast<UMaterialInstanceDynamic>(MeshComponent->GetMaterial(0)))
	{
		DynamicMaterial->SetVectorParameterValue(TEXT("BaseColor"), ColorToApply);
		CurrentGeometryColor = ColorToApply;
	}
}

void FChaosVDGeometryDataComponentBase::SetImplicitObject_Internal(const Chaos::FImplicitObject* InImplicitObject)
{
	ImplicitObject = InImplicitObject;
}

UMaterialInstanceDynamic* FChaosVDGeometryDataComponentBase::GetCachedMaterialInstance(EChaosVDMaterialType Type)
{
	const UChaosVDEditorSettings* EditorSettings = GetDefault<UChaosVDEditorSettings>();
	if (!EditorSettings)
	{
		return nullptr;
	}

	if (const TStrongObjectPtr<UMaterialInstanceDynamic>* MaterialInstance = MaterialInstancesByID.Find(Type))
	{
		return MaterialInstance->Get();
	}
	else
	{
		UMaterialInterface* MaterialToCreate = nullptr;
		switch(Type)
		{
			case EChaosVDMaterialType::QueryOnlyMaterial:
				{
					MaterialToCreate = EditorSettings->QueryOnlyMeshesMaterial.Get();
					break;
				}
			case EChaosVDMaterialType::SimOnlyMaterial:
				{
					MaterialToCreate = EditorSettings->SimOnlyMeshesMaterial.Get();
					break;
				}
		}
		
		if (MaterialToCreate)
		{
			UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(MaterialToCreate, nullptr);
			MaterialInstancesByID.Add(Type, TStrongObjectPtr<UMaterialInstanceDynamic>(DynamicMaterial));

			return DynamicMaterial;
		}
	}

	return nullptr;
}

const Chaos::FImplicitObject* FChaosVDGeometryDataComponentBase::GetImplicitObject() const
{
	// If the root object is no longer valid, the implicit object ptr we have is probably garbage
	return RootImplicitObject.IsValid() ? ImplicitObject : nullptr;
}
