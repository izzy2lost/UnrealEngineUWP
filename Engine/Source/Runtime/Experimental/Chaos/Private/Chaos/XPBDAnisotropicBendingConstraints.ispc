// Copyright Epic Games, Inc. All Rights Reserved.

#define EXPLICIT_VECTOR4 1

#include "Math/Vector.isph"
#include "Chaos/PBDSofts.isph"
#include "Chaos/XPBDBendingElement.isph"

export void ApplyXPBDAnisotropicBendingConstraints(uniform FVector4f PandInvM[],
									const uniform FIntVector4 Constraints[],
									const uniform float RestAngles[],
									const uniform bool IsBuckled[],
									const uniform FVector3f WarpWeftBiasBaseMultiplers[],
									uniform float Lambdas[],
									const uniform float Dt,
									const uniform float MinStiffness,
									const uniform FVector3f& Stiffness,
									const uniform FVector3f& BucklingStiffness,
									const uniform int32 NumConstraints)
{

	foreach(i = 0 ... NumConstraints)
	{
		const varying FIntVector4 Constraint = VectorLoad(&Constraints[extract(i,0)]);

		const varying int32 i1 = Constraint.V[0];
		const varying int32 i2 = Constraint.V[1];
		const varying int32 i3 = Constraint.V[2];
		const varying int32 i4 = Constraint.V[3];
		
		const varying FVector4f PandInvM1 = VectorGather(&PandInvM[i1]);
		const varying FVector4f PandInvM2 = VectorGather(&PandInvM[i2]);
		const varying FVector4f PandInvM3 = VectorGather(&PandInvM[i3]);
		const varying FVector4f PandInvM4 = VectorGather(&PandInvM[i4]);

		varying FVector3f P1, P2, P3, P4;
		varying float M1, M2, M3, M4;

		UnzipPandInvM(PandInvM1, P1, M1);
		UnzipPandInvM(PandInvM2, P2, M2);
		UnzipPandInvM(PandInvM3, P3, M3);
		UnzipPandInvM(PandInvM4, P4, M4);

		const varying FVector3f WarpWeftBiasBaseMultiplier = VectorGather(&WarpWeftBiasBaseMultiplers[i]);
		const varying float StiffnessValue = select(IsBuckled[i], VectorDot(WarpWeftBiasBaseMultiplier, BucklingStiffness), VectorDot(WarpWeftBiasBaseMultiplier, Stiffness));
		
		const varying float Lambda = Lambdas[i];

		varying FVector3f Delta1, Delta2, Delta3, Delta4;
		const varying float DLambda = GetXPBDBendingElementDeltas(P1, P2, P3, P4, M1, M2, M3, M4, Dt, RestAngles[i], Lambda, StiffnessValue, MinStiffness, Delta1, Delta2, Delta3, Delta4);

		Lambdas[i] = Lambda + DLambda;

		if (M1 > 0)
		{
			VectorScatter(&PandInvM[i1], SetVector4(P1 + Delta1, M1));
		}
		if (M2 > 0)
		{
			VectorScatter(&PandInvM[i2], SetVector4(P2 + Delta2, M2));
		}
		if (M3 > 0)
		{
			VectorScatter(&PandInvM[i3], SetVector4(P3 + Delta3, M3));
		}
		if (M4 > 0)
		{
			VectorScatter(&PandInvM[i4], SetVector4(P4 + Delta4, M4));
		}
	}
}

