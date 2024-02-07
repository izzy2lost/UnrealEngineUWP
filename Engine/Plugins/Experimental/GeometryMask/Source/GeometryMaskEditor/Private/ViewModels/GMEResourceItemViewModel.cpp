// Copyright Epic Games, Inc. All Rights Reserved.

#include "GMEResourceItemViewModel.h"

#include "Engine/CanvasRenderTarget2D.h"
#include "GeometryMaskCanvasResource.h"

TSharedRef<FGMEResourceItemViewModel> FGMEResourceItemViewModel::Create(
	const TWeakObjectPtr<const UGeometryMaskCanvasResource>& InResource)
{
	TSharedRef<FGMEResourceItemViewModel> ViewModel = MakeShared<FGMEResourceItemViewModel>(FPrivateToken{}, InResource);

	return ViewModel;
}

float FGMEResourceItemViewModel::GetMemoryUsage() const
{
	if (const UCanvasRenderTarget2D* Texture = GetResourceTexture())
	{
		return (static_cast<float>(Texture->CalcTextureMemorySizeEnum(TMC_ResidentMips)) / (1024.0f * 1024.0f));
	}
	
	return 0.0f;
}

FIntPoint FGMEResourceItemViewModel::GetDimensions() const
{
	if (const UCanvasRenderTarget2D* Texture = GetResourceTexture())
	{
		return {Texture->SizeX, Texture->SizeY};
	}

	return {0, 0};
}

FGMEResourceItemViewModel::FGMEResourceItemViewModel(
	FPrivateToken,
	const TWeakObjectPtr<const UGeometryMaskCanvasResource>& InResource)
{
	if (const UGeometryMaskCanvasResource* Resource = InResource.Get())
	{
		UniqueId = Resource->GetUniqueID();
		ResourceTextureWeak = const_cast<UGeometryMaskCanvasResource*>(Resource)->GetRenderTargetTexture();
	}
}

bool FGMEResourceItemViewModel::GetChildren(TArray<TSharedPtr<IGMETreeNodeViewModel>>& OutChildren)
{
	return false;
}
