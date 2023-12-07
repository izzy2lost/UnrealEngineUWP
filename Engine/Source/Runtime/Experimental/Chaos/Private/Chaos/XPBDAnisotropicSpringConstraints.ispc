// Copyright Epic Games, Inc. All Rights Reserved.

#define EXPLICIT_VECTOR4 1

#include "Math/Vector.isph"
#include "Chaos/PBDSofts.isph"
#include "Chaos/SoftsSpring.isph"

export void ApplyXPBDAnisoSpringConstraints(uniform FVector4f PandInvM[],
									const uniform FIntVector2 Constraints[],
									const uniform float Dists[],
									const uniform FVector3f WarpWeftBiasBaseMultiplers[],
									uniform float Lambdas[],
									const uniform float Dt,
									const uniform FVector3f& Stiffness,
									const uniform int32 NumConstraints)
{
	foreach(Index = 0 ... NumConstraints)
	{
		const varying FIntVector2 Constraint = VectorLoad(&Constraints[extract(Index,0)]);
		const varying int32 Index1 = Constraint.V[0];
		const varying int32 Index2 = Constraint.V[1];

		const varying FVector4f PandInvM1 = VectorGather(&PandInvM[Index1]);
		const varying FVector4f PandInvM2 = VectorGather(&PandInvM[Index2]);

		varying FVector3f P1, P2;
		varying float InvM1, InvM2;
		UnzipPandInvM(PandInvM1, P1, InvM1);
		UnzipPandInvM(PandInvM2, P2, InvM2);
		
		const varying float Dist = Dists[Index];
		const varying float Lambda = Lambdas[Index];
		const varying FVector3f WarpWeftBiasBaseMultiplier = VectorGather(&WarpWeftBiasBaseMultiplers[Index]);
		const varying float StiffnessValue = VectorDot(WarpWeftBiasBaseMultiplier, Stiffness);

		varying float DLambda;
		const varying FVector3f Delta = GetXPBDSpringDelta(P1, P2, InvM1, InvM2, Dt, Dist, Lambda, StiffnessValue, FLOAT_SMALL_NUMBER, DLambda);

		Lambdas[Index] = Lambda + DLambda;

		if (InvM1 > FLOAT_ZERO)
		{
			VectorScatter(&PandInvM[Index1], SetVector4( P1 - (InvM1 * Delta), InvM1 ));
		}
		if (InvM2 > FLOAT_ZERO)
		{
			VectorScatter(&PandInvM[Index2], SetVector4( P2 + (InvM2 * Delta), InvM2 ));
		}
	}
}

export void ApplyXPBDAnisoSpringConstraintsWithDamping(uniform FVector4f PandInvM[],
												const uniform FVector3f X[],
												const uniform FIntVector2 Constraints[],
												const uniform float Dists[],
												const uniform FVector3f WarpWeftBiasBaseMultiplers[],
												uniform float Lambdas[],
												const uniform float Dt,
												const uniform FVector3f& Stiffness,
												const uniform float DampingRatio,
												const uniform int32 NumConstraints)
{
	foreach(Index = 0 ... NumConstraints)
	{
		const varying FIntVector2 Constraint = VectorLoad(&Constraints[extract(Index,0)]);
		const varying int32 Index1 = Constraint.V[0];
		const varying int32 Index2 = Constraint.V[1];

		const varying FVector4f PandInvM1 = VectorGather(&PandInvM[Index1]);
		const varying FVector4f PandInvM2 = VectorGather(&PandInvM[Index2]);

		varying FVector3f P1, P2;
		varying float InvM1, InvM2;
		UnzipPandInvM(PandInvM1, P1, InvM1);
		UnzipPandInvM(PandInvM2, P2, InvM2);

		const varying FVector3f X1 = VectorGather(&X[Index1]);
		const varying FVector3f X2 = VectorGather(&X[Index2]);

		const varying float Dist = Dists[Index];
		const varying float Lambda = Lambdas[Index];
		const varying FVector3f WarpWeftBiasBaseMultiplier = VectorGather(&WarpWeftBiasBaseMultiplers[Index]);
		const varying float StiffnessValue = VectorDot(WarpWeftBiasBaseMultiplier, Stiffness);

		varying float DLambda;
		const FVector3f Delta = GetXPBDSpringDeltaWithDamping(P1, P2, X1, X2, InvM1, InvM2, Dt, Dist, Lambda, StiffnessValue, DampingRatio, FLOAT_SMALL_NUMBER, DLambda);

		Lambdas[Index] = Lambda + DLambda;

		if (InvM1 > FLOAT_ZERO)
		{
			VectorScatter(&PandInvM[Index1], SetVector4( P1 - (InvM1 * Delta), InvM1 ));
		}
		if (InvM2 > FLOAT_ZERO)
		{
			VectorScatter(&PandInvM[Index2], SetVector4( P2 + (InvM2 * Delta), InvM2 ));
		}
	}
}

