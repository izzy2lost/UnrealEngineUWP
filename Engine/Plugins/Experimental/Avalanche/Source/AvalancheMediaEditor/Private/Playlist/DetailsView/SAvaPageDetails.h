// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Playlist/AvaPlaylistDefines.h"
#include "Widgets/SCompoundWidget.h"

class FAvaPlaylistEditor;
class SAvaPageRemoteControlProps;
class SAvaRCControllerPanel;
struct FAvalanchePage;

class SAvaPageDetails : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAvaPageDetails) {}
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, const TSharedPtr<FAvaPlaylistEditor>& InPlaylistEditor);

	virtual ~SAvaPageDetails() override;

	void OnPageEvent(const TArray<int32>& InSelectedPageIds, UE::AvalanchePlaylist::EPageEvent InPageEvent);
	void OnPageSelectionChanged(const TArray<int32>& InSelectedPageIds);
	void OnManagedInstanceCacheEntryInvalidated(const FSoftObjectPath& InAssetPath);

protected:
	FReply ToggleExposedPropertiesVisibility();

	const FSlateBrush* GetExposedPropertiesVisibilityBrush() const;

	const FAvalanchePage& GetSelectedPage() const;
	FAvalanchePage& GetMutableSelectedPage() const;

	void RefreshSelectedPage();

	bool HasSelectedPage() const;

	FText GetPageId() const;

	/** Only update page id on commit. */
	void OnPageIdCommitted(const FText& InNewText, ETextCommit::Type InCommitType);

	FText GetPageDescription() const;

	/** Update page name live. */
	void OnPageNameChanged(const FText& InNewText);

	FReply DuplicateSelectedPage();

private:
	TWeakPtr<FAvaPlaylistEditor> PlaylistEditorWeak;

	TSharedPtr<SAvaPageRemoteControlProps> RemoteControlProps;

	TSharedPtr<SAvaRCControllerPanel> RCControllerPanel;

	bool bRefreshSelectedPageQueued = false;

	int32 ActivePageId;
};
