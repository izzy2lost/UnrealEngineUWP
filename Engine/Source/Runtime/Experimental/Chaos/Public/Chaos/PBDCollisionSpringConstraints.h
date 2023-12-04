// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if !COMPILE_WITHOUT_UNREAL_SUPPORT
#include "Chaos/PBDCollisionSpringConstraintsBase.h"
#include "Chaos/CollectionPropertyFacade.h"

namespace Chaos::Softs
{

class FPBDCollisionSpringConstraints : public FPBDCollisionSpringConstraintsBase
{
	typedef FPBDCollisionSpringConstraintsBase Base;
	using Base::Barys;
	using Base::Constraints;

public:
	static constexpr FSolverReal MinFrictionCoefficient = (FSolverReal)0.;
	static constexpr FSolverReal MaxFrictionCoefficient = (FSolverReal)1.;

	static bool IsEnabled(const FCollectionPropertyConstFacade& PropertyCollection)
	{
		return IsSelfCollisionStiffnessEnabled(PropertyCollection, false);
	}

	FPBDCollisionSpringConstraints(
		const int32 InOffset,
		const int32 InNumParticles,
		const FTriangleMesh& InTriangleMesh,
		const TArray<FSolverVec3>* InRestPositions,
		TSet<TVec2<int32>>&& InDisabledCollisionElements,
		const FCollectionPropertyConstFacade& PropertyCollection)
		: Base(
			InOffset,
			InNumParticles,
			InTriangleMesh,
			InRestPositions,
			MoveTemp(InDisabledCollisionElements),
			(FSolverReal)FMath::Max(GetSelfCollisionThickness(PropertyCollection, Base::BackCompatThickness), 0.f),
			(FSolverReal)FMath::Clamp(GetSelfCollisionStiffness(PropertyCollection, Base::BackCompatStiffness), 0.f, 1.f),
			FMath::Clamp((FSolverReal)GetSelfCollisionFriction(PropertyCollection, Base::BackCompatFrictionCoefficient), MinFrictionCoefficient, MaxFrictionCoefficient),
			(FSolverReal)GetSelfCollisionProximityStiffness(PropertyCollection, Base::DefaultProximityStiffness))
		, SelfCollisionThicknessIndex(PropertyCollection)
		, SelfCollisionStiffnessIndex(PropertyCollection)
		, SelfCollisionFrictionIndex(PropertyCollection)
		, SelfCollisionProximityStiffnessIndex(PropertyCollection)
	{}

	FPBDCollisionSpringConstraints(
		const int32 InOffset,
		const int32 InNumParticles,
		const FTriangleMesh& InTriangleMesh,
		const TArray<FSolverVec3>* InRestPositions,
		TSet<TVec2<int32>>&& InDisabledCollisionElements,
		const FSolverReal InThickness = Base::BackCompatThickness,
		const FSolverReal InStiffness = Base::BackCompatStiffness,
		const FSolverReal InFrictionCoefficient = Base::BackCompatFrictionCoefficient)
		: Base(
			InOffset,
			InNumParticles,
			InTriangleMesh,
			InRestPositions,
			MoveTemp(InDisabledCollisionElements),
			InThickness,
			InStiffness,
			InFrictionCoefficient)
		, SelfCollisionThicknessIndex(ForceInit)
		, SelfCollisionStiffnessIndex(ForceInit)
		, SelfCollisionFrictionIndex(ForceInit)
		, SelfCollisionProximityStiffnessIndex(ForceInit)
	{}

	virtual ~FPBDCollisionSpringConstraints() override {}

	using Base::Init;

	void SetProperties(const FCollectionPropertyConstFacade& PropertyCollection)
	{
		if (IsSelfCollisionThicknessMutable(PropertyCollection))
		{
			Thickness = (FSolverReal)FMath::Max(GetSelfCollisionThickness(PropertyCollection), 0.f);
		}
		if (IsSelfCollisionStiffnessMutable(PropertyCollection))
		{
			Stiffness = (FSolverReal)FMath::Clamp(GetSelfCollisionStiffness(PropertyCollection), 0.f, 1.f);
		}
		if (IsSelfCollisionFrictionMutable(PropertyCollection))
		{
			FrictionCoefficient = FMath::Clamp((FSolverReal)GetSelfCollisionFriction(PropertyCollection), MinFrictionCoefficient, MaxFrictionCoefficient);
		}
		if (IsSelfCollisionProximityStiffnessMutable(PropertyCollection))
		{
			ProximityStiffness = GetSelfCollisionProximityStiffness(PropertyCollection);
		}
	}

private:
	using Base::Thickness;
	using Base::Stiffness;
	using Base::FrictionCoefficient;
	using Base::ProximityStiffness;

	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(SelfCollisionThickness, float);
	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(SelfCollisionStiffness, float);
	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(SelfCollisionFriction, float);
	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(SelfCollisionProximityStiffness, float);
};

}  // End namespace Chaos::Softs

#endif
