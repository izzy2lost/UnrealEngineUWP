// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlateRHIRendererSettings.h"
#include "FX/SlateRHIPostBufferProcessor.h"
#include "HAL/IConsoleManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SlateRHIRendererSettings)

static TAutoConsoleVariable<int32> CVarDefaultEnablePostRenderTarget_0(
	TEXT("Slate.DefaultEnablePostRenderTarget_0"),
	1,
	TEXT("Experimental. Set true to enable slate post render target 0"),
	ECVF_ReadOnly);

FSlatePostSettings::FSlatePostSettings()
	: bEnabled(false)
	, PostProcessorClass(nullptr)
	, PathToSlatePostRT(FString())
	, CachedSlatePostRT(nullptr)
{
}

USlateRHIRendererSettings::USlateRHIRendererSettings()
{
	SlatePostSettings.Add(ESlatePostRT::ESlatePostRT_0, FSlatePostSettings());
	SlatePostSettings.Add(ESlatePostRT::ESlatePostRT_1, FSlatePostSettings());
	SlatePostSettings.Add(ESlatePostRT::ESlatePostRT_2, FSlatePostSettings());
	SlatePostSettings.Add(ESlatePostRT::ESlatePostRT_3, FSlatePostSettings());
	SlatePostSettings.Add(ESlatePostRT::ESlatePostRT_4, FSlatePostSettings());

	// By default, enable the first post RT
	SlatePostSettings[ESlatePostRT::ESlatePostRT_0].bEnabled = CVarDefaultEnablePostRenderTarget_0.GetValueOnAnyThread();

	// Hardcoded paths to engine assets
	SlatePostSettings[ESlatePostRT::ESlatePostRT_0].PathToSlatePostRT = "/Engine/EngineResources/SlatePost0_RT.SlatePost0_RT";
	SlatePostSettings[ESlatePostRT::ESlatePostRT_1].PathToSlatePostRT = "/Engine/EngineResources/SlatePost1_RT.SlatePost1_RT";
	SlatePostSettings[ESlatePostRT::ESlatePostRT_2].PathToSlatePostRT = "/Engine/EngineResources/SlatePost2_RT.SlatePost2_RT";
	SlatePostSettings[ESlatePostRT::ESlatePostRT_3].PathToSlatePostRT = "/Engine/EngineResources/SlatePost3_RT.SlatePost3_RT";
	SlatePostSettings[ESlatePostRT::ESlatePostRT_4].PathToSlatePostRT = "/Engine/EngineResources/SlatePost4_RT.SlatePost4_RT";
}

USlateRHIRendererSettings::~USlateRHIRendererSettings()
{
	for (TPair<ESlatePostRT, FSlatePostSettings>& SlatePostSetting : SlatePostSettings)
	{
		FSlatePostSettings& PostSetting = SlatePostSetting.Value;

		UObject* SlatePostBuffer = PostSetting.CachedSlatePostRT;
		if (SlatePostBuffer)
		{
			SlatePostBuffer->RemoveFromRoot();
		}
	}
}

void USlateRHIRendererSettings::BeginDestroy()
{
	// Flush rendering commands since these settings can be used in render thread
	FlushRenderingCommands();

	Super::BeginDestroy();
}

FSlatePostSettings& USlateRHIRendererSettings::GetMutableSlatePostSetting(ESlatePostRT InPostBufferBit)
{
	return SlatePostSettings[InPostBufferBit];
}

const FSlatePostSettings& USlateRHIRendererSettings::GetSlatePostSetting(ESlatePostRT InPostBufferBit) const
{
	return SlatePostSettings[InPostBufferBit];
}

UObject* USlateRHIRendererSettings::TryGetPostBufferRT(ESlatePostRT InPostBufferBit) const
{
	return SlatePostSettings[InPostBufferBit].CachedSlatePostRT;
}

UObject* USlateRHIRendererSettings::LoadGetPostBufferRT(ESlatePostRT InPostBufferBit)
{
	UObject* Result = TryGetPostBufferRT(InPostBufferBit);

	if (!Result)
	{
		Result = LoadObject<UObject>(nullptr, *SlatePostSettings[InPostBufferBit].PathToSlatePostRT, nullptr, LOAD_None, nullptr);

		if (Result)
		{
			Result->AddToRoot();
			SlatePostSettings[InPostBufferBit].CachedSlatePostRT = Result;
		}
	}

	return Result;
}

const TMap<ESlatePostRT, FSlatePostSettings>& USlateRHIRendererSettings::GetSlatePostSettings() const
{
	return SlatePostSettings;
}