export void ApplyXPBDAnisoSpringConstraintsWithWeightMaps(uniform FVector4f PandInvM[],
									const uniform FIntVector2 Constraints[],
									const uniform float Dists[],
									const uniform FVector3f WarpWeftBiasBaseMultiplers[],
									uniform float Lambdas[],
									const uniform float Dt,
									const uniform float MinStiffness,
									const uniform bool StiffnessHasMap,
									const uniform uint8 StiffnessIndices[],
									const uniform float StiffnessTable[],
									const uniform bool StiffnessWeftHasMap,
									const uniform uint8 StiffnessWeftIndices[],
									const uniform float StiffnessWeftTable[],
									const uniform bool StiffnessBiasHasMap,
									const uniform uint8 StiffnessBiasIndices[],
									const uniform float StiffnessBiasTable[],
									const uniform int32 NumConstraints)
{

	foreach(Index = 0 ... NumConstraints)
	{
		const varying FIntVector2 Constraint = VectorLoad(&Constraints[extract(Index,0)]);
		const varying int32 Index1 = Constraint.V[0];
		const varying int32 Index2 = Constraint.V[1];
		
		const varying int8 StiffnessIndex = StiffnessHasMap ? StiffnessIndices[Index] : 0;
		const varying int8 StiffnessWeftIndex = StiffnessWeftHasMap ? StiffnessWeftIndices[Index] : 0;
		const varying int8 StiffnessBiasIndex = StiffnessBiasHasMap ? StiffnessBiasIndices[Index] : 0;

		const varying FVector4f PandInvM1 = VectorGather(&PandInvM[Index1]);
		const varying FVector4f PandInvM2 = VectorGather(&PandInvM[Index2]);

		varying FVector3f P1, P2;
		varying float InvM1, InvM2;
		UnzipPandInvM(PandInvM1, P1, InvM1);
		UnzipPandInvM(PandInvM2, P2, InvM2);

		const varying float Dist = Dists[Index];
		const varying float Lambda = Lambdas[Index];
		
		#pragma ignore warning(perf)
		const varying float Stiffness = StiffnessTable[StiffnessIndex];
		#pragma ignore warning(perf)
		const varying float StiffnessWeft = StiffnessWeftTable[StiffnessWeftIndex];
		#pragma ignore warning(perf)
		const varying float StiffnessBias = StiffnessBiasTable[StiffnessBiasIndex];
		
		const varying FVector3f StiffnessVector = {{StiffnessWeft, Stiffness, StiffnessBias}};
		const varying FVector3f WarpWeftBiasBaseMultiplier = VectorGather(&WarpWeftBiasBaseMultiplers[Index]);
		const varying float StiffnessValue = VectorDot(WarpWeftBiasBaseMultiplier, StiffnessVector);

		varying float DLambda;
		const varying FVector3f Delta = GetXPBDSpringDelta(P1, P2, InvM1, InvM2, Dt, Dist, Lambda, StiffnessValue, MinStiffness, DLambda);

		Lambdas[Index] = Lambda + DLambda;

		if (InvM1 > FLOAT_ZERO)
		{
			VectorScatter(&PandInvM[Index1], SetVector4( P1 - (InvM1 * Delta), InvM1 ));
		}
		if (InvM2 > FLOAT_ZERO)
		{
			VectorScatter(&PandInvM[Index2], SetVector4( P2 + (InvM2 * Delta), InvM2 ));
		}
	}
}

