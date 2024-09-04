// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelStreaming2StreamerVideoProducerRenderTargetFactory.h"

#include <AssetTypeCategories.h>
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "Modules/ModuleManager.h"
#include "PixelStreaming2StreamerVideoProducerRenderTarget.h"

#define LOCTEXT_NAMESPACE "PixelStreaming2"

UPixelStreaming2StreamerVideoProducerRenderTargetFactory::UPixelStreaming2StreamerVideoProducerRenderTargetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;

	SupportedClass = UPixelStreaming2StreamerVideoProducerRenderTarget::StaticClass();
}

FText UPixelStreaming2StreamerVideoProducerRenderTargetFactory::GetDisplayName() const
{
	return LOCTEXT("VideoProducerRenderTargetDisplayName", "Render Target Streamer Video Input");
}

uint32 UPixelStreaming2StreamerVideoProducerRenderTargetFactory::GetMenuCategories() const
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	return AssetTools.RegisterAdvancedAssetCategory("PixelStreaming2", LOCTEXT("AssetCategoryDisplayName", "PixelStreaming2"));
}

UObject* UPixelStreaming2StreamerVideoProducerRenderTargetFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	if (UPixelStreaming2StreamerVideoProducerRenderTarget* Resource = NewObject<UPixelStreaming2StreamerVideoProducerRenderTarget>(InParent, InName, Flags | RF_Transactional))
	{
		return Resource;
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE
