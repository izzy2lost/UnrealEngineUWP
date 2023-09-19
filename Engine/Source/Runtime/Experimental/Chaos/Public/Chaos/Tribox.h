// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Chaos/Core.h"
#include "Chaos/Convex.h"

namespace Chaos
{
namespace Private
{
		
/**
 * @brief Tribox object that represents a k-DOP18 convex
 * 
 * @note This implementation provides a fast way of constructing Tribox in order to be used at runtime to build
 * approximate convex implicit object
 * 
 */

class FTribox
{
public :

	using FRealType = FRealSingle;
	using FVec3Type = TVec3<FRealType>;
	using FRigidTransform3Type = TRigidTransform<FRealType, 3>;
	using FMatrix33Type = PMatrix<FRealType, 3, 3>;

	// Number of planes that will be used to define the Tribox 
	static constexpr int32 NumPlanes = 18;

	// Number of principal planes
	static constexpr int32 NumPrincipalPlanes = 6;

	// Number of chamfer planes
	static constexpr int32 NumChamferPlanes = 12;

	// Distance used to inflate the Tribox to avoid degenerate case
	static constexpr FRealType InflateDistance = 0.5;

	// Base Constructor
	FORCEINLINE FTribox() : MaxDists()
	{
		for(int32 DistsIndex = 0; DistsIndex< NumPlanes; ++DistsIndex)
		{
			MaxDists[DistsIndex] = TNumericLimits<FRealType>::Lowest();
		}
	};

	// Add a point position to the Tribox
	void AddPoint(const FVec3Type& PointPosition);

	// Add convex vertices to a tribox
	void AddConvex(const FConvex* Convex, const FRigidTransform3Type& RelativeTransform);

	// Inflate + Scale the Max distances
	bool BuildTribox();

	// Find the overlapping tribox 
	bool OverlapTribox(const FTribox& OtherTribox, FTribox& OverlapTribox) const;

	// Compute the tribox volume
	FRealType ComputeVolume() const;

	// Create a convex from the Tribox
	FImplicitObjectPtr MakeConvex() const;

	// Add a tribox to this and return this
	FTribox& operator+=(const FTribox& OtherTribox);

	// Add a tribox to this and return a new one
	FTribox operator+( const FTribox& OtherTribox) const;

private : 
	// Solve the intersection point position
	FVec3 SolveIntersection(const int32 FaceIndex[3], const FMatrix33Type& A) const;

	// Add the intersection point to the list of faces/vertices
	void AddIntersection(const int32 FaceIndex[3], const int32 FaceOrder[3], const FVec3Type& VertexPosition, TArray<TArray<int32>>& FaceIndices, TArray<FConvex::FVec3Type>& ConvexVertices) const;

	// Solve and add the intersection point in between the 3 planes
	void ComputeIntersection(const int32 CornerIndex, const int32 PrincipalIndex, const int32 ChamferIndex, TArray<TArray<int32>>& FaceIndices, TArray<FConvex::FVec3Type>& ConvexVertices) const;

	// Build the max dist along the principal axis
	void BuildPrincipalDist(const FVec3Type& P, const int32 CoordIndexA, const int32 DistsIndexA, const int32 DistsIndexB);

	// Build the max dist along the chamfer axis
	void BuildChamferDist(const FVec3Type& P, const int32 CoordIndexA, const int32 CoordIndexB,
			const int32 DistsIndexA, const int32 DistsIndexB, const int32 DistsIndexC, const int32 DistsIndexD);

	// Max distance along eaxh tribox axis (principal + chamfer)
	FRealType MaxDists[NumPlanes] = {TNumericLimits<FRealType>::Lowest(), TNumericLimits<FRealType>::Lowest(),
									 TNumericLimits<FRealType>::Lowest(), TNumericLimits<FRealType>::Lowest(),
									 TNumericLimits<FRealType>::Lowest(), TNumericLimits<FRealType>::Lowest(),
									 TNumericLimits<FRealType>::Lowest(), TNumericLimits<FRealType>::Lowest(),
									 TNumericLimits<FRealType>::Lowest()};

	// Number of points used to build the simplified convexes
	int32 NumPoints = 0;
};

}
}