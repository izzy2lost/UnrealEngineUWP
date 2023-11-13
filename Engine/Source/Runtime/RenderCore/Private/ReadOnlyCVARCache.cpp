// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReadOnlyCVARCache.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "Interfaces/ITargetPlatform.h"
#include "ShaderPlatformCachedIniValue.h"

#if WITH_EDITOR
#include "Interfaces/ITargetPlatformManagerModule.h"
#endif

static bool bEnablePointLightShadows;
static bool bEnableStationarySkylight;
static bool bEnableLowQualityLightmaps;
static bool bAllowStaticLighting;
static bool bSupportSkyAtmosphere;

// Mobile specific
static bool bMobileHDR;
static bool bMobileAllowMovableDirectionalLights;
static bool bMobileAllowDistanceFieldShadows;
static bool bMobileEnableStaticAndCSMShadowReceivers;
static bool bMobileEnableMovableLightCSMShaderCulling;
static bool bMobileSupportsGPUScene;
static int32 MobileSkyLightPermutationValue;
static int32 MobileEarlyZPassValue;
static int32 MobileForwardLocalLightsValue;
static bool bMobileEnableNoPrecomputedLightingCSMShader;
static bool bMobileDeferredShadingValue;
static bool bMobileEnableMovableSpotlightsShadowValue;


static int32 MobileEarlyZPassIniValue(const FStaticShaderPlatform Platform)
{
	static FShaderPlatformCachedIniValue<int32> CVar(TEXT("r.Mobile.EarlyZPass"));
	return CVar.Get(Platform);
}

static int32 MobileForwardLocalLightsIniValue(const FStaticShaderPlatform Platform)
{
	static FShaderPlatformCachedIniValue<int32> CVar(TEXT("r.Mobile.Forward.EnableLocalLights"));
	return CVar.Get(Platform);
}

static bool MobileDeferredShadingIniValue(const FStaticShaderPlatform Platform)
{
	static FShaderPlatformCachedIniValue<bool> MobileShadingPathIniValue(TEXT("r.Mobile.ShadingPath"));
	static TConsoleVariableData<int32>* MobileAllowDeferredShadingOpenGL = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.AllowDeferredShadingOpenGL"));
	// a separate cvar so we can exclude deferred from OpenGL specificaly
	const bool bSupportedPlatform = !IsOpenGLPlatform(Platform) || (MobileAllowDeferredShadingOpenGL && MobileAllowDeferredShadingOpenGL->GetValueOnAnyThread() != 0);
	return MobileShadingPathIniValue.Get(Platform) && bSupportedPlatform;
}

static bool MobileEnableMovableSpotlightsShadowIniValue(const FStaticShaderPlatform Platform)
{
	static FShaderPlatformCachedIniValue<bool> CVar(TEXT("r.Mobile.EnableMovableSpotlightsShadow"));
	return CVar.Get(Platform);
}

bool FReadOnlyCVARCache::bInitialized = false;

void FReadOnlyCVARCache::Initialize()
{
	UE_LOG(LogInit, Log, TEXT("Initializing FReadOnlyCVARCache"));

	const auto CVarSupportStationarySkylight = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SupportStationarySkylight"));
	const auto CVarSupportLowQualityLightmaps = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SupportLowQualityLightmaps"));
	const auto CVarSupportPointLightWholeSceneShadows = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SupportPointLightWholeSceneShadows"));
	const auto CVarSupportAllShaderPermutations = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SupportAllShaderPermutations"));	
	const auto CVarVertexFoggingForOpaque = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.VertexFoggingForOpaque"));	
	const auto CVarAllowStaticLighting = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));
	const auto CVarSupportSkyAtmosphere = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SupportSkyAtmosphere"));
	
	const auto CVarMobileHDR = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileHDR"));
	const auto CVarMobileAllowMovableDirectionalLights = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.AllowMovableDirectionalLights"));
	const auto CVarMobileEnableStaticAndCSMShadowReceivers = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.EnableStaticAndCSMShadowReceivers"));
	const auto CVarMobileEnableMovableLightCSMShaderCulling = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.EnableMovableLightCSMShaderCulling"));
	const auto CVarMobileAllowDistanceFieldShadows = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.AllowDistanceFieldShadows"));
	const auto CVarMobileSkyLightPermutation = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.SkyLightPermutation"));
	const auto CVarMobileEnableNoPrecomputedLightingCSMShader = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.EnableNoPrecomputedLightingCSMShader"));
	const auto CVarMobileSupportGPUScene = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.SupportGPUScene"));
	
	const bool bForceAllPermutations = CVarSupportAllShaderPermutations && CVarSupportAllShaderPermutations->GetValueOnAnyThread() != 0;

	bEnableStationarySkylight = !CVarSupportStationarySkylight || CVarSupportStationarySkylight->GetValueOnAnyThread() != 0 || bForceAllPermutations;
	bEnablePointLightShadows = !CVarSupportPointLightWholeSceneShadows || CVarSupportPointLightWholeSceneShadows->GetValueOnAnyThread() != 0 || bForceAllPermutations;
	bEnableLowQualityLightmaps = !CVarSupportLowQualityLightmaps || CVarSupportLowQualityLightmaps->GetValueOnAnyThread() != 0 || bForceAllPermutations;
	bAllowStaticLighting = CVarAllowStaticLighting->GetValueOnAnyThread() != 0;
	bSupportSkyAtmosphere = !CVarSupportSkyAtmosphere || CVarSupportSkyAtmosphere->GetValueOnAnyThread() != 0 || bForceAllPermutations;

	// mobile
	bMobileHDR = CVarMobileHDR->GetValueOnAnyThread() == 1;
	bMobileAllowMovableDirectionalLights = CVarMobileAllowMovableDirectionalLights->GetValueOnAnyThread() != 0;
	bMobileAllowDistanceFieldShadows = CVarMobileAllowDistanceFieldShadows->GetValueOnAnyThread() != 0;
	bMobileEnableStaticAndCSMShadowReceivers = CVarMobileEnableStaticAndCSMShadowReceivers->GetValueOnAnyThread() != 0;
	bMobileEnableMovableLightCSMShaderCulling = CVarMobileEnableMovableLightCSMShaderCulling->GetValueOnAnyThread() != 0;
	MobileSkyLightPermutationValue = CVarMobileSkyLightPermutation->GetValueOnAnyThread();
	bMobileEnableNoPrecomputedLightingCSMShader = CVarMobileEnableNoPrecomputedLightingCSMShader->GetValueOnAnyThread() != 0;
	MobileEarlyZPassValue = MobileEarlyZPassIniValue(GMaxRHIShaderPlatform);
	MobileForwardLocalLightsValue = MobileForwardLocalLightsIniValue(GMaxRHIShaderPlatform);
	bMobileDeferredShadingValue = MobileDeferredShadingIniValue(GMaxRHIShaderPlatform);
	bMobileEnableMovableSpotlightsShadowValue = MobileEnableMovableSpotlightsShadowIniValue(GMaxRHIShaderPlatform);
	bMobileSupportsGPUScene = CVarMobileSupportGPUScene->GetValueOnAnyThread() != 0;
	
	bInitialized = true;
}

