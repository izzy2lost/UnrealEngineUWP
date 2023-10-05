// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chaos/ConvexOptimizer.h"
#include "Chaos/Tribox.h"
#include "ChaosStats.h"
#include "Chaos/ImplicitObjectBVH.h"
#include "Chaos/ImplicitObjectUnion.h"
#include "Chaos/PhysicsObjectCollisionInterface.h"

namespace Chaos
{
namespace CVars
{
	// Replace all the convexes within an implicit hierarchy with a simplified one (kdop18 tribox for now) for collision
	bool bChaosConvexSimplifyUnion = true;
	FAutoConsoleVariableRef CVarChaosSimplifyUnion(TEXT("p.Chaos.Convex.SimplifyUnion"), bChaosConvexSimplifyUnion, TEXT("If true replace all the convexes within an implcit hierarchy with a simplified one (kdop18 tribox for now) for collision"));

	// Max number of convex LODs used during simplification  for dynamic particles
	int32 ChaosConvexMaxDynamicLODs = 0;
	FAutoConsoleVariableRef CVarChaosConvexMaxDynamicLODs(TEXT("p.Chaos.Convex.MaxDynamicLODs"), ChaosConvexMaxDynamicLODs, TEXT("Max number of convex LODs used during simplication for dynamic particles"));

	// Max number of convex LODs used during simplification  for kinematic particles
	int32 ChaosConvexMaxKinematicLODs = -1;
	FAutoConsoleVariableRef CVarChaosConvexMaxKinematicLODs(TEXT("p.Chaos.Convex.MaxKinematicLODs"), ChaosConvexMaxKinematicLODs, TEXT("Max number of convex LODs used during simplication for kinematic particles"));
	
	// Tribox volume / convex hull threshold to trigger a volume splitting during tree construction
	float ChaosConvexSplittingThreshold = 1.2f;
	FAutoConsoleVariableRef CVarChaosConvexSplittingThreshold(TEXT("p.Chaos.Convex.SplittingThreshold"), ChaosConvexSplittingThreshold, TEXT("Tribox volume / convex hull threshold to trigger a volume splitting during tree construction"));

	// Min volume of the simplified convexes
	float ChaosConvexMinVolume = 0.2f;
	FAutoConsoleVariableRef CVarChaosConvexMinVolume(TEXT("p.Chaos.Convex.MinVolume"), ChaosConvexMinVolume, TEXT("Min volume of the simplified convexes"));
	
	extern int32 ChaosUnionBVHMaxDepth;
	extern int32 ChaosUnionBVHMinShapes;
}

namespace Private
{
/**
 * @brief Simple tribox node to store tribox hierarchy information
 */
struct FTriboxNode
{
	// Constructor with a given node tribox and the underling objects range 
	FTriboxNode(const FTribox& NodeBounds, const TArray<FTribox>& NodeObjects) : BoundsTribox(NodeBounds), ObjectTriboxes(NodeObjects)
	{}

	// Base constructor
	FTriboxNode() : BoundsTribox(), ObjectTriboxes()
	{}

	// Get the closest plane to the node center 
	void FindClosestPlane(int32& PlaneAxis, FTribox::FRealType& PlaneProjection) const;

	// Split the object triboxes into a left and a right tribox
	void SplitTriboxNode(const int32& PlaneAxis, const FTribox::FRealType& PlaneProjection,
							FTriboxNode& LeftTribox, FTribox::FRealType& LeftVolume,
							FTriboxNode& RightTribox, FTribox::FRealType& RightVolume) const;
	
	// Enclosing node tribox 
	FTribox BoundsTribox;

