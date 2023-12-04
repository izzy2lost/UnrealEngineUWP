// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/DisplayClusterStageIsosphereComponent.h"

#include "Engine/StaticMesh.h"
#include "KismetProceduralMeshLibrary.h"
#include "UObject/ConstructorHelpers.h"

UDisplayClusterStageIsosphereComponent::UDisplayClusterStageIsosphereComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ConstructorHelpers::FObjectFinder<UStaticMesh>  IsosphereMeshFinder(TEXT("/nDisplay/Meshes/SM_IcoSphere.SM_IcoSphere"));

	IsosphereMesh = IsosphereMeshFinder.Object;

	SetHiddenInGame(true);
	SetVisibility(false);
}

void UDisplayClusterStageIsosphereComponent::OnComponentCreated()
{
	Super::OnComponentCreated();

	// Load the isosphere's geometry into the procedural mesh
	if (ensure(IsosphereMesh))
	{
		IsosphereMesh->bAllowCPUAccess = true;

		const int32 NumSections = IsosphereMesh->GetNumSections(0);
		for (int32 SectionIndex = 0; SectionIndex < NumSections; SectionIndex++)
		{
			TArray<FVector> Vertices;
			TArray<int32> Triangles;
			TArray<FVector> Normals;
			TArray<FVector2D> UVs;
			TArray<FProcMeshTangent> Tangents;

			UKismetProceduralMeshLibrary::GetSectionFromStaticMesh(IsosphereMesh, 0, SectionIndex, Vertices, Triangles, Normals, UVs, Tangents);

			TArray<FVector2D> EmptyUVs;
			TArray<FLinearColor> EmptyColors;
			CreateMeshSection_LinearColor(SectionIndex, Vertices, Triangles, Normals, UVs, EmptyUVs, EmptyUVs, EmptyUVs, EmptyColors, Tangents, true);

			for (int32 Index = 0; Index < IsosphereMesh->GetStaticMaterials().Num(); ++Index)
			{
				if (UMaterialInterface* MaterialInterface = IsosphereMesh->GetStaticMaterials()[Index].MaterialInterface)
				{
					SetMaterial(Index, MaterialInterface);
				}
			}
		}
	}
}

void UDisplayClusterStageIsosphereComponent::ResetIsosphere()
{
	if (ensure(IsosphereMesh))
	{
		const int32 NumSections = IsosphereMesh->GetNumSections(0);
		for (int32 SectionIndex = 0; SectionIndex < NumSections; SectionIndex++)
		{
			TArray<FVector> Vertices;
			TArray<int32> Triangles;
			TArray<FVector> Normals;
			TArray<FVector2D> UVs;
			TArray<FProcMeshTangent> Tangents;

			UKismetProceduralMeshLibrary::GetSectionFromStaticMesh(IsosphereMesh, 0, SectionIndex, Vertices, Triangles, Normals, UVs, Tangents);

			if (FProcMeshSection* Section = GetProcMeshSection(SectionIndex))
			{
				for (int32 Index = 0; Index < Vertices.Num(); ++Index)
				{
					FProcMeshVertex& Vertex = Section->ProcVertexBuffer[Index];
					Vertex.Position = Vertices[Index];
					Vertex.Normal = Normals[Index];
				}

				SetProcMeshSection(0, *Section);
			}
		}
	}
}