export void ApplyXPBDAnisotropicBendingConstraintsWithDamping(uniform FVector4f PandInvM[],
									const uniform FVector3f X[],
									const uniform FIntVector4 Constraints[],
									const uniform float RestAngles[],
									const uniform bool IsBuckled[],
									const uniform FVector3f WarpWeftBiasBaseMultiplers[],
									uniform float Lambdas[],
									const uniform float Dt,
									const uniform float MinStiffness,
									const uniform FVector3f& Stiffness,
									const uniform FVector3f& BucklingStiffness,
									const uniform float DampingRatio,
									const uniform int32 NumConstraints)
{

	foreach(i = 0 ... NumConstraints)
	{
		const varying FIntVector4 Constraint = VectorLoad(&Constraints[extract(i,0)]);

		const varying int32 i1 = Constraint.V[0];
		const varying int32 i2 = Constraint.V[1];
		const varying int32 i3 = Constraint.V[2];
		const varying int32 i4 = Constraint.V[3];
		
		const varying FVector4f PandInvM1 = VectorGather(&PandInvM[i1]);
		const varying FVector4f PandInvM2 = VectorGather(&PandInvM[i2]);
		const varying FVector4f PandInvM3 = VectorGather(&PandInvM[i3]);
		const varying FVector4f PandInvM4 = VectorGather(&PandInvM[i4]);

		varying FVector3f P1, P2, P3, P4;
		varying float M1, M2, M3, M4;

		UnzipPandInvM(PandInvM1, P1, M1);
		UnzipPandInvM(PandInvM2, P2, M2);
		UnzipPandInvM(PandInvM3, P3, M3);
		UnzipPandInvM(PandInvM4, P4, M4);

		const varying FVector3f X1 = VectorGather(&X[i1]);
		const varying FVector3f X2 = VectorGather(&X[i2]);
		const varying FVector3f X3 = VectorGather(&X[i3]);
		const varying FVector3f X4 = VectorGather(&X[i4]);

		const varying FVector3f WarpWeftBiasBaseMultiplier = VectorGather(&WarpWeftBiasBaseMultiplers[i]);
		const varying float StiffnessValue = select(IsBuckled[i], VectorDot(WarpWeftBiasBaseMultiplier, BucklingStiffness), VectorDot(WarpWeftBiasBaseMultiplier, Stiffness));
		const varying float Lambda = Lambdas[i];

		varying FVector3f Delta1, Delta2, Delta3, Delta4;
		const varying float DLambda = GetXPBDBendingElementDeltasWithDamping(P1, P2, P3, P4, X1, X2, X3, X4, M1, M2, M3, M4, Dt, RestAngles[i], Lambda, StiffnessValue, DampingRatio, MinStiffness, Delta1, Delta2, Delta3, Delta4);

		Lambdas[i] = Lambda + DLambda;

		if (M1 > 0)
		{
			VectorScatter(&PandInvM[i1], SetVector4(P1 + Delta1, M1));
		}
		if (M2 > 0)
		{
			VectorScatter(&PandInvM[i2], SetVector4(P2 + Delta2, M2));
		}
		if (M3 > 0)
		{
			VectorScatter(&PandInvM[i3], SetVector4(P3 + Delta3, M3));
		}
		if (M4 > 0)
		{
			VectorScatter(&PandInvM[i4], SetVector4(P4 + Delta4, M4));
		}
	}
}

