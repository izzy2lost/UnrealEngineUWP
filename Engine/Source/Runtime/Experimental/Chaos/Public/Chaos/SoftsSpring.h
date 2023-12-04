// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "Chaos/SoftsEvolutionLinearSystem.h"

namespace Chaos::Softs
{

namespace Spring
{

template<typename SolverParticlesOrRange>
FSolverVec3 GetXPBDSpringDelta(const SolverParticlesOrRange& Particles, const FSolverReal Dt,
	const TVec2<int32>& Constraint, const FSolverReal RestLength, FSolverReal& Lambda,
	const FSolverReal StiffnessValue, const FSolverReal MinStiffness, const FSolverReal DampingRatioValue)
{
	const int32 Index1 = Constraint[0];
	const int32 Index2 = Constraint[1];

	if (StiffnessValue < MinStiffness || (Particles.InvM(Index2) == (FSolverReal)0. && Particles.InvM(Index1) == (FSolverReal)0.))
	{
		return FSolverVec3((FSolverReal)0.);
	}

	const FSolverReal CombinedInvMass = Particles.InvM(Index2) + Particles.InvM(Index1);
	const FSolverReal Damping = DampingRatioValue * 2.f * FMath::Sqrt(StiffnessValue / CombinedInvMass) * (RestLength > UE_SMALL_NUMBER ? (FSolverReal)1. / RestLength : (FSolverReal)1.);

	const FSolverVec3& P1 = Particles.P(Index1);
	const FSolverVec3& P2 = Particles.P(Index2);
	FSolverVec3 Direction = P1 - P2;
	const FSolverReal Distance = Direction.SafeNormalize();
	const FSolverReal Offset = Distance - RestLength;

	const FSolverVec3& X1 = Particles.X(Index1);
	const FSolverVec3& X2 = Particles.X(Index2);

	const FSolverVec3 RelativeVelocityTimesDt = P1 - X1 - P2 + X2;

	const FSolverReal AlphaInv = StiffnessValue * Dt * Dt;
	const FSolverReal BetaDt = Damping * Dt;

	const FSolverReal DLambda = (AlphaInv * Offset - Lambda + BetaDt * FSolverVec3::DotProduct(Direction, RelativeVelocityTimesDt)) / ((AlphaInv + BetaDt) * CombinedInvMass + (FSolverReal)1.);
	const FSolverVec3 Delta = DLambda * Direction;
	Lambda += DLambda;

	return Delta;
}

inline void UpdateSpringLinearSystem(const FSolverParticlesRange& Particles, const FSolverReal Dt,
	const TVec2<int32>& Constraint, const FSolverReal RestLength,
	const FSolverReal StiffnessValue, const FSolverReal MinStiffness, const FSolverReal DampingRatioValue,
	FEvolutionLinearSystem& LinearSystem)
{
	const int32 Index1 = Constraint[0];
	const int32 Index2 = Constraint[1];

	if (StiffnessValue < MinStiffness || (Particles.InvM(Index2) == (FSolverReal)0. && Particles.InvM(Index1) == (FSolverReal)0.))
	{
		return;
	}

	const FSolverVec3& P1 = Particles.P(Index1);
	const FSolverVec3& P2 = Particles.P(Index2);
	FSolverVec3 Direction = P1 - P2;
	const FSolverReal Distance = Direction.SafeNormalize();
	if (Distance < UE_SMALL_NUMBER)
	{
		// We can't calculate a direction if distance is zero. Just skip
		return;
	}
	const FSolverReal CombinedInvMass = Particles.InvM(Index2) + Particles.InvM(Index1);

	const FSolverReal Damping = DampingRatioValue * 2.f * FMath::Sqrt(StiffnessValue / CombinedInvMass) * (RestLength > UE_SMALL_NUMBER ? (FSolverReal)1. / RestLength : (FSolverReal)1.);
	const FSolverVec3 RelVel = Particles.V(Index1) - Particles.V(Index2);
	const FSolverReal CDot = FSolverVec3::DotProduct(Direction, RelVel);

	const FSolverReal Offset = Distance - RestLength; // C
	// const FSolverVec3 GradC1 = Direction;
	// const FSolverVec3 GradC2 = -GradC1;
	const FSolverReal Scalar = -(StiffnessValue * Offset + Damping * CDot);
	const FSolverVec3 Force1 = Scalar * Direction;
	// const FSolverVec3 Force2 = -Force1

	const FSolverMatrix33 DirDirT = FSolverMatrix33::OuterProduct(Direction, Direction); // = GradC1 GradC1^T
	const FSolverMatrix33 Hess11 = (FSolverReal)1. / Distance * (FSolverMatrix33::Identity - DirDirT); // = Hess22
	// Hess12 = Hess21 = -Hess11

	// Df1Dx1 = -Stiffness * (DirDirT + Offset * Hess11) - Damping * CDot * Hess11
	const FSolverReal ScalarModified = LinearSystem.RequiresSPDForceDerivatives() ? FMath::Min(Scalar, 0) : Scalar;
	const FSolverMatrix33 Df1Dx1 = -StiffnessValue * DirDirT + ScalarModified * Hess11;
	const FSolverMatrix33 Df1Dx2 = -Df1Dx1;
	// Df2Dx2 = Df1Dx1
	const FSolverMatrix33 Df1Dv1 = -Damping * DirDirT;
	const FSolverMatrix33 Df1Dv2 = -Df1Dv1;
	// Df2Dv2 = Df1Dv1

	if (Particles.InvM(Index1) != (FSolverReal)0.)
	{
		LinearSystem.AddForce(Particles, Force1, Index1, Dt);
		LinearSystem.AddSymmetricForceDerivative(Particles, &Df1Dx1, &Df1Dv1, Index1, Index1, Dt);
		LinearSystem.AddSymmetricForceDerivative(Particles, &Df1Dx2, &Df1Dv2, Index1, Index2, Dt);
		if (Particles.InvM(Index2) != (FSolverReal)0.)
		{
			LinearSystem.AddForce(Particles, -Force1, Index2, Dt);
			LinearSystem.AddSymmetricForceDerivative(Particles, &Df1Dx1, &Df1Dv1, Index2, Index2, Dt);
		}
	}
	else
	{
		check(Particles.InvM(Index2) != (FSolverReal)0.);
		LinearSystem.AddForce(Particles, -Force1, Index2, Dt);
		LinearSystem.AddSymmetricForceDerivative(Particles, &Df1Dx1, &Df1Dv1, Index2, Index2, Dt);
		LinearSystem.AddSymmetricForceDerivative(Particles, &Df1Dx2, &Df1Dv2, Index2, Index1, Dt);
	}
}
}
}
