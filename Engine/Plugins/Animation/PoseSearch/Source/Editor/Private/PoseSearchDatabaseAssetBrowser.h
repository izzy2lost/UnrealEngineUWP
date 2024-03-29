// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Layout/SBox.h"

struct FAssetData;

namespace UE::PoseSearch
{
	class FDatabaseViewModel;
	
	class SPoseSearchDatabaseAssetBrowser : public SBox
	{
	public:
	
		SLATE_BEGIN_ARGS(SPoseSearchDatabaseAssetBrowser) {}
		SLATE_END_ARGS();

		void Construct(const FArguments& InArgs, TSharedPtr<FDatabaseViewModel> InViewModel);
		
		void RefreshView();
		
	private:

		TSharedPtr<SBox> AssetBrowserBox;
		TSharedPtr<FDatabaseViewModel> DatabaseViewModel;
		
		void OnAssetDoubleClicked(const FAssetData& AssetData);
		bool OnShouldFilterAsset(const FAssetData& AssetData);
	};
}