	// List of objects that are part of that node
	TArray<FTribox> ObjectTriboxes;
};

FORCEINLINE void FTriboxNode::FindClosestPlane(int32& PlaneAxis, FTribox::FRealType& PlaneProjection) const
{
	const FTribox::FVec3Type PointPosition = BoundsTribox.GetCenter();
	
	int32 LocalAxis = INDEX_NONE;
	FTribox::FRealType LocalProjection = 0.0;
	FTribox::FRealType MinDistance = FLT_MAX;
			
	for(const FTribox& ObjectTribox : ObjectTriboxes)
	{
		const FTribox::FRealType ClosestDistance = ObjectTribox.GetClosestPlane(PointPosition, LocalAxis, LocalProjection);
		if(ClosestDistance < MinDistance)
		{
			MinDistance = ClosestDistance;
			PlaneAxis = LocalAxis;
			PlaneProjection = LocalProjection;
		}
	}
}
	
FORCEINLINE void FTriboxNode::SplitTriboxNode(const int32& PlaneAxis, const FTribox::FRealType& PlaneProjection,
	FTriboxNode& LeftTribox, FTribox::FRealType& LeftVolume, FTriboxNode& RightTribox,  FTribox::FRealType& RightVolume) const
{
	for(const FTribox& ObjectTribox : ObjectTriboxes)
	{
		FTribox LocalLeftTribox, LocalRightTribox;
		ObjectTribox.SplitTriboxSlab(PlaneAxis, PlaneProjection, LocalLeftTribox, LocalRightTribox);
				
		if(LocalLeftTribox.HasDatas())
		{
			LeftTribox.BoundsTribox += LocalLeftTribox;
			LeftTribox.ObjectTriboxes.Add(LocalLeftTribox);
			LeftVolume += LocalLeftTribox.ComputeVolume();
		}
		if(LocalRightTribox.HasDatas())
		{
			RightTribox.BoundsTribox += LocalRightTribox;
			RightTribox.ObjectTriboxes.Add(LocalRightTribox);
			RightVolume += LocalRightTribox.ComputeVolume();
		}
	}
}

/**
 * @brief Simple tribox binary tree to store tribox hierarchy
 */
struct FTriboxTree
{
	// Constructor given a LOD and a number of objects
	FTriboxTree(const int32 NumLODs, const FConvexOptimizer::FCachedTriboxes& RootTriboxes);
	
	// Build all the tree LODs
	void BuildTreeLODs(TArray<FImplicitObjectPtr>& SimplifiedConvexes, const TUniquePtr<Private::FCollisionObjects>& CollisionObjects);

	// Add a node to the tree nodes
	void AddTreeNode(const int32 LODIndex, const FTriboxNode& TriboxNode, const FTribox::FRealType& ObjectsVolume,
		TArray<FImplicitObjectPtr>& SimplifiedConvexes, const TUniquePtr<Private::FCollisionObjects>& CollisionObjects);
	
	// Swap the 2 buffers
	void SwapNodeBuffers()
	{
		BufferIndex = 1 - BufferIndex;
		TreeNodes[BufferIndex].Reset();
	}

	// Current buffer used to build tthe tree
	int32 CurrentBuffer() const {return BufferIndex;}

	// Previous buffer used to build the tree
	int32 PreviousBuffer() const {return 1-BufferIndex;}
	
	// List of tree nodes 
	TArray<FTriboxNode> TreeNodes[2];

	// Tree buffer index 
	int32 BufferIndex = 0;
	
