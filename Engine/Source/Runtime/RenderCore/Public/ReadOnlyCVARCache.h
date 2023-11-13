// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ReadOnlyCVarCache.h: Cache of read-only console variables used by the renderer
=============================================================================*/

#pragma once
#include "RHIShaderPlatform.h"

struct FReadOnlyCVARCache
{
	static void Initialize();
		
	static bool AllowStaticLighting();
	RENDERCORE_API static bool EnablePointLightShadows();
	RENDERCORE_API static bool EnableStationarySkylight();
	RENDERCORE_API static bool EnableLowQualityLightmaps();
	RENDERCORE_API static bool SupportSkyAtmosphere();

	// Mobile specific
	static bool MobileHDR();
	static bool MobileSupportsGPUScene();
	static bool MobileDeferredShading(const FStaticShaderPlatform Platform);
	static bool MobileEnableMovableSpotlightsShadow(const FStaticShaderPlatform Platform);

	RENDERCORE_API static bool MobileAllowMovableDirectionalLights();
	RENDERCORE_API static bool MobileAllowDistanceFieldShadows();
	RENDERCORE_API static bool MobileEnableStaticAndCSMShadowReceivers();
	RENDERCORE_API static bool MobileEnableMovableLightCSMShaderCulling();
	RENDERCORE_API static int32 MobileSkyLightPermutation();
	RENDERCORE_API static bool MobileEnableNoPrecomputedLightingCSMShader();
	RENDERCORE_API static int32 MobileEarlyZPass(const FStaticShaderPlatform Platform);
	RENDERCORE_API static int32 MobileForwardLocalLights(const FStaticShaderPlatform Platform);

private:
	static bool bInitialized;
};

