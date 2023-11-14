// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosModularVehicle/ModularVehicleAssetThumbnailRenderer.h"
#include "Misc/App.h"
#include "ShowFlags.h"
#include "SceneView.h"
#include "ChaosModularVehicle/ModularVehicleAssetThumbnailScene.h"
#include "ChaosModularVehicle/ModularVehicleAsset.h"

UModularVehicleAssetThumbnailRenderer::UModularVehicleAssetThumbnailRenderer(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	ThumbnailScene = nullptr;
}

void UModularVehicleAssetThumbnailRenderer::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* RenderTarget, FCanvas* Canvas, bool bAdditionalViewFamily)
{
	UModularVehicleAsset* ModularVehicleAsset = Cast<UModularVehicleAsset>(Object);
	if (IsValid(ModularVehicleAsset))
	{
		//if (ThumbnailScene == nullptr)
		//{
		//	ThumbnailScene = new FModularVehicleAssetThumbnailScene();
		//}

		//ThumbnailScene->SetModularVehicleAsset(ModularVehicleAsset);
		//ThumbnailScene->GetScene()->UpdateSpeedTreeWind(0.0);

		////FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(RenderTarget, ThumbnailScene->GetScene(), FEngineShowFlags(ESFIM_Game))
		////	.SetTime(UThumbnailRenderer::GetTime())
		////	.SetAdditionalViewFamily(bAdditionalViewFamily));

		//ViewFamily.EngineShowFlags.DisableAdvancedFeatures();
		//ViewFamily.EngineShowFlags.MotionBlur = 0;
		//ViewFamily.EngineShowFlags.LOD = 0;

		//RenderViewFamily(Canvas, &ViewFamily, ThumbnailScene->CreateView(&ViewFamily, X, Y, Width, Height));
		//ThumbnailScene->SetModularVehicleAsset(nullptr);
	}
}

void UModularVehicleAssetThumbnailRenderer::BeginDestroy()
{
	if (ThumbnailScene != nullptr)
	{
		delete ThumbnailScene;
		ThumbnailScene = nullptr;
	}

	Super::BeginDestroy();
}
