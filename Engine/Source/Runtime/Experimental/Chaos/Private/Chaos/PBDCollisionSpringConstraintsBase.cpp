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

int32 DebugParticleIndex = INDEX_NONE;
FAutoConsoleVariableRef CVarChaosClothDebugParticleIndex(TEXT("p.ChaosCloth.DebugParticleIndex"), DebugParticleIndex, TEXT("DebugParticleIndex"));

FSolverReal Chaos_CollisionSpring_MaxTimer = (FSolverReal)0.1f;
FAutoConsoleVariableRef CVarChaosCollisionSPringMaxTimer(TEXT("p.Chaos.CollisionSpring.MaxTimer"), Chaos_CollisionSpring_MaxTimer, TEXT("Max Timer"));



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
	const FSolverReal InKinematicColliderThickness,
	const FSolverReal InKinematicColliderStiffness,
	const FSolverReal InKinematicColliderFrictionCoefficient,
	const FSolverReal InProximityStiffness)
	: Thickness(InThickness)
	, Stiffness(InStiffness)
	, FrictionCoefficient(InFrictionCoefficient)
	, KinematicColliderThickness(InKinematicColliderThickness)
	, KinematicColliderStiffness(InKinematicColliderStiffness)
	, KinematicColliderFrictionCoefficient(InKinematicColliderFrictionCoefficient)
	, ProximityStiffness(InProximityStiffness)
	, TriangleMesh(InTriangleMesh)
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
	if (InFaceCollisionLayers.Num() != TriangleMesh.GetElements().Num())
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

template<typename SpatialAccelerator, typename SolverParticlesOrRange>
void FPBDCollisionSpringConstraintsBase::Init(const SolverParticlesOrRange& Particles, const FSolverReal Dt, const FPBDTriangleMeshCollisions::FTriangleSubMesh& CollidableSubMesh, const SpatialAccelerator& Spatial,
	const TConstArrayView<FPBDTriangleMeshCollisions::FGIAColor>& VertexGIAColors, const TArray<FPBDTriangleMeshCollisions::FGIAColor>& TriangleGIAColors)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	const FTriangleMesh& CollidableMesh = CollidableSubMesh.GetCollidableMesh();
	const TArray<TVec3<int32>>& Elements = CollidableMesh.GetElements();

	if (!Elements.Num())
	{
		Constraints.Reset();
		Barys.Reset();
		FlipNormal.Reset();
		ConstraintTypes.Reset();
		return;
	}
	{
		if (ExistingConstraintLookup.Num() != NumParticles)
		{
			ExistingConstraintLookup.Reset();
			ExistingConstraintLookup.SetNum(NumParticles);
		}

		bGlobalIntersectionAnalysis = VertexGIAColors.Num() == NumParticles + Offset && TriangleGIAColors.Num() == Elements.Num();
		TRACE_CPUPROFILER_EVENT_SCOPE(ChaosPBDCollisionSpring_ProximityQuery);

		const int32 NumCollidableParticles = CollidableSubMesh.GetCollidableVertices().IsEmpty() ? NumParticles : CollidableSubMesh.GetCollidableVertices().Num();

		// Preallocate enough space for all possible connections.
		constexpr int32 MaxConnectionsPerPoint = 3;
		Constraints.SetNumUninitialized(NumCollidableParticles * MaxConnectionsPerPoint);
		Barys.SetNumUninitialized(NumCollidableParticles * MaxConnectionsPerPoint);
		FlipNormal.SetNumUninitialized(NumCollidableParticles * MaxConnectionsPerPoint);
		ConstraintTypes.SetNumUninitialized(NumCollidableParticles * MaxConnectionsPerPoint);

		std::atomic<int32> ConstraintIndex(0);

		const FSolverReal MaxSingleSidedThickness = FMath::Max(Thickness, KinematicColliderThickness);
		const FSolverReal HeightSq = FMath::Square(Thickness + MaxSingleSidedThickness);

		const TConstArrayView<FSolverVec3> ReferencePositionsView = GetConstArrayView(Particles, Offset, NumParticles, ReferencePositions);

		PhysicsParallelFor(NumCollidableParticles,
			[this, Dt, &CollidableSubMesh, &CollidableMesh, &Elements, &Spatial, &Particles, &ConstraintIndex, MaxSingleSidedThickness, HeightSq, MaxConnectionsPerPoint, &VertexGIAColors, &TriangleGIAColors, &ReferencePositionsView](int32 CollidableIndex)
			{
				const int32 i = CollidableSubMesh.GetCollidableVertices().IsEmpty() ? CollidableIndex : CollidableSubMesh.GetCollidableVertices()[CollidableIndex];
				const int32 Index = i + Offset;
				if (Particles.InvM(Index) == (FSolverReal)0.)
				{
					return;
				}
				constexpr FSolverReal ExtraThicknessMult = 1.5f;

				const bool bVertexHasCollisionLayers = VertexCollisionLayers.IsValidIndex(i) && VertexCollisionLayers[i][0] != INDEX_NONE;
				check(!bVertexHasCollisionLayers || VertexCollisionLayers[i][0] <= VertexCollisionLayers[i][1]);

				TMap<int32, FExistingConstraintData> PrevExistingConstraintLookup;
				Swap(PrevExistingConstraintLookup, ExistingConstraintLookup[i]);

				TArray< TTriangleCollisionPoint<FSolverReal> > Result;
				int32 ConstraintsAdded = 0;
				if (CollidableMesh.PointProximityQuery(Spatial, static_cast<const TArrayView<const FSolverVec3>&>(Particles.XArray()), Index, Particles.GetX(Index), Thickness * ExtraThicknessMult, MaxSingleSidedThickness * ExtraThicknessMult,
					[this, bVertexHasCollisionLayers, &Particles, &CollidableSubMesh, &Elements, &VertexGIAColors, &TriangleGIAColors](const int32 PointIndex, const int32 SubMeshTriangleIndex)->bool
					{
						const TVector<int32, 3>& Elem = Elements[SubMeshTriangleIndex];
						const int32 FullMeshTriangleIndex = CollidableSubMesh.FullMeshIndexFromSubIndex(SubMeshTriangleIndex);

						const bool bFaceIsKinematic = CollidableSubMesh.GetSubMeshElementIsKinematic()[SubMeshTriangleIndex];
						bool bUseCollisionLayerOverride = false;
						if (!bFaceIsKinematic && bVertexHasCollisionLayers && FaceCollisionLayers[FullMeshTriangleIndex] != INDEX_NONE)
						{
							if (FaceCollisionLayers[FullMeshTriangleIndex] < VertexCollisionLayers[PointIndex - Offset][0] || FaceCollisionLayers[FullMeshTriangleIndex] > VertexCollisionLayers[PointIndex - Offset][1])
							{
								bUseCollisionLayerOverride = true;
							}
						}
						if (!bFaceIsKinematic && !bUseCollisionLayerOverride && bGlobalIntersectionAnalysis)
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
									|| TriangleGIAColors[SubMeshTriangleIndex].IsLoop());

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


						FSolverVec3 Bary(CollisionPoint.Bary[1], CollisionPoint.Bary[2], CollisionPoint.Bary[3]);

						const int32 FullMeshTriangleIndex = CollidableSubMesh.FullMeshIndexFromSubIndex(CollisionPoint.Indices[1]);

						bool bFlipNormal = false;
						// Check kinematic
						const bool bFaceIsKinematic = CollidableSubMesh.GetSubMeshElementIsKinematic()[CollisionPoint.Indices[1]];
						if (bFaceIsKinematic)
						{
							PrevExistingConstraintLookup.Remove(FullMeshTriangleIndex);
							// Chaos internal winding order is reversed.
							bFlipNormal = true;
						}

						EConstraintType ConstraintType = bFaceIsKinematic ? EConstraintType::Kinematic : EConstraintType::Default;

						// Check collision layers
						bool bUseCollisionLayerOverride = false;
						if (!bFaceIsKinematic && bVertexHasCollisionLayers && FaceCollisionLayers[FullMeshTriangleIndex] != INDEX_NONE)
						{
							if (FaceCollisionLayers[FullMeshTriangleIndex] < VertexCollisionLayers[i][0])
							{
								// Face is lower layer than the vertex. Vertex should always be in front of face (as UE sees it).
								// NOTE: Chaos internal winding order for normals is reversed, so flip normal in this case.
								bFlipNormal = true;
								bUseCollisionLayerOverride = true;
							}
							else if (FaceCollisionLayers[FullMeshTriangleIndex] > VertexCollisionLayers[i][1])
							{
								// Face is higher layer than the vertex. Vertex should always be behind face (as UE sees it).
								// NOTE: Chaos internal winding order for normals is reversed, so don't flip normal in this case.
								bFlipNormal = false;
								bUseCollisionLayerOverride = true;
							}
						}

						if (!bFaceIsKinematic && !bUseCollisionLayerOverride)
						{
							// NOTE: CollisionPoint.Normal has already been flipped to point toward the Point, so need to recalculate here.
							const TTriangle<FSolverReal> Triangle(Particles.GetX(Elem[0]), Particles.GetX(Elem[1]), Particles.GetX(Elem[2]));
							bFlipNormal = (Particles.GetX(Index) - CollisionPoint.Location).Dot(Triangle.GetNormal()) < 0; // Is Point currently behind Triangle?
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
								ConstraintType = EConstraintType::GIAFlipped;
							}
						}
						const int32 IndexToWrite = ConstraintIndex.fetch_add(1);

						Constraints[IndexToWrite] = { Index, Elem[0], Elem[1], Elem[2] };
						Barys[IndexToWrite] = Bary;
						FlipNormal[IndexToWrite] = bFlipNormal;
						ConstraintTypes[IndexToWrite] = ConstraintType;
						if (ConstraintType == EConstraintType::Kinematic)
						{
							// Remember this constraint
							ExistingConstraintLookup[i].Add(FullMeshTriangleIndex, {0.f});
						}
						++ConstraintsAdded;
					}
				}
				
				for (TMap<int32, FExistingConstraintData>::TConstIterator PrevData = PrevExistingConstraintLookup.CreateConstIterator(); PrevData && ConstraintsAdded < MaxConnectionsPerPoint; ++PrevData)
				{
					if (PrevData.Value().Timer + Dt <= Chaos_CollisionSpring_MaxTimer)
					{
						const int32 SubMeshIndex = CollidableSubMesh.SubMeshIndexFromFullIndex(PrevData.Key());
						if (SubMeshIndex != INDEX_NONE)
						{
							const int32 IndexToWrite = ConstraintIndex.fetch_add(1);
							const TVec3<int32>& Elem = Elements[SubMeshIndex];
							Constraints[IndexToWrite] = { Index, Elem[0], Elem[1], Elem[2] };
							Barys[IndexToWrite] = FSolverVec3(1.f, 0.f, 0.f); // Unused
							FlipNormal[IndexToWrite] = true;
							ConstraintTypes[IndexToWrite] = EConstraintType::Kinematic;
							ExistingConstraintLookup[i].Add(PrevData.Key(), {PrevData.Value().Timer + Dt});
							++ConstraintsAdded;
						}
					}
				}
			}
		);

		// Shrink the arrays to the actual number of found constraints.
		const int32 ConstraintNum = ConstraintIndex.load();
		Constraints.SetNum(ConstraintNum, EAllowShrinking::No);
		Barys.SetNum(ConstraintNum, EAllowShrinking::No);
		FlipNormal.SetNum(ConstraintNum, EAllowShrinking::No);
		ConstraintTypes.SetNum(ConstraintNum, EAllowShrinking::No);
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}
template void CHAOS_API FPBDCollisionSpringConstraintsBase::Init<FTriangleMesh::TBVHType<FSolverReal>>(const FSolverParticles& Particles, const FSolverReal Dt, const FPBDTriangleMeshCollisions::FTriangleSubMesh& CollidableSubMesh, const FTriangleMesh::TBVHType<FSolverReal>& Spatial,
	const TConstArrayView<FPBDTriangleMeshCollisions::FGIAColor>& VertexGIAColors, const TArray<FPBDTriangleMeshCollisions::FGIAColor>& TriangleGIAColors);
