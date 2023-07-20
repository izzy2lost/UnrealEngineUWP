// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaterViewExtension.h"
#include "WaterBodyComponent.h"
#include "WaterZoneActor.h"
#include "EngineUtils.h"
#include "SceneView.h"
#include "WaterMeshComponent.h"

static TAutoConsoleVariable<bool> CVarLocalTessellationFreeze(
	TEXT("r.Water.WaterMesh.LocalTessellation.Freeze"),
	false,
	TEXT("Pauses the local tessellation updates to allow the view to move forward without moving the sliding window.\n")
	TEXT("Can be used to view things outside the sliding window more closely."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarLocalTessellationUpdateMargin(
	TEXT("r.Water.WaterMesh.LocalTessellation.UpdateMargin"),
	15000.,
	TEXT("Controls the minimum distance between the view and the edge of the dynamic water mesh when local tessellation is enabled.\n")
	TEXT("If the view is less than UpdateMargin units away from the edge, it moves the sliding window forward."),
	ECVF_Default);

// ----------------------------------------------------------------------------------

FWaterViewExtension::FWaterViewExtension(const FAutoRegister& AutoReg, UWorld* InWorld)
	: FWorldSceneViewExtension(AutoReg, InWorld)
{
}

FWaterViewExtension::~FWaterViewExtension()
{
}

void FWaterViewExtension::SetupViewFamily(FSceneViewFamily& InViewFamily)
{
}


void FWaterViewExtension::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
	if (CVarLocalTessellationFreeze.GetValueOnGameThread())
	{
		return;
	}

	const FVector ViewLocation = InView.ViewLocation;

	const TWeakObjectPtr<UWorld> WorldPtr = GetWorld();
	check(WorldPtr.IsValid())

	// Prevent re-entrancy. 
	// Since the water info render will update the view extensions we could end up with a re-entrant case.
	static bool bUpdatingWaterInfo = false;
	if (bUpdatingWaterInfo)
	{
		return;
	}
	bUpdatingWaterInfo = true;
	ON_SCOPE_EXIT { bUpdatingWaterInfo = false; };

	for (const TPair<AWaterZone*, UE::WaterInfo::FRenderingContext>& Pair : WaterInfoContextsToRender)
	{
		AWaterZone* WaterZone = Pair.Key;
		check(WaterZone);

		if (WaterZone->IsLocalOnlyTessellationEnabled())
		{
			UWaterMeshComponent* WaterMesh= WaterZone->GetWaterMeshComponent();
			check(WaterMesh);

			const double TileSize = WaterMesh->GetTileSize();

			const FVector WaterMeshCenter(ViewLocation.GridSnap(TileSize));

			const FVector2D WaterInfoHalfExtent(WaterZone->GetDynamicWaterInfoExtent() / 2.0);
			const FBox2D WaterInfoBounds(FVector2D(WaterMeshCenter) - WaterInfoHalfExtent, FVector2D(WaterMeshCenter) + WaterInfoHalfExtent);

			WaterZone->SetLocalTessellationCenter(WaterMeshCenter);

			// Trigger the next update when the camera is <UpdateMargin> units away from the border of the current window.
			const FVector2D UpdateMargin(CVarLocalTessellationUpdateMargin.GetValueOnGameThread());

			// Keep a minimum of <1., 1.> bounds to avoid updating every frame if the update margin is larger than the zone.
			const FVector2D UpdateExtents = FVector2D::Max(FVector2D(1., 1.), WaterInfoHalfExtent - UpdateMargin);
			const FBox2D UpdateBounds(FVector2D(WaterMeshCenter) - UpdateExtents, FVector2D(WaterMeshCenter) + UpdateExtents);

			WaterInfoUpdateBounds.Add(WaterZone, UpdateBounds);

			const FVector2D WaterQuadTreeHalfExtent = WaterMesh->GetLocalTessellationExtentInTiles() * TileSize;
			const FVector2D WaterQuadTreeCenter = WaterMesh->GetDynamicWaterMeshCenter();
			const FBox2D WaterQuadTreeBounds(WaterQuadTreeCenter - WaterQuadTreeHalfExtent, WaterQuadTreeCenter + WaterQuadTreeHalfExtent);

			// If the new water info bounds would be outside the water mesh, recenter and rebuild the water mesh
			if (!WaterQuadTreeBounds.IsInside(WaterInfoBounds.ExpandBy(WaterInfoBounds.GetExtent())))
			{
				WaterMesh->SetDynamicWaterMeshCenter(FVector2D(WaterMeshCenter));
			}
		}

		// Update the material instances now that the tessellated mesh bounds have changed.
		WaterZone->ForEachWaterBodyComponent([](UWaterBodyComponent* WaterBodyComponent)
		{
			WaterBodyComponent->UpdateMaterialInstances();
			return true;
		});

		const UE::WaterInfo::FRenderingContext& Context(Pair.Value);
		UE::WaterInfo::UpdateWaterInfoRendering(WorldPtr.Get()->Scene, Context);
	}

	WaterInfoContextsToRender.Empty();

	// Check if the view location is no longer within the current update bounds of a water zone and if so, queue an update for it.
	for (AWaterZone* WaterZone : TActorRange<AWaterZone>(WorldPtr.Get()))
	{
		if (WaterZone->IsLocalOnlyTessellationEnabled())
		{
			const FBox2D* WaterInfoBounds = WaterInfoUpdateBounds.Find(WaterZone);
			if (WaterInfoBounds == nullptr || !WaterInfoBounds->IsInside(FVector2D(ViewLocation)))
			{
				WaterZone->MarkForRebuild(EWaterZoneRebuildFlags::UpdateWaterInfoTexture);
			}
		}
	}
}

void FWaterViewExtension::PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
}

void FWaterViewExtension::MarkWaterInfoTextureForRebuild(const UE::WaterInfo::FRenderingContext& RenderContext)
{
	WaterInfoContextsToRender.Emplace(RenderContext.ZoneToRender, RenderContext);
}

