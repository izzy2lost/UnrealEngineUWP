// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Chaos/Core.h"
#include "Chaos/ImplicitObject.h"

namespace Chaos
{
	
namespace Private
{
	class FImplicitBVHObject;
	struct FCollisionObjects
	{
		TArray<Private::FImplicitBVHObject> ImplicitObjects;
	};

	/**
	 * @brief The convex optimizer goal is to have a central place where
	 * implicits hierarchy could be modified in order to accelerate collision detection
	 */
	class FConvexOptimizer
	{
		public :
		CHAOS_API FConvexOptimizer();

		// Default destructor
		CHAOS_API ~FConvexOptimizer();

		// Simplify all the convexes in the hierarchy 
		CHAOS_API void SimplifyRootConvexes(const Chaos::FImplicitObjectUnionPtr& UnionGeometry, const FShapesArray& UnionShapes);

		// Check if the manager is valid or not
		CHAOS_API bool IsValid() const {return !SimplifiedConvexes.IsEmpty();}

		// Visit all the collision objects if they exist / otherwise forward it to the RootHierarchy
		CHAOS_API void VisitCollisionObjects(const FImplicitHierarchyVisitor& VisitorFunc) const;

		// Visit all the overlapping objects if they exist / otherwise forward it to the RootHierarchy
		CHAOS_API void VisitOverlappingObjects(const FAABB3& LocalBounds, const FImplicitHierarchyVisitor& VisitorFunc) const;

		// Get the shapes array 
		CHAOS_API const FShapeInstanceArray& GetShapeInstances() const { return ShapesArray;}

	private:

		// Build a single convex
		void BuildSingleConvex(const Chaos::FImplicitObjectUnionPtr& UnionGeometry, const FShapesArray& UnionShapes);

		// Build several convexes for a given LOD
		void BuildConvexLODs(const Chaos::FImplicitObjectUnionPtr& UnionGeometry, const FShapesArray& UnionShapes);

		// Build the simplified shapes
		void BuildConvexShapes(const FShapesArray& UnionShapes);

		// List of simplified convexes
		TArray<FImplicitObjectPtr> SimplifiedConvexes;

		// List of all the collision objects to avoid traversing all the hierarchy during midphase
		TUniquePtr<Private::FCollisionObjects> CollisionObjects;

		// Additional shapes array that could be used during collision midphase
		FShapeInstanceArray ShapesArray;
	};

	// Visit all the collision objects if they exist / otherwise forward it to the RootHierarchy
	void VisitCollisionObjects(const FConvexOptimizer* ConvexOptimizer, const FImplicitObject* ImplicitObject, const FImplicitHierarchyVisitor& VisitorFunc);

	// Visit all the overlapping objects if they exist / otherwise forward it to the RootHierarchy
	void VisitOverlappingObjects(const FConvexOptimizer* ConvexOptimizer, const FImplicitObject* ImplicitObject, const FAABB3& LocalBounds, const FImplicitHierarchyVisitor& VisitorFunc);


}
}