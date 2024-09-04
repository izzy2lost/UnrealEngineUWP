// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelStreaming2MediaTextureFactory.h"

#include "AssetTypeCategories.h"
#include "PixelStreaming2MediaTexture.h"

#define LOCTEXT_NAMESPACE "PixelStreaming2"

UPixelStreaming2MediaTextureFactory::UPixelStreaming2MediaTextureFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;

	SupportedClass = UPixelStreaming2MediaTexture::StaticClass();
}

FText UPixelStreaming2MediaTextureFactory::GetDisplayName() const
{
	return LOCTEXT("MediaTextureFactoryDisplayName", "Pixel Streaming 2 Media Texture");
}

uint32 UPixelStreaming2MediaTextureFactory::GetMenuCategories() const
{
	return EAssetTypeCategories::Textures;
}

UObject* UPixelStreaming2MediaTextureFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	if (UPixelStreaming2MediaTexture* Resource = NewObject<UPixelStreaming2MediaTexture>(InParent, InName, Flags | RF_Transactional))
	{
		Resource->UpdateResource();
		return Resource;
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE
