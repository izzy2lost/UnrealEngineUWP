// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Playlist/Pages/PageViews/AvaPageView.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class FReply;
class SAvaPageList;
class UAvalanchePlaylist;
struct FAssetData;
struct FAvalanchePage;

class FAvaPageViewImpl : public IAvaPageView
{
public:
	UE_AVA_INHERITS(FAvaPageViewImpl, IAvaPageView);

	FAvaPageViewImpl(int32 InPageId, UAvalanchePlaylist* InPlaylist, const TSharedPtr<SAvaPageList>& InPageList);

	virtual UAvalanchePlaylist* GetPlaylist() const override;
	
	virtual int32 GetPageId() const override;
	virtual FText GetPageIdText() const override;
	virtual FText GetPageNameText() const override;
	virtual FText GetPageTransitionLayerNameText() const override;
	virtual FText GetPageSummary() const override;
	virtual FText GetPageDescription() const override;

	virtual bool HasObjectPath(const UAvalanchePlaylist* InPlaylist) const override;
	virtual FSoftObjectPath GetObjectPath(const UAvalanchePlaylist* InPlaylist) const override;
	virtual FText GetObjectName(const UAvalanchePlaylist* InPlaylist) const override;
	virtual void OnObjectChanged(const FAssetData& InAssetData) override;

	virtual bool Rename(const FText& InNewName) override;
	virtual bool RenameFriendlyName(const FText& InNewName) override;
	virtual FOnPageAction& GetOnRename() override { return OnRename; }
	virtual FOnPageAction& GetOnRenumber() override { return OnRenumber; }

	virtual FReply OnAssetStatusButtonClicked() override;
	virtual bool CanChangeAssetStatus() const override;
	
	virtual FReply OnPreviewButtonClicked() override;
	virtual bool CanPreview() const override;

	virtual bool IsPageSelected() const override;
	virtual bool SetPageSelection(EAvaPageViewSelectionChangeType InSelectionChangeType) override;

protected:
	/**
	 * Performs the given work on the underlying Page of this Page View
	 * and, if this page is selected, to do this work on the rest of the selected pages too
	 * @param InTransactionSessionName the session name to use for the transaction
	 * @param InWork the function to execute on each page. needs to return true for the work to be considered done
	 * @return whether any work was performed and a transaction was completed 
	 */
	bool PerformWorkOnPages(const FText& InTransactionSessionName, TFunction<bool(FAvalanchePage&)>&& InWork);
	
	const FAvalanchePage& GetPage() const;

protected:
	/** The Id used to search the Actual Page from Playlist */
	int32 PageId;
	
	TWeakObjectPtr<UAvalanchePlaylist> PlaylistWeak;

	TWeakPtr<SAvaPageList> PageListWeak;
	
	FOnPageAction OnRename;
	
	FOnPageAction OnRenumber;

};
