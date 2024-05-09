// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cloner/CEClonerMeshBuilder.h"

#include "Components/BrushComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SplineMeshComponent.h"
#include "DynamicMesh/MeshNormals.h"
#include "DynamicMesh/MeshTransforms.h"
#include "DynamicMesh/Operations/MergeCoincidentMeshEdges.h"
#include "DynamicMeshEditor.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GeometryScript/MeshAssetFunctions.h"
#include "GeometryScript/MeshBasicEditFunctions.h"
#include "GeometryScript/SceneUtilityFunctions.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "StaticMeshOperations.h"
#include "UDynamicMesh.h"

FCEClonerMeshBuilder::FCEClonerMeshBuilder()
{
	OutputDynamicMesh = NewObject<UDynamicMesh>();
}

void FCEClonerMeshBuilder::Reset()
{
	OutputDynamicMesh->EditMesh([](FDynamicMesh3& EditMesh){ EditMesh.Clear(); });
	ConvertedMeshes.Empty();
	OutputMeshMaterials.Empty();
}

int32 FCEClonerMeshBuilder::AppendActor(const AActor* InActor)
{
	int32 ComponentConverted = 0;

	if (!IsValid(InActor))
	{
		return ComponentConverted;
	}

	const FTransform& SourceTransform = InActor->GetActorTransform();

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	InActor->GetComponents(PrimitiveComponents, /** IncludeChildrenActors */ false);

	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!IsValid(PrimitiveComponent))
		{
			continue;
		}

		if (UDynamicMeshComponent* DynamicMeshComponent = Cast<UDynamicMeshComponent>(PrimitiveComponent))
		{
			if (AppendComponent(DynamicMeshComponent, SourceTransform))
			{
				ComponentConverted++;
			}
		}
		else if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(PrimitiveComponent))
		{
			if (AppendComponent(SkeletalMeshComponent, SourceTransform))
			{
				ComponentConverted++;
			}
		}
		else if (UBrushComponent* BrushComponent = Cast<UBrushComponent>(PrimitiveComponent))
		{
			if (AppendComponent(BrushComponent, SourceTransform))
			{
				ComponentConverted++;
			}
		}
		else if (UProceduralMeshComponent* ProceduralMeshComponent = Cast<UProceduralMeshComponent>(PrimitiveComponent))
		{
			if (AppendComponent(ProceduralMeshComponent, SourceTransform))
			{
				ComponentConverted++;
			}
		}
		else if (UInstancedStaticMeshComponent* ISMComponent = Cast<UInstancedStaticMeshComponent>(PrimitiveComponent))
		{
			if (AppendComponent(ISMComponent, SourceTransform))
			{
				ComponentConverted++;
			}
		}
		else if (USplineMeshComponent* SplineMeshComponent = Cast<USplineMeshComponent>(PrimitiveComponent))
		{
			if (AppendComponent(SplineMeshComponent, SourceTransform))
			{
				ComponentConverted++;
			}
		}
		else if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(PrimitiveComponent))
		{
			if (AppendComponent(StaticMeshComponent, SourceTransform))
			{
				ComponentConverted++;
			}
		}
	}

	return ComponentConverted;
}

bool FCEClonerMeshBuilder::AppendMesh(const UDynamicMesh* InMesh, const TArray<TWeakObjectPtr<UMaterialInterface>>& InMaterials, const FTransform& InTransform)
{
	if (!IsValid(InMesh))
	{
		return false;
	}

	// Create a copy of the mesh
	InMesh->ProcessMesh([this, &InTransform](const FDynamicMesh3& EditMesh)
	{
		FDynamicMesh3 CopyMesh = EditMesh;
		MeshTransforms::ApplyTransform(CopyMesh, InTransform);

		ConvertedMeshes.Add(MoveTemp(CopyMesh));
	});

	OutputMeshMaterials.Append(InMaterials);

	return true;
}

