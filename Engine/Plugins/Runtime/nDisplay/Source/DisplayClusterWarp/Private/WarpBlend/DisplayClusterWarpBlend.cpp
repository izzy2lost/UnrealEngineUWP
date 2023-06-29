// Copyright Epic Games, Inc. All Rights Reserved.

#include "WarpBlend/DisplayClusterWarpBlend.h"
#include "WarpBlend/Math/DisplayClusterWarpBlendMath_Frustum.h"

#include "Render/Containers/IDisplayClusterRender_MeshComponent.h"

#include "HAL/IConsoleManager.h"
#include "ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"

// Setup frustum projection cache
static TAutoConsoleVariable<int32> CVarMPCDIFrustumCacheDepth(
	TEXT("nDisplay.render.mpcdi.cache_depth"),
	0,// Default disabled
	TEXT("Frustum values cache (depth, num).\n")
	TEXT("By default cache is disabled. For better performance (EDisplayClusterWarpBlendFrustumType::FULL) set value to 512).\n")
	TEXT(" 0: Disabled\n")
	TEXT(" N: Cache size, integer\n"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarMPCDIFrustumCachePrecision(
	TEXT("nDisplay.render.mpcdi.cache_precision"),
	0.1f, // 1mm
	TEXT("Frustum cache values comparison precision (float, unit is sm).\n"),
	ECVF_RenderThreadSafe
);

//--------------------------------------------------------------------------------------------
// FDisplayClusterWarpBlend
//--------------------------------------------------------------------------------------------
FDisplayClusterWarpBlend::FDisplayClusterWarpBlend()
{
	WarpData.AddDefaulted(2);
}

FDisplayClusterWarpBlend::~FDisplayClusterWarpBlend()
{
	// Release resources
	GeometryContext.GeometryProxy.ReleaseResources();
}

bool FDisplayClusterWarpBlend::MarkWarpGeometryComponentDirty(const FName& InComponentName)
{
	return GeometryContext.GeometryProxy.MarkWarpFrustumGeometryComponentDirty(InComponentName);
}

bool FDisplayClusterWarpBlend::ShouldSupportICVFX() const
{
	if (GetWarpProfileType() == EDisplayClusterWarpProfileType::warp_A3D)
	{
		return true;
	}

	return false;
}

bool FDisplayClusterWarpBlend::UpdateGeometryContext(const float InWorldScale)
{
	return GeometryContext.UpdateGeometryContext(InWorldScale);
}

const FDisplayClusterWarpGeometryContext& FDisplayClusterWarpBlend::GetGeometryContext() const
{
	return GeometryContext.Context;
}

bool FDisplayClusterWarpBlend::CalcFrustumContext(const TSharedPtr<FDisplayClusterWarpEye, ESPMode::ThreadSafe>& InWarpEye)
{
	if(!InWarpEye.IsValid())
	{
		return false;
	}

	// Update current warp data
	check(WarpData.IsValidIndex(InWarpEye->ContextNum));
	FDisplayClusterWarpData& CurrentWarpData = WarpData[InWarpEye->ContextNum];
	CurrentWarpData.WarpEye = InWarpEye;

	FDisplayClusterWarpBlendMath_Frustum Frustum(CurrentWarpData, GeometryContext);

	// Override settings from warp policy
	BeginCalcFrustum(InWarpEye);

	if (InWarpEye->bUpdateGeometryContext)
	{
		if (!GeometryContext.UpdateGeometryContext(InWarpEye->WorldScale))
		{
			// wrong geometry
			return false;
		}
	}

	if (!Frustum.CalcFrustum())
	{
		return false;
	}

	return true;
}

void FDisplayClusterWarpBlend::BeginCalcFrustum(const TSharedPtr<FDisplayClusterWarpEye, ESPMode::ThreadSafe>& InWarpEye)
{
	check(InWarpEye.IsValid());

	if (InWarpEye->WarpPolicy.IsValid())
	{
		if (IDisplayClusterViewport* Viewport = InWarpEye->GetViewport())
		{
			InWarpEye->WarpPolicy->BeginCalcFrustum(Viewport, InWarpEye->ContextNum);
		}
	}
}

FDisplayClusterWarpData& FDisplayClusterWarpBlend::GetWarpData(const uint32 ContextNum)
{
	check(WarpData.IsValidIndex(ContextNum));

	return WarpData[ContextNum];

}

const FDisplayClusterWarpData& FDisplayClusterWarpBlend::GetWarpData(const uint32 ContextNum) const
{
	check(WarpData.IsValidIndex(ContextNum));

	return WarpData[ContextNum];

}

UMeshComponent* FDisplayClusterWarpBlend::GetStaticMeshComponent() const
{
	const TSharedPtr<IDisplayClusterRender_MeshComponent, ESPMode::ThreadSafe>& MeshComponent = GeometryContext.GeometryProxy.MeshComponent;
	if (MeshComponent.IsValid())
	{
		switch (MeshComponent->GetGeometrySource())
		{
		case EDisplayClusterRender_MeshComponentGeometrySource::StaticMeshComponentRef:
			return MeshComponent->GetStaticMeshComponent();

		case EDisplayClusterRender_MeshComponentGeometrySource::ProceduralMeshComponentRef:
			return MeshComponent->GetProceduralMeshComponent();

		default:
			break;
		}
	}

	return nullptr;
}

bool FDisplayClusterWarpBlend::ExportWarpMapGeometry(FDisplayClusterWarpGeometryOBJ& OutMeshData, uint32 InMaxDimension) const
{
#if WITH_EDITOR
	return FDisplayClusterWarpBlendExporter_WarpMap::ExportWarpMap(GeometryContext, OutMeshData, InMaxDimension);
#endif
	return false;
}
