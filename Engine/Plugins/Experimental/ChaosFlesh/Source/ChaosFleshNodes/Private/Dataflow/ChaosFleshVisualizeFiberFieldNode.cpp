// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/ChaosFleshVisualizeFiberFieldNode.h"
#include "GeometryCollection/Facades/CollectionPositionTargetFacade.h"

#include "ChaosFlesh/TetrahedralCollection.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ChaosFleshVisualizeFiberFieldNode)

void FVisualizeFiberFieldNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FFieldCollection>(&VectorField))
	{
		FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		FFieldCollection OutVectorField = VectorField;
		
		if (TManagedArray<FVector3f>* Vertex = InCollection.FindAttribute<FVector3f>("Vertex", "Vertices"))
		{
			if (TManagedArray<FIntVector4>* Elements = InCollection.FindAttribute<FIntVector4>(FTetrahedralCollection::TetrahedronAttribute, FTetrahedralCollection::TetrahedralGroup))
			{
				if (TManagedArray<FVector3f>* FiberDirections = InCollection.FindAttribute<FVector3f>("FiberDirection", FTetrahedralCollection::TetrahedralGroup))
				{
					ensureMsgf(Elements->Num() == FiberDirections->Num(), TEXT("Fiber direction has different size than elements"));
					for (int32 ElemIndex = 0; ElemIndex < Elements->Num(); ElemIndex++)
					{
						if ((*FiberDirections)[ElemIndex].Length() > 1e-10)
						{
							FVector3f VectorStart = { 0,0,0 };
							for (int32 LocalIndex = 0; LocalIndex < 4; LocalIndex++)
							{
								VectorStart += (*Vertex)[(*Elements)[ElemIndex][LocalIndex]];
							}
							VectorStart /= float(4);
							FVector3f VectorEnd = VectorStart + (*FiberDirections)[ElemIndex] * VectorScale;
							OutVectorField.AddVectorToField(VectorStart, VectorEnd);
						}
					}
				}
			}
		}
		
		Out->SetValue(MoveTemp(OutVectorField), Context);
	}
}

void FVisualizePositionTargetsNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FFieldCollection>(&VectorField))
	{
		FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		FFieldCollection OutVectorField = VectorField;

		if (TManagedArray<FVector3f>* Vertex = InCollection.FindAttribute<FVector3f>("Vertex", "Vertices"))
		{
			GeometryCollection::Facades::FPositionTargetFacade PositionTargets(InCollection);

			// Read in position target info
			for (int i = 0; i < PositionTargets.NumPositionTargets(); i++)
			{
				GeometryCollection::Facades::FPositionTargetsData DataPackage = PositionTargets.GetPositionTarget(i);			
				FVector3f VectorStart = { 0,0,0 };
				for (int32 LocalIndex = 0; LocalIndex < DataPackage.SourceIndex.Num(); LocalIndex++)
				{
					VectorStart += DataPackage.SourceWeights[LocalIndex] * (*Vertex)[DataPackage.SourceIndex[LocalIndex]];
				}
				FVector3f VectorEnd = { 0,0,0 };
				for (int32 LocalIndex = 0; LocalIndex < DataPackage.TargetIndex.Num(); LocalIndex++)
				{
					VectorEnd += DataPackage.TargetWeights[LocalIndex] * (*Vertex)[DataPackage.TargetIndex[LocalIndex]];
				}
				OutVectorField.AddVectorToField(VectorStart, VectorEnd);
			}
		}

		Out->SetValue(MoveTemp(OutVectorField), Context);
	}
}