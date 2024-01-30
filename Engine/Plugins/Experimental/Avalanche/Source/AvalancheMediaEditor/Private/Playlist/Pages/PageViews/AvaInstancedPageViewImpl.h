// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaPageViewImpl.h"
#include "Playlist/Pages/PageViews/AvaInstancedPageView.h"

class FAvaInstancedPageViewImpl : public FAvaPageViewImpl, public IAvaInstancedPageView
{
public:
	UE_AVA_INHERITS(FAvaInstancedPageViewImpl, FAvaPageViewImpl, IAvaInstancedPageView);

	FAvaInstancedPageViewImpl(int32 InPageId, UAvalanchePlaylist* InPlaylist, const TSharedPtr<SAvaPageList>& InPageList);

	virtual bool IsTemplate() const override;

	virtual FReply OnPlayButtonClicked() override;
	virtual bool CanPlay() const override;

	virtual ECheckBoxState IsEnabled() const override;
	virtual void SetEnabled(ECheckBoxState InState) override;

	virtual FName GetChannelName() const override;
	virtual bool SetChannel(FName InChannel) override;

	virtual const FAvalanchePage& GetTemplate() const override;
	virtual FText GetTemplateDescription() const override;
};
