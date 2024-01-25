// Copyright Epic Games, Inc. All Rights Reserved.
#include "Chaos/PBDCollisionSpringConstraintsBase.h"
#include "Chaos/Plane.h"
#include "Chaos/Triangle.h"
#include "Chaos/TriangleCollisionPoint.h"
#include "Chaos/TriangleMesh.h"
#include "Chaos/PBDSoftsSolverParticles.h"
#include "Chaos/SoftsEvolutionLinearSystem.h"
#include "Chaos/SoftsSolverParticlesRange.h"

#if !COMPILE_WITHOUT_UNREAL_SUPPORT
#include "Chaos/Framework/Parallel.h"
#include <atomic>

namespace Chaos::Softs {

static TConstArrayView<FSolverVec3> GetConstArrayView(const FSolverParticles& Particles, int32 Offset, int32 NumParticles, const TArray<FSolverVec3>* const Data)
{
	if (Data)
	{
		return TConstArrayView<FSolverVec3>(Data->GetData(), NumParticles + Offset);
	}
	return TConstArrayView<FSolverVec3>();
}

static TConstArrayView<FSolverVec3> GetConstArrayView(const FSolverParticlesRange& Particles, int32 Offset, int32 NumParticles, const TArray<FSolverVec3>* const Data)
{
	if (Data)
	{
		return Particles.GetConstArrayView(*Data);
	}
	return TConstArrayView<FSolverVec3>();
}

FPBDCollisionSpringConstraintsBase::FPBDCollisionSpringConstraintsBase(
	const int32 InOffset,
	const int32 InNumParticles,
	const FTriangleMesh& InTriangleMesh,
	const TArray<FSolverVec3>* InReferencePositions,
	TSet<TVec2<int32>>&& InDisabledCollisionElements,
	const TConstArrayView<int32>& InSelfCollisionLayers,
	const FSolverReal InThickness,
	const FSolverReal InStiffness,
	const FSolverReal InFrictionCoefficient,
	const FSolverReal InProximityStiffness)
	: Thickness(InThickness)
	, Stiffness(InStiffness)
	, FrictionCoefficient(InFrictionCoefficient)
	, ProximityStiffness(InProximityStiffness)
	, TriangleMesh(InTriangleMesh)
	, Elements(InTriangleMesh.GetSurfaceElements())
	, ReferencePositions(InReferencePositions)
	, DisabledCollisionElements(InDisabledCollisionElements)
	, Offset(InOffset)
	, NumParticles(InNumParticles)
	, bGlobalIntersectionAnalysis(false)
{
	UpdateCollisionLayers(InSelfCollisionLayers);
}

void FPBDCollisionSpringConstraintsBase::UpdateCollisionLayers(const TConstArrayView<int32>& InFaceCollisionLayers)
{
	if (InFaceCollisionLayers.Num() != Elements.Num())
	{
		// Reset collision layers
		FaceCollisionLayers = TConstArrayView<int32>();
		VertexCollisionLayers.Reset();
	}
	else
	{
		FaceCollisionLayers = InFaceCollisionLayers;
		VertexCollisionLayers.SetNumUninitialized(NumParticles);

		TConstArrayView<TArray<int32>> PointToTriangle = TriangleMesh.GetPointToTriangleMap();
		for (int32 ParticleIndexNoOffset = 0; ParticleIndexNoOffset < NumParticles; ++ParticleIndexNoOffset)
		{
			const int32 ParticleIndex = ParticleIndexNoOffset + Offset;
			TVec2<int32>& VertexCollisionLayer = VertexCollisionLayers[ParticleIndexNoOffset];
			VertexCollisionLayer = TVec2<int32>(INDEX_NONE);
			for (const int32 FaceIndex : PointToTriangle[ParticleIndex])
			{
				if (FaceCollisionLayers[FaceIndex] != INDEX_NONE)
				{
					VertexCollisionLayer[0] = VertexCollisionLayer[0] == INDEX_NONE ? 
						FaceCollisionLayers[FaceIndex] : FMath::Min(FaceCollisionLayers[FaceIndex], VertexCollisionLayer[0]);

					VertexCollisionLayer[1] = VertexCollisionLayer[1] == INDEX_NONE ?
						FaceCollisionLayers[FaceIndex] : FMath::Max(FaceCollisionLayers[FaceIndex], VertexCollisionLayer[1]);
				}
			}
		}
	}
}

void FPBDCollisionSpringConstraintsBase::Init(const FSolverParticles& Particles)
{
	if (!Elements.Num())
	{
		Constraints.Reset();
		Barys.Reset();
		FlipNormal.Reset();
		return;
	}

	FTriangleMesh::TBVHType<FSolverReal> BVH;
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ChaosPBDCollisionSpring_BuildBVH);
		TriangleMesh.BuildBVH(static_cast<const TArrayView<const FSolverVec3>&>(Particles.XArray()), BVH);
	}
	TArray<FPBDTriangleMeshCollisions::FGIAColor> EmptyGIAColors;
	return Init(Particles, BVH, static_cast<TConstArrayView<FPBDTriangleMeshCollisions::FGIAColor>>(EmptyGIAColors), EmptyGIAColors);
}

