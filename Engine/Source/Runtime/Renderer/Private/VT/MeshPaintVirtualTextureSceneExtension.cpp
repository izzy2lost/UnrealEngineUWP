// Copyright Epic Games, Inc. All Rights Reserved.

#include "VT/MeshPaintVirtualTextureSceneExtension.h"

#include "GlobalRenderResources.h"
#include "ScenePrivate.h"
#include "SceneUniformBuffer.h"
#include "ShaderParameterMacros.h"
#include "VT/MeshPaintVirtualTexture.h"

IMPLEMENT_SCENE_EXTENSION(FMeshPaintVirtualTextureSceneExtension);

bool FMeshPaintVirtualTextureSceneExtension::ShouldCreateExtension(FScene& InScene)
{
	return MeshPaintVirtualTexture::IsSupported(InScene.GetShaderPlatform());
}

void FMeshPaintVirtualTextureSceneExtension::InitExtension(FScene& InScene)
{
}

ISceneExtensionRenderer* FMeshPaintVirtualTextureSceneExtension::CreateRenderer()
{
	return new FRenderer;
}

BEGIN_SHADER_PARAMETER_STRUCT(FMeshPaintTextureParameters, RENDERER_API)
	SHADER_PARAMETER_TEXTURE(Texture2D<uint4>, PageTableTexture)
	SHADER_PARAMETER_TEXTURE(Texture2D<float4>, PhysicalTexture)
	SHADER_PARAMETER(FUintVector4, PackedUniform)
END_SHADER_PARAMETER_STRUCT()

DECLARE_SCENE_UB_STRUCT(FMeshPaintTextureParameters, MeshPaint, RENDERER_API)

static void GetDefaultMeshPaintParameters(FMeshPaintTextureParameters& Parameters, FRDGBuilder& GraphBuilder)
{
	Parameters.PageTableTexture = GBlackUintTexture->TextureRHI;
	Parameters.PhysicalTexture = GBlackTextureWithSRV->TextureRHI;
	Parameters.PackedUniform = FUintVector4(0, 0, 0, 0);
}

IMPLEMENT_SCENE_UB_STRUCT(FMeshPaintTextureParameters, MeshPaint, GetDefaultMeshPaintParameters);

void FMeshPaintVirtualTextureSceneExtension::FRenderer::UpdateSceneUniformBuffer(FRDGBuilder& GraphBuilder, FSceneUniformBuffer& SceneUniformBuffer)
{
	MeshPaintVirtualTexture::FUniformParams MeshPaintParameters = MeshPaintVirtualTexture::GetUniformParams();

	FMeshPaintTextureParameters Parameters;
	Parameters.PageTableTexture = MeshPaintParameters.PageTableTexture ? MeshPaintParameters.PageTableTexture : GBlackUintTexture->TextureRHI;
	Parameters.PhysicalTexture = MeshPaintParameters.PhysicalTexture ? MeshPaintParameters.PhysicalTexture : GBlackTextureWithSRV->TextureRHI;
	Parameters.PackedUniform = MeshPaintParameters.PackedUniform;

	SceneUniformBuffer.Set(SceneUB::MeshPaint, Parameters);
}