	// Tree max LODs
	int32 MaxLODs = 0;
};
	
FORCEINLINE FTriboxTree::FTriboxTree(const int32 NumLODs, const FConvexOptimizer::FCachedTriboxes& RootTriboxes) : TreeNodes(), BufferIndex(0)
{
	const int32 NumNodes = 1 << NumLODs;
	
	TreeNodes[0].Reserve(NumNodes);
	TreeNodes[1].Reserve(NumNodes);
	
	FTriboxNode TriboxNode;
	for(auto& RootTribox : RootTriboxes)
	{
		if(RootTribox.Value.Tribox.HasDatas())
		{
			TriboxNode.BoundsTribox += RootTribox.Value.Tribox;
			TriboxNode.ObjectTriboxes.Add(RootTribox.Value.Tribox);
		}
	}
	TreeNodes[CurrentBuffer()].Emplace(TriboxNode);
	MaxLODs = NumLODs;
}

FORCEINLINE void FTriboxTree::BuildTreeLODs(TArray<FImplicitObjectPtr>& SimplifiedConvexes, const TUniquePtr<Private::FCollisionObjects>& CollisionObjects)
{
	//UE_LOG(LogTemp, Log, TEXT("	Building tribox binary tree"))
	for(int32 LODIndex = 1; LODIndex < MaxLODs; ++LODIndex)
	{
		// Swap the node buffers (working/result)
		SwapNodeBuffers();

		//int32 NodeIndex = 0;
		for(FTriboxNode& TriboxNode : TreeNodes[PreviousBuffer()])
		{
			//UE_LOG(LogTemp, Log, TEXT("			Iterating over node %d with num objects = %d"), NodeIndex++, TriboxNode.ObjectTriboxes.Num())

			int32 PlaneAxis = INDEX_NONE;
			FTribox::FRealType PlaneProjection = 0.0;

			// Find the closest plane to the node center
			TriboxNode.FindClosestPlane(PlaneAxis, PlaneProjection);
					
			FTriboxNode LeftTribox, RightTribox;
			FTribox::FRealType LeftVolume = 0.0, RightVolume = 0.0;

			// Split the node along the cutting plane
			TriboxNode.SplitTriboxNode(PlaneAxis, PlaneProjection, LeftTribox, LeftVolume, RightTribox, RightVolume);

			// Add left node to the tree
			AddTreeNode(LODIndex, LeftTribox, LeftVolume, SimplifiedConvexes, CollisionObjects);

			// Add right node to the tree
			AddTreeNode(LODIndex, RightTribox, RightVolume, SimplifiedConvexes, CollisionObjects);
		}
	}
}

FORCEINLINE void FTriboxTree::AddTreeNode(const int32 LODIndex, const FTriboxNode& TriboxNode, const FTribox::FRealType& ObjectsVolume,
	TArray<FImplicitObjectPtr>& SimplifiedConvexes, const TUniquePtr<Private::FCollisionObjects>& CollisionObjects)
{
	if(TriboxNode.BoundsTribox.HasDatas())
	{
		// Disgard all the objects with small volumes
		if(ObjectsVolume > CVars::ChaosConvexMinVolume)
		{
			const FTribox::FRealType NodeVolume = TriboxNode.BoundsTribox.ComputeVolume();
			//UE_LOG(LogTemp, Log, TEXT("				Node volume = [%f %f] -> %f"), NodeVolume, ObjectsVolume, NodeVolume/ObjectsVolume)
			
			if((NodeVolume/ObjectsVolume > CVars::ChaosConvexSplittingThreshold) &&
				(LODIndex < (MaxLODs-1)))
			{
				// If volume ratio (concavity) is big enough add this node to the current list to be processed by the next LOD
				TreeNodes[CurrentBuffer()].Add(TriboxNode);
			}
			else
			{
				// Otherwise add the node to the final convexes list
				SimplifiedConvexes.Add(TriboxNode.BoundsTribox.MakeConvex());
				Private::FImplicitBVH::CollectLeafObject(SimplifiedConvexes.Last(),
						FRigidTransform3::Identity, INDEX_NONE, CollisionObjects->ImplicitObjects);
			}
		}
	}
}

FConvexOptimizer::FConvexOptimizer() :
	SimplifiedConvexes(), CollisionObjects(MakeUnique<Private::FCollisionObjects>()), ShapesArray()
{}

FConvexOptimizer::~FConvexOptimizer() = default;

void FConvexOptimizer::VisitCollisionObjects(const FImplicitHierarchyVisitor& VisitorFunc) const
{
	if(CVars::bChaosConvexSimplifyUnion)
	{
		const TArray<Private::FImplicitBVHObject>& ImplicitObjects = BVH.IsValid() ? BVH->GetObjects() : CollisionObjects->ImplicitObjects;
		
		int32 ObjectIndex = 0;
		for(const Private::FImplicitBVHObject& CollisionObject : ImplicitObjects)
		{
			int32 LeafObjectIndex = CollisionObject.GetObjectIndex();
			CollisionObject.GetGeometry()->VisitLeafObjectsImpl(CollisionObject.GetTransform(),
				CollisionObject.GetRootObjectIndex(), ObjectIndex, LeafObjectIndex, VisitorFunc);
		}
	}	
}

void FConvexOptimizer::VisitOverlappingObjects(const FAABB3& LocalBounds, const FImplicitHierarchyVisitor& VisitorFunc) const
{
	if(CVars::bChaosConvexSimplifyUnion)
	{
		int32 ObjectIndex = 0;
		if(BVH.IsValid())
		{
			BVH->VisitAllIntersections(LocalBounds,
			[this, &ObjectIndex, &VisitorFunc](const FImplicitObject* Implicit, const FRigidTransform3f& RelativeTransformf,
				const FAABB3f& RelativeBoundsf, const int32 RootObjectIndex, const int32 LeafObjectIndex)
			{
				VisitorFunc(Implicit, FRigidTransform3(RelativeTransformf), RootObjectIndex, ObjectIndex++, LeafObjectIndex);
			});
		}
		else
		{
			for(const Private::FImplicitBVHObject& CollisionObject : CollisionObjects->ImplicitObjects)
			{
				int32 LeafObjectIndex = CollisionObject.GetObjectIndex();
				CollisionObject.GetGeometry()->VisitOverlappingLeafObjectsImpl(LocalBounds, CollisionObject.GetTransform(),
					CollisionObject.GetRootObjectIndex(), ObjectIndex, LeafObjectIndex, VisitorFunc);
			}
		}
	}
}

DECLARE_CYCLE_STAT(TEXT("Collisions::BuildConvexShapes"), STAT_BuildConvexShapes, STATGROUP_ChaosCollision);
void FConvexOptimizer::BuildConvexShapes(const FShapesArray& UnionShapes)
{
	SCOPE_CYCLE_COUNTER(STAT_BuildConvexShapes);
	
	ShapesArray.Reset();
	TArray<FImplicitObjectPtr> ImplicitObjects = SimplifiedConvexes;
	ShapesArray.Add(FShapeInstance::Make(UnionShapes.Num(),
		MakeImplicitObjectPtr<FImplicitObjectUnion>(MoveTemp(ImplicitObjects))));

	const TUniquePtr<FPerShapeData>& TemplateData = UnionShapes[0];
	const TUniquePtr<FShapeInstance>& ShapeData = ShapesArray.Last();

	ShapeData->SetQueryData(TemplateData->GetQueryData());
	ShapeData->SetSimData(TemplateData->GetSimData());
	ShapeData->SetCollisionTraceType(TemplateData->GetCollisionTraceType());

	// Only sim enabled if all the underlying shapes could be used for physics
	ShapeData->SetSimEnabled(true);

	// Disable simple shapes for query 
	ShapeData->SetQueryEnabled(false);
}

void FConvexOptimizer::SimplifyRootConvexes(const Chaos::FImplicitObjectUnionPtr& UnionGeometry, const FShapesArray& UnionShapes, const EObjectStateType ObjectState)
{
	SCOPE_CYCLE_COUNTER(STAT_Collisions_SimplifyConvexes);
	if(CVars::bChaosConvexSimplifyUnion && UnionGeometry && (UnionShapes.Num() > 0) &&
		(UnionShapes.Num() <= UnionGeometry->GetObjects().Num()))
	{
		SimplifiedConvexes.Reset();
		CollisionObjects->ImplicitObjects.Reset();
		ShapesArray.Reset();
		const int32 MaxLODs = (ObjectState == EObjectStateType::Dynamic) ? CVars::ChaosConvexMaxDynamicLODs : CVars::ChaosConvexMaxKinematicLODs;

		if(MaxLODs == 0)
		{
			BuildSingleConvex(UnionGeometry, UnionShapes);
		}
		else
		{
			BuildMultipleConvex(UnionGeometry, UnionShapes, MaxLODs);
		}
		if(!SimplifiedConvexes.IsEmpty())
		{
			// Build the simplified shapes
			BuildConvexShapes(UnionShapes);
		}
	}

	if(CVars::bChaosUnionBVHEnabled)
	{
		if(BVH.IsValid())
		{
			BVH.Reset();
		}
		BVH = FImplicitBVH::TryMakeFromLeaves(MoveTemp(CollisionObjects->ImplicitObjects), CVars::ChaosUnionBVHMinShapes, CVars::ChaosUnionBVHMaxDepth);
	}
}

FORCEINLINE void InvalidateCachedTriboxes(FConvexOptimizer::FCachedTriboxes& RootTriboxes)
{
	for(auto& RootTribox : RootTriboxes)
	{
		RootTribox.Value.Tribox.SetValid(false);
	}
}

FORCEINLINE void ResizeCachedTriboxes(FConvexOptimizer::FCachedTriboxes& RootTriboxes)
{
	TArray<FImplicitObject*> KeysToRemove;
	for(auto& RootTribox : RootTriboxes)
	{
		if(!RootTribox.Value.Tribox.IsValid())
		{
			KeysToRemove.Add(RootTribox.Key);
		}
	}
	for(auto& KeyToRemove : KeysToRemove)
	{
		RootTriboxes.Remove(KeyToRemove);
	}
}

DECLARE_CYCLE_STAT(TEXT("Collisions::BuildConvexTriboxes"), STAT_BuildConvexTriboxes, STATGROUP_ChaosCollision);
FORCEINLINE void BuildConvexTriboxes(const Chaos::FImplicitObjectUnionPtr& UnionGeometry, const FShapesArray& UnionShapes,
	TUniquePtr<Private::FCollisionObjects>& CollisionObjects, FConvexOptimizer::FCachedTriboxes& RootTriboxes)
{
	SCOPE_CYCLE_COUNTER(STAT_BuildConvexTriboxes);

	// Invalidate the cached root triboxes
	InvalidateCachedTriboxes(RootTriboxes);
	
	for(int32 RootObjectIndex = 0, NumRootObjects = UnionGeometry->GetObjects().Num(); RootObjectIndex < NumRootObjects; ++RootObjectIndex)
	{
		const int32 ShapeIndex = UnionShapes.IsValidIndex(RootObjectIndex) ? RootObjectIndex : 0;
		if(UnionShapes[ShapeIndex]->GetSimEnabled())
		{
			if(FImplicitObject* RootObject = UnionGeometry->GetObjects()[RootObjectIndex].GetReference())
			{
				FConvexOptimizer::FCachedTribox* RootTribox = RootTriboxes.Find(RootObject);
				const bool bHasRootTribox = (RootTribox != nullptr);
				
				FTribox LocalTribox;
				int32 LeafObjectIndex = 0, ObjectIndex = 0;
				
				RootObject->VisitLeafObjectsImpl(FRigidTransform3::Identity,RootObjectIndex, ObjectIndex, LeafObjectIndex,
					[&CollisionObjects, &LocalTribox, &bHasRootTribox](
					const FImplicitObject* ImplicitObject, const FRigidTransform3& RelativeTransform,
					const int32 RootObjectIndex, const int32, const int32)
				{
					if(const FConvex* Convex = ImplicitObject->AsA<FConvex>())
					{
						if(!bHasRootTribox)
						{
							const FTribox::FRigidTransform3Type ConvexTransform(RelativeTransform);
							// Disable collision for all the convexes that are going to be used to build the tribox
							// For now only used for debug draw
							const_cast<FImplicitObject*>(ImplicitObject)->SetDoCollide(false);
							LocalTribox.AddConvex(Convex, ConvexTransform);
						}
					}
					else if(const TImplicitObjectScaled<FConvex>* ScaledObject = TImplicitObjectScaled<FConvex>::AsScaled(*ImplicitObject))
					{
						if(!bHasRootTribox)
						{
							const FTribox::FRigidTransform3Type ConvexTransform(RelativeTransform.GetTranslation(),
								RelativeTransform.GetRotation(), RelativeTransform.GetScale3D() * ScaledObject->GetScale());
							// Disable collision for all the convexes that are going to be used to build the tribox
							// For now only used for debug draw
							const_cast<FImplicitObject*>(ImplicitObject)->SetDoCollide(false);
							LocalTribox.AddConvex(ScaledObject->GetUnscaledObject(), ConvexTransform);
						}
					}
					else if(const TImplicitObjectInstanced<FConvex>* InstancedObject = TImplicitObjectInstanced<FConvex>::AsInstanced(*ImplicitObject))
					{
						if(!bHasRootTribox)
						{
							const FTribox::FRigidTransform3Type ConvexTransform(RelativeTransform);
							// Disable collision for all the convexes that are going to be used to build the tribox
							// For now only used for debug draw
							const_cast<FImplicitObject*>(ImplicitObject)->SetDoCollide(false);
							LocalTribox.AddConvex(InstancedObject->Object(), ConvexTransform);
						}
					}
					else
					{
						// Add non convex implicits to the list of collision objects
						Private::FImplicitBVH::CollectLeafObject(ImplicitObject,
							RelativeTransform, RootObjectIndex, CollisionObjects->ImplicitObjects);	  
					}
				});
				if(!bHasRootTribox && LocalTribox.HasDatas())
				{
					LocalTribox.BuildTribox();
					RootTribox = &(RootTriboxes.Add(RootObject, {LocalTribox,nullptr}));
				}
				if(RootTribox)
				{
					RootTribox->Tribox.SetValid(true);
				}
			}
		}
	}
	ResizeCachedTriboxes(RootTriboxes);
}

void FConvexOptimizer::BuildSingleConvex(const Chaos::FImplicitObjectUnionPtr& UnionGeometry, const FShapesArray& UnionShapes)
{
	BuildConvexTriboxes(UnionGeometry, UnionShapes, CollisionObjects, RootTriboxes);
	
	FTribox MainTribox;
	for(auto& RootTribox : RootTriboxes)
	{
		if(RootTribox.Value.Tribox.HasDatas())
		{
			MainTribox += RootTribox.Value.Tribox;
		}
	}

	if(MainTribox.HasDatas())
	{
		// Build the tribox and add it to the list of optimized convexes
		SimplifiedConvexes.Add(MainTribox.MakeConvex());
		Private::FImplicitBVH::CollectLeafObject(SimplifiedConvexes.Last(), FRigidTransform3::Identity, INDEX_NONE, CollisionObjects->ImplicitObjects);	
	}
}

FORCEINLINE void FindClosestPlane(const FTribox::FVec3Type& PointPosition, const TArray<FTribox>& Triboxes,
						int32& PlaneAxis, FTribox::FRealType& PlaneProjection)
{
	int32 LocalAxis = INDEX_NONE;
	FTribox::FRealType LocalProjection = 0.0;
	FTribox::FRealType MinDistance = FLT_MAX;
				
	for(auto& Tribox : Triboxes)
	{
		const FTribox::FRealType ClosestDistance = Tribox.GetClosestPlane(PointPosition, LocalAxis, LocalProjection);
		if(ClosestDistance < MinDistance)
		{
			MinDistance = ClosestDistance;
			PlaneAxis = LocalAxis;
			PlaneProjection = LocalProjection;
		}
	}
}
	
void FConvexOptimizer::BuildMultipleConvex(const Chaos::FImplicitObjectUnionPtr& UnionGeometry, const FShapesArray& UnionShapes, const int32 MaxLODs)
{
	BuildConvexTriboxes(UnionGeometry, UnionShapes, CollisionObjects, RootTriboxes);

	if(MaxLODs < 0)
	{
		for(auto& RootTribox : RootTriboxes)
        {
			if(RootTribox.Value.Tribox.HasDatas() && (RootTribox.Value.Tribox.ComputeVolume() > CVars::ChaosConvexMinVolume))
			{
				// Build the tribox and add it to the list of optimized convexes
				if(!RootTribox.Value.Convex)
				{
					RootTribox.Value.Convex = RootTribox.Value.Tribox.MakeConvex();
				}
				SimplifiedConvexes.Add(RootTribox.Value.Convex);
				Private::FImplicitBVH::CollectLeafObject(SimplifiedConvexes.Last(), FRigidTransform3::Identity, INDEX_NONE, CollisionObjects->ImplicitObjects);
			}
        }
	}
	else
	{
		// Build the binary tree and add the leaves to the simplified convexes
		FTriboxTree TriboxTree(MaxLODs, RootTriboxes);
		TriboxTree.BuildTreeLODs(SimplifiedConvexes, CollisionObjects);
	}
}

int32 FConvexOptimizer::NumCollisionObjects() const
{
	return BVH.IsValid() ? BVH->GetObjects().Num() : CollisionObjects->ImplicitObjects.Num();
}

void VisitCollisionObjects(const FConvexOptimizer* ConvexOptimizer, const FImplicitObject* ImplicitObject, const FImplicitHierarchyVisitor& VisitorFunc)
{
	if(ConvexOptimizer && ConvexOptimizer->IsValid())
	{
		ConvexOptimizer->VisitCollisionObjects(VisitorFunc);
	}
	else
	{
		ImplicitObject->VisitLeafObjects(VisitorFunc);
	}
}

void VisitOverlappingObjects(const FConvexOptimizer* ConvexOptimizer, const FImplicitObject* ImplicitObject, const FAABB3& LocalBounds, const FImplicitHierarchyVisitor& VisitorFunc)
{
	if(ConvexOptimizer && ConvexOptimizer->IsValid())
	{
		ConvexOptimizer->VisitOverlappingObjects(LocalBounds, VisitorFunc);
	}
	else
	{
		ImplicitObject->VisitOverlappingLeafObjects(LocalBounds, VisitorFunc);
	}
}

}
}
