// Copyright Epic Games, Inc. All Rights Reserved.

#define EXPLICIT_VECTOR4 1

#include "Math/Vector.isph"
#include "Chaos/PBDSofts.isph"
#include "Chaos/XPBDBendingElement.isph"

export void ApplyXPBDBendingConstraints(uniform FVector4f PandInvM[],
									const uniform FIntVector4 Constraints[],
									const uniform float RestAngles[],
									const uniform bool IsBuckled[],
									uniform float Lambdas[],
									const uniform float Dt,
									const uniform float MinStiffness,
									const uniform float Stiffness,
									const uniform float BucklingStiffness,
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

		const varying float StiffnessValue = select(IsBuckled[i], BucklingStiffness, Stiffness);
		
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

export void ApplyXPBDBendingConstraintsWithDamping(uniform FVector4f PandInvM[],
									const uniform FVector3f X[],
									const uniform FIntVector4 Constraints[],
									const uniform float RestAngles[],
									const uniform bool IsBuckled[],
									uniform float Lambdas[],
									const uniform float Dt,
									const uniform float MinStiffness,
									const uniform float Stiffness,
									const uniform float BucklingStiffness,
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
		
		const varying float StiffnessValue = select(IsBuckled[i], BucklingStiffness, Stiffness);
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

export void ApplyXPBDBendingDampingConstraints(uniform FVector4f PandInvM[],
									const uniform FVector3f X[],
									const uniform FIntVector4 Constraints[],
									const uniform float RestAngles[],
									const uniform bool IsBuckled[],
									uniform float Lambdas[],
									const uniform float Dt,
									const uniform float MinStiffness,
									const uniform float Stiffness,
									const uniform float BucklingStiffness,
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
		
		const varying float StiffnessValue = select(IsBuckled[i], BucklingStiffness, Stiffness);
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

export void ApplyXPBDBendingConstraintsWithMaps(uniform FVector4f PandInvM[],
											const uniform FIntVector4 Constraints[],
											const uniform float RestAngles[],
											const uniform bool IsBuckled[],
											uniform float Lambdas[],
											const uniform float Dt,
											const uniform float MinStiffness,
											const uniform bool StiffnessHasMap,
											const uniform uint8 StiffnessIndices[],
											const uniform float StiffnessTable[],
											const uniform bool BucklingStiffnessHasMap,
											const uniform uint8 BucklingStiffnessIndices[],
											const uniform float BucklingStiffnessTable[],
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
		const varying int8 BucklingStiffnessIndex = BucklingStiffnessHasMap ? BucklingStiffnessIndices[i] : 0;

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
		const varying float BucklingStiffness = BucklingStiffnessTable[BucklingStiffnessIndex];
		
		const varying float StiffnessValue = select(IsBuckled[i], BucklingStiffness, Stiffness);
		
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

export void ApplyXPBDBendingConstraintsWithDampingAndMaps(uniform FVector4f PandInvM[],
											const uniform FVector3f X[],
											const uniform FIntVector4 Constraints[],
											const uniform float RestAngles[],
											const uniform bool IsBuckled[],
											uniform float Lambdas[],
											const uniform float Dt,
											const uniform float MinStiffness,
											const uniform bool StiffnessHasMap,
											const uniform uint8 StiffnessIndices[],
											const uniform float StiffnessTable[],
											const uniform bool BucklingStiffnessHasMap,
											const uniform uint8 BucklingStiffnessIndices[],
											const uniform float BucklingStiffnessTable[],
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
		const varying int8 BucklingStiffnessIndex = BucklingStiffnessHasMap ? BucklingStiffnessIndices[i] : 0;
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
		const varying float BucklingStiffness = BucklingStiffnessTable[BucklingStiffnessIndex];		
		#pragma ignore warning(perf)
		const varying float DampingRatio = DampingTable[DampingIndex];
		
		const varying float StiffnessValue = select(IsBuckled[i], BucklingStiffness, Stiffness);

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

export void ApplyXPBDBendingDampingConstraintsWithMaps(uniform FVector4f PandInvM[],
											const uniform FVector3f X[],
											const uniform FIntVector4 Constraints[],
											const uniform float RestAngles[],
											const uniform bool IsBuckled[],
											uniform float Lambdas[],
											const uniform float Dt,
											const uniform float MinStiffness,
											const uniform bool StiffnessHasMap,
											const uniform uint8 StiffnessIndices[],
											const uniform float StiffnessTable[],
											const uniform bool BucklingStiffnessHasMap,
											const uniform uint8 BucklingStiffnessIndices[],
											const uniform float BucklingStiffnessTable[],
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
		const varying int8 BucklingStiffnessIndex = BucklingStiffnessHasMap ? BucklingStiffnessIndices[i] : 0;
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
		const varying float BucklingStiffness = BucklingStiffnessTable[BucklingStiffnessIndex];		
		#pragma ignore warning(perf)
		const varying float DampingRatio = DampingTable[DampingIndex];
		
		const varying float StiffnessValue = select(IsBuckled[i], BucklingStiffness, Stiffness);

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
