// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelStreaming2StreamerVideoProducerMediaCaptureFactory.h"

#include "AssetToolsModule.h"
#include <AssetTypeCategories.h>
#include "IAssetTools.h"
#include "Modules/ModuleManager.h"
#include "PixelStreaming2StreamerVideoProducerMediaCapture.h"

#define LOCTEXT_NAMESPACE "PixelStreaming2"

UPixelStreaming2StreamerVideoProducerMediaCaptureFactory::UPixelStreaming2StreamerVideoProducerMediaCaptureFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UPixelStreaming2StreamerVideoProducerMediaCapture::StaticClass();
}

FText UPixelStreaming2StreamerVideoProducerMediaCaptureFactory::GetDisplayName() const
{
	return LOCTEXT("VideoProducerMediaCaptureDisplayName", "Media Capture Streamer Video Input");
}

uint32 UPixelStreaming2StreamerVideoProducerMediaCaptureFactory::GetMenuCategories() const
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	return AssetTools.RegisterAdvancedAssetCategory("PixelStreaming2", LOCTEXT("AssetCategoryDisplayName", "PixelStreaming2"));
}

UObject* UPixelStreaming2StreamerVideoProducerMediaCaptureFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	if (UPixelStreaming2StreamerVideoProducerMediaCapture* Resource = NewObject<UPixelStreaming2StreamerVideoProducerMediaCapture>(InParent, InName, Flags | RF_Transactional))
	{
		return Resource;
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE
