// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaterBodyInfoMeshComponent.h"
#include "StaticMeshSceneProxy.h"
#include  "UObject/UObjectIterator.h"
#include "Rendering/CustomRenderPass.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WaterBodyInfoMeshComponent)

void OnCVarWaterInfoSceneProxiesValueChanged(IConsoleVariable*)
{
	for (UWaterBodyInfoMeshComponent* WaterBodyInfoMeshComponent : TObjectRange<UWaterBodyInfoMeshComponent>(RF_ClassDefaultObject | RF_ArchetypeObject, true, EInternalObjectFlags::Garbage))
	{
		WaterBodyInfoMeshComponent->MarkRenderStateDirty();
	}
}

static TAutoConsoleVariable<int32> CVarShowWaterInfoSceneProxies(
	TEXT("r.Water.WaterInfo.ShowSceneProxies"),
	0,
	TEXT("When enabled, shows the water scene proxies in the main viewport. Useful for debugging only.\n")
	TEXT("0: Disabled\n")
	TEXT("1: Only show water info mesh\n")
	TEXT("2: Only show water info dilated mesh\n")
	TEXT("3: Show both water info meshes"),
	FConsoleVariableDelegate::CreateStatic(OnCVarWaterInfoSceneProxiesValueChanged),
	ECVF_RenderThreadSafe
);

int32 GetWaterInfoRenderingMethod()
{
	static IConsoleVariable* CVarWaterInfoRenderMethod = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Water.WaterInfo.RenderMethod"));
	return CVarWaterInfoRenderMethod ? CVarWaterInfoRenderMethod->GetInt() : 0;
}

UWaterBodyInfoMeshComponent::UWaterBodyInfoMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAffectDistanceFieldLighting = false;
	bSelectable = false;
	
	// Skip computing the bounds for this component since it should always be attached to the water body component and those bounds are the proper bounds for the info meshes.
	bUseAttachParentBound = true;
}

FPrimitiveSceneProxy* UWaterBodyInfoMeshComponent::CreateSceneProxy()
{
	if (!CanCreateSceneProxy())
	{
		return nullptr;
	}
	return new FWaterBodyInfoMeshSceneProxy(this, bIsDilatedMesh);
}

FWaterBodyInfoMeshSceneProxy::FWaterBodyInfoMeshSceneProxy(UWaterBodyInfoMeshComponent* Component, bool InbIsDilatedMesh)
	: FStaticMeshSceneProxy(Component, true),
	bIsDilatedMesh(InbIsDilatedMesh)
{
	// Disable Notify on WorldAddRemove. This prevents the component from being unhidden in FPrimitiveSceneProxy::OnLevelAddedToWorld_RenderThread if it was part of a streamed level.
	// WaterInfo proxies should only be unhidden during WaterInfo passes.
	bShouldNotifyOnWorldAddRemove = false;

	// When water info mesh is rendered with custom render passes, do not disable the mesh
	if (GetWaterInfoRenderingMethod() != 2)
	{
		SetEnabled(false);
	}
}

bool FWaterBodyInfoMeshSceneProxy::GetMeshElement(int32 LODIndex, int32 BatchIndex, int32 ElementIndex, uint8 InDepthPriorityGroup, bool bUseSelectionOutline, bool bAllowPreCulledIndices, FMeshBatch& OutMeshBatch) const
{
	bool bResult = FStaticMeshSceneProxy::GetMeshElement(LODIndex, BatchIndex, ElementIndex, InDepthPriorityGroup, bUseSelectionOutline, bAllowPreCulledIndices, OutMeshBatch);
	if (bResult)
	{
		OutMeshBatch.bUseForWaterInfoTextureDepth = bIsDilatedMesh; // The dilated mesh is drawn in the water info texture depth-only pass
	}
	return bResult;
}

static bool ShouldShowOutsideWaterInfoPass(bool bIsDilatedMesh)
{
#if !UE_BUILD_SHIPPING
	const int32 ShowWaterInfoSceneProxiesValue = CVarShowWaterInfoSceneProxies.GetValueOnAnyThread();
	return (ShowWaterInfoSceneProxiesValue == 1 && !bIsDilatedMesh) || (ShowWaterInfoSceneProxiesValue == 2 && bIsDilatedMesh) || (ShowWaterInfoSceneProxiesValue > 2);
#endif

	return false;
}

void FWaterBodyInfoMeshSceneProxy::SetEnabled(bool bInEnabled)
{
	SetForceHidden(!(bInEnabled || ShouldShowOutsideWaterInfoPass(bIsDilatedMesh)));
}

FPrimitiveViewRelevance FWaterBodyInfoMeshSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result = FStaticMeshSceneProxy::GetViewRelevance(View);

	// When water info mesh is rendered with custom render passes, enable the mesh for drawing
	if (GetWaterInfoRenderingMethod() == 2)
	{
		const FString PassName = View->CustomRenderPass ? const_cast<FSceneView*>(View)->CustomRenderPass->Name : TEXT("");
		Result.bDrawRelevance = (PassName == TEXT("WaterInfoDepthPass") || PassName == TEXT("WaterInfoColorPass") || PassName == TEXT("WaterInfoDilationPass"));
		Result.bShadowRelevance = false;
	}

	Result.bDrawRelevance |= ShouldShowOutsideWaterInfoPass(bIsDilatedMesh);

	return Result;
}