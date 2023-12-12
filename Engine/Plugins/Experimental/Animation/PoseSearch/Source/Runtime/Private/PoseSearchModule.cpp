// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimPoseSearchProvider.h"
#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchLibrary.h"

class FPoseSearchModule final : public IModuleInterface, public UE::Anim::IPoseSearchProvider
{
public:
	
	// IModuleInterface
	virtual void StartupModule() override
	{
		IModularFeatures::Get().RegisterModularFeature(UE::Anim::IPoseSearchProvider::GetModularFeatureName(), this);
	}

	virtual void ShutdownModule() override
	{
		IModularFeatures::Get().UnregisterModularFeature(UE::Anim::IPoseSearchProvider::GetModularFeatureName(), this);
	}

	// IPoseSearchProvider
	virtual UE::Anim::IPoseSearchProvider::FSearchResult Search(const FAnimationBaseContext& GraphContext, TConstArrayView<UAnimationAsset*> AnimationAssets,
		const UAnimationAsset* PlayingAnimationAsset, float PlayingAnimationAssetAccumulatedTime) override
	{
		const UE::PoseSearch::FSearchResult SearchResult = UPoseSearchLibrary::MotionMatch(GraphContext, AnimationAssets, PlayingAnimationAsset, PlayingAnimationAssetAccumulatedTime);
		UE::Anim::IPoseSearchProvider::FSearchResult ProviderResult;
		if (const UE::PoseSearch::FSearchIndexAsset* SearchIndexAsset = SearchResult.GetSearchIndexAsset())
		{
			if (const FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAssetBase = SearchResult.Database->GetAnimationAssetBase(*SearchIndexAsset))
			{
				ProviderResult.AnimationAsset = DatabaseAnimationAssetBase->GetAnimationAsset();
				ProviderResult.Dissimilarity = SearchResult.PoseCost.GetTotalCost();
				ProviderResult.TimeOffsetSeconds = SearchResult.AssetTime;
			}
		}

		return ProviderResult;
	}
};

IMPLEMENT_MODULE(FPoseSearchModule, PoseSearch);