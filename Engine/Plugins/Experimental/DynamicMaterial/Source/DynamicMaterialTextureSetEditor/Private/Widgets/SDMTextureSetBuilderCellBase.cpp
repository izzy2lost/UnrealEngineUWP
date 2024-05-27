// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SDMTextureSetBuilderCellBase.h"

#include "AssetRegistry/AssetData.h"
#include "Engine/Texture.h"
#include "Widgets/DMTextureSetBuilderDragDropOperation.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SDMTextureSetBuilder.h"

#define LOCTEXT_NAMESPACE "SDMTextureSetBuilderCellBase"

SDMTextureSetBuilderCellBase::SDMTextureSetBuilderCellBase()
	: Index(-1)
	, bIsMaterialProperty(false)
{
}

void SDMTextureSetBuilderCellBase::Construct(const FArguments& InArgs, const TSharedRef<SDMTextureSetBuilder>& InTextureSetBuilder,
	UTexture* InTexture, int32 InIndex, bool bInIsMaterialProperty)
{
	TextureSetBuilderWeak = InTextureSetBuilder;
	Texture.Reset(InTexture);
	Index = InIndex;
	bIsMaterialProperty = bInIsMaterialProperty;
}

UTexture* SDMTextureSetBuilderCellBase::GetTexture() const
{
	return Texture.Get();
}

void SDMTextureSetBuilderCellBase::SetTexture(UTexture* InTexture)
{
	Texture.Reset(InTexture);
}

FReply SDMTextureSetBuilderCellBase::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && Texture.IsValid())
	{
		return FReply::Handled().DetectDrag(SharedThis(this), EKeys::LeftMouseButton);
	}

	return SCompoundWidget::OnMouseButtonDown(MyGeometry, MouseEvent);
}

FReply SDMTextureSetBuilderCellBase::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const TSharedRef<FDMTextureSetBuilderDragDropOperation> Operation = FDMTextureSetBuilderDragDropOperation::New(
		FAssetData(Texture.Get()),
		Index,
		bIsMaterialProperty
	);

	return FReply::Handled().BeginDragDrop(Operation);
}

bool SDMTextureSetBuilderCellBase::OnAssetDraggedOver(TArrayView<FAssetData> InAssets)
{
	for (const FAssetData& Asset : InAssets)
	{
		UClass* AssetClass = Asset.GetClass(EResolveClass::Yes);

		if (AssetClass && AssetClass->IsChildOf(UTexture::StaticClass()))
		{
			return true;
		}
	}

	return false;
}

void SDMTextureSetBuilderCellBase::OnAssetsDropped(const FDragDropEvent& InDragDropEvent, TArrayView<FAssetData> InAssets)
{
	if (TSharedPtr<FDMTextureSetBuilderDragDropOperation> BuilderOperation = InDragDropEvent.GetOperationAs<FDMTextureSetBuilderDragDropOperation>())
	{
		if (TSharedPtr<SDMTextureSetBuilder> TextureSetBuilder = TextureSetBuilderWeak.Pin())
		{
			TextureSetBuilder->SwapTexture(
				BuilderOperation->GetIndex(),
				BuilderOperation->IsMaterialProperty(),
				Index,
				bIsMaterialProperty
			);
		}
	}
}

EVisibility SDMTextureSetBuilderCellBase::GetImageVisibility() const
{
	return Texture.IsValid() ? EVisibility::Visible : EVisibility::Hidden;
}

FText SDMTextureSetBuilderCellBase::GetToolTipText() const
{
	UTexture* TextureObject = Texture.Get();

	if (!TextureObject)
	{
		return LOCTEXT("NoTexture", "Texture slot empty.");
	}

	return FText::FromString(TextureObject->GetPathName());
}

FText SDMTextureSetBuilderCellBase::GetTextureName() const
{
	UTexture* TextureObject = Texture.Get();

	if (!TextureObject)
	{
		return LOCTEXT("-", "-");
	}

	return FText::FromString(TextureObject->GetName());
}

#undef LOCTEXT_NAMESPACE
