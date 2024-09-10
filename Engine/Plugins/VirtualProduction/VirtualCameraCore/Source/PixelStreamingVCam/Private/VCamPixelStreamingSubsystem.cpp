// Copyright Epic Games, Inc. All Rights Reserved.

#include "VCamPixelStreamingSubsystem.h"

#include "BuiltinProviders/VCamPixelStreamingSession.h"
#include "Networking/VCamPixelStreamingLiveLink.h"

#include "Engine/Engine.h"
#include "Features/IModularFeatures.h"
#include "ILiveLinkClient.h"
#include "PixelStreamingVCamLog.h"

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
	UpdateLiveLinkSource(OutputProvider);
}

void UVCamPixelStreamingSubsystem::UnregisterActiveOutputProvider(UVCamPixelStreamingSession* OutputProvider)
{
	check(OutputProvider);
	RegisteredSessions.RemoveSingle(OutputProvider);
	
	if (LiveLinkSource)
	{
		LiveLinkSource->RemoveSubject();
	}
}

void UVCamPixelStreamingSubsystem::UpdateLiveLinkSource(UVCamPixelStreamingSession* OutputProvider)
{
	if (!IsValid(OutputProvider))
	{
		return;
	}
	
	IModularFeatures& ModularFeatures = IModularFeatures::Get();
	if (!ModularFeatures.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
	{
		UE_LOG(LogPixelStreamingVCam, Warning, TEXT("Failed to create subobject. VCam's camera transform will not update."))
		return;
	}

	ILiveLinkClient* LiveLinkClient = &ModularFeatures.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
	if (!LiveLinkSource)
	{
		LiveLinkSource = MakeShared<FPixelStreamingLiveLinkSource>();
		LiveLinkClient->AddSource(LiveLinkSource);
	}
	
	// This will delete the old subject, if it exists, and create a new subject with the target name.
	const FName SubjectName = FName(OutputProvider->StreamerId);
	LiveLinkSource->CreateSubject(SubjectName);
	LiveLinkSource->PushTransformForSubject(SubjectName, FTransform::Identity);
}

TSharedPtr<FPixelStreamingLiveLinkSource> UVCamPixelStreamingSubsystem::TryGetLiveLinkSource(UVCamPixelStreamingSession* OutputProvider)
{
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