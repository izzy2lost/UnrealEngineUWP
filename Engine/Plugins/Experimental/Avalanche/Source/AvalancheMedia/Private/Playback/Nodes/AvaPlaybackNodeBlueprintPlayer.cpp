// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playback/Nodes/AvaPlaybackNodeBlueprintPlayer.h"
#include "AvaBlueprint.h"
#include "Framework/AvaGameInstance.h"
#include "Http/AvaMediaHttpServer.h"
#include "Playback/AvalanchePlayback.h"

#define LOCTEXT_NAMESPACE "AvalanchePlayback"

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

void UAvaPlaybackNodeBlueprintPlayer::SetAvalancheAsset(const TSoftObjectPtr<UAvalancheBlueprint>& InAsset)
{
	BlueprintAsset = InAsset;
}

void UAvaPlaybackNodeBlueprintPlayer::UpdateDisplayNameText()
{
	const FString AssetName = BlueprintAsset.GetAssetName();
	
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

FAvaSoftAssetPtr UAvaPlaybackNodeBlueprintPlayer::GetAvalancheAssetPtr() const
{
	FAvaSoftAssetPtr OutAvalancheAsset;
	OutAvalancheAsset.AssetClassPath = FSoftClassPath(UAvalancheBlueprint::StaticClass());
	OutAvalancheAsset.AssetPtr = BlueprintAsset;
	return OutAvalancheAsset;
}

#undef LOCTEXT_NAMESPACE
