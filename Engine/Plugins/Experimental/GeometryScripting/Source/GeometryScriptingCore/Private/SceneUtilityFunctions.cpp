// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryScript/SceneUtilityFunctions.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "UDynamicMesh.h"
#include "GeometryScript/MeshAssetFunctions.h"
#include "DynamicMesh/MeshTransforms.h"
#include "DynamicMesh/MeshNormals.h"
#include "DynamicMeshEditor.h"
#include "Operations/DetectExteriorVisibility.h"

#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "Components/BrushComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/SplineMeshComponent.h"
#include "ConversionUtils/VolumeToDynamicMesh.h"
#include "ConversionUtils/SkinnedMeshToDynamicMesh.h"
#include "ConversionUtils/SplineComponentDeformDynamicMesh.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "Physics/ComponentCollisionUtil.h"
#include "PlanarCut.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SceneUtilityFunctions)

using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "UGeometryScriptLibrary_SceneUtilityFunctions"

UDynamicMeshPool* UGeometryScriptLibrary_SceneUtilityFunctions::CreateDynamicMeshPool()
{
	return NewObject<UDynamicMeshPool>();
}


UDynamicMesh* UGeometryScriptLibrary_SceneUtilityFunctions::CopyMeshFromComponent(
	USceneComponent* Component,
	UDynamicMesh* ToDynamicMesh,
	FGeometryScriptCopyMeshFromComponentOptions Options,
	bool bTransformToWorld,
	FTransform& LocalToWorld,
	EGeometryScriptOutcomePins& Outcome,
	UGeometryScriptDebug* Debug)
{
	Outcome = EGeometryScriptOutcomePins::Failure;

	if (USkinnedMeshComponent* SkinnedMeshComponent = Cast<USkinnedMeshComponent>(Component))
	{
		LocalToWorld = SkinnedMeshComponent->GetComponentTransform();

		const int32 NumLODs = SkinnedMeshComponent->GetNumLODs();
		const int32 RequestedLOD = Options.RequestedLOD.LODIndex;
		if (RequestedLOD < 0 || RequestedLOD > NumLODs - 1)
		{
			UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshFromComponent_MissingSkinnedMeshComponentLOD", "CopyMeshFromComponent: SkinnedMeshComponent requested LOD does not exist"));
		}
		else
		{ 
			USkinnedAsset* SkinnedAsset = SkinnedMeshComponent->GetSkinnedAsset();
			if (SkinnedAsset)
			{
				FDynamicMesh3 NewMesh;
				UE::Conversion::SkinnedMeshComponentToDynamicMesh(*SkinnedMeshComponent, NewMesh, RequestedLOD, Options.bWantTangents);
				NewMesh.DiscardTriangleGroups();
				ToDynamicMesh->SetMesh(MoveTemp(NewMesh));
				Outcome = EGeometryScriptOutcomePins::Success;
			}
			else
			{
				UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshFromComponent_MissingSkinnedAsset", "CopyMeshFromComponent: SkinnedMeshComponent has a null SkinnedAsset"));
			}
		}

	}
	else if (USplineMeshComponent* SplineMeshComponent = Cast<USplineMeshComponent>(Component))
	{
		LocalToWorld = SplineMeshComponent->GetComponentTransform();
		UStaticMesh* StaticMesh = SplineMeshComponent->GetStaticMesh();
		if (StaticMesh)
		{
			FGeometryScriptCopyMeshFromAssetOptions AssetOptions;
			AssetOptions.bApplyBuildSettings = (Options.bWantNormals || Options.bWantTangents);
			AssetOptions.bRequestTangents = Options.bWantTangents;
			UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromStaticMesh(
				StaticMesh, ToDynamicMesh, AssetOptions, Options.RequestedLOD, Outcome, Debug);	// will set Outcome pin

			// deform the dynamic mesh and its tangent space with the spline
			constexpr bool bUpdateTangentSpace = true;
			UE::Geometry::SplineDeformDynamicMesh(*SplineMeshComponent, ToDynamicMesh->GetMeshRef(), bUpdateTangentSpace);

		}
		else
		{
			UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshFromSplineMeshComponent_MissingStaticMesh", "CopyMeshFromComponent: SplineMeshComponent has a null StaticMesh"));
		}
	}
	else if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Component))
	{
		LocalToWorld = StaticMeshComponent->GetComponentTransform();
		UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
		if (StaticMesh)
		{
			FGeometryScriptCopyMeshFromAssetOptions AssetOptions;
			AssetOptions.bApplyBuildSettings = (Options.bWantNormals || Options.bWantTangents);
			AssetOptions.bRequestTangents = Options.bWantTangents;
			UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromStaticMesh(
				StaticMesh, ToDynamicMesh, AssetOptions, Options.RequestedLOD, Outcome, Debug);	// will set Outcome pin

			// if we have an ISMC, append instances
			if (UInstancedStaticMeshComponent* ISMComponent = Cast<UInstancedStaticMeshComponent>(StaticMeshComponent))
			{
				FDynamicMesh3 InstancedMesh;
				ToDynamicMesh->EditMesh([&](FDynamicMesh3& EditMesh) { InstancedMesh = MoveTemp(EditMesh); EditMesh.Clear(); },
					EDynamicMeshChangeType::MeshChange, EDynamicMeshAttributeChangeFlags::Unknown, true);
				
				FDynamicMesh3 AccumMesh;
				AccumMesh.EnableMatchingAttributes(InstancedMesh);
				FDynamicMeshEditor Editor(&AccumMesh);
				FMeshIndexMappings Mappings;

				int32 NumInstances = ISMComponent->GetInstanceCount();
				for (int32 InstanceIdx = 0; InstanceIdx < NumInstances; ++InstanceIdx)
				{
					if (ISMComponent->IsValidInstance(InstanceIdx))
					{
						FTransform InstanceTransform;
						ISMComponent->GetInstanceTransform(InstanceIdx, InstanceTransform, /*bWorldSpace=*/false);
						FTransformSRT3d XForm(InstanceTransform);

						Mappings.Reset();
						Editor.AppendMesh(&InstancedMesh, Mappings,
							[&](int, const FVector3d& Position) { return XForm.TransformPosition(Position); },
							[&](int, const FVector3d& Normal) { return XForm.TransformNormal(Normal); });
					}
				}

				ToDynamicMesh->EditMesh([&](FDynamicMesh3& EditMesh) { EditMesh = MoveTemp(AccumMesh); },
					EDynamicMeshChangeType::MeshChange, EDynamicMeshAttributeChangeFlags::Unknown, true);
			}
		}
		else
		{
			UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshFromComponent_MissingStaticMesh", "CopyMeshFromComponent: StaticMeshComponent has a null StaticMesh"));
		}
	}
	else if (UDynamicMeshComponent* DynamicMeshComponent = Cast<UDynamicMeshComponent>(Component))
	{
		LocalToWorld = DynamicMeshComponent->GetComponentTransform();
		UDynamicMesh* CopyDynamicMesh = DynamicMeshComponent->GetDynamicMesh();
		if (CopyDynamicMesh)
		{
			CopyDynamicMesh->ProcessMesh([&](const FDynamicMesh3& Mesh)
			{
				ToDynamicMesh->SetMesh(Mesh);
			});
			Outcome = EGeometryScriptOutcomePins::Success;
		}
		else
		{
			UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshFromComponent_MissingDynamicMesh", "CopyMeshFromComponent: DynamicMeshComponent has a null DynamicMesh"));
		}
	}
	else if (UBrushComponent* BrushComponent = Cast<UBrushComponent>(Component))
	{
		LocalToWorld = BrushComponent->GetComponentTransform();

		UE::Conversion::FVolumeToMeshOptions VolOptions;
		VolOptions.bMergeVertices = true;
		VolOptions.bAutoRepairMesh = true;
		VolOptions.bOptimizeMesh = true;
		VolOptions.bSetGroups = true;

		FDynamicMesh3 ConvertedMesh(EMeshComponents::FaceGroups);
		UE::Conversion::BrushComponentToDynamicMesh(BrushComponent, ConvertedMesh, VolOptions);

		// compute normals for current polygroup topology
		ConvertedMesh.EnableAttributes();
		if (Options.bWantNormals)
		{
			FDynamicMeshNormalOverlay* Normals = ConvertedMesh.Attributes()->PrimaryNormals();
			FMeshNormals::InitializeOverlayTopologyFromFaceGroups(&ConvertedMesh, Normals);
			FMeshNormals::QuickRecomputeOverlayNormals(ConvertedMesh);
		}

		if (ConvertedMesh.TriangleCount() > 0)
		{
			ToDynamicMesh->SetMesh(MoveTemp(ConvertedMesh));
			Outcome = EGeometryScriptOutcomePins::Success;
		}
		else
		{
			UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshFromComponent_InvalidBrushConversion", "CopyMeshFromComponent: BrushComponent conversion produced 0 triangles"));
		}
	}
	else if (UGeometryCollectionComponent* GeometryCollectionComponent = Cast<UGeometryCollectionComponent>(Component))
	{
		LocalToWorld = GeometryCollectionComponent->GetComponentTransform();

		if (const UGeometryCollection* RestCollection = GeometryCollectionComponent->GetRestCollection())
		{
			if (const FGeometryCollection* Collection = RestCollection->GetGeometryCollection().Get())
			{
				ToDynamicMesh->EditMesh([&](FDynamicMesh3& EditMesh)
				{
					FTransform UnusedTransform;
					const TArray<FTransform3f>& DynamicTransforms = GeometryCollectionComponent->GetComponentSpaceTransforms3f();
					if (!DynamicTransforms.IsEmpty())
					{
						ConvertGeometryCollectionToDynamicMesh(EditMesh, UnusedTransform, false, *Collection, true, DynamicTransforms, false, Collection->TransformIndex.GetConstArray());
					}
					else
					{
						ConvertGeometryCollectionToDynamicMesh(EditMesh, UnusedTransform, false, *Collection, true, TArrayView<const FTransform3f>(Collection->Transform.GetConstArray()), true, Collection->TransformIndex.GetConstArray());
					}
				},
				EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, true);
				Outcome = EGeometryScriptOutcomePins::Success;
			}
			else
			{
				UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshFromComponent_MissingCollectionData", "CopyMeshFromComponent: GeometryCollectionComponent has null Geometry Collection data"));
			}
		}
		else
		{
			UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyMeshFromComponent_MissingRestCollection", "CopyMeshFromComponent: GeometryCollectionComponent has null Rest Collection object"));
		}
	}

	// transform mesh to world
	if (Outcome == EGeometryScriptOutcomePins::Success && bTransformToWorld)
	{
		ToDynamicMesh->EditMesh([&](FDynamicMesh3& EditMesh) 
		{
			MeshTransforms::ApplyTransform(EditMesh, (FTransformSRT3d)LocalToWorld, true);

		}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, false);	
	}

	return ToDynamicMesh;
}



