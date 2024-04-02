// Copyright Epic Games, Inc. All Rights Reserved.

#include "EOSOverlayInputProviderModule.h"

#include "CoreGlobals.h"
#include "Features/IModularFeatures.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"

#include "EOSOverlayInputProviderPreProcessor.h"

IMPLEMENT_MODULE(FEOSOverlayInputProviderModule, EOSOverlayInputProvider);

void FEOSOverlayInputProviderModule::StartupTicker()
{
	if (!TickerHandle.IsValid())
	{
		TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FEOSOverlayInputProviderModule::Tick), 0);
	}
}

void FEOSOverlayInputProviderModule::ShutdownTicker()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}
}

bool FEOSOverlayInputProviderModule::Tick(float DeltaTime)
{
	if (IsRenderReady())
	{
		// HACK: The EOS input processor needs to be first in the list of input processors,
		// but will be scooted down the list when new systems register their own processors.
		// Currently common input also wants to be first and will conflict if this input processor
		// also requests that index. This conflict will need to be resolved before the EOS social
		// overlay can be used.
		// Re-adding the input processor when it is not at its expected location will bump newly
		// added processors down the list.
		if (FSlateApplication::IsInitialized() && InputPreprocessor)
		{
			const int32 CurrentEOSInputProcessorIndex = FSlateApplication::Get().FindInputPreProcessor(InputPreprocessor);
			if (CurrentEOSInputProcessorIndex != EOSInputProcessorIndex)
			{
				UE_LOG(LogEOSSDK, Verbose, TEXT("[%hs] EOS input processor not at expected index, re-registering. CurrentEOSInputProcessorIndex: %d, ExpectedEOSInputProcessorIndex: %d"), __FUNCTION__, CurrentEOSInputProcessorIndex, EOSInputProcessorIndex);
				FSlateApplication::Get().UnregisterInputPreProcessor(InputPreprocessor);
				FSlateApplication::Get().RegisterInputPreProcessor(InputPreprocessor, EOSInputProcessorIndex);
			}
		}
	}

	return true;
}

bool FEOSOverlayInputProviderModule::IsRenderReady()
{
	if (bRenderReady)
	{
		return true;
	}

	// The FApp check ensures that we don't register the Input Processor in the wrong places, like the UE Editor Project list window
	if (!IsRunningCommandlet() && FApp::HasProjectName())
	{
		if (!FSlateApplication::IsInitialized())
		{
			return false;
		}

		if (!FSlateApplication::Get().GetRenderer())
		{
			return false;
		}

		InputPreprocessor = MakeShared<FEOSOverlayInputProviderPreProcessor>();

		// Store a pointer to the processor in your module
		FSlateApplication::Get().RegisterInputPreProcessor(InputPreprocessor, EOSInputProcessorIndex);
	}

	bRenderReady = true;

	return true;
}

void FEOSOverlayInputProviderModule::StartupModule()
{
	StartupTicker();
}

void FEOSOverlayInputProviderModule::ShutdownModule()
{
	if (!IsRunningCommandlet())
	{
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().UnregisterInputPreProcessor(InputPreprocessor);
		}
	}

	InputPreprocessor.Reset();

	ShutdownTicker();
}