// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// The packed size of the spline mesh data in float4s
#define SPLINE_MESH_PARAMS_FLOAT4_SIZE 7

// Definitions for sizing the scene spline mesh texture and encoding spline addresses
#define SPLINE_MESH_TEXEL_WIDTH_BITS		(6u)
#define SPLINE_MESH_TEXEL_WIDTH				(1u << SPLINE_MESH_TEXEL_WIDTH_BITS)
#define SPLINE_MESH_TEXEL_WIDTH_MASK		(SPLINE_MESH_TEXEL_WIDTH - 1u)
#define SPLINE_MESH_TEXTURE_MAX_DIMENSION	(16 * 1024)

#ifdef __cplusplus
#include "HLSLTypeAliases.h"

namespace UE::HLSL
{
#endif // __cplusplus

struct FSplineMeshShaderParams
{
	float3 StartPos;
	float3 EndPos;
	float3 StartTangent;
	float3 EndTangent;
	float2 StartScale;
	float2 EndScale;
	float2 StartOffset;
	float2 EndOffset;
	float StartRoll;
	float EndRoll;
	float MeshScaleZ;
	float MeshMinZ;
	float2 MeshDeformScaleMinMax;
	bool bSmoothInterpRollScale;
	float3 SplineUpDir;
	float3 MeshDir;
	float3 MeshX;
	float3 MeshY;
	uint2 TextureCoord;
	float NaniteClusterBoundsScale;
};

#ifdef __cplusplus
} // namespace UE::HLSL
using FSplineMeshShaderParams = UE::HLSL::FSplineMeshShaderParams;
#endif // __cplusplus

#ifndef __cplusplus
#include "/Engine/Private/Quaternion.ush"

// HLSL unpack method
FSplineMeshShaderParams UnpackSplineMeshParams(float4 PackedParams[SPLINE_MESH_PARAMS_FLOAT4_SIZE])
{
	FSplineMeshShaderParams Output;
	Output.StartPos 				= PackedParams[0].xyz;
	Output.EndPos 					= PackedParams[1].xyz;
	Output.StartTangent 			= PackedParams[2].xyz;
	Output.EndTangent 				= float3(PackedParams[0].w, PackedParams[1].w, PackedParams[2].w);
	Output.StartOffset 				= PackedParams[3].xy;
	Output.EndOffset 				= PackedParams[3].zw;
	Output.StartScale 				= float2(f16tof32(asuint(PackedParams[4].x)),
											 f16tof32(asuint(PackedParams[4].x) >> 16u));
	Output.EndScale 				= float2(f16tof32(asuint(PackedParams[4].y)),
											 f16tof32(asuint(PackedParams[4].y) >> 16u));
	Output.StartRoll 				= f16tof32(asuint(PackedParams[4].z));
	Output.EndRoll 					= f16tof32(asuint(PackedParams[4].z) >> 16u);
	Output.TextureCoord				= uint2(asuint(PackedParams[4].w) & 0xFFFFu,
											asuint(PackedParams[4].w) >> 16u);
	Output.MeshDeformScaleMinMax	= PackedParams[5].xy;
	Output.MeshScaleZ 				= PackedParams[5].z;
	Output.MeshMinZ 				= PackedParams[5].w;
	Output.SplineUpDir 				= float3(SNorm16ToF32(asuint(PackedParams[6].x)),
											 SNorm16ToF32(asuint(PackedParams[6].x) >> 16u),
											 SNorm16ToF32(asuint(PackedParams[6].y)));
	Output.NaniteClusterBoundsScale	= f16tof32((asuint(PackedParams[6].y) >> 16u) & 0x7FFFu);
	Output.bSmoothInterpRollScale	= (asuint(PackedParams[6].y) >> 31u) != 0;
	FQuat MeshQuat					= FQuat(SNorm16ToF32(asuint(PackedParams[6].z)),
											SNorm16ToF32(asuint(PackedParams[6].z) >> 16u),
											SNorm16ToF32(asuint(PackedParams[6].w)),
											SNorm16ToF32(asuint(PackedParams[6].w) >> 16u));
	float3x3 MeshRot 				= QuatToMatrix(MeshQuat);
	Output.MeshDir					= MeshRot[0];
	Output.MeshX					= MeshRot[1];
	Output.MeshY					= MeshRot[2];

	return Output;
}
#endif // !__cplusplus
