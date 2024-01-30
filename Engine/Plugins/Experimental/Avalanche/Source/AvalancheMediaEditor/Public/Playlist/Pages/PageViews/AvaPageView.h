// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaType.h"
#include "CoreMinimal.h"

class FReply;
class UAvalanchePlaylist;
struct FAssetData;

enum class EAvaPageActionState : uint8
{
	None,
	Requested,
	Completed,
	Cancelled,
};

enum class EAvaPageViewSelectionChangeType : uint8
{
	Deselect,
	ReplaceSelection,
	AddToSelection
};

class IAvaPageView : public IAvaTypeCastable, public TSharedFromThis<IAvaPageView>
{
public:
	UE_AVA_INHERITS(IAvaPageView, IAvaTypeCastable);

	virtual UAvalanchePlaylist* GetPlaylist() const = 0;
	
	virtual int32 GetPageId() const = 0;
	virtual FText GetPageIdText() const = 0;
	virtual FText GetPageNameText() const = 0;
	virtual FText GetPageTransitionLayerNameText() const = 0;

	virtual FText GetPageSummary() const = 0;
	virtual FText GetPageDescription() const = 0;

	virtual bool IsTemplate() const = 0;

	virtual bool HasObjectPath(const UAvalanchePlaylist* InPlaylist) const = 0;
	virtual FSoftObjectPath GetObjectPath(const UAvalanchePlaylist* InPlaylist) const = 0;
	virtual FText GetObjectName(const UAvalanchePlaylist* InPlaylist) const = 0;
	virtual void OnObjectChanged(const FAssetData& InAssetData) = 0;

	virtual bool Rename(const FText& InNewName) = 0;
	virtual bool RenameFriendlyName(const FText& InNewName) = 0;
	
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPageAction, EAvaPageActionState);
	virtual FOnPageAction& GetOnRename() = 0;
	virtual FOnPageAction& GetOnRenumber() = 0;

	virtual FReply OnAssetStatusButtonClicked() = 0;
	virtual bool CanChangeAssetStatus() const = 0;

	virtual FReply OnPreviewButtonClicked() = 0;
	virtual bool CanPreview() const = 0;

	virtual FReply OnPlayButtonClicked() = 0;
	virtual bool CanPlay() const = 0;

	virtual bool IsPageSelected() const = 0;
	virtual bool SetPageSelection(EAvaPageViewSelectionChangeType InSelectionChangeType) = 0;
};
