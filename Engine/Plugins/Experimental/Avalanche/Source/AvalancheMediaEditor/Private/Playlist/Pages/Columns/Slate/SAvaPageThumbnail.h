// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Playlist/AvaPlaylistDefines.h"
#include "Templates/SharedPointer.h"
#include "Widgets/SCompoundWidget.h"

class FAssetThumbnail;
class IToolTip;
class SAvaPageViewRow;
class UAvalanchePlaylist;
enum class EAvaPageChanges : uint8;

class SAvaPageThumbnail : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SAvaPageThumbnail)
		: _ThumbnailWidgetSize(64)
	{}

	SLATE_ARGUMENT(int32, ThumbnailWidgetSize)

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, const FAvaPageViewPtr& InPageView, const TSharedPtr<SAvaPageViewRow>& InRow);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	~SAvaPageThumbnail() override;

private:
	void OnAssetUpdate(const UAvalanchePlaylist* InPlaylist, const FAvalanchePage& InPage, EAvaPageChanges InPageChangeType);

	void InitThumbnailWidget();

private:
	TWeakPtr<IAvaPageView> PageViewWeak;

	TWeakPtr<SAvaPageViewRow> PageViewRowWeak;

	TSharedPtr<FAssetThumbnail> AssetThumbnail;

	TSharedPtr<SWidget> ThumbnailWidget;

	TSharedPtr<IToolTip> Tooltip;

	bool bWasAssetLoaded = false;

	int32 ThumbnailWidgetSize = 0;
};
