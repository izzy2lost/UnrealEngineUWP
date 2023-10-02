// Copyright Epic Games, Inc. All Rights Reserved.

#include "InstanceUniformShaderParameters.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "SceneDefinitions.h"

uint32 FInstanceSceneShaderData::GetDataStrideInFloat4s()
{
	if (FDataDrivenShaderPlatformInfo::GetSupportSceneDataCompressedTransforms(GMaxRHIShaderPlatform))
	{
		return CompressedTransformDataStrideInFloat4s;
	}
	else
	{
		return UnCompressedTransformDataStrideInFloat4s;
	}
}

void FInstanceSceneShaderData::Build
(
	uint32 PrimitiveId,
	uint32 RelativeId,
	uint32 InstanceFlags,
	uint32 LastUpdateFrame,
	uint32 CustomDataCount,
	float RandomID,
	bool bIsVisible
)
{
	BuildInternal(PrimitiveId, RelativeId, InstanceFlags, LastUpdateFrame, CustomDataCount, RandomID, FRenderTransform::Identity, bIsVisible);
}

void FInstanceSceneShaderData::Build
(
	uint32 PrimitiveId,
	uint32 RelativeId,
	uint32 InstanceFlags,
	uint32 LastUpdateFrame,
	uint32 CustomDataCount,
	float RandomID,
	const FRenderTransform& LocalToPrimitive,
	const FRenderTransform& PrimitiveToWorld,
	bool bIsVisible
)
{
	FRenderTransform LocalToWorld = LocalToPrimitive * PrimitiveToWorld;

	// Remove shear
	LocalToWorld.Orthogonalize();

	BuildInternal(PrimitiveId, RelativeId, InstanceFlags, LastUpdateFrame, CustomDataCount, RandomID, LocalToWorld, bIsVisible);
}

void FInstanceSceneShaderData::BuildInternal
(
	uint32 PrimitiveId,
	uint32 RelativeId,
	uint32 InstanceFlags,
	uint32 LastUpdateFrame,
	uint32 CustomDataCount,
	float RandomID,
	const FRenderTransform& LocalToWorld, // Assumes shear has been removed already
	bool bIsVisible
)
{
	// Note: layout must match GetInstanceData in SceneData.ush and InitializeInstanceSceneData in GPUSceneWriter.ush

	const float RotDeterminant = LocalToWorld.RotDeterminant();
	if (RotDeterminant < 0.0f)
	{
		InstanceFlags |= INSTANCE_SCENE_DATA_FLAG_DETERMINANT_SIGN;
	}
	else
	{
		InstanceFlags &= ~INSTANCE_SCENE_DATA_FLAG_DETERMINANT_SIGN;
	}

	// Mark zero scaled instances as hidden.
	if (!bIsVisible || RotDeterminant == 0)
	{
		InstanceFlags |= INSTANCE_SCENE_DATA_FLAG_HIDDEN;
	}

	checkSlow((PrimitiveId		& 0x000FFFFF) == PrimitiveId);
	checkSlow((InstanceFlags	& 0x00000FFF) == InstanceFlags);
	checkSlow((RelativeId		& 0x00FFFFFF) == RelativeId);
	checkSlow((CustomDataCount	& 0x000000FF) == CustomDataCount);

	const uint32 Packed0 = (InstanceFlags   << 20u) | PrimitiveId;
	const uint32 Packed1 = (CustomDataCount << 24u) | RelativeId;

	Data[0].X  = *(const float*)&Packed0;
	Data[0].Y  = *(const float*)&Packed1;
	Data[0].Z  = *(const float*)&LastUpdateFrame;
	Data[0].W  = *(const float*)&RandomID;

	if (FDataDrivenShaderPlatformInfo::GetSupportSceneDataCompressedTransforms(GMaxRHIShaderPlatform))
	{
		FCompressedTransform CompressedLocalToWorld(LocalToWorld);
		Data[1] = *(const FVector4f*)&CompressedLocalToWorld.Rotation[0];
		Data[2] = *(const FVector3f*)&CompressedLocalToWorld.Translation;
	}
	else
	{
		// Note: writes 3x float4s
		LocalToWorld.To3x4MatrixTranspose((float*)&Data[1]);
	}
}