bool FReadOnlyCVARCache::AllowStaticLighting()
{
	checkSlow(bInitialized);
	return bAllowStaticLighting;
}

bool FReadOnlyCVARCache::EnableLowQualityLightmaps()
{
	return bEnableLowQualityLightmaps;
}

bool FReadOnlyCVARCache::SupportSkyAtmosphere()
{
	return bSupportSkyAtmosphere;
}

bool FReadOnlyCVARCache::EnablePointLightShadows()
{
	return bEnablePointLightShadows;
}

bool FReadOnlyCVARCache::EnableStationarySkylight()
{
	return bEnableStationarySkylight;
}

bool FReadOnlyCVARCache::MobileHDR()
{
	checkSlow(bInitialized);
	return bMobileHDR;
}

int32 FReadOnlyCVARCache::MobileSkyLightPermutation()
{
	return MobileSkyLightPermutationValue;
}

bool FReadOnlyCVARCache::MobileEnableNoPrecomputedLightingCSMShader()
{
	return bMobileEnableNoPrecomputedLightingCSMShader;
}

bool FReadOnlyCVARCache::MobileEnableStaticAndCSMShadowReceivers()
{
	return bMobileEnableStaticAndCSMShadowReceivers;
}

bool FReadOnlyCVARCache::MobileEnableMovableLightCSMShaderCulling()
{
	return bMobileEnableMovableLightCSMShaderCulling;
}

bool FReadOnlyCVARCache::MobileSupportsGPUScene()
{
	checkSlow(bInitialized);
	return bMobileSupportsGPUScene;
}

bool FReadOnlyCVARCache::MobileAllowMovableDirectionalLights()
{
	return bMobileAllowMovableDirectionalLights;
}

bool FReadOnlyCVARCache::MobileAllowDistanceFieldShadows()
{
	return bMobileAllowDistanceFieldShadows;
}

int32 FReadOnlyCVARCache::MobileEarlyZPass(const FStaticShaderPlatform Platform)
{
#if WITH_EDITOR
	return MobileEarlyZPassIniValue(Platform);
#else
	return MobileEarlyZPassValue;
#endif
}

int32 FReadOnlyCVARCache::MobileForwardLocalLights(const FStaticShaderPlatform Platform)
{
#if WITH_EDITOR
	return MobileForwardLocalLightsIniValue(Platform);
#else
	return MobileForwardLocalLightsValue;
#endif
}

bool FReadOnlyCVARCache::MobileDeferredShading(const FStaticShaderPlatform Platform)
{
#if WITH_EDITOR
	return MobileDeferredShadingIniValue(Platform);
#else
	return bMobileDeferredShadingValue;
#endif
}

bool FReadOnlyCVARCache::MobileEnableMovableSpotlightsShadow(const FStaticShaderPlatform Platform)
{
#if WITH_EDITOR
	return MobileEnableMovableSpotlightsShadowIniValue(Platform);
#else
	return bMobileEnableMovableSpotlightsShadowValue;
#endif
}