void UGeometryScriptLibrary_SceneUtilityFunctions::SetComponentMaterialList(
	UPrimitiveComponent* Component,
	const TArray<UMaterialInterface*>& MaterialList,
	UGeometryScriptDebug* Debug)
{
	if (Component == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("SetComponentMaterialList_InvalidInput1", "SetComponentMaterialList: FromStaticMeshAsset is Null"));
		return;
	}

	for (int32 k = 0; k < MaterialList.Num(); ++k)
	{
		Component->SetMaterial(k, MaterialList[k]);
	}
}




UDynamicMesh* UGeometryScriptLibrary_SceneUtilityFunctions::CopyCollisionMeshesFromObject(
	UObject* FromObject,
	UDynamicMesh* ToDynamicMesh,
	bool bTransformToWorld,
	FTransform& LocalToWorld,
	EGeometryScriptOutcomePins& Outcome,
	bool bUseComplexCollision,
	int SphereResolution,
	UGeometryScriptDebug* Debug)
{
	Outcome = EGeometryScriptOutcomePins::Failure;

	FDynamicMesh3 AccumulatedMesh;
	ToDynamicMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
	{
		AccumulatedMesh.EnableMatchingAttributes(ReadMesh, true);
	});

	if (bUseComplexCollision)
	{
		// find the Complex Collision mesh interface
		IInterface_CollisionDataProvider* CollisionProvider = nullptr;
		if (UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(FromObject))
		{
			LocalToWorld = StaticMeshComp->GetComponentTransform();
			UStaticMesh* StaticMesh = StaticMeshComp->GetStaticMesh();
			CollisionProvider = (IInterface_CollisionDataProvider*)StaticMesh;
		}
		else if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(FromObject))
		{
			LocalToWorld = FTransform::Identity;
			CollisionProvider = (IInterface_CollisionDataProvider*)StaticMesh;
		}
		else if (UPrimitiveComponent* DynamicMeshComp = Cast<UDynamicMeshComponent>(FromObject))
		{
			LocalToWorld = DynamicMeshComp->GetComponentTransform();
			CollisionProvider = (IInterface_CollisionDataProvider*)DynamicMeshComp;
		}

		if (CollisionProvider == nullptr)
		{
			UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyCollisionMeshesFromObject_NoCollisionProvider", "CopyCollisionMeshesFromObject: Complex Collision Provider is not available for this object type"));
			return ToDynamicMesh;
		}

		FTransformSequence3d Transforms;
		if (bTransformToWorld)
		{
			Transforms.Append(LocalToWorld);
		}

		// generate mesh
		bool bFoundMeshErrors = false;
		UE::Geometry::ConvertComplexCollisionToMeshes(CollisionProvider, AccumulatedMesh, Transforms, bFoundMeshErrors, true, true);
	}
	else
	{
		const UBodySetup* BodySetup = nullptr;
		if (UPrimitiveComponent* AnyComponent = Cast<UPrimitiveComponent>(FromObject))
		{
			LocalToWorld = AnyComponent->GetComponentTransform();
			BodySetup = AnyComponent->GetBodySetup();
		}
		else if (const UStaticMesh* StaticMesh = Cast<UStaticMesh>(FromObject))
		{
			LocalToWorld = FTransform::Identity;
			BodySetup = StaticMesh->GetBodySetup();
		}

		if (BodySetup == nullptr)
		{
			UE::Geometry::AppendWarning(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("CopyCollisionMeshesFromObject_NoBodySetup", "CopyCollisionMeshesFromObject: BodySetup is null or Object type is not supported"));
			return ToDynamicMesh;
		}

		FTransformSequence3d Transforms;
		if (bTransformToWorld)
		{
			Transforms.Append(LocalToWorld);
		}

		UE::Geometry::ConvertSimpleCollisionToMeshes(BodySetup->AggGeom, AccumulatedMesh, Transforms, SphereResolution, true, true);
	}

	ToDynamicMesh->EditMesh([&](FDynamicMesh3& EditMesh)
	{
		EditMesh = MoveTemp(AccumulatedMesh);
	});

	Outcome = EGeometryScriptOutcomePins::Success;

	return ToDynamicMesh;
}