template<typename SpatialAccelerator, typename SolverParticlesOrRange>
void FPBDCollisionSpringConstraintsBase::Init(const SolverParticlesOrRange& Particles, const SpatialAccelerator& Spatial,
	const TConstArrayView<FPBDTriangleMeshCollisions::FGIAColor>& VertexGIAColors, const TArray<FPBDTriangleMeshCollisions::FGIAColor>& TriangleGIAColors)
{
	if (!Elements.Num())
	{
		Constraints.Reset();
		Barys.Reset();
		FlipNormal.Reset();
		return;
	}
	{
		bGlobalIntersectionAnalysis = VertexGIAColors.Num() == NumParticles + Offset && TriangleGIAColors.Num() == Elements.Num();
		TRACE_CPUPROFILER_EVENT_SCOPE(ChaosPBDCollisionSpring_ProximityQuery);

		// Preallocate enough space for all possible connections.
		constexpr int32 MaxConnectionsPerPoint = 3;
		Constraints.SetNumUninitialized(NumParticles * MaxConnectionsPerPoint);
		Barys.SetNumUninitialized(NumParticles * MaxConnectionsPerPoint);
		FlipNormal.SetNumUninitialized(NumParticles * MaxConnectionsPerPoint);

		std::atomic<int32> ConstraintIndex(0);

		const FSolverReal HeightSq = FMath::Square(Thickness + Thickness);

		const TConstArrayView<FSolverVec3> ReferencePositionsView = GetConstArrayView(Particles, Offset, NumParticles, ReferencePositions);

		PhysicsParallelFor(NumParticles,
			[this, &Spatial, &Particles, &ConstraintIndex, HeightSq, MaxConnectionsPerPoint, &VertexGIAColors, &TriangleGIAColors, &ReferencePositionsView](int32 i)
			{
				const int32 Index = i + Offset;
				if (Particles.InvM(Index) == (FSolverReal)0.)
				{
					return;
				}
				constexpr FSolverReal ExtraThicknessMult = 1.5f;

				const bool bVertexHasCollisionLayers = VertexCollisionLayers.IsValidIndex(i) && VertexCollisionLayers[i][0] != INDEX_NONE;
				check(!bVertexHasCollisionLayers || VertexCollisionLayers[i][0] <= VertexCollisionLayers[i][1]);

				TArray< TTriangleCollisionPoint<FSolverReal> > Result;
				if (TriangleMesh.PointProximityQuery(Spatial, static_cast<const TArrayView<const FSolverVec3>&>(Particles.XArray()), Index, Particles.X(Index), Thickness * ExtraThicknessMult, Thickness * ExtraThicknessMult,
					[this, bVertexHasCollisionLayers, &VertexGIAColors, &TriangleGIAColors](const int32 PointIndex, const int32 TriangleIndex)->bool
					{
						const TVector<int32, 3>& Elem = Elements[TriangleIndex];						

						bool bUseCollisionLayerOverride = false;
						if (bVertexHasCollisionLayers && FaceCollisionLayers[TriangleIndex] != INDEX_NONE)
						{
							if (FaceCollisionLayers[TriangleIndex] < VertexCollisionLayers[PointIndex - Offset][0] || FaceCollisionLayers[TriangleIndex] > VertexCollisionLayers[PointIndex - Offset][1])
							{
								bUseCollisionLayerOverride = true;
							}
						}
						if (!bUseCollisionLayerOverride && bGlobalIntersectionAnalysis)
						{
							const bool bIsAnyBoundary = VertexGIAColors[PointIndex].IsBoundary()
								|| VertexGIAColors[Elem[0]].IsBoundary() 
								|| VertexGIAColors[Elem[1]].IsBoundary() 
								|| VertexGIAColors[Elem[2]].IsBoundary();
							if (bIsAnyBoundary)
							{
								return false;
							}

							const bool bAreBothLoop = VertexGIAColors[PointIndex].IsLoop() &&
								(VertexGIAColors[Elem[0]].IsLoop() 
									|| VertexGIAColors[Elem[1]].IsLoop() 
									|| VertexGIAColors[Elem[2]].IsLoop() 
									|| TriangleGIAColors[TriangleIndex].IsLoop());

							if (bAreBothLoop)
							{
								return false;
							}
						}

						if (DisabledCollisionElements.Contains({ PointIndex, Elem[0] }) ||
							DisabledCollisionElements.Contains({ PointIndex, Elem[1] }) ||
							DisabledCollisionElements.Contains({ PointIndex, Elem[2] }))
						{
							return false;
						}

						return true;
					},
					Result))
				{

					if (Result.Num() > MaxConnectionsPerPoint)
					{
						// TODO: once we have a PartialSort, use that instead here.
						Result.Sort(
							[](const TTriangleCollisionPoint<FSolverReal>& First, const TTriangleCollisionPoint<FSolverReal>& Second)->bool
							{
								return First.Phi < Second.Phi;
							}
						);
						Result.SetNum(MaxConnectionsPerPoint, EAllowShrinking::No);
					}

					for (const TTriangleCollisionPoint<FSolverReal>& CollisionPoint : Result)
					{
						const TVector<int32, 3>& Elem = Elements[CollisionPoint.Indices[1]];
						if (ReferencePositionsView.Num())
						{
							const FSolverVec3& RefP = ReferencePositionsView[Index];
							const FSolverVec3& RefP0 = ReferencePositionsView[Elem[0]];
							const FSolverVec3& RefP1 = ReferencePositionsView[Elem[1]];
							const FSolverVec3& RefP2 = ReferencePositionsView[Elem[2]];
							const FSolverVec3 RefDiff = RefP - CollisionPoint.Bary[1] * RefP0 - CollisionPoint.Bary[2] * RefP1 - CollisionPoint.Bary[3] * RefP2;
							if (RefDiff.SizeSquared() < HeightSq)
							{
								continue;
							}
						}

						bool bFlipNormal = false;
						// Check collision layers
						bool bUseCollisionLayerOverride = false;
						if (bVertexHasCollisionLayers && FaceCollisionLayers[CollisionPoint.Indices[1]] != INDEX_NONE)
						{
							if (FaceCollisionLayers[CollisionPoint.Indices[1]] < VertexCollisionLayers[i][0])
							{
								// Face is lower layer than the vertex. Vertex should always be in front of face (as UE sees it).
								// NOTE: Chaos internal winding order for normals is reversed, so flip normal in this case.
								bFlipNormal = true;
								bUseCollisionLayerOverride = true;
							}
							else if (FaceCollisionLayers[CollisionPoint.Indices[1]] > VertexCollisionLayers[i][1])
							{
								// Face is higher layer than the vertex. Vertex should always be behind face (as UE sees it).
								// NOTE: Chaos internal winding order for normals is reversed, so don't flip normal in this case.
								bFlipNormal = false;
								bUseCollisionLayerOverride = true;
							}
						}

						if (!bUseCollisionLayerOverride)
						{
							// NOTE: CollisionPoint.Normal has already been flipped to point toward the Point, so need to recalculate here.
							const TTriangle<FSolverReal> Triangle(Particles.X(Elem[0]), Particles.X(Elem[1]), Particles.X(Elem[2]));
							bFlipNormal = (Particles.X(Index) - CollisionPoint.Location).Dot(Triangle.GetNormal()) < 0; // Is Point currently behind Triangle?
							// Doing a check against ANY (plus the TriangleGIAColors which captures sub-triangle intersections) seems to work better than checking against ALL vertex colors where the triangle must agree.
							// In particular, it's better at handling thin regions of intersection where a single vertex or line of vertices intersect through faces.
							// Want Point to push to opposite side of triangle
							if (bGlobalIntersectionAnalysis &&
								(FPBDTriangleMeshCollisions::FGIAColor::ShouldFlipNormal(VertexGIAColors[Index], VertexGIAColors[Elem[0]]) ||
									FPBDTriangleMeshCollisions::FGIAColor::ShouldFlipNormal(VertexGIAColors[Index], VertexGIAColors[Elem[1]]) ||
									FPBDTriangleMeshCollisions::FGIAColor::ShouldFlipNormal(VertexGIAColors[Index], VertexGIAColors[Elem[2]]) ||
									FPBDTriangleMeshCollisions::FGIAColor::ShouldFlipNormal(VertexGIAColors[Index], TriangleGIAColors[CollisionPoint.Indices[1]])))
							{

								bFlipNormal = !bFlipNormal;
							}
						}
						const int32 IndexToWrite = ConstraintIndex.fetch_add(1);

						Constraints[IndexToWrite] = { Index, Elem[0], Elem[1], Elem[2] };
						Barys[IndexToWrite] = { CollisionPoint.Bary[1], CollisionPoint.Bary[2], CollisionPoint.Bary[3] };
						FlipNormal[IndexToWrite] = bFlipNormal;
					}
				}
			}
		);

		// Shrink the arrays to the actual number of found constraints.
		const int32 ConstraintNum = ConstraintIndex.load();
		Constraints.SetNum(ConstraintNum, EAllowShrinking::No);
		Barys.SetNum(ConstraintNum, EAllowShrinking::No);
		FlipNormal.SetNum(ConstraintNum, EAllowShrinking::No);
	}
}
template void CHAOS_API FPBDCollisionSpringConstraintsBase::Init<FTriangleMesh::TBVHType<FSolverReal>>(const FSolverParticles& Particles, const FTriangleMesh::TBVHType<FSolverReal>& Spatial, 
	const TConstArrayView<FPBDTriangleMeshCollisions::FGIAColor>& VertexGIAColors, const TArray<FPBDTriangleMeshCollisions::FGIAColor>& TriangleGIAColors);
