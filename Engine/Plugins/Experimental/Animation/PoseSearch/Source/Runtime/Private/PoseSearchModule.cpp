// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimPoseSearchProvider.h"
#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"
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
	virtual UE::Anim::IPoseSearchProvider::FSearchResult Search(const FAnimationBaseContext& GraphContext, const UObject* Object) override
	{
		const UE::PoseSearch::FSearchResult SearchResult = UPoseSearchLibrary::MotionMatch(GraphContext, Object);
		
		UE::Anim::IPoseSearchProvider::FSearchResult ProviderResult;
		ProviderResult.Dissimilarity = SearchResult.PoseCost.GetTotalCost();
		ProviderResult.PoseIdx = SearchResult.PoseIdx;
		ProviderResult.TimeOffsetSeconds = SearchResult.AssetTime;

		return ProviderResult;
	}
};

IMPLEMENT_MODULE(FPoseSearchModule, PoseSearch);