bool FCEClonerMeshBuilder::AppendComponent(const UStaticMeshComponent* InComponent, const FTransform& InSourceTransform)
{
	if (!IsValid(InComponent))
	{
		return false;
	}

	UStaticMesh* StaticMesh = InComponent->GetStaticMesh();

	if (!IsValid(StaticMesh))
	{
		return false;
	}

	// convert to dynamic mesh
	FGeometryScriptMeshReadLOD StaticMeshLOD;
	StaticMeshLOD.LODType = EGeometryScriptLODType::RenderData;

	FGeometryScriptCopyMeshFromAssetOptions OutputMeshOptions;
	OutputMeshOptions.bIgnoreRemoveDegenerates = false;
	OutputMeshOptions.bRequestTangents = false;
	OutputMeshOptions.bApplyBuildSettings = false;

	EGeometryScriptOutcomePins OutResult;
	UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromStaticMesh(StaticMesh, OutputDynamicMesh, OutputMeshOptions, StaticMeshLOD, OutResult);

	if (OutResult != EGeometryScriptOutcomePins::Success)
	{
		return false;
	}

	// Transform the new mesh relative to the component
	const FTransform RelativeTransform = InComponent->GetComponentTransform().GetRelativeTransform(InSourceTransform);
	OutputMeshMaterials.Append(InComponent->GetMaterials());
	OutputDynamicMesh->EditMesh([this, RelativeTransform](FDynamicMesh3& EditMesh)
	{
		MeshTransforms::ApplyTransform(EditMesh, RelativeTransform);
		ConvertedMeshes.Add(MoveTemp(EditMesh));
		// replace by empty mesh
		FDynamicMesh3 EmptyMesh;
		EditMesh = MoveTemp(EmptyMesh);
	}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, true);

	return true;
}

bool FCEClonerMeshBuilder::AppendComponent(UProceduralMeshComponent* InComponent, const FTransform& InSourceTransform)
{
	if (!IsValid(InComponent))
	{
		return false;
	}

	const int32 SectionCount = InComponent->GetNumSections();

	if (SectionCount == 0)
	{
		return false;
	}

	// Transform the new mesh relative to the component
	const FTransform RelativeTransform = InComponent->GetComponentTransform().GetRelativeTransform(InSourceTransform);

	FDynamicMesh3 Mesh;
	Mesh.EnableAttributes();
	Mesh.Attributes()->EnablePrimaryColors();
	Mesh.Attributes()->EnableMaterialID();
	Mesh.Attributes()->SetNumNormalLayers(1);
	Mesh.Attributes()->SetNumUVLayers(1);
	Mesh.Attributes()->SetNumPolygroupLayers(1);
	Mesh.Attributes()->EnableTangents();

	using namespace UE::Geometry;

	FDynamicMeshColorOverlay* ColorOverlay = Mesh.Attributes()->PrimaryColors();
	FDynamicMeshNormalOverlay* NormalOverlay = Mesh.Attributes()->PrimaryNormals();
	FDynamicMeshUVOverlay* UVOverlay = Mesh.Attributes()->PrimaryUV();
	FDynamicMeshMaterialAttribute* MaterialAttr = Mesh.Attributes()->GetMaterialID();
	FDynamicMeshPolygroupAttribute* GroupAttr = Mesh.Attributes()->GetPolygroupLayer(0);
	FDynamicMeshNormalOverlay* TangentOverlay = Mesh.Attributes()->PrimaryTangents();

	for (int32 SectionIdx = 0; SectionIdx < SectionCount; SectionIdx++)
	{
		if (FProcMeshSection* Section = InComponent->GetProcMeshSection(SectionIdx))
		{
			if (Section->bSectionVisible)
			{
				TArray<int32> VtxIds;
				TArray<int32> NormalIds;
				TArray<int32> ColorIds;
				TArray<int32> UVIds;
				TArray<int32> TaIds;

				// copy vertices data (position, normal, color, UV, tangent)
				for (FProcMeshVertex& SectionVertex : Section->ProcVertexBuffer)
				{
					int32 VId = Mesh.AppendVertex(SectionVertex.Position);
					VtxIds.Add(VId);

					int32 NId = NormalOverlay->AppendElement(static_cast<FVector3f>(SectionVertex.Normal));
					NormalIds.Add(NId);

					int32 CId = ColorOverlay->AppendElement(static_cast<FVector4f>(SectionVertex.Color));
					ColorIds.Add(CId);

					int32 UVId = UVOverlay->AppendElement(static_cast<FVector2f>(SectionVertex.UV0));
					UVIds.Add(UVId);

					int32 TaId = TangentOverlay->AppendElement(static_cast<FVector3f>(SectionVertex.Tangent.TangentX));
					TaIds.Add(TaId);
				}

				// copy tris data
				if (Section->ProcIndexBuffer.Num() % 3 != 0)
				{
					continue;
				}
				for (int32 Idx = 0; Idx < Section->ProcIndexBuffer.Num(); Idx+=3)
				{
					int32 VIdx1 = Section->ProcIndexBuffer[Idx];
					int32 VIdx2 = Section->ProcIndexBuffer[Idx + 1];
					int32 VIdx3 = Section->ProcIndexBuffer[Idx + 2];

					int32 VId1 = VtxIds[VIdx1];
					int32 VId2 = VtxIds[VIdx2];
					int32 VId3 = VtxIds[VIdx3];

					int32 TId = Mesh.AppendTriangle(VId1, VId2, VId3, SectionIdx);
					if (TId < 0)
					{
						continue;
					}

					NormalOverlay->SetTriangle(TId, FIndex3i(NormalIds[VIdx1], NormalIds[VIdx2], NormalIds[VIdx3]), true);
					ColorOverlay->SetTriangle(TId, FIndex3i(ColorIds[VIdx1], ColorIds[VIdx2], ColorIds[VIdx3]), true);
					UVOverlay->SetTriangle(TId, FIndex3i(UVIds[VIdx1], UVIds[VIdx2], UVIds[VIdx3]), true);
					TangentOverlay->SetTriangle(TId, FIndex3i(TaIds[VIdx1], TaIds[VIdx2], TaIds[VIdx3]), true);

					MaterialAttr->SetValue(TId, SectionIdx);
					GroupAttr->SetValue(TId, SectionIdx);
				}
			}
		}
	}

	if (Mesh.TriangleCount() == 0)
	{
		return false;
	}

	MeshTransforms::ApplyTransform(Mesh, RelativeTransform);

	OutputMeshMaterials.Append(InComponent->GetMaterials());
	ConvertedMeshes.Add(MoveTemp(Mesh));

	return true;
}