template void CHAOS_API FPBDCollisionSpringConstraintsBase::Init<FTriangleMesh::TSpatialHashType<FSolverReal>>(const FSolverParticles& Particles, const FTriangleMesh::TSpatialHashType<FSolverReal>& Spatial, 
	const TConstArrayView<FPBDTriangleMeshCollisions::FGIAColor>& VertexGIAColors, const TArray<FPBDTriangleMeshCollisions::FGIAColor>& TriangleGIAColors);
template void CHAOS_API FPBDCollisionSpringConstraintsBase::Init<FTriangleMesh::TBVHType<FSolverReal>>(const FSolverParticlesRange& Particles, const FTriangleMesh::TBVHType<FSolverReal>& Spatial,
	const TConstArrayView<FPBDTriangleMeshCollisions::FGIAColor>& VertexGIAColors, const TArray<FPBDTriangleMeshCollisions::FGIAColor>& TriangleGIAColors);
template void CHAOS_API FPBDCollisionSpringConstraintsBase::Init<FTriangleMesh::TSpatialHashType<FSolverReal>>(const FSolverParticlesRange& Particles, const FTriangleMesh::TSpatialHashType<FSolverReal>& Spatial,
	const TConstArrayView<FPBDTriangleMeshCollisions::FGIAColor>& VertexGIAColors, const TArray<FPBDTriangleMeshCollisions::FGIAColor>& TriangleGIAColors);

