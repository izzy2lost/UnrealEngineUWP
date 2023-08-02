// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDGeometryDataComponent.h"

#include "ChaosVDEditorSettings.h"
#include "ChaosVDGeometryBuilder.h"
#include "ChaosVDModule.h"
#include "Components/MeshComponent.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"

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
			if (UMaterialInterface* QueryOnlyMaterial = EditorSettings->QueryOnlyMeshesMaterial.Get())
			{
				const bool bIsQueryOnly = InCollisionData.bQueryCollision && !InCollisionData.bSimCollision;

				if (bIsQueryOnly)
				{
					MeshComponent->SetMaterial(0, QueryOnlyMaterial);
				}
				else
				{
					MeshComponent->SetMaterial(0, UMaterial::GetDefaultMaterial(EMaterialDomain::MD_Surface));
				}
			}
			else
			{
				ensure(false);
				UE_LOG(LogChaosVDEditor, Error, TEXT("[%hs] Failed to get Query Only material for, applying the default mesh to all geometry"), ANSI_TO_TCHAR(__FUNCTION__));
				
				MeshComponent->SetMaterial(0, UMaterial::GetDefaultMaterial(EMaterialDomain::MD_Surface));
			}

			bool bShouldGeometryBeVisible = false;

			// Complex vs Simple takes priority although this is subject to change
			const bool bShouldBeVisibleIfComplex = InCollisionData.bIsComplex && EnumHasAnyFlags(static_cast<EChaosVDGeometryVisibilityFlags>(EditorSettings->GeometryVisibilityFlags), EChaosVDGeometryVisibilityFlags::Complex);
			const bool bShouldBeVisibleIfSimple = !InCollisionData.bIsComplex && EnumHasAnyFlags(static_cast<EChaosVDGeometryVisibilityFlags>(EditorSettings->GeometryVisibilityFlags), EChaosVDGeometryVisibilityFlags::Simple);
			if (bShouldBeVisibleIfComplex || bShouldBeVisibleIfSimple)
			{
				bShouldGeometryBeVisible = (InCollisionData.bSimCollision && EnumHasAnyFlags(static_cast<EChaosVDGeometryVisibilityFlags>(EditorSettings->GeometryVisibilityFlags), EChaosVDGeometryVisibilityFlags::Simulated))
				|| (InCollisionData.bQueryCollision && EnumHasAnyFlags(static_cast<EChaosVDGeometryVisibilityFlags>(EditorSettings->GeometryVisibilityFlags), EChaosVDGeometryVisibilityFlags::Query));
			}

			MeshComponent->SetVisibility(bShouldGeometryBeVisible);
		}
	}
}

UE_DISABLE_OPTIMIZATION
void FChaosVDGeometryDataComponentBase::UpdateDataFromShapeArray_Internal(const TArray<FChaosVDShapeCollisionData>& InShapeArray, FChaosVDShapeCollisionData& CollisionDataToUpdate)
{
	if (!ensureMsgf(RootImplicitObject.IsValid(), TEXT("Tried to Update Collision Data without a valid Implicit Object")))
	{
		return;
	}

	RootImplicitObject.GetReference()->VisitObjects([this, &CollisionDataToUpdate, &InShapeArray] (const Chaos::FImplicitObject* ImplicitA, const Chaos::FRigidTransform3& RelativeTransformA, const int32 RootObjectIndexA, const int32 ObjectIndex, const int32 LeafObjectIndexA)
	{
		if (!InShapeArray.IsValidIndex(RootObjectIndexA))
		{
			return true;
		}

		if (ImplicitA->GetTypeHash() == GeometryID)
		{
			CollisionDataToUpdate = InShapeArray[RootObjectIndexA];
			CollisionDataToUpdate.bIsComplex = FChaosVDGeometryBuilder::DoesImplicitContainType(ImplicitA, Chaos::ImplicitObjectType::HeightField) || FChaosVDGeometryBuilder::DoesImplicitContainType(ImplicitA, Chaos::ImplicitObjectType::TriangleMesh);
			CollisionDataToUpdate.bIsValid = true;

			return false;
		}

		return true;
	});
}
UE_ENABLE_OPTIMIZATION