export void ApplyXPBDAnisotropicBendingDampingConstraints(uniform FVector4f PandInvM[],
									const uniform FVector3f X[],
									const uniform FIntVector4 Constraints[],
									const uniform float RestAngles[],
									const uniform bool IsBuckled[],
									const uniform FVector3f WarpWeftBiasBaseMultiplers[],
									uniform float Lambdas[],
									const uniform float Dt,
									const uniform float MinStiffness,
									const uniform FVector3f& Stiffness,
									const uniform FVector3f& BucklingStiffness,
									const uniform float DampingRatio,
									const uniform int32 NumConstraints)
{

	foreach(i = 0 ... NumConstraints)
	{
		const varying FIntVector4 Constraint = VectorLoad(&Constraints[extract(i,0)]);

		const varying int32 i1 = Constraint.V[0];
		const varying int32 i2 = Constraint.V[1];
		const varying int32 i3 = Constraint.V[2];
		const varying int32 i4 = Constraint.V[3];
		
		const varying FVector4f PandInvM1 = VectorGather(&PandInvM[i1]);
		const varying FVector4f PandInvM2 = VectorGather(&PandInvM[i2]);
		const varying FVector4f PandInvM3 = VectorGather(&PandInvM[i3]);
		const varying FVector4f PandInvM4 = VectorGather(&PandInvM[i4]);

		varying FVector3f P1, P2, P3, P4;
		varying float M1, M2, M3, M4;

		UnzipPandInvM(PandInvM1, P1, M1);
		UnzipPandInvM(PandInvM2, P2, M2);
		UnzipPandInvM(PandInvM3, P3, M3);
		UnzipPandInvM(PandInvM4, P4, M4);

		const varying FVector3f X1 = VectorGather(&X[i1]);
		const varying FVector3f X2 = VectorGather(&X[i2]);
		const varying FVector3f X3 = VectorGather(&X[i3]);
		const varying FVector3f X4 = VectorGather(&X[i4]);

		const varying FVector3f WarpWeftBiasBaseMultiplier = VectorGather(&WarpWeftBiasBaseMultiplers[i]);
		const varying float StiffnessValue = select(IsBuckled[i], VectorDot(WarpWeftBiasBaseMultiplier, BucklingStiffness), VectorDot(WarpWeftBiasBaseMultiplier, Stiffness));
		const varying float Lambda = Lambdas[i];

		varying FVector3f Delta1, Delta2, Delta3, Delta4;
		const varying float DLambda = GetXPBDBendingElementDampingDeltas(P1, P2, P3, P4, X1, X2, X3, X4, M1, M2, M3, M4, Dt, RestAngles[i], Lambda, StiffnessValue, DampingRatio, MinStiffness, Delta1, Delta2, Delta3, Delta4);

		Lambdas[i] = Lambda + DLambda;

		if (M1 > 0)
		{
			VectorScatter(&PandInvM[i1], SetVector4(P1 + Delta1, M1));
		}
		if (M2 > 0)
		{
			VectorScatter(&PandInvM[i2], SetVector4(P2 + Delta2, M2));
		}
		if (M3 > 0)
		{
			VectorScatter(&PandInvM[i3], SetVector4(P3 + Delta3, M3));
		}
		if (M4 > 0)
		{
			VectorScatter(&PandInvM[i4], SetVector4(P4 + Delta4, M4));
		}
	}
}

