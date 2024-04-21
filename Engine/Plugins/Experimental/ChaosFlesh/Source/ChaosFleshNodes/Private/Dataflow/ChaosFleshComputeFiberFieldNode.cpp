// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/ChaosFleshComputeFiberFieldNode.h"

#include "Chaos/Math/Poisson.h"
#include "ChaosFlesh/ChaosFlesh.h"
#include "ChaosFlesh/TetrahedralCollection.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ChaosFleshComputeFiberFieldNode)

void FComputeFiberFieldNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		//
		// Gather inputs
		//

		FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		TArray<int32> InOriginIndices = GetValue<TArray<int32>>(Context, &OriginIndices);
		TArray<int32> InInsertionIndices = GetValue<TArray<int32>>(Context, &InsertionIndices);

		// Tetrahedra
		TManagedArray<FIntVector4>* Elements = InCollection.FindAttribute<FIntVector4>(
			FTetrahedralCollection::TetrahedronAttribute, FTetrahedralCollection::TetrahedralGroup);
		if (!Elements)
		{
			UE_LOG(LogChaosFlesh, Warning,
				TEXT("ComputeFiberFieldNode: Failed to find geometry collection attr '%s' in group '%s'"),
				*FTetrahedralCollection::TetrahedronAttribute.ToString(), *FTetrahedralCollection::TetrahedralGroup.ToString());
			Out->SetValue(MoveTemp(InCollection), Context);
			return;
		}

		// Vertices
		TManagedArray<FVector3f>* Vertex = InCollection.FindAttribute<FVector3f>("Vertex", "Vertices");
		if (!Vertex)
		{
			UE_LOG(LogChaosFlesh, Warning,
				TEXT("ComputeFiberFieldNode: Failed to find geometry collection attr 'Vertex' in group 'Vertices'"));
			Out->SetValue(MoveTemp(InCollection), Context);
			return;
		}

		// Incident elements
		TManagedArray<TArray<int32>>* IncidentElements = InCollection.FindAttribute<TArray<int32>>(
			FTetrahedralCollection::IncidentElementsAttribute, FGeometryCollection::VerticesGroup);
		if (!IncidentElements)
		{
			UE_LOG(LogChaosFlesh, Warning,
				TEXT("ComputeFiberFieldNode: Failed to find geometry collection attr '%s' in group '%s'"),
				*FTetrahedralCollection::IncidentElementsAttribute.ToString(), *FGeometryCollection::VerticesGroup.ToString());
			Out->SetValue(MoveTemp(InCollection), Context);
			return;
		}
		TManagedArray<TArray<int32>>* IncidentElementsLocalIndex = InCollection.FindAttribute<TArray<int32>>(
			FTetrahedralCollection::IncidentElementsLocalIndexAttribute, FGeometryCollection::VerticesGroup);
		if (!IncidentElementsLocalIndex)
		{
			UE_LOG(LogChaosFlesh, Warning,
				TEXT("ComputeFiberFieldNode: Failed to find geometry collection attr '%s' in group '%s'"),
				*FTetrahedralCollection::IncidentElementsLocalIndexAttribute.ToString(), *FGeometryCollection::VerticesGroup.ToString());
			Out->SetValue(MoveTemp(InCollection), Context);
			return;
		}

		//
		// Pull Origin & Insertion data out of the geometry collection.  We may want other ways of specifying
		// these via an input on the node...
		//

		// Origin & Insertion
		TManagedArray<int32>* Origin = nullptr; 
		TManagedArray<int32>* Insertion = nullptr;
		if (InOriginIndices.IsEmpty() || InInsertionIndices.IsEmpty())
		{
			// Origin & Insertion group
			if (OriginInsertionGroupName.IsEmpty())
			{
				UE_LOG(LogChaosFlesh, Warning, TEXT("ComputeFiberFieldNode: Attr 'OriginInsertionGroupName' cannot be empty."));
				Out->SetValue(MoveTemp(InCollection), Context);
				return;
			}

			// Origin vertices
			if (InOriginIndices.IsEmpty())
			{
				if (OriginVertexFieldName.IsEmpty())
				{
					UE_LOG(LogChaosFlesh, Warning, TEXT("ComputeFiberFieldNode: Attr 'OriginVertexFieldName' cannot be empty."));
					Out->SetValue(MoveTemp(InCollection), Context);
					return;
				}
				Origin = InCollection.FindAttribute<int32>(FName(OriginVertexFieldName), FName(OriginInsertionGroupName));
				if (!Origin)
				{
					UE_LOG(LogChaosFlesh, Warning,
						TEXT("ComputeFiberFieldNode: Failed to find geometry collection attr '%s' in group '%s'"),
						*OriginVertexFieldName, *OriginInsertionGroupName);
					Out->SetValue(MoveTemp(InCollection), Context);
					return;
				}
			}

			// Insertion vertices
			if (InInsertionIndices.IsEmpty())
			{
				if (InsertionVertexFieldName.IsEmpty())
				{
					UE_LOG(LogChaosFlesh, Warning, TEXT("ComputeFiberFieldNode: Attr 'InsertionVertexFieldName' cannot be empty."));
					Out->SetValue(MoveTemp(InCollection), Context);
					return;
				}
				Insertion = InCollection.FindAttribute<int32>(FName(InsertionVertexFieldName), FName(OriginInsertionGroupName));
				if (!Insertion)
				{
					UE_LOG(LogChaosFlesh, Warning,
						TEXT("ComputeFiberFieldNode: Failed to find geometry collection attr '%s' in group '%s'"),
						*InsertionVertexFieldName, *OriginInsertionGroupName);
					Out->SetValue(MoveTemp(InCollection), Context);
					return;
				}
			}
		}

		//
		// Do the thing
		//

		TArray<FVector3f> FiberDirs =
			ComputeFiberField(*Elements, *Vertex, *IncidentElements, *IncidentElementsLocalIndex, 
				Origin ? Origin->GetConstArray() : InOriginIndices,
				Insertion ? Insertion->GetConstArray() : InInsertionIndices);

		//
		// Set output(s)
		//

		TManagedArray<FVector3f>* FiberDirections =
			InCollection.FindAttribute<FVector3f>("FiberDirection", FTetrahedralCollection::TetrahedralGroup);
		if (!FiberDirections)
		{
			FiberDirections =
				&InCollection.AddAttribute<FVector3f>("FiberDirection", FTetrahedralCollection::TetrahedralGroup);
		}
		(*FiberDirections) = MoveTemp(FiberDirs);

		Out->SetValue(MoveTemp(InCollection), Context);
	}
}

