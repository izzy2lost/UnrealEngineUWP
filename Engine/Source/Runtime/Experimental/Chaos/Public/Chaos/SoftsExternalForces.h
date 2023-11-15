// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/PBDSoftsEvolutionFwd.h"
#include "Chaos/PBDSoftsSolverParticles.h"
#include "Chaos/SoftsSolverParticlesRange.h"
#include "Chaos/CollectionPropertyFacade.h"

namespace Chaos::Softs
{
class FExternalForcesBase
{
public:
	FExternalForcesBase(const FSolverVec3& InGravity,
		const TArray<FSolverVec3>& InNormals)
		: Gravity(InGravity)
		, FictitiousAngularDisplacement(0.f)
		, ReferenceSpaceLocation(0.f)
		, bUsePointBasedWindModel(false)
		, PointBasedWind(0.f)
		, LegacyWindAdaptation(0.f)
		, Normals(InNormals)
	{}

	virtual ~FExternalForcesBase() {}

	void Apply(FSolverParticlesRange& Particles, const FSolverReal Dt) const
	{
		FSolverVec3* const Acceleration = Particles.GetAcceleration().GetData();
		const FSolverReal* const InvM = Particles.GetInvM().GetData();
		const FSolverVec3* const X = Particles.XArray().GetData();
		const FSolverVec3* const V = Particles.GetV().GetData();
		const FSolverVec3* const N = Particles.GetConstArrayView(Normals).GetData();

		const bool bHasFictitiousForces = !FictitiousAngularDisplacement.IsNearlyZero();
		const FSolverVec3 W = FictitiousAngularDisplacement / Dt;

		for (int32 Index = 0; Index < Particles.GetRangeSize(); ++Index)
		{
			if (InvM[Index] != (FSolverReal)0.)
			{
				Acceleration[Index] = Gravity;

				if (bHasFictitiousForces)
				{
					// Centrifugal force (*InvM to get acceleration)
					Acceleration[Index] -= FSolverVec3::CrossProduct(W, FSolverVec3::CrossProduct(W, X[Index] - ReferenceSpaceLocation));
				}
				if (bUsePointBasedWindModel)
				{
					const FSolverVec3 VelocityDelta = PointBasedWind - V[Index];
					FSolverVec3 Direction = VelocityDelta;
					if (Direction.Normalize())
					{
						// Scale by angle
						const FReal DirectionDot = FVec3::DotProduct(Direction, N[Index]);
						const FReal ScaleFactor = FMath::Min(1.f, FMath::Abs(DirectionDot) * LegacyWindAdaptation);
						Acceleration[Index] += VelocityDelta * ScaleFactor;
					}
				}
			}
		}
	}

	bool UsePointBasedWindModel() const { return bUsePointBasedWindModel; }
	const FSolverVec3& GetGravity() const { return Gravity; }

protected:
	FSolverVec3 Gravity; 
	FSolverVec3 FictitiousAngularDisplacement;
	FSolverVec3 ReferenceSpaceLocation;
	bool bUsePointBasedWindModel;
	FSolverVec3 PointBasedWind;
	FSolverReal LegacyWindAdaptation;
	const TArray<FSolverVec3>& Normals;
};

class FExternalForces : public FExternalForcesBase
{
	typedef FExternalForcesBase Base;
public:
	static constexpr FSolverReal DefaultGravityScale = (FSolverReal)1.f;
	static constexpr FSolverReal DefaultGravityZOverride = (FSolverReal)-980.665f;
	static constexpr bool bDefaultUseGravityOverride = false;
	static constexpr FSolverReal DefaultFictitiousAngularScale = (FSolverReal)1.f;
	static constexpr bool bDefaultUsePointBasedWindModel = false;

