// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryMaskWriteComponent.h"

#include "Algo/RemoveIf.h"
#include "Async/ParallelFor.h"
#include "BatchedElements.h"
#include "CanvasTypes.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GeometryMaskCanvas.h"
#include "GlobalRenderResources.h"
#include "StaticMeshResources.h"

void UGeometryMaskWriteMeshComponent::SetParameters(FGeometryMaskWriteParameters& InParameters)
{
	Parameters = InParameters;

	UGeometryMaskCanvas* Canvas = CanvasWeak.Get();

	// We have changed canvas, unregister this writer
	if (Canvas && Canvas->GetFName() != InParameters.CanvasName)
	{
		Canvas->RemoveWriter(this);
		CanvasWeak.Reset();
	}

	// Update canvas
	if (!CanvasWeak.IsValid())
	{
		TryResolveCanvas();
	}
}

void UGeometryMaskWriteMeshComponent::DrawToCanvas(FCanvas* InCanvas)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UGeometryMaskWriteMeshComponent::DrawToCanvas);

	if (!bWriteWhenHidden)
	{
		if (const AActor* Owner = GetOwner())
		{
			if (Owner->IsHidden())
			{
				return;
			}
		}
	}

	UpdateCachedData();

	FColor Color = UE::GeometryMask::MaskChannelEnumToColor[GetParameters().ColorChannel];
	ESimpleElementBlendMode ElementBlendMode = SE_BLEND_Additive;
	if (GetParameters().OperationType == EGeometryMaskCompositeOperation::Subtract)
	{
		Color = FColor(255 - Color.R, 255 - Color.G, 255 - Color.B, 255);
		ElementBlendMode = SE_BLEND_Modulate;
	}

	// @todo: cached mesh data keyed by SM name, but transforms also tied to SM, not SMC - will break if same SM used in multiple SMC's

	// Write to Canvas BatchElements
	{
		FHitProxyId CanvasHitProxyId = InCanvas->GetHitProxyId();

		TArray<FName> KeyNames;
		CachedMeshData.GenerateKeyArray(KeyNames);

		TRACE_CPUPROFILER_EVENT_SCOPE(UGeometryMaskWriteMeshComponent::DrawToCanvas::WriteToCanvas);

		ParallelFor(KeyNames.Num(), [&](int32 TaskIdx)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(UGeometryMaskWriteMeshComponent::DrawToCanvas::WriteToCanvas::Task);

			const FName KeyName = KeyNames[TaskIdx];
			FTransform LocalToWorld;
			
			if (const USceneComponent* Component = CachedComponents[KeyName].Get())
			{
				LocalToWorld = Component->GetComponentToWorld();
			}
			else
			{
				// @todo: remove from cache
				return;
			}

			const FGeometryMaskBatchElementData& MeshBatchElementData = CachedMeshData[KeyName];

			{
				FScopeLock CanvasLock(&CanvasCS);
				InCanvas->PushAbsoluteTransform(LocalToWorld.ToMatrixWithScale());	
			}
			
			{
				FBatchedElements* CanvasTriangleBatchedElements = nullptr;				
				{
					FScopeLock CanvasLock(&CanvasCS);
					CanvasTriangleBatchedElements = InCanvas->GetBatchedElements(FCanvas::ET_Triangle);
				}
				
				CanvasTriangleBatchedElements->ReserveVertices(MeshBatchElementData.Vertices.Num());
				CanvasTriangleBatchedElements->ReserveTriangles(MeshBatchElementData.NumTriangles, GWhiteTexture, ElementBlendMode);
				
				auto AddVertex = [CanvasTriangleBatchedElements, &MeshBatchElementData, CanvasHitProxyId, &Color](int32 VertexIdx) -> int32
				{
					return CanvasTriangleBatchedElements->AddVertexf(
						MeshBatchElementData.Vertices[VertexIdx],
						FVector2f::ZeroVector,
						Color,
						CanvasHitProxyId);
				};

				{
					TRACE_CPUPROFILER_EVENT_SCOPE(UGeometryMaskWriteMeshComponent::DrawToCanvas::WriteToCanvas::AddVertices);
					for (int32 VertexIdx = 0; VertexIdx < MeshBatchElementData.Vertices.Num(); ++VertexIdx)
					{
						AddVertex(VertexIdx);
					}
				}

				{
					TRACE_CPUPROFILER_EVENT_SCOPE(UGeometryMaskWriteMeshComponent::DrawToCanvas::WriteToCanvas::AddTriangles);
					for (int32 VertexIdx = 0; VertexIdx <= MeshBatchElementData.Indices.Num() - 3; VertexIdx += 3)
					{
						const int32 V0 = MeshBatchElementData.Indices[VertexIdx];
						const int32 V1 = MeshBatchElementData.Indices[VertexIdx + 1];
						const int32 V2 = MeshBatchElementData.Indices[VertexIdx + 2];

						CanvasTriangleBatchedElements->AddTriangle(V0, V1, V2, GWhiteTexture, ElementBlendMode);
					}
				}
			}
			
			{
				FScopeLock CanvasLock(&CanvasCS);
				InCanvas->PopTransform();
			}
		});
	}
}

void UGeometryMaskWriteMeshComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	Super::OnComponentDestroyed(bDestroyingHierarchy);

	if (UGeometryMaskCanvas* Canvas = CanvasWeak.Get())
	{
		Canvas->RemoveWriter(this);
		CanvasWeak.Reset();
	}
}

void UGeometryMaskWriteMeshComponent::UpdateCachedData()
{
	if (const AActor* Owner = GetOwner())
	{
		TArray<UPrimitiveComponent*> PrimitiveComponents;
		Owner->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

		// @todo: better check for cached data state?
		if (LastPrimitiveComponentCount == PrimitiveComponents.Num())
		{
			return;
		}

		LastPrimitiveComponentCount = PrimitiveComponents.Num();
		CachedComponents.Reserve(LastPrimitiveComponentCount);
		CachedMeshData.Reserve(LastPrimitiveComponentCount);

		UpdateCachedStaticMeshData(PrimitiveComponents);
		UpdateCachedDynamicMeshData(PrimitiveComponents);
	}
}

void UGeometryMaskWriteMeshComponent::UpdateCachedStaticMeshData(TConstArrayView<UPrimitiveComponent*> InPrimitiveComponents)
{
	TArray<UStaticMeshComponent*> StaticMeshComponents;
	StaticMeshComponents.Reserve(InPrimitiveComponents.Num());
	
	Algo::TransformIf(
		InPrimitiveComponents,
		StaticMeshComponents,
		[](const UPrimitiveComponent* InComponent)
		{
			return InComponent->IsA<UStaticMeshComponent>();
		},
		[](UPrimitiveComponent* InComponent)
		{
			return Cast<UStaticMeshComponent>(InComponent);
		});

	if (StaticMeshComponents.IsEmpty())
	{
		return;
	}

	// Remove built/already cached
	StaticMeshComponents.SetNum(Algo::RemoveIf(StaticMeshComponents, [this](const UStaticMeshComponent* InStaticMeshComponent)
	{
		if (const UStaticMesh* StaticMesh = InStaticMeshComponent->GetStaticMesh())
		{
			if (const FGeometryMaskBatchElementData* CachedData = CachedMeshData.Find(StaticMesh->GetFName()))
			{
				if (CachedData->Vertices.Num() == StaticMesh->GetNumVertices(0))
				{
					return true; // Cached, remove from "to build" list							
				}
			}
		}

		return false;
	}));

	// Subscribe to change events
	{
		#if WITH_EDITOR
		for (UStaticMeshComponent* StaticMeshComponent : StaticMeshComponents)
		{
			StaticMeshComponent->OnStaticMeshChanged().AddUObject(this, &UGeometryMaskWriteMeshComponent::OnStaticMeshChanged);
		}
		#endif
	}

	// Used for cache lookup
	TArray<FName> StaticMeshObjectNames;
	StaticMeshObjectNames.Reserve(StaticMeshComponents.Num());
	
	TArray<FStaticMeshLODResources*> StaticMeshResources;
	StaticMeshResources.Reserve(StaticMeshComponents.Num());

	// Collect valid mesh resources
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UGeometryMaskWriteMeshComponent::DrawToCanvas::CollectMeshResources);
		for (UStaticMeshComponent* StaticMeshComponent : StaticMeshComponents)
		{
			if (UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh())
			{
				if (!StaticMesh->HasValidRenderData())
				{
					continue;
				}
			
				if (FStaticMeshRenderData* RenderData = StaticMesh->GetRenderData())
				{
					if (RenderData->LODResources.IsEmpty())
						{
							continue;
						}

						if (RenderData->LODResources[0].GetNumVertices() == 0)
						{
							continue;
						}

						StaticMeshObjectNames.Add(StaticMesh->GetFName());
						StaticMeshResources.Add(&RenderData->LODResources[0]);
						CachedComponents.Add(StaticMesh->GetFName(), StaticMeshComponent);
					}
				}
			}
		}

		// Convert mesh resources to batch elements
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(UGeometryMaskWriteMeshComponent::DrawToCanvas::ConvertMeshResources);
			ParallelFor(StaticMeshResources.Num(),
			[this, &StaticMeshResources, &StaticMeshObjectNames](int32 TaskIdx)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(UGeometryMaskWriteMeshComponent::DrawToCanvas::ConvertMeshResources::Task);
				
				FStaticMeshLODResources* MeshResources = StaticMeshResources[TaskIdx];
				const int32 NumVertices = MeshResources->GetNumVertices();
				const int32 NumIndices = MeshResources->IndexBuffer.GetNumIndices();
				const int32 NumTriangles = MeshResources->GetNumTriangles();

				FGeometryMaskBatchElementData& MeshBatchElementData = CachedMeshData.Emplace(StaticMeshObjectNames[TaskIdx]);
				MeshBatchElementData.Reserve(NumVertices, NumIndices, NumTriangles);
				MeshResources->IndexBuffer.GetCopy(MeshBatchElementData.Indices);

				for (int32 VertexIdx = 0; VertexIdx < NumVertices; ++VertexIdx)
				{
					MeshBatchElementData.Vertices.Add(FVector4f(MeshResources->VertexBuffers.PositionVertexBuffer.VertexPosition(VertexIdx)));
				}
			});
		}
}