template<typename SolverParticlesOrRange>
FSolverVec3 FPBDCollisionSpringConstraintsBase::GetDelta(const SolverParticlesOrRange& Particles, const int32 ConstraintIndex) const
{
	const TVec4<int32>& Constraint = Constraints[ConstraintIndex];
	const int32 Index1 = Constraint[0];
	const int32 Index2 = Constraint[1];
	const int32 Index3 = Constraint[2];
	const int32 Index4 = Constraint[3];

	const FSolverReal TrianglePointInvM =
		Particles.InvM(Index2) * Barys[ConstraintIndex][0] +
		Particles.InvM(Index3) * Barys[ConstraintIndex][1] +
		Particles.InvM(Index4) * Barys[ConstraintIndex][2];

	const FSolverReal CombinedMass = Particles.InvM(Index1) + TrianglePointInvM;
	if (CombinedMass <= (FSolverReal)1e-7)
	{
		return FSolverVec3(0);
	}

	const FSolverVec3& P1 = Particles.P(Index1);
	const FSolverVec3& P2 = Particles.P(Index2);
	const FSolverVec3& P3 = Particles.P(Index3);
	const FSolverVec3& P4 = Particles.P(Index4);

	const FSolverReal Height = Thickness + Thickness;
	const FSolverVec3 P = Barys[ConstraintIndex][0] * P2 + Barys[ConstraintIndex][1] * P3 + Barys[ConstraintIndex][2] * P4;
	const FSolverVec3 Difference = P1 - P;

	// Normal repulsion with friction
	const TTriangle<FSolverReal> Triangle(P2, P3, P4);
	const FSolverVec3 Normal = FlipNormal[ConstraintIndex] ? -Triangle.GetNormal() : Triangle.GetNormal();

	const FSolverReal NormalDifference = Difference.Dot(Normal);
	if (NormalDifference > Height)
	{
		return FSolverVec3(0);
	}

	const FSolverReal NormalDelta = Height - NormalDifference;
	const FSolverVec3 RepulsionDelta = Stiffness * NormalDelta * Normal / CombinedMass;

	if (FrictionCoefficient > 0)
	{
		const FSolverVec3& X1 = Particles.X(Index1);
		const FSolverVec3 X = Barys[ConstraintIndex][0] * Particles.X(Index2) + Barys[ConstraintIndex][1] * Particles.X(Index3) + Barys[ConstraintIndex][2] * Particles.X(Index4);
		const FSolverVec3 RelativeDisplacement = (P1 - X1) - (P - X) + (Particles.InvM(Index1) - TrianglePointInvM) * RepulsionDelta;
		const FSolverVec3 RelativeDisplacementTangent = RelativeDisplacement - RelativeDisplacement.Dot(Normal) * Normal;
		const FSolverReal RelativeDisplacementTangentLength = RelativeDisplacementTangent.Length();
		const FSolverReal PositionCorrection = FMath::Min(NormalDelta * FrictionCoefficient, RelativeDisplacementTangentLength);
		const FSolverReal CorrectionRatio = RelativeDisplacementTangentLength < UE_SMALL_NUMBER ? 0.f : PositionCorrection / RelativeDisplacementTangentLength;
		const FSolverVec3 FrictionDelta = -CorrectionRatio * RelativeDisplacementTangent / CombinedMass;
		return RepulsionDelta + FrictionDelta;
	}
	else
	{
		return RepulsionDelta;
	}
}
template CHAOS_API FSolverVec3 FPBDCollisionSpringConstraintsBase::GetDelta(const FSolverParticles& Particles, const int32 i) const;
template CHAOS_API FSolverVec3 FPBDCollisionSpringConstraintsBase::GetDelta(const FSolverParticlesRange& Particles, const int32 i) const;

