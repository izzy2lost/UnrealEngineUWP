// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "AssetTypeActions_StreamerVideoProducer.h"
#include "CoreUtils.h"
#include "PixelStreaming2Style.h"

#define IMAGE_BRUSH_SVG(Style, RelativePath, ...) FSlateVectorImageBrush(Style.RootToContentDir(RelativePath, TEXT(".svg")), __VA_ARGS__)

class FPixelStreaming2BlueprintEditorModule : public IModuleInterface
{
public:
private:
	/** IModuleInterface implementation */
	void StartupModule() override
	{
		if (!UE::PixelStreaming2::IsStreamingSupported())
		{
			return;
		}

		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		AssetTools.RegisterAssetTypeActions(MakeShared<FAssetTypeActions_StreamerVideoProducer>());

		FSlateStyleSet& StyleInstance = UE::EditorPixelStreaming2::FPixelStreaming2Style::Get();

		const FVector2D Icon20x20(20.0f, 20.0f);
		const FVector2D Icon64x64(64.0f, 64.0f);

		StyleInstance.Set("ClassThumbnail.PixelStreaming2StreamerVideoProducerBackBuffer", new IMAGE_BRUSH_SVG(StyleInstance, "PixelStreaming2_64", Icon64x64));
		StyleInstance.Set("ClassIcon.PixelStreaming2StreamerVideoProducerBackBuffer", new IMAGE_BRUSH_SVG(StyleInstance, "PixelStreaming2_20", Icon20x20));
		StyleInstance.Set("ClassThumbnail.PixelStreaming2StreamerVideoProducerRenderTarget", new IMAGE_BRUSH_SVG(StyleInstance, "PixelStreaming2_64", Icon64x64));
		StyleInstance.Set("ClassIcon.PixelStreaming2StreamerVideoProducerRenderTarget", new IMAGE_BRUSH_SVG(StyleInstance, "PixelStreaming2_20", Icon20x20));
		StyleInstance.Set("ClassThumbnail.PixelStreaming2StreamerVideoProducerMediaCapture", new IMAGE_BRUSH_SVG(StyleInstance, "PixelStreaming2_64", Icon64x64));
		StyleInstance.Set("ClassIcon.PixelStreaming2StreamerVideoProducerMediaCapture", new IMAGE_BRUSH_SVG(StyleInstance, "PixelStreaming2_20", Icon20x20));
	}

	void ShutdownModule() override
	{
		if (!UE::PixelStreaming2::IsStreamingSupported())
		{
			return;
		}
	}
};

IMPLEMENT_MODULE(FPixelStreaming2BlueprintEditorModule, PixelStreaming2BlueprintEditor)
