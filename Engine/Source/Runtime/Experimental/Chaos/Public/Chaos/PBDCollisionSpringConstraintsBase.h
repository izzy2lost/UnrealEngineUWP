// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if !COMPILE_WITHOUT_UNREAL_SUPPORT
#include "Chaos/Core.h"
#include "Chaos/PBDSoftsEvolutionFwd.h"
#include "Chaos/PBDSoftsSolverParticles.h"
#include "Chaos/PBDTriangleMeshCollisions.h"
#include "Containers/Set.h"


namespace Chaos
{
	class FTriangleMesh;
}

namespace Chaos::Softs
{

// This is an invertible spring class, typical springs are not invertible aware
class FPBDCollisionSpringConstraintsBase
{
public:
	static constexpr FSolverReal BackCompatThickness = (FSolverReal)1.f;
	static constexpr FSolverReal BackCompatStiffness = (FSolverReal)0.5f;
	static constexpr FSolverReal BackCompatFrictionCoefficient = (FSolverReal)0.f;
	static constexpr FSolverReal DefaultProximityStiffness = (FSolverReal)1.;

	CHAOS_API FPBDCollisionSpringConstraintsBase(
		const int32 InOffset,
		const int32 InNumParticles,
		const FTriangleMesh& InTriangleMesh,
		const TArray<FSolverVec3>* InReferencePositions,
		TSet<TVec2<int32>>&& InDisabledCollisionElements,
		const TConstArrayView<int32>& InSelfCollisionLayers,
		const FSolverReal InThickness = BackCompatThickness,
		const FSolverReal InStiffness = BackCompatStiffness,
		const FSolverReal InFrictionCoefficient = BackCompatFrictionCoefficient,
		const FSolverReal InProximityStiffness = DefaultProximityStiffness);

	UE_DEPRECATED(5.4, "Use constructor with SelfCollisionLayers")
	FPBDCollisionSpringConstraintsBase(
		const int32 InOffset,
		const int32 InNumParticles,
		const FTriangleMesh& InTriangleMesh,
		const TArray<FSolverVec3>* InReferencePositions,
		TSet<TVec2<int32>>&& InDisabledCollisionElements,
		const FSolverReal InThickness = BackCompatThickness,
		const FSolverReal InStiffness = BackCompatStiffness,
		const FSolverReal InFrictionCoefficient = BackCompatFrictionCoefficient,
		const FSolverReal InProximityStiffness = DefaultProximityStiffness)
		: FPBDCollisionSpringConstraintsBase(InOffset, InNumParticles, InTriangleMesh, InReferencePositions, MoveTemp(InDisabledCollisionElements),
			TConstArrayView<int32>(), InThickness, InStiffness, InFrictionCoefficient, InProximityStiffness)
	{}

	virtual ~FPBDCollisionSpringConstraintsBase() {}

	UE_DEPRECATED(5.0, "Use Init(Particles, Spatial, GIAColors) instead.")
	CHAOS_API void Init(const FSolverParticles& Particles);

	template<typename SpatialAccelerator, typename SolverParticlesOrRange>
	void Init(const SolverParticlesOrRange& Particles, const SpatialAccelerator& Spatial, const TConstArrayView<FPBDTriangleMeshCollisions::FGIAColor>& VertexGIAColors, const TArray<FPBDTriangleMeshCollisions::FGIAColor>& TriangleGIAColors);

	template<typename SolverParticlesOrRange>
	CHAOS_API FSolverVec3 GetDelta(const SolverParticlesOrRange& InParticles, const int32 i) const;

	const TArray<TVec4<int32>>& GetConstraints() const { return Constraints;  }
	const TArray<FSolverVec3>& GetBarys() const { return Barys; }
	FSolverReal GetThickness() const { return Thickness; }
	bool GetGlobalIntersectionAnalysis() const { return bGlobalIntersectionAnalysis; }
	const TArray<bool>& GetFlipNormals() const { return FlipNormal; }

	void SetThickness(FSolverReal InThickness) { Thickness = FMath::Max(InThickness, (FSolverReal)0.);  }
	void SetFrictionCoefficient(FSolverReal InFrictionCoefficient) { FrictionCoefficient = InFrictionCoefficient; }

	template<typename SolverParticlesOrRange>
	void Apply(SolverParticlesOrRange& Particles, const FSolverReal Dt, const int32 ConstraintIndex) const
	{
		const TVector<int32, 4>& Constraint = Constraints[ConstraintIndex];
		const int32 Index1 = Constraint[0];
		const int32 Index2 = Constraint[1];
		const int32 Index3 = Constraint[2];
		const int32 Index4 = Constraint[3];
		const FSolverVec3 Delta = GetDelta(Particles, ConstraintIndex);
		if (Particles.InvM(Index1) > 0)
		{
			Particles.P(Index1) += Particles.InvM(Index1) * Delta;
		}
		if (Particles.InvM(Index2) > (FSolverReal)0.)
		{
			Particles.P(Index2) -= Particles.InvM(Index2) * Barys[ConstraintIndex][0] * Delta;
		}
		if (Particles.InvM(Index3) > (FSolverReal)0.)
		{
			Particles.P(Index3) -= Particles.InvM(Index3) * Barys[ConstraintIndex][1] * Delta;
		}
		if (Particles.InvM(Index4) > (FSolverReal)0.)
		{
			Particles.P(Index4) -= Particles.InvM(Index4) * Barys[ConstraintIndex][2] * Delta;
		}
	}

	template<typename SolverParticlesOrRange>
	void Apply(SolverParticlesOrRange& InParticles, const FSolverReal Dt) const
	{
		for (int32 ConstraintIndex = 0; ConstraintIndex < Constraints.Num(); ++ConstraintIndex)
		{
			Apply(InParticles, Dt, ConstraintIndex);
		}
	}

	void Apply(FSolverParticles& InParticles, const FSolverReal Dt, const TArray<int32>& InConstraintIndices) const
	{
		for (int32 ConstraintIndex : InConstraintIndices)
		{
			Apply(InParticles, Dt, ConstraintIndex);
		}
	}

	CHAOS_API void UpdateLinearSystem(const FSolverParticlesRange& Particles, const FSolverReal Dt, FEvolutionLinearSystem& LinearSystem) const;
	
	TConstArrayView<int32> GetFaceCollisionLayers() const { return FaceCollisionLayers; }
	const TArray<TVector<int32, 2>>& GetVertexCollisionLayers() const { return VertexCollisionLayers; }

protected:
	TArray<TVec4<int32>> Constraints;
	TArray<FSolverVec3> Barys;
	TArray<bool> FlipNormal;
	FSolverReal Thickness;
	FSolverReal Stiffness; // (0-1 compliance for PBD)
	FSolverReal FrictionCoefficient;
	FSolverReal ProximityStiffness; // (actual spring stiffness for force-based solver)

	CHAOS_API void UpdateCollisionLayers(const TConstArrayView<int32>& InFaceCollisionLayers);

private:
	const FTriangleMesh& TriangleMesh;
	const TArray<TVec3<int32>>& Elements;
	const TArray<FSolverVec3>* ReferencePositions;
	const TSet<TVec2<int32>> DisabledCollisionElements;  // TODO: Make this a bitarray
	TConstArrayView<int32> FaceCollisionLayers;
	TArray<TVector<int32, 2>> VertexCollisionLayers; // Only non-empty if FaceCollisionLayers is non-empty. Values are Min and Max layers for that vertex

	int32 Offset;
	int32 NumParticles;
	bool bGlobalIntersectionAnalysis; // This is set based on which Init is called.
};

}  // End namespace Chaos::Softs

#endif