	// We don't have the solver values at the time this is constructed. Need to SetProperties at least once after setting 
	// data from the solver.
	FExternalForces(
		const TArray<FSolverVec3>& InNormals,
		const FCollectionPropertyConstFacade& PropertyCollection)
		: Base(
			FSolverVec3((FSolverReal)0.f, (FSolverReal)0.f, DefaultGravityZOverride),
			InNormals
			)
		, WorldGravityMultiplier((FSolverReal)1.f)
		, SolverGravity((FSolverReal)0.f, (FSolverReal)0.f, DefaultGravityZOverride)
		, bPerSoftBodyGravityOverrideEnabled(true)
		, FictitiousAngularDisplacementNoScale(0.f)
		, UseGravityOverrideIndex(PropertyCollection)
		, GravityOverrideIndex(PropertyCollection)
		, GravityScaleIndex(PropertyCollection)
		, FictitiousAngularScaleIndex(PropertyCollection)
		, UsePointBasedWindModelIndex(PropertyCollection)
	{}

	void SetWorldGravityMultiplier(FSolverReal InWorldGravityMultiplier) { WorldGravityMultiplier = InWorldGravityMultiplier; }
	void SetSolverGravityProperties(const FSolverVec3& InSolverGravity, bool bInPerSoftBodyGravityOverrideEnabled)
	{
		SolverGravity = InSolverGravity;
		bPerSoftBodyGravityOverrideEnabled = bInPerSoftBodyGravityOverrideEnabled;
	}
	void SetFictitiousForcesData(const FSolverVec3& InFictitiousAngularDisplacementNoScale, const FSolverVec3& InReferenceSpaceLocation)
	{
		FictitiousAngularDisplacementNoScale = InFictitiousAngularDisplacementNoScale;
		ReferenceSpaceLocation = InReferenceSpaceLocation;
	}
	void SetSolverWind(const FSolverVec3& SolverWind, const FSolverReal InLegacyWindAdaptation)
	{
		PointBasedWind = SolverWind;
		LegacyWindAdaptation = InLegacyWindAdaptation;
	}

	/** This should be called after all other data is set as it will populate the final base class values.*/
	void SetProperties(const FCollectionPropertyConstFacade& PropertyCollection)
	{
		const bool bUseGravityOverride = UseGravityOverrideIndex != INDEX_NONE ? GetUseGravityOverride(PropertyCollection) : bDefaultUseGravityOverride;
		const FSolverVec3 GravityOverride = GravityOverrideIndex != INDEX_NONE ? FSolverVec3(GetGravityOverride(PropertyCollection)) :
			FSolverVec3(0.f, 0.f, DefaultGravityZOverride);
		const FSolverReal GravityScale = GravityScaleIndex != INDEX_NONE ? GetGravityScale(PropertyCollection) : DefaultGravityScale;
		CalculateGravity(bUseGravityOverride, GravityOverride, GravityScale);

		const FSolverReal FictitiousAngularScale = FMath::Min((FSolverReal)2., FictitiousAngularScaleIndex != INDEX_NONE ? GetFictitiousAngularScale(PropertyCollection) : DefaultFictitiousAngularScale);

		FictitiousAngularDisplacement = FictitiousAngularDisplacementNoScale * FictitiousAngularScale;

		bUsePointBasedWindModel = UsePointBasedWindModelIndex != INDEX_NONE ? GetUsePointBasedWindModel(PropertyCollection) : bDefaultUsePointBasedWindModel;
	}
private:
	void CalculateGravity(bool bUseGravityOverride, const FSolverVec3& GravityOverride, FSolverReal GravityScale)
	{
		Gravity = (bPerSoftBodyGravityOverrideEnabled && bUseGravityOverride ? GravityOverride : SolverGravity * GravityScale) * WorldGravityMultiplier;
	}

	/** Gravity */
	// "World" level properties (this comes from a world-level CVar)
	FSolverReal WorldGravityMultiplier;

	// "Solver" level properties (set from Solver-level BP)
	FSolverVec3 SolverGravity;
	bool bPerSoftBodyGravityOverrideEnabled;

	/** Fictitious Forces */
	FSolverVec3 FictitiousAngularDisplacementNoScale; // Without Scale parameter applied.

	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(UseGravityOverride, bool);
	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(GravityOverride, FVector3f);
	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(GravityScale, float);
	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(FictitiousAngularScale, float);
	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(UsePointBasedWindModel, bool);
};
}
