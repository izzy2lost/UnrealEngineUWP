// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

enum class EMarketplaceType : int32
{
	AppStore = 0,
	TestFlight = 1,
	Marketplace = 2,
	Web = 3,
	Other = 4,
	NotAvailable = 5,
};

class FMarketplaceKitModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	virtual bool SupportsDynamicReloading() override;
	
	void GetCurrentType(TFunction<void(EMarketplaceType Type, const FString& Name)> Callback);
};