export void ApplyXPBDAnisotropicBendingConstraintsWithMaps(uniform FVector4f PandInvM[],
											const uniform FIntVector4 Constraints[],
											const uniform float RestAngles[],
											const uniform bool IsBuckled[],
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
											const uniform bool BucklingStiffnessHasMap,
											const uniform uint8 BucklingStiffnessIndices[],
											const uniform float BucklingStiffnessTable[],
											const uniform bool BucklingStiffnessWeftHasMap,
											const uniform uint8 BucklingStiffnessWeftIndices[],
											const uniform float BucklingStiffnessWeftTable[],
											const uniform bool BucklingStiffnessBiasHasMap,
											const uniform uint8 BucklingStiffnessBiasIndices[],
											const uniform float BucklingStiffnessBiasTable[],
											const uniform int32 NumConstraints)
{

	foreach(i = 0 ... NumConstraints)
	{
		const varying FIntVector4 Constraint = VectorLoad(&Constraints[extract(i,0)]);

		const varying int32 i1 = Constraint.V[0];
		const varying int32 i2 = Constraint.V[1];
		const varying int32 i3 = Constraint.V[2];
		const varying int32 i4 = Constraint.V[3];
		const varying int8 StiffnessIndex = StiffnessHasMap ? StiffnessIndices[i] : 0;
		const varying int8 StiffnessWeftIndex = StiffnessWeftHasMap ? StiffnessWeftIndices[i] : 0;
		const varying int8 StiffnessBiasIndex = StiffnessBiasHasMap ? StiffnessBiasIndices[i] : 0;
		const varying int8 BucklingStiffnessIndex = BucklingStiffnessHasMap ? BucklingStiffnessIndices[i] : 0;
		const varying int8 BucklingStiffnessWeftIndex = BucklingStiffnessWeftHasMap ? BucklingStiffnessWeftIndices[i] : 0;
		const varying int8 BucklingStiffnessBiasIndex = BucklingStiffnessBiasHasMap ? BucklingStiffnessBiasIndices[i] : 0;

		const varying FVector4f PandInvM2 = VectorGather(&PandInvM[i2]);
		const varying FVector4f PandInvM3 = VectorGather(&PandInvM[i3]);
		const varying FVector4f PandInvM1 = VectorGather(&PandInvM[i1]);
		const varying FVector4f PandInvM4 = VectorGather(&PandInvM[i4]);

		varying FVector3f P1, P2, P3, P4;
		varying float M1, M2, M3, M4;

		UnzipPandInvM(PandInvM1, P1, M1);
		UnzipPandInvM(PandInvM2, P2, M2);
		UnzipPandInvM(PandInvM3, P3, M3);
		UnzipPandInvM(PandInvM4, P4, M4);
				
		#pragma ignore warning(perf)
		const varying float Stiffness = StiffnessTable[StiffnessIndex];
		#pragma ignore warning(perf)
		const varying float StiffnessWeft = StiffnessWeftTable[StiffnessWeftIndex];
		#pragma ignore warning(perf)
		const varying float StiffnessBias = StiffnessBiasTable[StiffnessBiasIndex];

		const varying FVector3f StiffnessVector = {{StiffnessWeft, Stiffness, StiffnessBias}};

		#pragma ignore warning(perf)
		const varying float BucklingStiffness = BucklingStiffnessTable[BucklingStiffnessIndex];
		#pragma ignore warning(perf)
		const varying float BucklingStiffnessWeft = BucklingStiffnessWeftTable[BucklingStiffnessWeftIndex];
		#pragma ignore warning(perf)
		const varying float BucklingStiffnessBias = BucklingStiffnessBiasTable[BucklingStiffnessBiasIndex];
		
		const varying FVector3f BucklingStiffnessVector = {{BucklingStiffnessWeft, BucklingStiffness, BucklingStiffnessBias}};
		
		const varying FVector3f WarpWeftBiasBaseMultiplier = VectorGather(&WarpWeftBiasBaseMultiplers[i]);
		const varying float StiffnessValue = select(IsBuckled[i], VectorDot(WarpWeftBiasBaseMultiplier, BucklingStiffnessVector), VectorDot(WarpWeftBiasBaseMultiplier, StiffnessVector));
		
		const varying float Lambda = Lambdas[i];

		varying FVector3f Delta1, Delta2, Delta3, Delta4;
		const varying float DLambda = GetXPBDBendingElementDeltas(P1, P2, P3, P4, M1, M2, M3, M4, Dt, RestAngles[i], Lambda, StiffnessValue, MinStiffness, Delta1, Delta2, Delta3, Delta4);

		Lambdas[i] = Lambda + DLambda;

		if (M1 > 0)
		{
			VectorScatter(&PandInvM[i1], SetVector4(P1 + Delta1, M1));
		}
		if (M2 > 0)
		{
			VectorScatter(&PandInvM[i2], SetVector4(P2 + Delta2, M2));
		}
		if (M3 > 0)
		{
			VectorScatter(&PandInvM[i3], SetVector4(P3 + Delta3, M3));
		}
		if (M4 > 0)
		{
			VectorScatter(&PandInvM[i4], SetVector4(P4 + Delta4, M4));
		}
	}
}

