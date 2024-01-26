// Copyright Epic Games, Inc. All Rights Reserved.

#include "GMECanvasItemViewModel.h"

#include "Engine/Texture.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GeometryMaskCanvas.h"

TSharedRef<FGMECanvasItemViewModel> FGMECanvasItemViewModel::Create(const TWeakObjectPtr<const UGeometryMaskCanvas>& InCanvas)
{
	TSharedRef<FGMECanvasItemViewModel> ViewModel = MakeShared<FGMECanvasItemViewModel>(FPrivateToken{}, InCanvas);

	return ViewModel;
}

const UTexture* FGMECanvasItemViewModel::GetCanvasTexture() const
{
	if (CanvasTexture.IsValid())
	{
		return CanvasTexture.Get();
	}
	
	return nullptr;
}

float FGMECanvasItemViewModel::GetMemoryUsage()
{
	if (const UTexture* Texture = GetCanvasTexture())
	{
		return (static_cast<float>(Texture->CalcTextureMemorySizeEnum(TMC_ResidentMips)) / (1024.0f * 1024.0f));
	}
	
	return 0.0f;
}

FGMECanvasItemViewModel::FGMECanvasItemViewModel(FPrivateToken, const TWeakObjectPtr<const UGeometryMaskCanvas>& InCanvas)
	: KnownReaderCount(0)
	, KnownWriterCount(0)
{
	if (const UGeometryMaskCanvas* Canvas = InCanvas.Get())
	{
		CanvasName = Canvas->GetCanvasName();
		ColorChannel = Canvas->GetColorChannel();
		CanvasTexture = Canvas->GetTexture();
		KnownWriterCount = Canvas->GetWriters().Num();
	}
}

bool FGMECanvasItemViewModel::GetChildren(TArray<TSharedPtr<IGMETreeNodeViewModel>>& OutChildren)
{
	return false;
}