template void CHAOS_API FPBDCollisionSpringConstraintsBase::Init<FTriangleMesh::TSpatialHashType<FSolverReal>>(const FSolverParticles& Particles, const FSolverReal Dt, const FPBDTriangleMeshCollisions::FTriangleSubMesh& CollidableSubMesh, const FTriangleMesh::TSpatialHashType<FSolverReal>& Spatial,
	const TConstArrayView<FPBDTriangleMeshCollisions::FGIAColor>& VertexGIAColors, const TArray<FPBDTriangleMeshCollisions::FGIAColor>& TriangleGIAColors);
template void CHAOS_API FPBDCollisionSpringConstraintsBase::Init<FTriangleMesh::TBVHType<FSolverReal>>(const FSolverParticlesRange& Particles, const FSolverReal Dt, const FPBDTriangleMeshCollisions::FTriangleSubMesh& CollidableSubMesh, const FTriangleMesh::TBVHType<FSolverReal>& Spatial,
	const TConstArrayView<FPBDTriangleMeshCollisions::FGIAColor>& VertexGIAColors, const TArray<FPBDTriangleMeshCollisions::FGIAColor>& TriangleGIAColors);
template void CHAOS_API FPBDCollisionSpringConstraintsBase::Init<FTriangleMesh::TSpatialHashType<FSolverReal>>(const FSolverParticlesRange& Particles, const FSolverReal Dt, const FPBDTriangleMeshCollisions::FTriangleSubMesh& CollidableSubMesh, const FTriangleMesh::TSpatialHashType<FSolverReal>& Spatial,
	const TConstArrayView<FPBDTriangleMeshCollisions::FGIAColor>& VertexGIAColors, const TArray<FPBDTriangleMeshCollisions::FGIAColor>& TriangleGIAColors);

template<typename SolverParticlesOrRange>
FSolverVec3 FPBDCollisionSpringConstraintsBase::GetDelta(const SolverParticlesOrRange& Particles, const int32 ConstraintIndex) const
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
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

	const FSolverReal Height = GetConstraintThickness(ConstraintIndex);

	const TTriangle<FSolverReal> Triangle(P2, P3, P4);
	const FSolverVec3 Normal = FlipNormal[ConstraintIndex] ? -Triangle.GetNormal() : Triangle.GetNormal();

	FSolverVec3 P;
	FSolverVec3 Difference;
	FSolverReal NormalDifference;
	FSolverReal TangentialFalloff = 1.f;
	FSolverVec3 Bary;
	if (ConstraintTypes[ConstraintIndex] == EConstraintType::Kinematic)
	{
		P = FindClosestPointAndBaryOnTriangle(P2, P3, P4, P1, Bary);
		Difference = P1 - P;
		NormalDifference = Difference.Dot(Normal);
		const FSolverReal TangentialDifference = (Difference - NormalDifference * Normal).Size();
		TangentialFalloff = 1.f - TangentialDifference / FMath::Max(1.5f * Height, 1e-4f);
		if (TangentialFalloff <= 0.f)
		{
			return FSolverVec3(0);
		}
	}
	else
	{
		Bary = Barys[ConstraintIndex];
		P = Bary[0] * P2 + Bary[1] * P3 + Bary[2] * P4;
		Difference = P1 - P;
		NormalDifference = Difference.Dot(Normal);
	}

	// Normal repulsion with friction
	if (NormalDifference > Height)
	{
		return FSolverVec3(0);
	}

	const FSolverReal ConstraintStiffness = GetConstraintStiffness(ConstraintIndex) * TangentialFalloff;
	const FSolverReal ConstraintFriction = GetConstraintFrictionCoefficient(ConstraintIndex);

	const FSolverReal NormalDelta = Height - NormalDifference;
	const FSolverVec3 RepulsionDelta = ConstraintStiffness * NormalDelta * Normal / CombinedMass;

	if (ConstraintFriction > 0)
	{
		const FSolverVec3& X1 = Particles.GetX(Index1);
		const FSolverVec3 X = Bary[0] * Particles.GetX(Index2) + Bary[1] * Particles.GetX(Index3) + Bary[2] * Particles.GetX(Index4);
		const FSolverVec3 RelativeDisplacement = (P1 - X1) - (P - X) + (Particles.InvM(Index1) - TrianglePointInvM) * RepulsionDelta;
		const FSolverVec3 RelativeDisplacementTangent = RelativeDisplacement - RelativeDisplacement.Dot(Normal) * Normal;
		const FSolverReal RelativeDisplacementTangentLength = RelativeDisplacementTangent.Length();
		const FSolverReal PositionCorrection = FMath::Min(NormalDelta * ConstraintFriction, RelativeDisplacementTangentLength);
		const FSolverReal CorrectionRatio = RelativeDisplacementTangentLength < UE_SMALL_NUMBER ? 0.f : PositionCorrection / RelativeDisplacementTangentLength;
		const FSolverVec3 FrictionDelta = -CorrectionRatio * RelativeDisplacementTangent / CombinedMass;
		return RepulsionDelta + FrictionDelta;
	}
	else
	{
		return RepulsionDelta;
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}
template CHAOS_API FSolverVec3 FPBDCollisionSpringConstraintsBase::GetDelta(const FSolverParticles& Particles, const int32 i) const;
template CHAOS_API FSolverVec3 FPBDCollisionSpringConstraintsBase::GetDelta(const FSolverParticlesRange& Particles, const int32 i) const;

void FPBDCollisionSpringConstraintsBase::UpdateLinearSystem(const FSolverParticlesRange& Particles, const FSolverReal Dt, FEvolutionLinearSystem& LinearSystem) const
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
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

		const FSolverReal Height = GetConstraintThickness(Index);
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

		const FSolverReal ConstraintFriction = GetConstraintFrictionCoefficient(Index);
		const FSolverVec3 Force = ProximityStiffness * NormalDelta * Normal;
		const FSolverMatrix33 DfDx = -ProximityStiffness * (((FSolverReal)1. - ConstraintFriction) * FSolverMatrix33::OuterProduct(Normal, Normal) + FSolverMatrix33(ConstraintFriction, ConstraintFriction, ConstraintFriction));

		if (Particles.InvM(Index1) > (FSolverReal)0.)
		{
			LinearSystem.AddForce(Particles, Force, Index1, Dt);
			LinearSystem.AddSymmetricForceDerivative(Particles, &DfDx, nullptr, Index1, Index1, Dt);
		}
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
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}
}  // End namespace Chaos::Softs

#endif