export void ApplyXPBDAnisotropicBendingConstraintsWithDampingAndMaps(uniform FVector4f PandInvM[],
											const uniform FVector3f X[],
											const uniform FIntVector4 Constraints[],
											const uniform float RestAngles[],
											const uniform bool IsBuckled[],
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
											const uniform bool BucklingStiffnessHasMap,
											const uniform uint8 BucklingStiffnessIndices[],
											const uniform float BucklingStiffnessTable[],
											const uniform bool BucklingStiffnessWeftHasMap,
											const uniform uint8 BucklingStiffnessWeftIndices[],
											const uniform float BucklingStiffnessWeftTable[],
											const uniform bool BucklingStiffnessBiasHasMap,
											const uniform uint8 BucklingStiffnessBiasIndices[],
											const uniform float BucklingStiffnessBiasTable[],
											const uniform bool DampingHasMap,
											const uniform uint8 DampingIndices[],
											const uniform float DampingTable[],
											const uniform int32 NumConstraints)
{

	foreach(i = 0 ... NumConstraints)
	{
		const varying FIntVector4 Constraint = VectorLoad(&Constraints[extract(i,0)]);

		const varying int32 i1 = Constraint.V[0];
		const varying int32 i2 = Constraint.V[1];
		const varying int32 i3 = Constraint.V[2];
		const varying int32 i4 = Constraint.V[3];
		const varying int8 StiffnessIndex = StiffnessHasMap ? StiffnessIndices[i] : 0;
		const varying int8 StiffnessWeftIndex = StiffnessWeftHasMap ? StiffnessWeftIndices[i] : 0;
		const varying int8 StiffnessBiasIndex = StiffnessBiasHasMap ? StiffnessBiasIndices[i] : 0;
		const varying int8 BucklingStiffnessIndex = BucklingStiffnessHasMap ? BucklingStiffnessIndices[i] : 0;
		const varying int8 BucklingStiffnessWeftIndex = BucklingStiffnessWeftHasMap ? BucklingStiffnessWeftIndices[i] : 0;
		const varying int8 BucklingStiffnessBiasIndex = BucklingStiffnessBiasHasMap ? BucklingStiffnessBiasIndices[i] : 0;
		const varying int8 DampingIndex = DampingHasMap ? DampingIndices[i] : 0;

		const varying FVector4f PandInvM2 = VectorGather(&PandInvM[i2]);
		const varying FVector4f PandInvM3 = VectorGather(&PandInvM[i3]);
		const varying FVector4f PandInvM1 = VectorGather(&PandInvM[i1]);
		const varying FVector4f PandInvM4 = VectorGather(&PandInvM[i4]);

		varying FVector3f P1, P2, P3, P4;
		varying float M1, M2, M3, M4;

		UnzipPandInvM(PandInvM1, P1, M1);
		UnzipPandInvM(PandInvM2, P2, M2);
		UnzipPandInvM(PandInvM3, P3, M3);
		UnzipPandInvM(PandInvM4, P4, M4);

		#pragma ignore warning(perf)
		const varying float Stiffness = StiffnessTable[StiffnessIndex];
		#pragma ignore warning(perf)
		const varying float StiffnessWeft = StiffnessWeftTable[StiffnessWeftIndex];
		#pragma ignore warning(perf)
		const varying float StiffnessBias = StiffnessBiasTable[StiffnessBiasIndex];

		const varying FVector3f StiffnessVector = {{StiffnessWeft, Stiffness, StiffnessBias}};

		#pragma ignore warning(perf)
		const varying float BucklingStiffness = BucklingStiffnessTable[BucklingStiffnessIndex];
		#pragma ignore warning(perf)
		const varying float BucklingStiffnessWeft = BucklingStiffnessWeftTable[BucklingStiffnessWeftIndex];
		#pragma ignore warning(perf)
		const varying float BucklingStiffnessBias = BucklingStiffnessBiasTable[BucklingStiffnessBiasIndex];
		
		const varying FVector3f BucklingStiffnessVector = {{BucklingStiffnessWeft, BucklingStiffness, BucklingStiffnessBias}};
		
		#pragma ignore warning(perf)
		const varying float DampingRatio = DampingTable[DampingIndex];
		
		const varying float RestAngle = RestAngles[i];
		const varying FVector3f WarpWeftBiasBaseMultiplier = VectorGather(&WarpWeftBiasBaseMultiplers[i]);
		const varying float StiffnessValue = select(IsBuckled[i], VectorDot(WarpWeftBiasBaseMultiplier, BucklingStiffnessVector), VectorDot(WarpWeftBiasBaseMultiplier, StiffnessVector));

		const varying FVector3f X1 = VectorGather(&X[i1]);
		const varying FVector3f X2 = VectorGather(&X[i2]);
		const varying FVector3f X3 = VectorGather(&X[i3]);
		const varying FVector3f X4 = VectorGather(&X[i4]);
		
		const varying float Lambda = Lambdas[i];

		varying FVector3f Delta1, Delta2, Delta3, Delta4;
		const varying float DLambda = GetXPBDBendingElementDeltasWithDamping(P1, P2, P3, P4, X1, X2, X3, X4, M1, M2, M3, M4, Dt, RestAngles[i], Lambda, StiffnessValue, DampingRatio, MinStiffness, Delta1, Delta2, Delta3, Delta4);

		Lambdas[i] = Lambda + DLambda;

		if (M1 > 0)
		{
			VectorScatter(&PandInvM[i1], SetVector4(P1 + Delta1, M1));
		}
		if (M2 > 0)
		{
			VectorScatter(&PandInvM[i2], SetVector4(P2 + Delta2, M2));
		}
		if (M3 > 0)
		{
			VectorScatter(&PandInvM[i3], SetVector4(P3 + Delta3, M3));
		}
		if (M4 > 0)
		{
			VectorScatter(&PandInvM[i4], SetVector4(P4 + Delta4, M4));
		}
	}
}