export void ApplyXPBDAnisoSpringConstraintsWithDampingAndWeightMaps(uniform FVector4f PandInvM[],
															const uniform FVector3f X[],
															const uniform FIntVector2 Constraints[],
															const uniform float Dists[],
															const uniform FVector3f WarpWeftBiasBaseMultiplers[],
															uniform float Lambdas[],
															const uniform float Dt,
															const uniform float MinStiffness,
															const uniform bool StiffnessHasMap,
															const uniform uint8 StiffnessIndices[],
															const uniform float StiffnessTable[],
															const uniform bool StiffnessWeftHasMap,
															const uniform uint8 StiffnessWeftIndices[],
															const uniform float StiffnessWeftTable[],
															const uniform bool StiffnessBiasHasMap,
															const uniform uint8 StiffnessBiasIndices[],
															const uniform float StiffnessBiasTable[],
															const uniform bool DampingHasMap,
															const uniform uint8 DampingIndices[],
															const uniform float DampingTable[],
															const uniform int32 NumConstraints)
{

	foreach(Index = 0 ... NumConstraints)
	{
		const varying FIntVector2 Constraint = VectorLoad(&Constraints[extract(Index,0)]);
		const varying int32 Index1 = Constraint.V[0];
		const varying int32 Index2 = Constraint.V[1];
		const varying int8 StiffnessIndex = StiffnessHasMap ? StiffnessIndices[Index] : 0;
		const varying int8 StiffnessWeftIndex = StiffnessWeftHasMap ? StiffnessWeftIndices[Index] : 0;
		const varying int8 StiffnessBiasIndex = StiffnessBiasHasMap ? StiffnessBiasIndices[Index] : 0;
		const varying int8 DampingIndex = DampingHasMap ? DampingIndices[Index] : 0;

		const varying FVector4f PandInvM1 = VectorGather(&PandInvM[Index1]);
		const varying FVector4f PandInvM2 = VectorGather(&PandInvM[Index2]);

		varying FVector3f P1, P2;
		varying float InvM1, InvM2;
		UnzipPandInvM(PandInvM1, P1, InvM1);
		UnzipPandInvM(PandInvM2, P2, InvM2);

		const varying FVector3f X1 = VectorGather(&X[Index1]);
		const varying FVector3f X2 = VectorGather(&X[Index2]);

		const varying float Dist = Dists[Index];
		const varying float Lambda = Lambdas[Index];

		#pragma ignore warning(perf)
		const varying float Stiffness = StiffnessTable[StiffnessIndex];
		#pragma ignore warning(perf)
		const varying float StiffnessWeft = StiffnessWeftTable[StiffnessWeftIndex];
		#pragma ignore warning(perf)
		const varying float StiffnessBias = StiffnessBiasTable[StiffnessBiasIndex];
		#pragma ignore warning(perf)
		const varying float DampingRatio = DampingTable[DampingIndex];

		const varying FVector3f StiffnessVector = {{StiffnessWeft, Stiffness, StiffnessBias}};
		const varying FVector3f WarpWeftBiasBaseMultiplier = VectorGather(&WarpWeftBiasBaseMultiplers[Index]);
		const varying float StiffnessValue = VectorDot(WarpWeftBiasBaseMultiplier, StiffnessVector);

		varying float DLambda;
		const FVector3f Delta = GetXPBDSpringDeltaWithDamping(P1, P2, X1, X2, InvM1, InvM2, Dt, Dist, Lambda, StiffnessValue, DampingRatio, MinStiffness, DLambda);

		Lambdas[Index] = Lambda + DLambda;

		if (InvM1 > FLOAT_ZERO)
		{
			VectorScatter(&PandInvM[Index1], SetVector4( P1 - (InvM1 * Delta), InvM1 ));
		}
		if (InvM2 > FLOAT_ZERO)
		{
			VectorScatter(&PandInvM[Index2], SetVector4( P2 + (InvM2 * Delta), InvM2 ));
		}
	}
}

