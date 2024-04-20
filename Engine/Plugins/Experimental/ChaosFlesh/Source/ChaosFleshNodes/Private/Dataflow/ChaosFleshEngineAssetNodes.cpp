// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/ChaosFleshEngineAssetNodes.h"

#include "Chaos/Math/Poisson.h"
#include "Chaos/Matrix.h"
#include "Chaos/Utilities.h"
#include "ChaosFlesh/ChaosFlesh.h"
#include "ChaosFlesh/FleshAsset.h"
#include "ChaosFlesh/FleshCollection.h"
#include "ChaosFlesh/FleshCollectionUtility.h"
#include "Dataflow/DataflowInputOutput.h"
#include "Dataflow/DataflowObjectInterface.h"
#include "Dataflow/DataflowNodeFactory.h"
#include "GeometryCollection/Facades/CollectionMeshFacade.h"
#include "GeometryCollection/Facades/CollectionMuscleActivationFacade.h"
#include "GeometryCollection/TransformCollection.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(ChaosFleshEngineAssetNodes)

namespace Dataflow
{
	void RegisterChaosFleshEngineAssetNodes()
	{
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGetFleshAssetDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FFleshAssetTerminalDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FComputeFiberFieldNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FComputeMuscleActivationDataNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FVisualizeFiberFieldNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FComputeIslandsNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGenerateOriginInsertionNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FIsolateComponentNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGetSurfaceIndicesNode);
	}
}

void FGetFleshAssetDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Output))
	{
		FManagedArrayCollection Collection;
		SetValue(Context, MoveTemp(Collection), &Output);

		const UFleshAsset* FleshAssetValue = FleshAsset;
		if (!FleshAssetValue)
		{
			if (const Dataflow::FEngineContext* EngineContext = Context.AsType<Dataflow::FEngineContext>())
			{
				FleshAssetValue = Cast<UFleshAsset>(EngineContext->Owner);
			}
		}

		if (FleshAssetValue)
		{
			if (const FFleshCollection* AssetCollection = FleshAssetValue->GetCollection())
			{
				SetValue(Context, (const FManagedArrayCollection&)(*AssetCollection), &Output);
			}
		}
	}
}

void FFleshAssetTerminalDataflowNode::SetAssetValue(TObjectPtr<UObject> Asset, Dataflow::FContext& Context) const
{
	if (UFleshAsset* FleshAsset = Cast<UFleshAsset>(Asset.Get()))
	{
		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		FleshAsset->SetCollection(InCollection.NewCopy<FFleshCollection>());
	}
}

void FFleshAssetTerminalDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
	SetValue(Context, InCollection, &Collection);
}

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
		
		Out->SetValue(MoveTemp(OutVectorField), Context);
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

void FComputeMuscleActivationDataNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		TArray<int32> InOriginIndices = GetValue<TArray<int32>>(Context, &OriginIndicesIn);
		TArray<int32> InInsertionIndices = GetValue<TArray<int32>>(Context, &InsertionIndicesIn);
		TArray<float> OriginInsertionRestLength;
		if (InOriginIndices.Num() > 0 && InInsertionIndices.Num() > 0)
		{
			if (TManagedArray<FVector3f>* Vertex = InCollection.FindAttribute<FVector3f>("Vertex", "Vertices"))
			{
				if (TManagedArray<FIntVector4>* Elements = InCollection.FindAttribute<FIntVector4>(FTetrahedralCollection::TetrahedronAttribute, FTetrahedralCollection::TetrahedralGroup))
				{
					if (TManagedArray<FVector3f>* FiberDirections = InCollection.FindAttribute<FVector3f>("FiberDirection", FTetrahedralCollection::TetrahedralGroup))
					{
						GeometryCollection::Facades::FMuscleActivationFacade FMuscleActivation(InCollection);
						TArray<TArray<int32>> MuscleActivationElements;
						TArray<int32> OriginNodes;
						TArray<int32> InsertionNodes;
						GeometryCollection::Facades::FCollectionMeshFacade MeshFacade(InCollection);
						TArray<int32> ComponentIndex = MeshFacade.GetGeometryGroupIndexArray();
						TMap<int32, int32> ComponentToIndex; //Component index to muscle index
						for (int32 i = 0; i < InOriginIndices.Num(); i++)
						{
							if (!ComponentToIndex.Contains(ComponentIndex[InOriginIndices[i]]))
							{
								//TODO: use one pair of separately defined origin and insertion for each muscle instead of choosing the first from kinematic origins and last from insertions
								ComponentToIndex.Add(ComponentIndex[InOriginIndices[i]], OriginNodes.Num());
								OriginNodes.Add(InOriginIndices[i]);
							}
						}
						InsertionNodes.Init(INDEX_NONE, OriginNodes.Num());
						for (int32 i = 0; i < InInsertionIndices.Num(); i++)
						{
							if (!ComponentToIndex.Contains(ComponentIndex[InInsertionIndices[i]]))
							{
								ensureMsgf(false, TEXT("No origin in this component"));
							}
							else if (InsertionNodes[ComponentToIndex[ComponentIndex[InInsertionIndices[i]]]] == INDEX_NONE)
							{
								InsertionNodes[ComponentToIndex[ComponentIndex[InInsertionIndices[i]]]] = InInsertionIndices[i];
							}
						}
						MuscleActivationElements.SetNum(OriginNodes.Num());
						for (int32 ElemIdx = 0; ElemIdx < Elements->Num(); ElemIdx++)
						{
							if (ComponentToIndex.Contains(ComponentIndex[(*Elements)[ElemIdx][0]]))
							{
								MuscleActivationElements[ComponentToIndex[ComponentIndex[(*Elements)[ElemIdx][0]]]].Add(ElemIdx);
							}
						}
						OriginInsertionRestLength.SetNum(OriginNodes.Num());

						for (int32 MuscleComponentIdx = 0; MuscleComponentIdx < OriginNodes.Num(); MuscleComponentIdx++)
						{
							if (ensureMsgf(InsertionNodes[MuscleComponentIdx] != INDEX_NONE, TEXT("InsertionNodes[%d] is not a pair"), MuscleComponentIdx))
							{
								GeometryCollection::Facades::FMuscleActivationData MuscleActivationData;
								MuscleActivationData.MuscleActivationElement = MuscleActivationElements[MuscleComponentIdx];
								MuscleActivationData.OriginInsertionPair = FIntVector2(OriginNodes[MuscleComponentIdx], InsertionNodes[MuscleComponentIdx]);
								MuscleActivationData.OriginInsertionRestLength = ((*Vertex)[OriginNodes[MuscleComponentIdx]] - (*Vertex)[InsertionNodes[MuscleComponentIdx]]).Size();
								MuscleActivationData.FiberDirectionMatrix.SetNum(MuscleActivationElements[MuscleComponentIdx].Num());
								for (int32 LocalElemIdx = 0; LocalElemIdx < MuscleActivationElements[MuscleComponentIdx].Num(); LocalElemIdx++)
								{
									FVector3f V = (*FiberDirections)[MuscleActivationElements[MuscleComponentIdx][LocalElemIdx]];
									// QR decomposition on vvT
									FVector3f W = V;
									if (V.X < V.Y)
									{
										W.X += 1.f;
									}
									else
									{
										W.Y += 1.f;
									}
									FVector3f U = (V ^ W).GetSafeNormal();
									W = (U ^ V).GetSafeNormal();
									MuscleActivationData.FiberDirectionMatrix[LocalElemIdx] = Chaos::PMatrix33d(V, W, U);
								}
								FMuscleActivation.AddMuscleActivationData(MuscleActivationData);
							}
						}
					}
				}
			}
		}
		Out->SetValue(MoveTemp(InCollection), Context);
	}
}

void FComputeIslandsNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);

		TManagedArray<int32>& ParticleComponentIndex = InCollection.AddAttribute<int32>("ComponentIndex", FGeometryCollection::VerticesGroup);
		TManagedArray<FIntVector4>* Elements = InCollection.FindAttribute<FIntVector4>(
			FTetrahedralCollection::TetrahedronAttribute, FTetrahedralCollection::TetrahedralGroup);


		if (Elements)
		{

			int32 VertsNum = InCollection.NumElements(FGeometryCollection::VerticesGroup);
			int32 TetsNum = InCollection.NumElements(FTetrahedralCollection::TetrahedralGroup);
			if (VertsNum && TetsNum)
			{
				TArray<TArray<int32>> ConnectedComponents;
				Chaos::Utilities::FindConnectedRegions(Elements->GetConstArray(), ConnectedComponents);
				TManagedArray<int32>& ComponentIndex = InCollection.ModifyAttribute<int32>("ComponentIndex", FGeometryCollection::VerticesGroup);

				ComponentIndex.Fill(INDEX_NONE); //Isolated points will get index -1 

				for (int32 i = 0; i < ConnectedComponents.Num(); i++)
				{
					for (int32 j = 0; j < ConnectedComponents[i].Num(); j++)
					{
						int32 ElementIndex = ConnectedComponents[i][j];
						for (int32 ie = 0; ie < 4; ie++)
						{
							int32 ParticleIndex = (*Elements)[ElementIndex][ie];
							if (ComponentIndex[ParticleIndex] == INDEX_NONE)
							{
								ComponentIndex[ParticleIndex] = i;
							}
						}
					}
				}
			}
		}
		
		SetValue(Context, MoveTemp(InCollection), &Collection);
	}
}


void FGenerateOriginInsertionNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		//
		// Gather inputs
		//

		FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		TArray<int32> InOriginIndices = GetValue<TArray<int32>>(Context, &OriginIndicesIn);
		TArray<int32> InInsertionIndices = GetValue<TArray<int32>>(Context, &InsertionIndicesIn);
		TArray<int32> OutOriginIndices;
		TArray<int32> OutInsertionIndices;
		// Tetrahedra
		TManagedArray<FIntVector4>* Elements = InCollection.FindAttribute<FIntVector4>(
			FTetrahedralCollection::TetrahedronAttribute, FTetrahedralCollection::TetrahedralGroup);
		if (!Elements)
		{
			UE_LOG(LogChaosFlesh, Warning,
				TEXT("GenerateOriginInsertionNode: Failed to find geometry collection attr '%s' in group '%s'"),
				*FTetrahedralCollection::TetrahedronAttribute.ToString(), *FTetrahedralCollection::TetrahedralGroup.ToString());
			Out->SetValue(MoveTemp(InCollection), Context);
			return;
		}

		// Vertices
		TManagedArray<FVector3f>* Vertex = InCollection.FindAttribute<FVector3f>("Vertex", "Vertices");
		//TArray<FVector3f>* MeshVertex;
		if (!Vertex)
		{
			UE_LOG(LogChaosFlesh, Warning,
				TEXT("GenerateOriginInsertionNode: Failed to find geometry collection attr 'Vertex' in group 'Vertices'"));
			Out->SetValue(MoveTemp(InCollection), Context);
			return;
		}

		// Incident elements
		TManagedArray<TArray<int32>>* IncidentElements = InCollection.FindAttribute<TArray<int32>>(
			FTetrahedralCollection::IncidentElementsAttribute, FGeometryCollection::VerticesGroup);
		if (!IncidentElements)
		{
			UE_LOG(LogChaosFlesh, Warning,
				TEXT("GenerateOriginInsertionNode: Failed to find geometry collection attr '%s' in group '%s'"),
				*FTetrahedralCollection::IncidentElementsAttribute.ToString(), *FGeometryCollection::VerticesGroup.ToString());
			Out->SetValue(MoveTemp(InCollection), Context);
			return;
		}
		TManagedArray<TArray<int32>>* IncidentElementsLocalIndex = InCollection.FindAttribute<TArray<int32>>(
			FTetrahedralCollection::IncidentElementsLocalIndexAttribute, FGeometryCollection::VerticesGroup);
		if (!IncidentElementsLocalIndex)
		{
			UE_LOG(LogChaosFlesh, Warning,
				TEXT("GenerateOriginInsertionNode: Failed to find geometry collection attr '%s' in group '%s'"),
				*FTetrahedralCollection::IncidentElementsLocalIndexAttribute.ToString(), *FGeometryCollection::VerticesGroup.ToString());
			Out->SetValue(MoveTemp(InCollection), Context);
			return;
		}

		//
		// Pull Origin & Insertion data out of the geometry collection.  We may want other ways of specifying
		// these via an input on the node...
		//
		auto DoubleVert = [](FVector3f V) { return FVector3d(V.X, V.Y, V.Z); };
		GeometryCollection::Facades::FCollectionMeshFacade MeshFacade(InCollection);
		TArray<int32> ComponentIndex = MeshFacade.GetGeometryGroupIndexArray();
		// Origin vertices
		if (!InOriginIndices.IsEmpty())
		{
			for (int32 i = 0; i < InOriginIndices.Num(); ++i)
			{
				if (InOriginIndices[i] < Vertex->Num())
				{
					for (int32 j = 0; j < Vertex->Num(); ++j)
					{
						if (ComponentIndex[InOriginIndices[i]] == ComponentIndex[j] 
							&& ComponentIndex[InOriginIndices[i]] >= 0
							&& ComponentIndex[j] >= 0
							&& ((*Vertex)[InOriginIndices[i]] - (*Vertex)[j]).Size() < Radius)
						{
							OutOriginIndices.Add(j);
						}
					}
				}
			}
		}

		// Insertion vertices
		if (!InInsertionIndices.IsEmpty())
		{
			for (int32 i = 0; i < InInsertionIndices.Num(); ++i)
			{
				if (InInsertionIndices[i] < Vertex->Num())
				{
					for (int32 j = 0; j < Vertex->Num(); ++j)
					{
						if (ComponentIndex[InInsertionIndices[i]] == ComponentIndex[j]
							&& ComponentIndex[InInsertionIndices[i]] >= 0
							&& ComponentIndex[j] >= 0
							&& ((*Vertex)[InInsertionIndices[i]] - (*Vertex)[j]).Size() < Radius)
						{
							OutInsertionIndices.Add(j);
						}
					}
				}
			}
		}

		SetValue(Context, MoveTemp(InCollection), &Collection);
		SetValue(Context, MoveTemp(OutOriginIndices), &OriginIndicesOut);
		SetValue(Context, MoveTemp(OutInsertionIndices), &InsertionIndicesOut);
	}
}

void FIsolateComponentNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{	
		FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		TArray<int32> DeleteList;
		TManagedArray<FIntVector>* Indices = InCollection.FindAttribute<FIntVector>("Indices", FGeometryCollection::FacesGroup);
		TManagedArray<bool>*FaceVisibility = InCollection.FindAttribute<bool>("Visible", FGeometryCollection::FacesGroup);
		TManagedArray<int32>* FaceStart = InCollection.FindAttribute<int32>("FaceStart", FGeometryCollection::GeometryGroup);
		TManagedArray<int32>* FaceCount = InCollection.FindAttribute<int32>("FaceCount", FGeometryCollection::GeometryGroup);
		if (Indices && FaceVisibility && FaceStart && FaceCount)
		{
			FaceVisibility->Fill(false);
			TSet<int32> GeometrySet;
			TArray<FString> StrArray;
			TargetGeometryIndex.ParseIntoArray(StrArray, *FString(" "));
			for (FString GeometryIdx : StrArray)
			{
				if (GeometryIdx.Len() && FCString::IsNumeric(*GeometryIdx))
				{
					GeometrySet.Add(FCString::Atoi(*GeometryIdx));
				}
			}
			for (TSet<int32>::TConstIterator It = GeometrySet.CreateConstIterator(); It; ++It)
			{
				for (int32 FaceIdx = (*FaceStart)[*It]; FaceIdx < (*FaceStart)[*It] + (*FaceCount)[*It]; FaceIdx++)
				{
					(*FaceVisibility)[FaceIdx] = true;
					DeleteList.Add(FaceIdx);
				}
			}
			if (bDeleteHiddenFaces)
			{
				InCollection.RemoveElements(FGeometryCollection::FacesGroup, DeleteList);
			}
		}
		SetValue(Context, MoveTemp(InCollection), &Collection);
	}
}

void FGetSurfaceIndicesNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
	TArray<int32> SurfaceIndicesLocal;
	if (TManagedArray<FIntVector>* Indices = InCollection.FindAttribute<FIntVector>("Indices", FGeometryCollection::FacesGroup))
	{
		if (FindInput(&GeometryGroupGuidsIn) && FindInput(&GeometryGroupGuidsIn)->GetConnection())
		{
			TArray<FString> GeometryGroupGuidsLocal = GetValue<TArray<FString>>(Context, &GeometryGroupGuidsIn);
			TManagedArray<int32>* IndicesStart = InCollection.FindAttribute<int32>("FaceStart", FGeometryCollection::GeometryGroup);
			TManagedArray<int32>* IndicesCount = InCollection.FindAttribute<int32>("FaceCount", FGeometryCollection::GeometryGroup);
			if (TManagedArray<FString>* Guids = InCollection.FindAttribute<FString>("Guid", FGeometryCollection::GeometryGroup))
			{
				for (int32 Idx = 0; Idx < IndicesStart->Num(); Idx++)
				{
					if (GeometryGroupGuidsLocal.Num() && Guids)
					{
						if (GeometryGroupGuidsLocal.Contains((*Guids)[Idx]))
						{
							for (int32 i = (*IndicesStart)[Idx]; i < (*IndicesStart)[Idx] + (*IndicesCount)[Idx]; i++)
							{
								for (int32 j = 0; j < 3; j++)
								{
									SurfaceIndicesLocal.AddUnique((*Indices)[i][j]);
								}
							}
						}
					}
				}
			}
		}
		else
		{
			for (int32 i = 0; i < Indices->Num(); i++)
			{
				for (int32 j = 0; j < 3; j++)
				{
					SurfaceIndicesLocal.AddUnique((*Indices)[i][j]);
				}
			}
		}
	}
	SetValue(Context, MoveTemp(SurfaceIndicesLocal), &SurfaceIndicesOut);
}