bool FCEClonerMeshBuilder::AppendComponent(UBrushComponent* InComponent, const FTransform& InSourceTransform)
{
	return AppendPrimitiveComponent(InComponent, InSourceTransform);
}

bool FCEClonerMeshBuilder::AppendComponent(const USkeletalMeshComponent* InComponent, const FTransform& InSourceTransform)
{
	if (!IsValid(InComponent))
	{
		return false;
	}

	USkeletalMesh* SkeletalMesh = InComponent->GetSkeletalMeshAsset();

	if (!IsValid(SkeletalMesh))
	{
		return false;
	}

	// convert to dynamic mesh
	FGeometryScriptMeshReadLOD SkeletalMeshLOD;
	SkeletalMeshLOD.LODType = EGeometryScriptLODType::SourceModel;

	FGeometryScriptCopyMeshFromAssetOptions OutputMeshOptions;
	OutputMeshOptions.bIgnoreRemoveDegenerates = false;
	OutputMeshOptions.bRequestTangents = false;
	OutputMeshOptions.bApplyBuildSettings = false;

	EGeometryScriptOutcomePins OutResult;
	UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromSkeletalMesh(SkeletalMesh, OutputDynamicMesh, OutputMeshOptions, SkeletalMeshLOD, OutResult);

	if (OutResult != EGeometryScriptOutcomePins::Success)
	{
		return false;
	}

	// Transform the new mesh relative to the component
	const FTransform RelativeTransform = InComponent->GetComponentTransform().GetRelativeTransform(InSourceTransform);

	// Copy materials
	OutputMeshMaterials.Append(InComponent->GetMaterials());

	OutputDynamicMesh->EditMesh([this, RelativeTransform](FDynamicMesh3& EditMesh)
	{
		MeshTransforms::ApplyTransform(EditMesh, RelativeTransform);
		ConvertedMeshes.Add(MoveTemp(EditMesh));

		// replace by empty mesh for next usage
		FDynamicMesh3 EmptyMesh;
		EditMesh = MoveTemp(EmptyMesh);
	}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, /** bDeferChanges */true);

	return true;
}