void UGeometryMaskWriteMeshComponent::UpdateCachedDynamicMeshData(TConstArrayView<UPrimitiveComponent*> InPrimitiveComponents)
{
	TArray<UDynamicMeshComponent*> DynamicMeshComponents;
	DynamicMeshComponents.Reserve(InPrimitiveComponents.Num());
	
	Algo::TransformIf(
		InPrimitiveComponents,
		DynamicMeshComponents,
		[](const UPrimitiveComponent* InComponent)
		{
			return InComponent->IsA<UDynamicMeshComponent>();
		},
		[](UPrimitiveComponent* InComponent)
		{
			return Cast<UDynamicMeshComponent>(InComponent);
		});

	if (DynamicMeshComponents.IsEmpty())
	{
		return;
	}

	// Remove built/already cached
	DynamicMeshComponents.SetNum(Algo::RemoveIf(DynamicMeshComponents, [this](UDynamicMeshComponent* InDynamicMeshComponent)
	{
		if (const UDynamicMesh* DynamicMesh = InDynamicMeshComponent->GetDynamicMesh())
		{
			if (const FGeometryMaskBatchElementData* CachedData = CachedMeshData.Find(InDynamicMeshComponent->GetFName()))
			{
				if (CachedData->Vertices.Num() == DynamicMesh->GetMeshRef().VertexCount()
					&& CachedData->ChangeStamp == DynamicMesh->GetMeshRef().GetChangeStamp())
				{
					return true; // Cached, remove from "to build" list							
				}
			}
		}

		return false;
	}));

	// Subscribe to change events
	{
		for (UDynamicMeshComponent* DynamicMeshComponent : DynamicMeshComponents)
		{
			DynamicMeshComponent->GetMesh()->SetShapeChangeStampEnabled(true);			
			DynamicMeshComponent->OnMeshChanged.AddUObject(this, &UGeometryMaskWriteMeshComponent::OnDynamicMeshChanged, DynamicMeshComponent);
		}
	}

	// Used for cache lookup
	TArray<FName> DynamicMeshObjectNames;
	DynamicMeshObjectNames.Reserve(DynamicMeshComponents.Num());
	
	TArray<FDynamicMesh3*> DynamicMeshes;
	DynamicMeshes.Reserve(DynamicMeshComponents.Num());

	// Collect valid meshes
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UGeometryMaskWriteMeshComponent::DrawToCanvas::CollectMeshResources);
		for (UDynamicMeshComponent* DynamicMeshComponent : DynamicMeshComponents)
		{
			if (UDynamicMesh* DynamicMeshObject = DynamicMeshComponent->GetDynamicMesh())
			{
				if (FDynamicMesh3* DynamicMesh = DynamicMeshObject->GetMeshPtr())
				{
					DynamicMeshObjectNames.Add(DynamicMeshComponent->GetFName());
					DynamicMeshes.Add(DynamicMesh);
					CachedComponents.Add(DynamicMeshComponent->GetFName(), DynamicMeshComponent);
				}
			}
		}
	}

	// Convert meshes to batch elements
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UGeometryMaskWriteMeshComponent::DrawToCanvas::ConvertMeshResources);
		ParallelFor(DynamicMeshes.Num(),
		[this, &DynamicMeshes, &DynamicMeshObjectNames](int32 TaskIdx)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(UGeometryMaskWriteMeshComponent::DrawToCanvas::ConvertMeshResources::Task);

			FDynamicMesh3 CompactMesh;
			const FDynamicMesh3* DynamicMesh = DynamicMeshes[TaskIdx];
			CompactMesh.CompactCopy(*DynamicMesh);

			const int32 NumVertices = CompactMesh.VertexCount();
			const int32 NumIndices = CompactMesh.TriangleCount() * 3;
			const int32 NumTriangles = CompactMesh.TriangleCount();

			FGeometryMaskBatchElementData& MeshBatchElementData = CachedMeshData.Emplace(DynamicMeshObjectNames[TaskIdx]);
			MeshBatchElementData.ChangeStamp = DynamicMesh->GetChangeStamp();
			MeshBatchElementData.Reserve(NumVertices, NumIndices, NumTriangles);

			for (const int32 VertexIdx : CompactMesh.VertexIndicesItr())
			{
				MeshBatchElementData.Vertices.Add(FVector4f(FVector3f(CompactMesh.GetVertex(VertexIdx))));				
			}

			for (const UE::Geometry::FIndex3i Triangle : CompactMesh.TrianglesItr())
			{
				MeshBatchElementData.Indices.Append({
					static_cast<uint32>(Triangle.A),
					static_cast<uint32>(Triangle.B),
					static_cast<uint32>(Triangle.C)});
			}
		});
	}
}

#if WITH_EDITOR
void UGeometryMaskWriteMeshComponent::OnStaticMeshChanged(UStaticMeshComponent* InStaticMeshComponent)
{
	// Triggers a cache refresh
	LastPrimitiveComponentCount = -1;
}
#endif

void UGeometryMaskWriteMeshComponent::OnDynamicMeshChanged(UDynamicMeshComponent* InDynamicMeshComponent)
{
	// Triggers a specific cache refresh
	if (FGeometryMaskBatchElementData* CachedData = CachedMeshData.Find(InDynamicMeshComponent->GetFName()))
	{
		CachedData->ChangeStamp = INDEX_NONE;
	}
	
	// Triggers a general cache refresh
	LastPrimitiveComponentCount = -1;
}

bool UGeometryMaskWriteMeshComponent::TryResolveCanvas()
{
	if (TryResolveNamedCanvas(Parameters.CanvasName))
	{
		if (UGeometryMaskCanvas* Canvas = CanvasWeak.Get())
		{
			// Register this writer
			Parameters.ColorChannel = Canvas->GetColorChannel();
			Canvas->AddWriter(this);
			return true;
		}
	}

	return false;
}
