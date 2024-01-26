// Copyright Epic Games, Inc. All Rights Reserved.

#include "SGMEResourceItem.h"

#include "Engine/TextureRenderTarget2D.h"
#include "SlateOptMacros.h"
#include "ViewModels/GMEResourceItemViewModel.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SGMEResourceItem::Construct(
	const FArguments& InArgs,
	const TSharedRef<STableViewBase>& InOwnerTableView,
	const TSharedRef<FGMEResourceItemViewModel>& InViewModel)
{
	ViewModel = InViewModel;

	TextureBrush = MakeShared<FSlateBrush>();
	TextureBrush->SetResourceObject(const_cast<UTextureRenderTarget2D*>(InViewModel->GetResourceTexture()));
	TextureBrush->ImageSize = FVector2D(InViewModel->GetResourceTexture()->SizeX, InViewModel->GetResourceTexture()->SizeY);

	ImageWidget = SNew(SImage)
					.Image(TextureBrush.IsValid()
						? TextureBrush.Get()
						: FAppStyle::GetBrush("WhiteTexture"));
	
	SGMEImageItem::Construct(
		SGMEImageItem::FArguments()
			.Label_Lambda([InViewModel]()
			{
				return FText::FromString(InViewModel->GetDimensions().ToString());
			})
		, InOwnerTableView);
}

FOptionalSize SGMEResourceItem::GetAspectRatio()
{
	float AspectRatio = 16.0f / 9.0f;
	if (const UTexture* ResourceTexture = ViewModel->GetResourceTexture())
	{
		AspectRatio = ResourceTexture->GetSurfaceWidth() / ResourceTexture->GetSurfaceHeight();
	}

	return AspectRatio;
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
