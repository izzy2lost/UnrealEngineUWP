// Copyright Epic Games, Inc. All Rights Reserved.

#include "Visualizers/AvaViewportChannelVisualizer.h"
#include "AvaViewportPostProcessManager.h"
#include "Containers/Map.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/Package.h"
#include "Viewport/Interaction/AvaViewportPostProcessInfo.h"

namespace UE::AvalancheViewport::Private
{
	const FString ChannelReferencerName = FString(TEXT("AvaViewportChannelVisualizer"));

	const TMap<EAvaViewportPostProcessType, TSoftObjectPtr<UMaterial>> ChannelAssets = {
		{ EAvaViewportPostProcessType::RedChannel,   TSoftObjectPtr<UMaterial>(FSoftObjectPath("/Script/Engine.Material'/Avalanche/EditorResources/M_AvalancheChannelVisualizer_Red.M_AvalancheChannelVisualizer_Red'")) },
		{ EAvaViewportPostProcessType::GreenChannel, TSoftObjectPtr<UMaterial>(FSoftObjectPath("/Script/Engine.Material'/Avalanche/EditorResources/M_AvalancheChannelVisualizer_Green.M_AvalancheChannelVisualizer_Green'")) },
		{ EAvaViewportPostProcessType::BlueChannel,  TSoftObjectPtr<UMaterial>(FSoftObjectPath("/Script/Engine.Material'/Avalanche/EditorResources/M_AvalancheChannelVisualizer_Blue.M_AvalancheChannelVisualizer_Blue'")) },
		{ EAvaViewportPostProcessType::AlphaChannel, TSoftObjectPtr<UMaterial>(FSoftObjectPath("/Script/Engine.Material'/Avalanche/EditorResources/M_AvalancheChannelVisualizer_Alpha.M_AvalancheChannelVisualizer_Alpha'")) }
	};
}

FAvaViewportChannelVisualizer::FAvaViewportChannelVisualizer(TSharedRef<IAvaViewportClient> InAvaViewportClient, EAvaViewportPostProcessType InChannel)
	: FAvaViewportPostProcessVisualizer(InAvaViewportClient)
{
	bRequiresTonemapperSetting = (InChannel == EAvaViewportPostProcessType::AlphaChannel);

	using namespace UE::AvalancheViewport::Private;

	if (const TSoftObjectPtr<UMaterial>* BaseMaterialPtr = ChannelAssets.Find(InChannel))
	{
		if (UMaterial* BaseMaterial = BaseMaterialPtr->LoadSynchronous())
		{
			PostProcessBaseMaterial = BaseMaterial;
			PostProcessMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, GetTransientPackage());
		}
	}
}

FString FAvaViewportChannelVisualizer::GetReferencerName() const
{
	return UE::AvalancheViewport::Private::ChannelReferencerName;
}
