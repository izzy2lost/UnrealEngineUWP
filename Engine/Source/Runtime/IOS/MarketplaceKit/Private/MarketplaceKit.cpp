// Copyright Epic Games, Inc. All Rights Reserved.

#include "MarketplaceKit.h"
#include "MarketplaceKitWrapper.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogMarketplaceKit, Log, All);

void FMarketplaceKitModule::StartupModule()
{
}

void FMarketplaceKitModule::ShutdownModule()
{
}

bool FMarketplaceKitModule::SupportsDynamicReloading()
{
	return true;
}

static constexpr EMarketplaceType ConvertMarketplaceType(const AppDistributorType Type)
{
	switch (Type)
	{
		case AppDistributorTypeAppStore:		return EMarketplaceType::AppStore;
		case AppDistributorTypeTestFlight:		return EMarketplaceType::TestFlight;
		case AppDistributorTypeMarketplace:		return EMarketplaceType::Marketplace;
		case AppDistributorTypeWeb:				return EMarketplaceType::Web;
		case AppDistributorTypeOther:			return EMarketplaceType::Other;
		case AppDistributorTypeNotAvailable:	return EMarketplaceType::NotAvailable;
		default:								return EMarketplaceType::Other;
	}
}

void FMarketplaceKitModule::GetCurrentType(TFunction<void(EMarketplaceType Type, const FString& Name)> Callback)
{
	[AppDistributorWrapper getCurrentWithCompletionHandler:^(enum AppDistributorType Type, NSString* _Nonnull Name)
	{
		const EMarketplaceType ConvertedType = ConvertMarketplaceType(Type);
		const FString ConvertedName(Name);

		UE_LOG(LogMarketplaceKit, Log, TEXT("AppDistributorWrapper getCurrentWithCompletionHandler %i %s"), (int32)ConvertedType, *ConvertedName);

		Callback(ConvertedType, ConvertedName);
	}];
}

IMPLEMENT_MODULE(FMarketplaceKitModule, MarketplaceKit);
