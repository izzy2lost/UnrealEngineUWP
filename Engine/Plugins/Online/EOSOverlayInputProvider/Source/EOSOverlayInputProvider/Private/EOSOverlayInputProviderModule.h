// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "EOSOverlayInputProviderPreProcessor.h"

class FEOSOverlayInputProviderModule : public IModuleInterface
{
public:
	FEOSOverlayInputProviderModule() = default;
	~FEOSOverlayInputProviderModule() = default;

private:
	void StartupTicker();
	void ShutdownTicker();
	bool Tick(float);

	// ~Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// ~End IModuleInterface

	bool IsRenderReady();

private:
	FTSTicker::FDelegateHandle TickerHandle;

	bool bRenderReady = false;

	// Currently common input wants to be index 0 and will conflict if this input processor
	// also requests that index. This conflict will need to be resolved before the EOS social
	// overlay can be used.
	// For now choose index 1 which allows the overlay to work with logins.
	static constexpr int32 EOSInputProcessorIndex = 1;

	TSharedPtr<FEOSOverlayInputProviderPreProcessor> InputPreprocessor;
};