
#include "Chaos/ConvexOptimizer.h"
#include "Chaos/Tribox.h"
#include "Chaos/ImplicitObjectBVH.h"
#include "Chaos/PhysicsObjectCollisionInterface.h"
#include "Chaos/Collision/CollisionContext.h"

namespace Chaos
{
	namespace CVars
	{
		// Replace all the convexes within an implicit hierarchy with a simplified one (kdop18 tribox for now) for collision
		bool bChaosSimplifyConvexes = false;
		FAutoConsoleVariableRef CVarChaosSimplifyConvexes(TEXT("p.Chaos.Collision.SimplifyConvexes"), bChaosSimplifyConvexes, TEXT("If true replace all the convexes within an implcit hierarchy with a simplified one (kdop18 tribox for now) for collision"));

		// Num LODs for the simplification
		int32 ChaosNumConvexLODs = 0;
		FAutoConsoleVariableRef CVarChaosNumConvexLODs(TEXT("p.Chaos.Collision.NumConvexLODs"), ChaosNumConvexLODs, TEXT("Max number of collisions LODs required during simplication"));
	}

namespace Private
{

	FConvexOptimizer::FConvexOptimizer() :
		SimplifiedConvexes(), CollisionObjects(MakeUnique<Private::FCollisionObjects>()), ShapesArray()
	{}

	FConvexOptimizer::~FConvexOptimizer() = default;

	void FConvexOptimizer::VisitCollisionObjects(const FImplicitHierarchyVisitor& VisitorFunc) const
	{
		if(CVars::bChaosSimplifyConvexes)
		{
			int32 ObjectIndex = 0;
			for(const Private::FImplicitBVHObject& CollisionObject : CollisionObjects->ImplicitObjects)
			{
				int32 LeafObjectIndex = CollisionObject.GetObjectIndex();
				CollisionObject.GetGeometry()->VisitLeafObjectsImpl(CollisionObject.GetTransform(),
					CollisionObject.GetRootObjectIndex(), ObjectIndex, LeafObjectIndex, VisitorFunc);
			}
		}	
	}

	void FConvexOptimizer::VisitOverlappingObjects(const FAABB3& LocalBounds, const FImplicitHierarchyVisitor& VisitorFunc) const
	{
		if(CVars::bChaosSimplifyConvexes)
		{
			int32 ObjectIndex = 0;
			for(const Private::FImplicitBVHObject& CollisionObject : CollisionObjects->ImplicitObjects)
			{
				int32 LeafObjectIndex = CollisionObject.GetObjectIndex();
				CollisionObject.GetGeometry()->VisitOverlappingLeafObjectsImpl(LocalBounds, CollisionObject.GetTransform(),
					CollisionObject.GetRootObjectIndex(), ObjectIndex, LeafObjectIndex, VisitorFunc);
			}
		}
	}

	void FConvexOptimizer::BuildConvexShapes(const FShapesArray& UnionShapes)
	{
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

	DECLARE_CYCLE_STAT(TEXT("FConvexOptimizer::SimplifyRootConvexes"), STAT_SimplifyRootConvexes, STATGROUP_ChaosCollision);
	void FConvexOptimizer::SimplifyRootConvexes(const Chaos::FImplicitObjectUnionPtr& UnionGeometry, const FShapesArray& UnionShapes)
	{
		if(CVars::ChaosNumConvexLODs > 0)
		{
			BuildConvexLODs(UnionGeometry, UnionShapes);
		}
		else
		{
			BuildSingleConvex(UnionGeometry, UnionShapes);
		}
	}

	void FConvexOptimizer::BuildSingleConvex(const Chaos::FImplicitObjectUnionPtr& UnionGeometry, const FShapesArray& UnionShapes)
	{
		if(CVars::bChaosSimplifyConvexes && UnionGeometry && (UnionShapes.Num() == UnionGeometry->GetObjects().Num()))
		{
			FTribox Tribox;

			SimplifiedConvexes.Reset();
			CollisionObjects->ImplicitObjects.Reset();
	
			UnionGeometry->VisitLeafObjects([&Tribox, this, &UnionShapes](
				const FImplicitObject* ImplicitObject, const FRigidTransform3& RelativeTransform,
				const int32 RootObjectIndex, const int32 ObjectIndex, const int32 LeafObjectIndex)
			{
				if(UnionShapes[RootObjectIndex]->GetSimEnabled())
				{
					const FTribox::FRigidTransform3Type ConvexTransform(RelativeTransform);
					if(const FConvex* Convex = ImplicitObject->AsA<FConvex>())
					{
						// Disable collision for all the convexes that are going to be used to build the tribox
						// For now only used for debug draw
						const_cast<FImplicitObject*>(ImplicitObject)->SetDoCollide(false);
						Tribox.AddConvex(Convex, ConvexTransform);
					}
					else
					{
						// Add non convex implicits to the list of collision objects
						Private::FImplicitBVH::CollectLeafObject(ImplicitObject, RelativeTransform, RootObjectIndex, CollisionObjects->ImplicitObjects);	  
					}
				}
			});
			if(Tribox.BuildTribox())
			{
				// Build the tribox and add it to the list of optimized convexes
				SimplifiedConvexes.Add(Tribox.MakeConvex());
				Private::FImplicitBVH::CollectLeafObject(SimplifiedConvexes.Last(), FRigidTransform3::Identity, INDEX_NONE, CollisionObjects->ImplicitObjects);	  

				// Build the simplified shapes
				BuildConvexShapes(UnionShapes);
			}
		}
	}

	void FConvexOptimizer::BuildConvexLODs(const Chaos::FImplicitObjectUnionPtr& UnionGeometry, const FShapesArray& UnionShapes)
	{
		// WIP Another CL
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
