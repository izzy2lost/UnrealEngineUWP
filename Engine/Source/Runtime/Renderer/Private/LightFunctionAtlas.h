// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LightSceneProxy.h"
#include "RenderGraphFwd.h"
#include "ShaderParameterMacros.h"

class FViewFamilyInfo;
class FViewInfo;
class FScene;
class FMaterialRenderProxy;
class FRDGBuilder;
class FLightSceneInfo;
class FRDGTexture;

struct FScreenPassTexture;
struct FSortedLightSetSceneInfo;
struct IPooledRenderTarget;
struct FLightFunctionAtlas;



struct FLightFunctionAtlasSceneData
{
	void SetData(FLightFunctionAtlas* InLightFunctionAtlas, bool bInLightFunctionAtlasEnabled, bool bInVolumetricFogUsesLightFunctionAtlas, bool bInDeferredlightingUsesLightFunctionAtlas)
	{
		LightFunctionAtlas = InLightFunctionAtlas;
		bLightFunctionAtlasEnabled = bInLightFunctionAtlasEnabled;
		bVolumetricFogUsesLightFunctionAtlas = bInVolumetricFogUsesLightFunctionAtlas;
		bDeferredlightingUsesLightFunctionAtlas = bInDeferredlightingUsesLightFunctionAtlas;
	}

	FLightFunctionAtlas* GetLightFunctionAtlas()		const { return LightFunctionAtlas; }
	bool GetLightFunctionAtlasEnabled()					const { return bLightFunctionAtlasEnabled; }
	bool GetVolumetricFogUsesLightFunctionAtlas()		const { return bVolumetricFogUsesLightFunctionAtlas; }
	bool GetDeferredlightingUsesLightFunctionAtlas()	const { return bDeferredlightingUsesLightFunctionAtlas; }

private:
	FLightFunctionAtlas* LightFunctionAtlas = nullptr;
	bool bLightFunctionAtlasEnabled = false;
	bool bVolumetricFogUsesLightFunctionAtlas = false;
	bool bDeferredlightingUsesLightFunctionAtlas = false;
};

struct FLightFunctionAtlasViewData
{
	FLightFunctionAtlasViewData() : SceneData(nullptr) {}
	FLightFunctionAtlasViewData(FLightFunctionAtlasSceneData* InSceneData) : SceneData(InSceneData) {}

	FLightFunctionAtlas* GetLightFunctionAtlas()		const { return SceneData ? SceneData->GetLightFunctionAtlas() : nullptr; }
	bool GetLightFunctionAtlasEnabled()					const { return SceneData ? SceneData->GetLightFunctionAtlasEnabled() : false; }
	bool GetVolumetricFogUsesLightFunctionAtlas()		const { return SceneData ? SceneData->GetVolumetricFogUsesLightFunctionAtlas()		: false; }
	bool GetDeferredlightingUsesLightFunctionAtlas()	const { return SceneData ? SceneData->GetDeferredlightingUsesLightFunctionAtlas()	: false; }

private:
	FLightFunctionAtlasSceneData* SceneData;
};



struct FLightFunctionSlotKey
{
	uint32 LFMaterialUniqueID = 0;

	uint32 EffectiveLightFunctionSlotIndex = 0;	// Not used to de-duplicate light function. It is the index of the effective lighting function for this frame in EffectiveLightFunctionSlotArray.

	FLightFunctionSlotKey() {};

	FLightFunctionSlotKey(FLightSceneInfo* InLightSceneInfo);

	inline bool operator==(const FLightFunctionSlotKey& Other) const 
	{ 
		return Other.LFMaterialUniqueID == LFMaterialUniqueID;
	}
};

inline uint32 GetTypeHash(FLightFunctionSlotKey Key)
{
	return GetTypeHash(Key.LFMaterialUniqueID) ;
}

struct FLightFunctionSlot
{
	const FMaterialRenderProxy* LightFunctionMaterial;
	FIntPoint Min;
	FIntPoint Max;
};

#define LIGHT_FUNCTION_ATLAS_MAX_LIGHT_FUNCTION_COUNT 256

/**
 * On devices using OpenGL, we are limited to 256 vec4s. 
 * Light parameters take 5 vec4s. So we can fit at maximum of 51.2 light info data. We round it down to 48 lights.
 * This is already plenty. And we can move to use a buffer later on.
 */
#define LIGHT_FUNCTION_ATLAS_MAX_LIGHT_COUNT 48

