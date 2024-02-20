// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chaos/VelocityField.h"
#include "HAL/IConsoleManager.h"
#if INTEL_ISPC
#include "VelocityField.ispc.generated.h"

static_assert(sizeof(ispc::FVector3f) == sizeof(Chaos::Softs::FSolverVec3), "sizeof(ispc::FVector3f) != sizeof(Chaos::Softs::FSolverVec3)");
static_assert(sizeof(ispc::FIntVector) == sizeof(Chaos::TVec3<int32>), "sizeof(ispc::FIntVector) != sizeof(Chaos::TVec3<int32>)");
#endif

#if INTEL_ISPC && !UE_BUILD_SHIPPING
bool bChaos_VelocityField_ISPC_Enabled = true;
FAutoConsoleVariableRef CVarChaosVelocityFieldISPCEnabled(TEXT("p.Chaos.VelocityField.ISPC"), bChaos_VelocityField_ISPC_Enabled, TEXT("Whether to use ISPC optimizations in velocity field calculations"));
#endif

namespace Chaos::Softs {

namespace Private
{
	static float VelocityFieldMaxVelocity = 0.f;
	static FAutoConsoleVariableRef CVarChaosVelocityFieldMaxVelocity(TEXT("p.Chaos.VelocityField.MaxVelocity"), VelocityFieldMaxVelocity, TEXT("The maximum relative velocity to process the aerodynamics forces with."));
}

void FVelocityAndPressureField::SetProperties(
	const FCollectionPropertyConstFacade& PropertyCollection,
	const TMap<FString, TConstArrayView<FRealSingle>>& Weightmaps,
	FSolverReal WorldScale,
	bool bEnableAerodynamics)
{
	bool bSetMultipliers = false;

	if (IsDragMutable(PropertyCollection))
	{
		const FSolverVec2 Drag(GetWeightedFloatDrag(PropertyCollection));
		DragBase = FMath::Clamp(Drag[0], MinCoefficient, MaxCoefficient);
		DragRange = FMath::Clamp(Drag[1], MinCoefficient, MaxCoefficient) - DragBase;

		if (IsDragStringDirty(PropertyCollection))
		{
			bSetMultipliers = true;
		}
	}

	if (IsLiftMutable(PropertyCollection))
	{
		const FSolverVec2 Lift(GetWeightedFloatLift(PropertyCollection));
		LiftBase = FMath::Clamp(Lift[0], MinCoefficient, MaxCoefficient);
		LiftRange = FMath::Clamp(Lift[1], MinCoefficient, MaxCoefficient) - LiftBase;

		if (IsLiftStringDirty(PropertyCollection))
		{
			bSetMultipliers = true;
		}
	}

	if (IsFluidDensityMutable(PropertyCollection))
	{
		Rho = (FSolverReal)FMath::Max(GetFluidDensity(PropertyCollection), 0.f) / FMath::Cube(WorldScale);
	}

	if (IsPressureMutable(PropertyCollection))
	{
		const FSolverVec2 Pressure(GetWeightedFloatPressure(PropertyCollection));
		PressureBase = Pressure[0] / WorldScale;
		PressureRange = Pressure[1] / WorldScale - PressureBase;

		if (IsPressureStringDirty(PropertyCollection))
		{
			bSetMultipliers = true;
		}
	}

	if (bSetMultipliers)
	{
		SetMultipliers(PropertyCollection, Weightmaps);
	}

	// Update QuarterRho
	constexpr FSolverReal OneQuarter = (FSolverReal)0.25;
	QuarterRho = bEnableAerodynamics ? Rho * OneQuarter : (FSolverReal)0.;
}

void FVelocityAndPressureField::SetPropertiesAndWind(
	const FCollectionPropertyConstFacade& PropertyCollection,
	const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
	FSolverReal WorldScale,
	bool bEnableAerodynamics,
	const FSolverVec3& SolverWind)
{
	SetProperties(PropertyCollection, WeightMaps, WorldScale, bEnableAerodynamics);
	const FSolverVec3 WindVelocity = WindVelocityIndex != INDEX_NONE ? WorldScale * FSolverVec3(GetWindVelocity(PropertyCollection)) : FSolverVec3(0.f);
	SetVelocity(WindVelocity + SolverWind);
}

void FVelocityAndPressureField::SetProperties(
	const FSolverVec2& Drag,
	const FSolverVec2& Lift,
	const FSolverReal FluidDensity,
	const FSolverVec2& Pressure,
	FSolverReal WorldScale)
{
	DragBase = FMath::Clamp(Drag[0], MinCoefficient, MaxCoefficient);
	DragRange = FMath::Clamp(Drag[1], MinCoefficient, MaxCoefficient) - DragBase;
	LiftBase = FMath::Clamp(Lift[0], MinCoefficient, MaxCoefficient);
	LiftRange = FMath::Clamp(Lift[1], MinCoefficient, MaxCoefficient) - LiftBase;
	Rho = FMath::Max(FluidDensity / FMath::Cube(WorldScale), (FSolverReal)0.);
	PressureBase = Pressure[0] / WorldScale;
	PressureRange = Pressure[1] / WorldScale - PressureBase;

	constexpr FSolverReal OneQuarter = (FSolverReal)0.25;
	QuarterRho = Rho * OneQuarter;
}

void FVelocityAndPressureField::SetGeometry(
	const FTriangleMesh* TriangleMesh,
	const FCollectionPropertyConstFacade& PropertyCollection,
	const TMap<FString, TConstArrayView<FRealSingle>>& Weightmaps,
	FSolverReal WorldScale)
{
	// Reinit indices
	DragIndex = FDragIndex(PropertyCollection);
	LiftIndex = FLiftIndex(PropertyCollection);
	FluidDensityIndex = FFluidDensityIndex(PropertyCollection);
	PressureIndex = FPressureIndex(PropertyCollection);

	// Reset geometry, properties, and weight maps
	SetGeometry(TriangleMesh);
	SetProperties(
		FSolverVec2(GetWeightedFloatDrag(PropertyCollection, 0.f)),  // If these properties don't exist, set their values to 0, not to DefaultCoefficients!
		FSolverVec2(GetWeightedFloatLift(PropertyCollection, 0.f)),
		(FSolverReal)GetFluidDensity(PropertyCollection, 0.f),
		FSolverVec2(GetWeightedFloatPressure(PropertyCollection, 0.f)),  // These getters also initialize the property indices, so keep before SetMultipliers
		WorldScale);
	SetMultipliers(PropertyCollection, Weightmaps);
}

void FVelocityAndPressureField::SetGeometry(
	const FTriangleMesh* TriangleMesh,
	const TConstArrayView<FRealSingle>& DragMultipliers,
	const TConstArrayView<FRealSingle>& LiftMultipliers,
	const TConstArrayView<FRealSingle>& PressureMultipliers)
{
	SetGeometry(TriangleMesh);
	SetMultipliers(DragMultipliers, LiftMultipliers, PressureMultipliers);
}

void FVelocityAndPressureField::SetGeometry(const FTriangleMesh* TriangleMesh)
{
	if (TriangleMesh)
	{
		PointToTriangleMap = TriangleMesh->GetPointToTriangleMap();
		Elements = TriangleMesh->GetElements();
		const TVec2<int32> Range = TriangleMesh->GetVertexRange();
		Offset = Range[0];
		NumParticles = 1 + Range[1] - Offset;
		Forces.SetNumUninitialized(Elements.Num());
	}
	else
	{
		PointToTriangleMap = TArrayView<TArray<int32>>();
		Elements = TArrayView<TVector<int32, 3>>();
		Offset = 0;
		NumParticles = 0;
		Forces.Reset();
	}
}

void FVelocityAndPressureField::SetMultipliers(
	const FCollectionPropertyConstFacade& PropertyCollection,
	const TMap<FString, TConstArrayView<FRealSingle>>& Weightmaps)
{
	const TConstArrayView<FRealSingle> DragMultipliers = (DragIndex != INDEX_NONE) ?
		Weightmaps.FindRef(GetDragString(PropertyCollection)) : TConstArrayView<FRealSingle>();

	const TConstArrayView<FRealSingle> LiftMultipliers = (LiftIndex != INDEX_NONE) ?
		Weightmaps.FindRef(GetLiftString(PropertyCollection)) : TConstArrayView<FRealSingle>();

	const TConstArrayView<FRealSingle> PressureMultipliers = (PressureIndex != INDEX_NONE) ?
		Weightmaps.FindRef(GetPressureString(PropertyCollection)) : TConstArrayView<FRealSingle>();

	SetMultipliers(DragMultipliers, LiftMultipliers, PressureMultipliers);
}

void FVelocityAndPressureField::SetMultipliers(
	const TConstArrayView<FRealSingle>& DragMultipliers,
	const TConstArrayView<FRealSingle>& LiftMultipliers,
	const TConstArrayView<FRealSingle>& PressureMultipliers)
{
	Multipliers.Reset();

	const bool bHasDragMultipliers = DragMultipliers.Num() == NumParticles;
	const bool bHasLiftMultipliers = LiftMultipliers.Num() == NumParticles;
	const bool bHasPressureMultipliers = PressureMultipliers.Num() == NumParticles;

	if (bHasDragMultipliers || bHasLiftMultipliers || bHasPressureMultipliers)
	{
		constexpr FSolverReal OneThird = (FSolverReal)1. / (FSolverReal)3.;

		Multipliers.SetNumUninitialized(Elements.Num());

		for (int32 ElementIndex = 0; ElementIndex < Elements.Num(); ++ElementIndex)
		{
			const TVec3<int32>& Element = Elements[ElementIndex];
			const int32 I0 = Element[0] - Offset;
			const int32 I1 = Element[1] - Offset;
			const int32 I2 = Element[2] - Offset;

			const FSolverReal DragMultiplier = bHasDragMultipliers ? (FSolverReal)(DragMultipliers[I0] + DragMultipliers[I1] + DragMultipliers[I2]) * OneThird : (FSolverReal)0.;
			const FSolverReal LiftMultiplier = bHasLiftMultipliers ? (FSolverReal)(LiftMultipliers[I0] + LiftMultipliers[I1] + LiftMultipliers[I2]) * OneThird : (FSolverReal)0.;
			const FSolverReal PressureMultiplier = bHasPressureMultipliers ? (FSolverReal)(PressureMultipliers[I0] + PressureMultipliers[I1] + PressureMultipliers[I2]) * OneThird : (FSolverReal)0.;

			Multipliers[ElementIndex] = FSolverVec3(DragMultiplier, LiftMultiplier, PressureMultiplier);
		}
	}
}

template<typename SolverParticlesOrRange>
void FVelocityAndPressureField::UpdateForces(const SolverParticlesOrRange& InParticles, const FSolverReal /*Dt*/)
{
	const FSolverReal MaxVelocitySquared = (Private::VelocityFieldMaxVelocity > 0.f) ? FMath::Square((FSolverReal)Private::VelocityFieldMaxVelocity) : TNumericLimits<FSolverReal>::Max();

	if (!Multipliers.Num())
	{
#if INTEL_ISPC
		if (bRealTypeCompatibleWithISPC && bChaos_VelocityField_ISPC_Enabled)
		{
			if (MaxVelocitySquared == TNumericLimits<FSolverReal>::Max())
			{
				ispc::UpdateField(
					(ispc::FVector3f*)Forces.GetData(),
					(const ispc::FIntVector*)Elements.GetData(),
					(const ispc::FVector3f*)InParticles.GetV().GetData(),
					(const ispc::FVector3f*)InParticles.XArray().GetData(),
					(const ispc::FVector3f&)Velocity,
					QuarterRho,
					DragBase,
					LiftBase,
					PressureBase,
					Elements.Num());
			}
			else
			{
				ispc::UpdateFieldAndClampVelocity(
					(ispc::FVector3f*)Forces.GetData(),
					(const ispc::FIntVector*)Elements.GetData(),
					(const ispc::FVector3f*)InParticles.GetV().GetData(),
					(const ispc::FVector3f*)InParticles.XArray().GetData(),
					(const ispc::FVector3f&)Velocity,
					QuarterRho,
					DragBase,
					LiftBase,
					PressureBase,
					Elements.Num(),
					MaxVelocitySquared);
			}
		}
		else
#endif
		{
			if (MaxVelocitySquared == TNumericLimits<FSolverReal>::Max())
			{
				for (int32 ElementIndex = 0; ElementIndex < Elements.Num(); ++ElementIndex)
				{
					UpdateField(InParticles, ElementIndex, Velocity, DragBase, LiftBase, PressureBase);
				}
			}
			else
			{
				for (int32 ElementIndex = 0; ElementIndex < Elements.Num(); ++ElementIndex)
				{
					UpdateField(InParticles, ElementIndex, Velocity, DragBase, LiftBase, PressureBase, MaxVelocitySquared);
				}
			}
		}
	}
	else
	{
#if INTEL_ISPC
		if (bRealTypeCompatibleWithISPC && bChaos_VelocityField_ISPC_Enabled)
		{
			if (MaxVelocitySquared == TNumericLimits<FSolverReal>::Max())
			{
				ispc::UpdateFieldWithWeightMaps(
					(ispc::FVector3f*)Forces.GetData(),
					(const ispc::FIntVector*)Elements.GetData(),
					(const ispc::FVector3f*)InParticles.GetV().GetData(),
					(const ispc::FVector3f*)InParticles.XArray().GetData(),
					(const ispc::FVector3f*)Multipliers.GetData(),
					(const ispc::FVector3f&)Velocity,
					QuarterRho,
					DragBase,
					DragRange,
					LiftBase,
					LiftRange,
					PressureBase,
					PressureRange,
					Elements.Num());
			}
			else
			{
				ispc::UpdateFieldWithWeightMapsAndClampVelocity(
					(ispc::FVector3f*)Forces.GetData(),
					(const ispc::FIntVector*)Elements.GetData(),
					(const ispc::FVector3f*)InParticles.GetV().GetData(),
					(const ispc::FVector3f*)InParticles.XArray().GetData(),
					(const ispc::FVector3f*)Multipliers.GetData(),
					(const ispc::FVector3f&)Velocity,
					QuarterRho,
					DragBase,
					DragRange,
					LiftBase,
					LiftRange,
					PressureBase,
					PressureRange,
					Elements.Num(),
					MaxVelocitySquared);
			}
		}
		else
#endif
		{
			if (MaxVelocitySquared == TNumericLimits<FSolverReal>::Max())
			{
				for (int32 ElementIndex = 0; ElementIndex < Elements.Num(); ++ElementIndex)
				{
					const FSolverVec3& Multiplier = Multipliers[ElementIndex];
					const FSolverReal Cd = DragBase + DragRange * Multiplier[0];
					const FSolverReal Cl = LiftBase + LiftRange * Multiplier[1];
					const FSolverReal Cp = PressureBase + PressureRange * Multiplier[2];

					UpdateField(InParticles, ElementIndex, Velocity, Cd, Cl, Cp);
				}
			}
			else
			{
				for (int32 ElementIndex = 0; ElementIndex < Elements.Num(); ++ElementIndex)
				{
					const FSolverVec3& Multiplier = Multipliers[ElementIndex];
					const FSolverReal Cd = DragBase + DragRange * Multiplier[0];
					const FSolverReal Cl = LiftBase + LiftRange * Multiplier[1];
					const FSolverReal Cp = PressureBase + PressureRange * Multiplier[2];

					UpdateField(InParticles, ElementIndex, Velocity, Cd, Cl, Cp, MaxVelocitySquared);
				}
			}
		}
	}
}
template CHAOS_API void FVelocityAndPressureField::UpdateForces(const FSolverParticles& InParticles, const FSolverReal /*Dt*/);
template CHAOS_API void FVelocityAndPressureField::UpdateForces(const FSolverParticlesRange& InParticles, const FSolverReal /*Dt*/);

}  // End namespace Chaos::Softs