void UGeometryScriptLibrary_SceneUtilityFunctions::DetermineMeshOcclusion(
	const TArray<UDynamicMesh*>& SourceMeshes,
	const TArray<FTransform>& SourceMeshTransforms,
	TArray<bool>& OutMeshIsHidden,
	const TArray<UDynamicMesh*>& OccludeMeshes,
	const TArray<FTransform>& OccludeMeshTransforms,
	const FGeometryScriptDetermineMeshOcclusionOptions& OcclusionOptions,
	UGeometryScriptDebug* Debug)
{
	if (SourceMeshes.Num() != SourceMeshTransforms.Num())
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("DetermineMeshOcclusion_SourceArrayMismatch", "DetermineMeshOcclusion: SourceMeshes and SourceMeshTransforms arrays must have same length"));
		return;
	}
	if (OccludeMeshes.Num() != OccludeMeshTransforms.Num())
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("DetermineMeshOcclusion_OccludeArrayMismatch", "DetermineMeshOcclusion: OccludeMeshes and OccludeMeshTransforms arrays must have same length"));
		return;
	}

	FDetectPerDynamicMeshExteriorVisibility Occlusion;
	for (int32 SourceMeshIndex = 0; SourceMeshIndex < SourceMeshes.Num(); ++SourceMeshIndex)
	{
		UDynamicMesh* Mesh = SourceMeshes[SourceMeshIndex];
		if (!Mesh)
		{
			UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("DetermineMeshOcclusion_InvalidSourceMesh", "DetermineMeshOcclusion: SourceMeshes array contained null mesh"));
			return;
		}
		Occlusion.Instances.Emplace(Mesh->GetMeshPtr(), SourceMeshTransforms[SourceMeshIndex]);
	}
	for (int32 OccludeMeshIndex = 0; OccludeMeshIndex < OccludeMeshes.Num(); ++OccludeMeshIndex)
	{
		UDynamicMesh* Mesh = OccludeMeshes[OccludeMeshIndex];
		if (!Mesh)
		{
			UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("DetermineMeshOcclusion_InvalidOccludeMesh", "DetermineMeshOcclusion: OccludeMeshes array contained null mesh"));
			return;
		}
		Occlusion.OccludeInstances.Emplace(Mesh->GetMeshPtr(), OccludeMeshTransforms[OccludeMeshIndex]);
	}
	Occlusion.SamplingParameters.bDoubleSided = OcclusionOptions.bDoubleSided;
	Occlusion.SamplingParameters.SamplingDensity = OcclusionOptions.SamplingDensity;
	Occlusion.SamplingParameters.NumSearchDirections = OcclusionOptions.NumSearchDirections;

	Occlusion.ComputeHidden(OutMeshIsHidden);
}

#undef LOCTEXT_NAMESPACE