export void ApplyXPBDAnisotropicBendingDampingConstraintsWithMaps(uniform FVector4f PandInvM[],
											const uniform FVector3f X[],
											const uniform FIntVector4 Constraints[],
											const uniform float RestAngles[],
											const uniform bool IsBuckled[],
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
											const uniform bool BucklingStiffnessHasMap,
											const uniform uint8 BucklingStiffnessIndices[],
											const uniform float BucklingStiffnessTable[],
											const uniform bool BucklingStiffnessWeftHasMap,
											const uniform uint8 BucklingStiffnessWeftIndices[],
											const uniform float BucklingStiffnessWeftTable[],
											const uniform bool BucklingStiffnessBiasHasMap,
											const uniform uint8 BucklingStiffnessBiasIndices[],
											const uniform float BucklingStiffnessBiasTable[],
											const uniform bool DampingHasMap,
											const uniform uint8 DampingIndices[],
											const uniform float DampingTable[],
											const uniform int32 NumConstraints)
{

	foreach(i = 0 ... NumConstraints)
	{
		const varying FIntVector4 Constraint = VectorLoad(&Constraints[extract(i,0)]);

		const varying int32 i1 = Constraint.V[0];
		const varying int32 i2 = Constraint.V[1];
		const varying int32 i3 = Constraint.V[2];
		const varying int32 i4 = Constraint.V[3];
		const varying int8 StiffnessIndex = StiffnessHasMap ? StiffnessIndices[i] : 0;
		const varying int8 StiffnessWeftIndex = StiffnessWeftHasMap ? StiffnessWeftIndices[i] : 0;
		const varying int8 StiffnessBiasIndex = StiffnessBiasHasMap ? StiffnessBiasIndices[i] : 0;
		const varying int8 BucklingStiffnessIndex = BucklingStiffnessHasMap ? BucklingStiffnessIndices[i] : 0;
		const varying int8 BucklingStiffnessWeftIndex = BucklingStiffnessWeftHasMap ? BucklingStiffnessWeftIndices[i] : 0;
		const varying int8 BucklingStiffnessBiasIndex = BucklingStiffnessBiasHasMap ? BucklingStiffnessBiasIndices[i] : 0;
		const varying int8 DampingIndex = DampingHasMap ? DampingIndices[i] : 0;

		const varying FVector4f PandInvM2 = VectorGather(&PandInvM[i2]);
		const varying FVector4f PandInvM3 = VectorGather(&PandInvM[i3]);
		const varying FVector4f PandInvM1 = VectorGather(&PandInvM[i1]);
		const varying FVector4f PandInvM4 = VectorGather(&PandInvM[i4]);

		varying FVector3f P1, P2, P3, P4;
		varying float M1, M2, M3, M4;

		UnzipPandInvM(PandInvM1, P1, M1);
		UnzipPandInvM(PandInvM2, P2, M2);
		UnzipPandInvM(PandInvM3, P3, M3);
		UnzipPandInvM(PandInvM4, P4, M4);

		#pragma ignore warning(perf)
		const varying float Stiffness = StiffnessTable[StiffnessIndex];
		#pragma ignore warning(perf)
		const varying float StiffnessWeft = StiffnessWeftTable[StiffnessWeftIndex];
		#pragma ignore warning(perf)
		const varying float StiffnessBias = StiffnessBiasTable[StiffnessBiasIndex];

		const varying FVector3f StiffnessVector = {{StiffnessWeft, Stiffness, StiffnessBias}};

		#pragma ignore warning(perf)
		const varying float BucklingStiffness = BucklingStiffnessTable[BucklingStiffnessIndex];
		#pragma ignore warning(perf)
		const varying float BucklingStiffnessWeft = BucklingStiffnessWeftTable[BucklingStiffnessWeftIndex];
		#pragma ignore warning(perf)
		const varying float BucklingStiffnessBias = BucklingStiffnessBiasTable[BucklingStiffnessBiasIndex];
		
		const varying FVector3f BucklingStiffnessVector = {{BucklingStiffnessWeft, BucklingStiffness, BucklingStiffnessBias}};
		
		#pragma ignore warning(perf)
		const varying float DampingRatio = DampingTable[DampingIndex];
		
		const varying float RestAngle = RestAngles[i];
		const varying FVector3f WarpWeftBiasBaseMultiplier = VectorGather(&WarpWeftBiasBaseMultiplers[i]);
		const varying float StiffnessValue = select(IsBuckled[i], VectorDot(WarpWeftBiasBaseMultiplier, BucklingStiffnessVector), VectorDot(WarpWeftBiasBaseMultiplier, StiffnessVector));

		const varying FVector3f X1 = VectorGather(&X[i1]);
		const varying FVector3f X2 = VectorGather(&X[i2]);
		const varying FVector3f X3 = VectorGather(&X[i3]);
		const varying FVector3f X4 = VectorGather(&X[i4]);
		
		const varying float Lambda = Lambdas[i];

		varying FVector3f Delta1, Delta2, Delta3, Delta4;
		const varying float DLambda = GetXPBDBendingElementDampingDeltas(P1, P2, P3, P4, X1, X2, X3, X4, M1, M2, M3, M4, Dt, RestAngles[i], Lambda, StiffnessValue, DampingRatio, MinStiffness, Delta1, Delta2, Delta3, Delta4);

		Lambdas[i] = Lambda + DLambda;

		if (M1 > 0)
		{
			VectorScatter(&PandInvM[i1], SetVector4(P1 + Delta1, M1));
		}
		if (M2 > 0)
		{
			VectorScatter(&PandInvM[i2], SetVector4(P2 + Delta2, M2));
		}
		if (M3 > 0)
		{
			VectorScatter(&PandInvM[i3], SetVector4(P3 + Delta3, M3));
		}
		if (M4 > 0)
		{
			VectorScatter(&PandInvM[i4], SetVector4(P4 + Delta4, M4));
		}
	}
}