bool FCEClonerMeshBuilder::AppendComponent(UDynamicMeshComponent* InComponent, const FTransform& InSourceTransform)
{
	if (!IsValid(InComponent))
	{
		return false;
	}

	const UDynamicMesh* DynamicMesh = InComponent->GetDynamicMesh();

	if (!IsValid(DynamicMesh))
	{
		return false;
	}

	// Transform the new mesh relative to the component
	const FTransform RelativeTransform = InComponent->GetComponentTransform().GetRelativeTransform(InSourceTransform);

	// Copy all materials
	OutputMeshMaterials.Append(InComponent->GetMaterials());

	// Create a copy of the mesh
	DynamicMesh->ProcessMesh([this, RelativeTransform](const FDynamicMesh3& EditMesh)
	{
		FDynamicMesh3 CopyMesh = EditMesh;
		MeshTransforms::ApplyTransform(CopyMesh, RelativeTransform);
		ConvertedMeshes.Add(MoveTemp(CopyMesh));
	});

	return true;
}

bool FCEClonerMeshBuilder::AppendComponent(UInstancedStaticMeshComponent* InComponent, const FTransform& InSourceTransform)
{
	if (!IsValid(InComponent) || !IsValid(InComponent->GetStaticMesh()))
	{
		return false;
	}

	return AppendPrimitiveComponent(InComponent, InSourceTransform);
}

bool FCEClonerMeshBuilder::AppendComponent(USplineMeshComponent* InComponent, const FTransform& InSourceTransform)
{
	if (!IsValid(InComponent) || !IsValid(InComponent->GetStaticMesh()))
	{
		return false;
	}

	return AppendPrimitiveComponent(InComponent, InSourceTransform);
}

bool FCEClonerMeshBuilder::BuildDynamicMesh(UDynamicMesh* OutMesh, TArray<TWeakObjectPtr<UMaterialInterface>>& OutMaterials)
{
	if (!IsValid(OutMesh))
	{
		return false;
	}

	// Lets combine all meshes components from this actor together
	OutMesh->EditMesh([this](FDynamicMesh3& InMergedMesh)
	{
		using namespace UE::Geometry;

		InMergedMesh.Clear();
		InMergedMesh.EnableAttributes();
		InMergedMesh.Attributes()->SetNumNormalLayers(1);

		FDynamicMeshEditor Editor(&InMergedMesh);
		FGeometryScriptAppendMeshOptions AppendOptions;
		AppendOptions.CombineMode = EGeometryScriptCombineAttributesMode::EnableAllMatching;
		int32 MaterialCount = 0;

		// Convert meshes
		for (int32 MeshIdx = 0; MeshIdx < ConvertedMeshes.Num(); MeshIdx++)
		{
			const FDynamicMesh3& ConvertedMesh = ConvertedMeshes[MeshIdx];

			// Enable matching attributes & append mesh
			FMeshIndexMappings TmpMappings;
			AppendOptions.UpdateAttributesForCombineMode(InMergedMesh, ConvertedMesh);
			Editor.AppendMesh(&ConvertedMesh, TmpMappings);

			// Fix triangles materials linking
			if (ConvertedMesh.HasAttributes() && ConvertedMesh.Attributes()->HasMaterialID())
			{
				const FDynamicMeshMaterialAttribute* FromMaterialIDAttrib = ConvertedMesh.Attributes()->GetMaterialID();
				FDynamicMeshMaterialAttribute* ToMaterialIDAttrib = InMergedMesh.Attributes()->GetMaterialID();
				TMap<int32, int32> MaterialMap;
				for (const TPair<int32, int32>& FromToTId : TmpMappings.GetTriangleMap().GetForwardMap())
				{
					const int32 FromMatId = FromMaterialIDAttrib->GetValue(FromToTId.Key);
					const int32 ToMatId = FromMatId + MaterialCount;
					MaterialMap.Add(FromMatId, ToMatId);
					ToMaterialIDAttrib->SetNewValue(FromToTId.Value, ToMatId);
				}
				MaterialCount += MaterialMap.Num();
			}
		}

		if (InMergedMesh.TriangleCount() > 0)
		{
			// Merge shared edges
			FMergeCoincidentMeshEdges WeldOp(&InMergedMesh);
			WeldOp.Apply();
		}
	}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, /** bDeferChange */true);

	OutMaterials = MoveTemp(OutputMeshMaterials);

	ConvertedMeshes.Empty();
	OutputMeshMaterials = {};

	return true;
}