void FPBDCollisionSpringConstraintsBase::UpdateLinearSystem(const FSolverParticlesRange& Particles, const FSolverReal Dt, FEvolutionLinearSystem& LinearSystem) const
{
	LinearSystem.ReserveForParallelAdd(Constraints.Num() * 4, Constraints.Num() * 3);
	for (int32 Index = 0; Index < Constraints.Num(); ++Index)
	{
		const TVector<int32, 4>& Constraint = Constraints[Index];
		const int32 Index1 = Constraint[0];
		const int32 Index2 = Constraint[1];
		const int32 Index3 = Constraint[2];
		const int32 Index4 = Constraint[3];
		const FSolverVec3& P1 = Particles.P(Index1);
		const FSolverVec3& P2 = Particles.P(Index2);
		const FSolverVec3& P3 = Particles.P(Index3);
		const FSolverVec3& P4 = Particles.P(Index4);

		const FSolverReal Height = Thickness + Thickness;
		const FSolverVec3 P = Barys[Index][0] * P2 + Barys[Index][1] * P3 + Barys[Index][2] * P4;
		const FSolverVec3 Difference = P1 - P;

		// Normal repulsion with some stiction
		const TTriangle<FSolverReal> Triangle(P2, P3, P4);
		const FSolverVec3 Normal = FlipNormal[Index] ? -Triangle.GetNormal() : Triangle.GetNormal();

		const FSolverReal NormalDifference = Difference.Dot(Normal);
		if (NormalDifference > Height)
		{
			continue;
		}

		const FSolverReal NormalDelta = Height - NormalDifference;

		const FSolverVec3 Force = ProximityStiffness * NormalDelta * Normal;
		const FSolverMatrix33 DfDx = -ProximityStiffness * (((FSolverReal)1. - FrictionCoefficient) * FSolverMatrix33::OuterProduct(Normal, Normal) + FSolverMatrix33(FrictionCoefficient, FrictionCoefficient, FrictionCoefficient));

		LinearSystem.AddForce(Particles, Force, Index1, Dt);
		LinearSystem.AddSymmetricForceDerivative(Particles, &DfDx, nullptr, Index1, Index1, Dt);
		if (Particles.InvM(Index2) > (FSolverReal)0.)
		{
			LinearSystem.AddForce(Particles, -Barys[Index][0] * Force, Index2, Dt);
			FSolverMatrix33 DfDxScaled = -Barys[Index][0] * DfDx;
			LinearSystem.AddSymmetricForceDerivative(Particles, &DfDxScaled, nullptr, Index1, Index2, Dt);
			DfDxScaled *= -Barys[Index][0];
			LinearSystem.AddSymmetricForceDerivative(Particles, &DfDxScaled, nullptr, Index2, Index2, Dt);
		}
		if (Particles.InvM(Index3) > (FSolverReal)0.)
		{
			LinearSystem.AddForce(Particles, -Barys[Index][1] * Force, Index3, Dt);
			FSolverMatrix33 DfDxScaled = -Barys[Index][1] * DfDx;
			LinearSystem.AddSymmetricForceDerivative(Particles, &DfDxScaled, nullptr, Index1, Index3, Dt);
			DfDxScaled *= -Barys[Index][1];
			LinearSystem.AddSymmetricForceDerivative(Particles, &DfDxScaled, nullptr, Index3, Index3, Dt);
		}
		if (Particles.InvM(Index4) > (FSolverReal)0.)
		{
			LinearSystem.AddForce(Particles, -Barys[Index][2] * Force, Index4, Dt);
			FSolverMatrix33 DfDxScaled = -Barys[Index][2] * DfDx;
			LinearSystem.AddSymmetricForceDerivative(Particles, &DfDxScaled, nullptr, Index1, Index4, Dt);
			DfDxScaled *= -Barys[Index][2];
			LinearSystem.AddSymmetricForceDerivative(Particles, &DfDxScaled, nullptr, Index4, Index4, Dt);
		}
	}
}
}  // End namespace Chaos::Softs

#endif