export void ApplyXPBDAnisoAxialSpringConstraints(uniform FVector4f PandInvM[],
									const uniform FIntVector Constraints[],
									const uniform float Barys[],
									const uniform float Dists[],
									const uniform FVector3f WarpWeftBiasBaseMultiplers[],
									uniform float Lambdas[],
									const uniform float Dt,
									const uniform FVector3f& Stiffness,
									const uniform int32 NumConstraints)
{
	foreach(Index = 0 ... NumConstraints)
	{
		const varying FIntVector Constraint = VectorLoad(&Constraints[extract(Index,0)]);
		const varying int32 Index1 = Constraint.V[0];
		const varying int32 Index2 = Constraint.V[1];
		const varying int32 Index3 = Constraint.V[2];

		const varying FVector4f PandInvM1 = VectorGather(&PandInvM[Index1]);
		const varying FVector4f PandInvM2 = VectorGather(&PandInvM[Index2]);
		const varying FVector4f PandInvM3 = VectorGather(&PandInvM[Index3]);

		varying FVector3f P1, P2, P3;
		varying float InvM1, InvM2, InvM3;
		UnzipPandInvM(PandInvM1, P1, InvM1);
		UnzipPandInvM(PandInvM2, P2, InvM2);
		UnzipPandInvM(PandInvM3, P3, InvM3);
		
		const varying float Bary = Barys[Index];
		const varying float Dist = Dists[Index];
		const varying float Lambda = Lambdas[Index];
		const varying FVector3f WarpWeftBiasBaseMultiplier = VectorGather(&WarpWeftBiasBaseMultiplers[Index]);
		const varying float StiffnessValue = VectorDot(WarpWeftBiasBaseMultiplier, Stiffness);

		varying float DLambda;
		const varying FVector3f Delta = GetXPBDAxialSpringDelta(P1, P2, P3, InvM1, InvM2, InvM3, Dt, Bary, Dist, Lambda, StiffnessValue, FLOAT_SMALL_NUMBER, DLambda);

		Lambdas[Index] = Lambda + DLambda;

		if (InvM1 > FLOAT_ZERO)
		{
			VectorScatter(&PandInvM[Index1], SetVector4( P1 - (InvM1 * Delta), InvM1 ));
		}
		if (InvM2 > FLOAT_ZERO)
		{
			VectorScatter(&PandInvM[Index2], SetVector4( P2 + (InvM2 * Bary * Delta), InvM2 ));
		}
		if (InvM3 > FLOAT_ZERO)
		{
			VectorScatter(&PandInvM[Index3], SetVector4( P3 + (InvM3 * (1.f - Bary) * Delta), InvM3 ));
		}
	}
}

export void ApplyXPBDAnisoAxialSpringConstraintsWithDamping(uniform FVector4f PandInvM[],
												const uniform FVector3f X[],
												const uniform FIntVector Constraints[],
												const uniform float Barys[],
												const uniform float Dists[],
												const uniform FVector3f WarpWeftBiasBaseMultiplers[],
												uniform float Lambdas[],
												const uniform float Dt,
												const uniform FVector3f& Stiffness,
												const uniform float DampingRatio,
												const uniform int32 NumConstraints)
{
	foreach(Index = 0 ... NumConstraints)
	{
		const varying FIntVector Constraint = VectorLoad(&Constraints[extract(Index,0)]);
		const varying int32 Index1 = Constraint.V[0];
		const varying int32 Index2 = Constraint.V[1];
		const varying int32 Index3 = Constraint.V[2];
		
		const varying FVector4f PandInvM1 = VectorGather(&PandInvM[Index1]);
		const varying FVector4f PandInvM2 = VectorGather(&PandInvM[Index2]);
		const varying FVector4f PandInvM3 = VectorGather(&PandInvM[Index3]);

		varying FVector3f P1, P2, P3;
		varying float InvM1, InvM2, InvM3;
		UnzipPandInvM(PandInvM1, P1, InvM1);
		UnzipPandInvM(PandInvM2, P2, InvM2);
		UnzipPandInvM(PandInvM3, P3, InvM3);

		const varying FVector3f X1 = VectorGather(&X[Index1]);
		const varying FVector3f X2 = VectorGather(&X[Index2]);
		const varying FVector3f X3 = VectorGather(&X[Index3]);
		
		const varying float Bary = Barys[Index];
		const varying float Dist = Dists[Index];
		const varying float Lambda = Lambdas[Index];
		const varying FVector3f WarpWeftBiasBaseMultiplier = VectorGather(&WarpWeftBiasBaseMultiplers[Index]);
		const varying float StiffnessValue = VectorDot(WarpWeftBiasBaseMultiplier, Stiffness);

		varying float DLambda;
		const FVector3f Delta = GetXPBDAxialSpringDeltaWithDamping(P1, P2, P3, X1, X2, X3, InvM1, InvM2, InvM3, Dt, Bary, Dist, Lambda, StiffnessValue, DampingRatio, FLOAT_SMALL_NUMBER, DLambda);

		Lambdas[Index] = Lambda + DLambda;

		if (InvM1 > FLOAT_ZERO)
		{
			VectorScatter(&PandInvM[Index1], SetVector4( P1 - (InvM1 * Delta), InvM1 ));
		}
		if (InvM2 > FLOAT_ZERO)
		{
			VectorScatter(&PandInvM[Index2], SetVector4( P2 + (InvM2 * Bary * Delta), InvM2 ));
		}
		if (InvM3 > FLOAT_ZERO)
		{
			VectorScatter(&PandInvM[Index3], SetVector4( P3 + (InvM3 * (1.f - Bary) * Delta), InvM3 ));
		}
	}
}

