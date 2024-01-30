// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaPageViewImpl.h"
#include "Playlist/Pages/PageViews/AvaTemplatePageView.h"

class FReply;
struct FAssetData;

class FAvaTemplatePageViewImpl : public FAvaPageViewImpl, public IAvaTemplatePageView
{
public:
	UE_AVA_INHERITS(FAvaTemplatePageViewImpl, FAvaPageViewImpl, IAvaTemplatePageView);

	FAvaTemplatePageViewImpl(int32 InPageId, UAvalanchePlaylist* InPlaylist, const TSharedPtr<SAvaPageList>& InPageList);

	virtual UAvalanchePlaylist* GetPlaylist() const override;

	virtual bool IsTemplate() const override;
	virtual FReply OnPlayButtonClicked() override;
	virtual bool CanPlay() const override;

	FReply OnSyncStatusButtonClicked();
	bool CanChangeSyncStatus() const;
};