// This allows to not have to store more data per GPU light representation on GPU. The light only needs an index into the array.
// Using a constant buffer also workaround the fact that we would otherwise need another SRV in forward shaders. 
// The light atlas texture itself already use 1 extra SRV. We could an extra SRV and have LightInfoDataXXX be in a buffer that scale with amount of light in the scene.
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FLightFunctionAtlasGlobalParameters, )
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, LightFunctionAtlasTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, LightFunctionAtlasSampler)
	SHADER_PARAMETER(float, Slot_UVSize)
	SHADER_PARAMETER_ARRAY(FMatrix44f, LightInfoDataMatrix, [LIGHT_FUNCTION_ATLAS_MAX_LIGHT_COUNT])		// Light data
	SHADER_PARAMETER_ARRAY(FVector4f,  LightInfoDataParameters, [LIGHT_FUNCTION_ATLAS_MAX_LIGHT_COUNT]) // Light data
END_GLOBAL_SHADER_PARAMETER_STRUCT()



// This class holds all data and resources related light function for a single scene, including multiple views.
struct FLightFunctionAtlas
{
	FLightFunctionAtlas();
	virtual ~FLightFunctionAtlas();

	bool IsLightFunctionAtlasEnabled() const { return bLightFunctionAtlasEnabled; }

	void ClearEmptySceneFrame(FViewInfo* View = nullptr, FLightFunctionAtlasSceneData* LightFunctionAtlasSceneData = nullptr);

	void BeginSceneFrame(FViewFamilyInfo& ViewFamily, TArray<FViewInfo>& Views, FLightFunctionAtlasSceneData& LightFunctionAtlasSceneData, bool bShouldRenderVolumetricFog);

	void UpdateRegisterLightSceneInfo(FLightSceneInfo* LightSceneInfo);

	void UpdateLightFunctionAtlas(const TArray<FViewInfo>& Views);

	void RenderLightFunctionAtlas(FRDGBuilder& GraphBuilder, TArray<FViewInfo>& Views);


	FScreenPassTexture AddDebugVisualizationPasses(FRDGBuilder& GraphBuilder, const FViewInfo& View, FScreenPassTexture& ScreenPassSceneColor)  const;

	FLightFunctionAtlasGlobalParameters*						GetLightFunctionAtlasGlobalParametersStruct(uint32 ViewIndex, FRDGBuilder& GraphBuilder);
	TRDGUniformBufferRef<FLightFunctionAtlasGlobalParameters>	GetLightFunctionAtlasGlobalParameters(uint32 ViewIndex, FRDGBuilder& GraphBuilder);
	
	static FLightFunctionAtlasGlobalParameters*					GetDefaultLightFunctionAtlasGlobalParametersStruct(FRDGBuilder& GraphBuilder);
	TRDGUniformBufferRef<FLightFunctionAtlasGlobalParameters>	GetDefaultLightFunctionAtlasGlobalParameters(FRDGBuilder& GraphBuilder);

private:

	void AllocateAtlasSlots(const TArray<FViewInfo>& Views);

	void AllocateTexture2DAtlas(FRDGBuilder& GraphBuilder);

	void RenderAtlasSlots(FRDGBuilder& GraphBuilder, const TArray<FViewInfo>& Views);

	bool bLightFunctionAtlasEnabled = false;

	FRDGTextureRef RDGAtlasTexture2D = nullptr;

	// All the lights that wants to sample light functions
	TArray<FLightSceneInfo*> RegisteredLights;

	// This set is used to de-duplicate light functions in the atlas. An alternative to set would be to use simple array with simple loops.
	TSet<FLightFunctionSlotKey> LightFunctionsSet;

	// The structure used to render light function and generate the associated constant buffer, containing UVs and transformation matrices.
	struct EffectiveLightFunctionSlot
	{
		const FMaterialRenderProxy* LightFunctionMaterial = nullptr;
		FIntPoint Min;
		FIntPoint Max;
		float MinU;
		float MinV;
	};
	TArray<EffectiveLightFunctionSlot> EffectiveLightFunctionSlotArray;

	struct EffectiveLocalLightSlot
	{
		FLightSceneInfo*	LightSceneInfo = nullptr;
		uint8				LightFunctionAtlasSlotIndex = 0;
	};
	TArray<EffectiveLocalLightSlot> EffectiveLocalLightSlotArray;

	FLightFunctionAtlasGlobalParameters*								DefaultLightFunctionAtlasGlobalParameters = nullptr;
	TRDGUniformBufferRef<FLightFunctionAtlasGlobalParameters>			DefaultLightFunctionAtlasGlobalParametersUB;

	TArray<FLightFunctionAtlasGlobalParameters*>							ViewLightFunctionAtlasGlobalParametersArray;
	TArray<TRDGUniformBufferRef<FLightFunctionAtlasGlobalParameters>>	ViewLightFunctionAtlasGlobalParametersUBArray;
};