export void ApplyXPBDAnisoAxialSpringConstraintsWithWeightMaps(uniform FVector4f PandInvM[],
									const uniform FIntVector Constraints[],
									const uniform float Barys[],
									const uniform float Dists[],
									const uniform FVector3f WarpWeftBiasBaseMultiplers[],
									uniform float Lambdas[],
									const uniform float Dt,
									const uniform float MinStiffness,
									const uniform bool StiffnessHasMap,
									const uniform uint8 StiffnessIndices[],
									const uniform float StiffnessTable[],
									const uniform bool StiffnessWeftHasMap,
									const uniform uint8 StiffnessWeftIndices[],
									const uniform float StiffnessWeftTable[],
									const uniform bool StiffnessBiasHasMap,
									const uniform uint8 StiffnessBiasIndices[],
									const uniform float StiffnessBiasTable[],
									const uniform int32 NumConstraints)
{

	foreach(Index = 0 ... NumConstraints)
	{
		const varying FIntVector Constraint = VectorLoad(&Constraints[extract(Index,0)]);
		const varying int32 Index1 = Constraint.V[0];
		const varying int32 Index2 = Constraint.V[1];
		const varying int32 Index3 = Constraint.V[2];
		
		const varying int8 StiffnessIndex = StiffnessHasMap ? StiffnessIndices[Index] : 0;
		const varying int8 StiffnessWeftIndex = StiffnessWeftHasMap ? StiffnessWeftIndices[Index] : 0;
		const varying int8 StiffnessBiasIndex = StiffnessBiasHasMap ? StiffnessBiasIndices[Index] : 0;
		
		const varying FVector4f PandInvM1 = VectorGather(&PandInvM[Index1]);
		const varying FVector4f PandInvM2 = VectorGather(&PandInvM[Index2]);
		const varying FVector4f PandInvM3 = VectorGather(&PandInvM[Index3]);

		varying FVector3f P1, P2, P3;
		varying float InvM1, InvM2, InvM3;
		UnzipPandInvM(PandInvM1, P1, InvM1);
		UnzipPandInvM(PandInvM2, P2, InvM2);
		UnzipPandInvM(PandInvM3, P3, InvM3);
		
		const varying float Bary = Barys[Index];
		const varying float Dist = Dists[Index];
		const varying float Lambda = Lambdas[Index];
		
		#pragma ignore warning(perf)
		const varying float Stiffness = StiffnessTable[StiffnessIndex];
		#pragma ignore warning(perf)
		const varying float StiffnessWeft = StiffnessWeftTable[StiffnessWeftIndex];
		#pragma ignore warning(perf)
		const varying float StiffnessBias = StiffnessBiasTable[StiffnessBiasIndex];
		
		const varying FVector3f StiffnessVector = {{StiffnessWeft, Stiffness, StiffnessBias}};
		const varying FVector3f WarpWeftBiasBaseMultiplier = VectorGather(&WarpWeftBiasBaseMultiplers[Index]);
		const varying float StiffnessValue = VectorDot(WarpWeftBiasBaseMultiplier, StiffnessVector);

		varying float DLambda;
		const varying FVector3f Delta = GetXPBDAxialSpringDelta(P1, P2, P3, InvM1, InvM2, InvM3, Dt, Bary, Dist, Lambda, StiffnessValue, FLOAT_SMALL_NUMBER, DLambda);

		Lambdas[Index] = Lambda + DLambda;

		if (InvM1 > FLOAT_ZERO)
		{
			VectorScatter(&PandInvM[Index1], SetVector4( P1 - (InvM1 * Delta), InvM1 ));
		}
		if (InvM2 > FLOAT_ZERO)
		{
			VectorScatter(&PandInvM[Index2], SetVector4( P2 + (InvM2 * Bary * Delta), InvM2 ));
		}
		if (InvM3 > FLOAT_ZERO)
		{
			VectorScatter(&PandInvM[Index3], SetVector4( P3 + (InvM3 * (1.f - Bary) * Delta), InvM3 ));
		}
	}
}