TArray<int32>
FComputeFiberFieldNode::GetNonZeroIndices(const TArray<uint8>& Map) const
{
	int32 NumNonZero = 0;
	for (int32 i = 0; i < Map.Num(); i++)
		if (Map[i])
			NumNonZero++;
	TArray<int32> Indices; Indices.AddUninitialized(NumNonZero);
	int32 Idx = 0;
	for (int32 i = 0; i < Map.Num(); i++)
		if (Map[i])
			Indices[Idx++] = i;
	return Indices;
}

TArray<FVector3f>
FComputeFiberFieldNode::ComputeFiberField(
	const TManagedArray<FIntVector4>& Elements,
	const TManagedArray<FVector3f>& Vertex,
	const TManagedArray<TArray<int32>>& IncidentElements,
	const TManagedArray<TArray<int32>>& IncidentElementsLocalIndex,
	const TArray<int32>& Origin,
	const TArray<int32>& Insertion) const
{
	TArray<FVector3f> Directions;
	Chaos::ComputeFiberField<float>(
		Elements.GetConstArray(),
		Vertex.GetConstArray(),
		IncidentElements.GetConstArray(),
		IncidentElementsLocalIndex.GetConstArray(),
		Origin,
		Insertion,
		Directions,
		MaxIterations,
		Tolerance);
	return Directions;
}