bool FCEClonerMeshBuilder::BuildStaticMesh(UStaticMesh* OutMesh, TArray<TWeakObjectPtr<UMaterialInterface>>& OutMaterials)
{
	if (!IsValid(OutMesh))
	{
		return false;
	}

	UDynamicMesh* OutDynamicMesh = OutputDynamicMesh;
	if (!BuildDynamicMesh(OutDynamicMesh, OutMaterials))
	{
		return false;
	}

	if (OutDynamicMesh->GetTriangleCount() == 0)
	{
		return false;
	}

	TArray<TObjectPtr<UMaterialInterface>> NewMaterials;
	Algo::Transform(OutMaterials, NewMaterials, [](const TWeakObjectPtr<UMaterialInterface>& InMaterialWeak)
	{
		return InMaterialWeak.Get();
	});

	// Export options
	FGeometryScriptCopyMeshToAssetOptions AssetOptions;
	AssetOptions.bReplaceMaterials = true;
	AssetOptions.bEnableRecomputeNormals = false;
	AssetOptions.bEnableRecomputeTangents = false;
	AssetOptions.bEnableRemoveDegenerates = true;
	AssetOptions.bEmitTransaction = false;
	AssetOptions.bApplyNaniteSettings = false;
	AssetOptions.bDeferMeshPostEditChange = false;
	AssetOptions.NewMaterials = NewMaterials;

	// LOD options
	FGeometryScriptMeshWriteLOD TargetLOD;
	TargetLOD.LODIndex = 0;
	TargetLOD.bWriteHiResSource = false;

	// Convert merged mesh to static mesh
	EGeometryScriptOutcomePins OutResult;
	UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshToStaticMesh(OutDynamicMesh, OutMesh, AssetOptions, TargetLOD, OutResult);

#if WITH_EDITORONLY_DATA
	// Compute normals and tangents
	FMeshDescription* MeshDescription = OutMesh->GetMeshDescription(TargetLOD.LODIndex);
	FStaticMeshOperations::ComputeTriangleTangentsAndNormals(*MeshDescription);
	FStaticMeshOperations::ComputeTangentsAndNormals(*MeshDescription, EComputeNTBsFlags::Normals);
#endif

	OutDynamicMesh->EditMesh([](FDynamicMesh3& EditMesh){ EditMesh.Clear(); });

	return true;
}

bool FCEClonerMeshBuilder::AppendPrimitiveComponent(UPrimitiveComponent* InComponent, const FTransform& InSourceTransform)
{
	if (!IsValid(InComponent))
	{
		return false;
	}

	constexpr FGeometryScriptCopyMeshFromComponentOptions Options;
	FTransform LocalToWorld = FTransform::Identity;

	// convert to dynamic mesh
	EGeometryScriptOutcomePins OutResult;
	UGeometryScriptLibrary_SceneUtilityFunctions::CopyMeshFromComponent(InComponent, OutputDynamicMesh, Options, false, LocalToWorld, OutResult);

	if (OutResult != EGeometryScriptOutcomePins::Success)
	{
		return false;
	}

	// Transform the new mesh relative to the component
	const FTransform RelativeTransform = InComponent->GetComponentTransform().GetRelativeTransform(InSourceTransform);

	// Copy all materials
	for (int32 Index = 0; Index < InComponent->GetNumMaterials(); Index++)
	{
		OutputMeshMaterials.Add(InComponent->GetMaterial(Index));
	}

	OutputDynamicMesh->EditMesh([this, RelativeTransform](FDynamicMesh3& EditMesh)
	{
		MeshTransforms::ApplyTransform(EditMesh, RelativeTransform);
		ConvertedMeshes.Add(MoveTemp(EditMesh));

		// replace by empty mesh
		FDynamicMesh3 EmptyMesh;
		EditMesh = MoveTemp(EmptyMesh);
	}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, /** bDeferChange */true);

	return true;
}