export void ApplyXPBDAnisoAxialSpringConstraintsWithDampingAndWeightMaps(uniform FVector4f PandInvM[],
															const uniform FVector3f X[],
															const uniform FIntVector Constraints[],
															const uniform float Barys[],
															const uniform float Dists[],
															const uniform FVector3f WarpWeftBiasBaseMultiplers[],
															uniform float Lambdas[],
															const uniform float Dt,
															const uniform float MinStiffness,
															const uniform bool StiffnessHasMap,
															const uniform uint8 StiffnessIndices[],
															const uniform float StiffnessTable[],
															const uniform bool StiffnessWeftHasMap,
															const uniform uint8 StiffnessWeftIndices[],
															const uniform float StiffnessWeftTable[],
															const uniform bool StiffnessBiasHasMap,
															const uniform uint8 StiffnessBiasIndices[],
															const uniform float StiffnessBiasTable[],
															const uniform bool DampingHasMap,
															const uniform uint8 DampingIndices[],
															const uniform float DampingTable[],
															const uniform int32 NumConstraints)
{

	foreach(Index = 0 ... NumConstraints)
	{
		const varying FIntVector Constraint = VectorLoad(&Constraints[extract(Index,0)]);
		const varying int32 Index1 = Constraint.V[0];
		const varying int32 Index2 = Constraint.V[1];
		const varying int32 Index3 = Constraint.V[2];

		const varying int8 StiffnessIndex = StiffnessHasMap ? StiffnessIndices[Index] : 0;
		const varying int8 StiffnessWeftIndex = StiffnessWeftHasMap ? StiffnessWeftIndices[Index] : 0;
		const varying int8 StiffnessBiasIndex = StiffnessBiasHasMap ? StiffnessBiasIndices[Index] : 0;
		const varying int8 DampingIndex = DampingHasMap ? DampingIndices[Index] : 0;
		
		const varying FVector4f PandInvM1 = VectorGather(&PandInvM[Index1]);
		const varying FVector4f PandInvM2 = VectorGather(&PandInvM[Index2]);
		const varying FVector4f PandInvM3 = VectorGather(&PandInvM[Index3]);

		varying FVector3f P1, P2, P3;
		varying float InvM1, InvM2, InvM3;
		UnzipPandInvM(PandInvM1, P1, InvM1);
		UnzipPandInvM(PandInvM2, P2, InvM2);
		UnzipPandInvM(PandInvM3, P3, InvM3);
		
		const varying FVector3f X1 = VectorGather(&X[Index1]);
		const varying FVector3f X2 = VectorGather(&X[Index2]);
		const varying FVector3f X3 = VectorGather(&X[Index3]);
		
		const varying float Bary = Barys[Index];
		const varying float Dist = Dists[Index];
		const varying float Lambda = Lambdas[Index];

		#pragma ignore warning(perf)
		const varying float Stiffness = StiffnessTable[StiffnessIndex];
		#pragma ignore warning(perf)
		const varying float StiffnessWeft = StiffnessWeftTable[StiffnessWeftIndex];
		#pragma ignore warning(perf)
		const varying float StiffnessBias = StiffnessBiasTable[StiffnessBiasIndex];
		#pragma ignore warning(perf)
		const varying float DampingRatio = DampingTable[DampingIndex];

		const varying FVector3f StiffnessVector = {{StiffnessWeft, Stiffness, StiffnessBias}};
		const varying FVector3f WarpWeftBiasBaseMultiplier = VectorGather(&WarpWeftBiasBaseMultiplers[Index]);
		const varying float StiffnessValue = VectorDot(WarpWeftBiasBaseMultiplier, StiffnessVector);

		varying float DLambda;
		const FVector3f Delta = GetXPBDAxialSpringDeltaWithDamping(P1, P2, P3, X1, X2, X3, InvM1, InvM2, InvM3, Dt, Bary, Dist, Lambda, StiffnessValue, DampingRatio, FLOAT_SMALL_NUMBER, DLambda);

		Lambdas[Index] = Lambda + DLambda;

		if (InvM1 > FLOAT_ZERO)
		{
			VectorScatter(&PandInvM[Index1], SetVector4( P1 - (InvM1 * Delta), InvM1 ));
		}
		if (InvM2 > FLOAT_ZERO)
		{
			VectorScatter(&PandInvM[Index2], SetVector4( P2 + (InvM2 * Bary * Delta), InvM2 ));
		}
		if (InvM3 > FLOAT_ZERO)
		{
			VectorScatter(&PandInvM[Index3], SetVector4( P3 + (InvM3 * (1.f - Bary) * Delta), InvM3 ));
		}
	}
}
