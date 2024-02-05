// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playback/Nodes/AvaPlaybackNodeBlueprintPlayer.h"

#include "AvaBlueprint.h"
#include "Playback/AvaPlaybackGraph.h"

#define LOCTEXT_NAMESPACE "AvaPlaybackNodeBlueprintPlayer"

UAvaPlaybackNodeBlueprintPlayer::UAvaPlaybackNodeBlueprintPlayer()
{
	//Update to Default Text
	UpdateDisplayNameText();
}

void UAvaPlaybackNodeBlueprintPlayer::RefreshNode(bool bDryRunGraph)
{
	UpdateDisplayNameText();
	Super::RefreshNode(bDryRunGraph);
}

void UAvaPlaybackNodeBlueprintPlayer::PostLoad()
{
	Super::PostLoad();
	UpdateDisplayNameText();
}

void UAvaPlaybackNodeBlueprintPlayer::SetAsset(const TSoftObjectPtr<UAvalancheBlueprint>& InAsset)
{
	Asset = InAsset;
}

void UAvaPlaybackNodeBlueprintPlayer::UpdateDisplayNameText()
{
	const FString AssetName = Asset.GetAssetName();
	
	if (AssetName.IsEmpty())
	{
		DisplayNameText = LOCTEXT("AvaPlaybackNode_BlueprintPlayerNoName", "Motion Design Blueprint Player");
	}
	else
	{
		DisplayNameText = FText::Format(LOCTEXT("AvaPlaybackNode_BlueprintPlayerName", "Motion Design Blueprint Player\n{0}")
			, FText::FromString(AssetName));
	}
}

FAvaSoftAssetPtr UAvaPlaybackNodeBlueprintPlayer::GetAssetPtr() const
{
	FAvaSoftAssetPtr OutAsset;
	OutAsset.AssetClassPath = FSoftClassPath(UAvalancheBlueprint::StaticClass());
	OutAsset.AssetPtr = Asset;
	return OutAsset;
}

#undef LOCTEXT_NAMESPACE
