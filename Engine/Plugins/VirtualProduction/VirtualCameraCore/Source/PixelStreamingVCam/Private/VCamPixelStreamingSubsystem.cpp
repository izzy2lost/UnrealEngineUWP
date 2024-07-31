// Copyright Epic Games, Inc. All Rights Reserved.

#include "VCamPixelStreamingSubsystem.h"

#include "BuiltinProviders/VCamPixelStreamingSession.h"
#include "Networking/VCamPixelStreamingLiveLink.h"

#include "Engine/Engine.h"
#include "Features/IModularFeatures.h"
#include "ILiveLinkClient.h"

UVCamPixelStreamingSubsystem* UVCamPixelStreamingSubsystem::Get()
{
	return GEngine ? GEngine->GetEngineSubsystem<UVCamPixelStreamingSubsystem>() : nullptr;
}

void UVCamPixelStreamingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	MissingSignallingServerNotifier = MakeUnique<UE::PixelStreamingVCam::FMissingSignallingServerNotifier>(*this);
	SignalingServerLifecycle = MakeUnique<UE::PixelStreamingVCam::FSignalingServerLifecycle>(*this);
}

void UVCamPixelStreamingSubsystem::Deinitialize()
{
	Super::Deinitialize();
	RegisteredSessions.Empty();
	
	IModularFeatures& ModularFeatures = IModularFeatures::Get();
	if (LiveLinkSource && ModularFeatures.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
	{
		ILiveLinkClient* LiveLinkClient = &ModularFeatures.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
		LiveLinkClient->RemoveSource(LiveLinkSource);
	}
	LiveLinkSource.Reset();

	MissingSignallingServerNotifier.Reset();
	SignalingServerLifecycle.Reset();
}

void UVCamPixelStreamingSubsystem::RegisterActiveOutputProvider(UVCamPixelStreamingSession* OutputProvider)
{
	check(OutputProvider);
	RegisteredSessions.AddUnique(OutputProvider);
	
	if (LiveLinkSource)
	{
		FName SubjectName = FName(OutputProvider->StreamerId);
		LiveLinkSource->CreateSubject(SubjectName);
		LiveLinkSource->PushTransformForSubject(SubjectName, FTransform::Identity);
	}
}

void UVCamPixelStreamingSubsystem::UnregisterActiveOutputProvider(UVCamPixelStreamingSession* OutputProvider)
{
	check(OutputProvider);
	RegisteredSessions.RemoveSingle(OutputProvider);
	
	if (LiveLinkSource)
	{
		FName SubjectName = FName(OutputProvider->StreamerId);
		LiveLinkSource->RemoveSubject(SubjectName);
	}
}

TSharedPtr<FPixelStreamingLiveLinkSource> UVCamPixelStreamingSubsystem::TryGetLiveLinkSource(UVCamPixelStreamingSession* OutputProvider)
{
	IModularFeatures& ModularFeatures = IModularFeatures::Get();
	if (!ModularFeatures.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
	{
		return nullptr;
	}

	ILiveLinkClient* LiveLinkClient = &ModularFeatures.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
	if (!LiveLinkSource.IsValid()
		// User can manually remove live link sources via UI
		|| !LiveLinkClient->HasSourceBeenAdded(LiveLinkSource))
	{
		LiveLinkSource = MakeShared<FPixelStreamingLiveLinkSource>();
		LiveLinkClient->AddSource(LiveLinkSource);

		if (IsValid(OutputProvider))
		{
			FName SubjectName = FName(OutputProvider->StreamerId);
			LiveLinkSource->CreateSubject(SubjectName);
			LiveLinkSource->PushTransformForSubject(SubjectName, FTransform::Identity);
		}
	}
	return LiveLinkSource;
}

void UVCamPixelStreamingSubsystem::LaunchSignallingServerIfNeeded(UVCamPixelStreamingSession& Session)
{
	SignalingServerLifecycle->LaunchSignallingServerIfNeeded(Session);
}

void UVCamPixelStreamingSubsystem::StopSignallingServerIfNeeded(UVCamPixelStreamingSession& Session)
{
	SignalingServerLifecycle->StopSignallingServerIfNeeded(Session);
}