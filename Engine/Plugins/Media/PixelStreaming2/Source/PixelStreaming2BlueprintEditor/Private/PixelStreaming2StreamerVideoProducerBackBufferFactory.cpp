// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelStreaming2StreamerVideoProducerBackBufferFactory.h"

#include <AssetTypeCategories.h>
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "Modules/ModuleManager.h"
#include "PixelStreaming2StreamerVideoProducerBackBuffer.h"

#define LOCTEXT_NAMESPACE "PixelStreaming2"

UPixelStreaming2StreamerVideoProducerBackBufferFactory::UPixelStreaming2StreamerVideoProducerBackBufferFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UPixelStreaming2StreamerVideoProducerBackBuffer::StaticClass();
}

FText UPixelStreaming2StreamerVideoProducerBackBufferFactory::GetDisplayName() const
{
	return LOCTEXT("VideoProducerBackBufferDisplayName", "Back Buffer Streamer Video Input");
}

uint32 UPixelStreaming2StreamerVideoProducerBackBufferFactory::GetMenuCategories() const
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	return AssetTools.RegisterAdvancedAssetCategory("PixelStreaming2", LOCTEXT("AssetCategoryDisplayName", "PixelStreaming2"));
}

UObject* UPixelStreaming2StreamerVideoProducerBackBufferFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	if (UPixelStreaming2StreamerVideoProducerBackBuffer* Resource = NewObject<UPixelStreaming2StreamerVideoProducerBackBuffer>(InParent, InName, Flags | RF_Transactional))
	{
		return Resource;
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE
