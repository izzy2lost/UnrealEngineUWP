// Copyright Epic Games, Inc. All Rights Reserved.

#include "Channel/AvaOutputChannel.h"

#include "AvaRenderTargetUtils.h"
#include "AvalancheBroadcast.h"
#include "AvalancheMediaSettings.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Channel/AvalancheOutputChannelViewInterface.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Extensions/UserWidgetExtension.h"
#include "Framework/Application/SlateApplication.h"
#include "IAvaMediaModule.h"
#include "IAvalancheBroadcastSettings.h"
#include "MediaCapture.h"
#include "MediaOutput.h"
#include "MediaShaders.h"
#include "OutputDevices/AvaMediaOutputUtils.h"
#include "OutputDevices/AvaRenderTargetMediaUtils.h"
#include "Playback/AvaMediaPlayableGroup.h"
#include "Playback/IAvaMediaPlaybackClient.h"
#include "RHICommandList.h"
#include "RenderResource.h"
#include "ScreenPass.h"
#include "Slate/WidgetRenderer.h"
#include "TextureResource.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogAvaOutputChannel, Log, All);

void FAvaMediaCapture::Reset(UMediaCapture* InMediaCapture, UMediaOutput* InMediaOutput, UTextureRenderTarget2D* InRenderTarget)
{
	if (IsValid(MediaCapture))
	{
		MediaCapture->OnStateChangedNative.RemoveAll(this);
	}
	
	OnMediaCaptureStateChanged.Clear();
	MediaCapture = InMediaCapture;
	MediaOutput = InMediaOutput;
	RenderTarget = InRenderTarget;
	if (IsValid(MediaCapture))
	{
		MediaCapture->OnStateChangedNative.AddRaw(this, &FAvaMediaCapture::OnStateChangedNative);
	}
}

void FAvaMediaCapture::OnStateChangedNative()
{
	if (OnMediaCaptureStateChanged.IsBound())
	{
		OnMediaCaptureStateChanged.Broadcast(MediaCapture, MediaOutput);
	}
}

namespace Avalanche
{
	bool IsCapturing(const UMediaCapture* InMediaCapture)
	{
		if (IsValid(InMediaCapture))
		{
			const EMediaCaptureState CaptureState = InMediaCapture->GetState();

			return CaptureState == EMediaCaptureState::Capturing
				|| CaptureState == EMediaCaptureState::Preparing;
		}
		return false;
	}

	static void AddInvertAlphaConversionPass(FRDGBuilder& GraphBuilder
		, const FRDGTextureRef& SourceRGBTexture
		, const FResolveRect& ResolveRect
		, const FVector2D& SizeU
		, const FVector2D& SizeV
		, FRDGTextureRef OutputResource)
	{
		// Rectangle area to use from source
		const FIntRect ViewRect(ResolveRect.X1, ResolveRect.Y1, ResolveRect.X2, ResolveRect.Y2);

		//Dummy ViewFamily/ViewInfo created to use built in Draw Screen/Texture Pass
		FSceneViewFamily ViewFamily(FSceneViewFamily::ConstructionValues(nullptr, nullptr, FEngineShowFlags(ESFIM_Game))
			.SetTime(FGameTime()));
		FSceneViewInitOptions ViewInitOptions;
		ViewInitOptions.ViewFamily = &ViewFamily;
		ViewInitOptions.SetViewRectangle(ViewRect);
		ViewInitOptions.ViewOrigin = FVector::ZeroVector;
		ViewInitOptions.ViewRotationMatrix = FMatrix::Identity;
		ViewInitOptions.ProjectionMatrix = FMatrix::Identity;
		FSceneView ViewInfo(ViewInitOptions);

		//At some point we should support color conversion (ocio) but for now we push incoming texture as is
		constexpr bool bDoLinearToSRGB = false;

		FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		TShaderMapRef<FScreenPassVS> VertexShader(GlobalShaderMap);

		//Configure source/output viewport to get the right UV scaling from source texture to output texture
		FScreenPassTextureViewport InputViewport(SourceRGBTexture, ViewRect);
		FScreenPassTextureViewport OutputViewport(OutputResource);

		// In cases where texture is converted from a format that doesn't have A channel, we want to force set it to 1.
		EMediaCaptureConversionOperation MediaConversionOperation = EMediaCaptureConversionOperation::INVERT_ALPHA;
		FModifyAlphaSwizzleRgbaPS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FModifyAlphaSwizzleRgbaPS::FConversionOp>(static_cast<int32>(MediaConversionOperation));

		TShaderMapRef<FModifyAlphaSwizzleRgbaPS> PixelShader(GlobalShaderMap, PermutationVector);
		FModifyAlphaSwizzleRgbaPS::FParameters* Parameters = PixelShader->AllocateAndSetParameters(GraphBuilder, SourceRGBTexture, OutputResource);
		AddDrawScreenPass(GraphBuilder, RDG_EVENT_NAME("AvaOutputInvertAlpha"), ViewInfo, OutputViewport, InputViewport, VertexShader, PixelShader, Parameters);
	}

	void ExecuteInvertAlphaConversionPass(FRHICommandListImmediate& RHICmdList, UTextureRenderTarget2D* SourceRT, UTextureRenderTarget2D* DestinationRT)
	{
		FRDGBuilder GraphBuilder(RHICmdList);

		const FTexture2DRHIRef SourceTexture = SourceRT->GetRenderTargetResource()->GetTexture2DRHI();
		const FRDGTextureRef SourceRGBTexture = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(SourceTexture, TEXT("SourceTexture")));

		const FTexture2DRHIRef DestinationTexture = DestinationRT->GetRenderTargetResource()->GetTexture2DRHI();
		const FRDGTextureRef OutputResource = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(DestinationTexture, TEXT("DestTexture")));

		const FVector2D SizeU = { 0.0f, 1.0f };
		const FVector2D SizeV = { 0.0f, 1.0f };

		const FResolveRect ResolveRect(0, 0, SourceTexture->GetSizeX(), SourceTexture->GetSizeY());

		AddInvertAlphaConversionPass(GraphBuilder, SourceRGBTexture, ResolveRect, SizeU, SizeV, OutputResource);

		GraphBuilder.Execute();
	}

	namespace MediaOutputInfoHelper
	{
		/**
		 * Legacy support helper.
		 * Initialize the media output info from a given media output object.
		 * Note: this will only work correctly if the device can be found in
		 * a device provider. If the device is on a remote server, the server
		 * must be connected when this function is called.
		*/
		FAvaMediaOutputInfo InitFrom(const UMediaOutput* InMediaOutput)
		{
			FAvaMediaOutputInfo OutputInfo;

			if (::IsValid(InMediaOutput))
			{
				const FString DeviceNameString = UE::AvaMediaOutputUtils::GetDeviceName(InMediaOutput);
				if (!DeviceNameString.IsEmpty())
				{
					const FName DeviceName = *DeviceNameString;
					const FName DeviceProviderName = UE::AvaMediaOutputUtils::GetDeviceProviderName(InMediaOutput);
					const FString ServerName = IAvaMediaModule::Get().GetServerNameForDevice(DeviceProviderName, DeviceName);
					if (!ServerName.IsEmpty() || IAvaMediaModule::Get().IsLocalDevice(DeviceProviderName, DeviceName))
					{
						OutputInfo.DeviceName = DeviceName;
						OutputInfo.DeviceProviderName = DeviceProviderName;
						OutputInfo.ServerName = ServerName;
					}
				}
			}
			return OutputInfo;
		}
	}
}

FAvaOutputChannel FAvaOutputChannel::NullChannel;
FAvaOutputChannel::FOnAvaChannelChanged FAvaOutputChannel::OnChannelChanged;
FAvaOutputChannel::FMediaOutputStateChanged FAvaOutputChannel::OnMediaOutputStateChanged;

FAvaOutputChannel::~FAvaOutputChannel()
{
	StopPlaceholderTick();

#if WITH_EDITOR
	for (UMediaOutput* MediaOutput : MediaOutputs)
	{
		MediaOutput->OnOutputModified().RemoveAll(this);
	}
#endif
}

void FAvaOutputChannel::ReleasePlaceholderResources()
{
	OverridePlaceholderWidget.Reset();
	if (OverridePlaceholderUserWidget)
	{
		OverridePlaceholderUserWidget->ReleaseSlateResources(true);
		OverridePlaceholderUserWidget = nullptr;
	}
}

void FAvaOutputChannel::ReleasePlaceholderRenderTargets()
{
	PlaceholderRenderTarget = nullptr;
	PlaceholderRenderTargetTmp = nullptr;
}

void FAvaOutputChannel::ReleaseOutputs()
{
	for (UMediaOutput* MediaOutput : MediaOutputs)
	{
		StopCaptureForOutput(MediaOutput);
#if WITH_EDITOR
		MediaOutput->OnOutputModified().RemoveAll(this);
#endif
	}
	MediaOutputs.Reset();
}

void FAvaOutputChannel::DuplicateChannel(const FAvaOutputChannel& InSourceChannel, FAvaOutputChannel& OutTargetChannel)
{
	OutTargetChannel.MediaOutputs.Reset(InSourceChannel.MediaOutputs.Num());
	for (const UMediaOutput* const SourceMediaOutput : InSourceChannel.MediaOutputs)
	{
		UMediaOutput* const TargetMediaOutput = DuplicateObject<UMediaOutput>(SourceMediaOutput
			, SourceMediaOutput->GetOuter()
			, NAME_None);

		OutTargetChannel.MediaOutputs.Add(TargetMediaOutput);
		OutTargetChannel.MediaOutputInfos.Add(InSourceChannel.GetMediaOutputInfo(SourceMediaOutput));
		OutTargetChannel.MediaOutputInfos.Last().Guid = FGuid::NewGuid();	// Allocate a new guid for the duplicate.
	}
	OnChannelChanged.Broadcast(OutTargetChannel, EAvaChannelChange::MediaOutputs);
	OutTargetChannel.UpdateChannelResources(false);
}

void FAvaOutputChannel::SetChannelIndex(int32 InIndex)
{
	ChannelIndex = InIndex;
}

FName FAvaOutputChannel::GetChannelName() const
{
	return GetProfile().GetBroadcast().GetOrAddChannelName(ChannelIndex);
}

EAvaBroadcastChannelType FAvaOutputChannel::GetChannelType() const
{
	UAvalancheBroadcast& Broadcast = GetProfile().GetBroadcast();
	const FName ChannelName = Broadcast.GetOrAddChannelName(ChannelIndex);
	return Broadcast.GetChannelType(ChannelName);
}

FName FAvaOutputChannel::GetProfileName() const
{
	// Note: during the migration towards a non-singleton UAvalancheBroadcast,
	// a fallback path to the singleton is kept, but it has an ensure to try and catch
	// the code paths that lead to this.
	return ensure(Profile) ? Profile->GetName() : UAvalancheBroadcast::Get().GetCurrentProfile().GetName();
}

FAvaBroadcastProfile& FAvaOutputChannel::GetProfile() const
{
	// Note: during the migration towards a non-singleton UAvalancheBroadcast,
	// a fallback path to the singleton is kept, but it has an ensure to try and catch
	// the code paths that lead to this.
	return ensure(Profile) ? *Profile : UAvalancheBroadcast::Get().GetCurrentProfile();
}

EAvaChannelState FAvaOutputChannel::RefreshState()
{
	// Resolve the channel state from current output states. (PROTOTYPE LOGIC)
	EAvaChannelState NewChannelState = EAvaChannelState::Offline;
	EAvaMediaIssueSeverity NewChannelIssueSeverity = EAvaMediaIssueSeverity::None;

	for (const UMediaOutput* MediaOutput : GetMediaOutputs())
	{
		// Offline outputs don't contribute to state (i.e. channel will be offline too).
		const EAvaMediaOutputState OutputState = GetMediaOutputState(MediaOutput);
		if (OutputState == EAvaMediaOutputState::Live || OutputState == EAvaMediaOutputState::Preparing)
		{
			NewChannelState = EAvaChannelState::Live;
		}
		else if (OutputState == EAvaMediaOutputState::Idle && NewChannelState != EAvaChannelState::Live)
		{
			NewChannelState = EAvaChannelState::Idle;
		}

		const EAvaMediaIssueSeverity OutputIssueSeverity = GetMediaOutputIssueSeverity(OutputState, MediaOutput);
		if (OutputIssueSeverity > NewChannelIssueSeverity)
		{
			NewChannelIssueSeverity = OutputIssueSeverity;
		}
	}
	
	// Note: this will broadcast to delegates which may then request states of media outputs.
	// So the back store needs to be updated before calling this.
	SetState(NewChannelState, NewChannelIssueSeverity);

	return GetState();
}

EAvaMediaIssueSeverity FAvaOutputChannel::GetMediaOutputIssueSeverity(EAvaMediaOutputState InOutputState, const UMediaOutput* InMediaOutput) const
{
	if (InOutputState == EAvaMediaOutputState::Live || InOutputState == EAvaMediaOutputState::Preparing)
	{
		// If the output is broadcasting remote, fetch the status from the playback client (which is proxying that output's status).
		if (IAvaMediaModule::Get().IsMediaPlaybackClientStarted() && IsMediaOutputRemote(InMediaOutput))
		{
			const FAvaMediaOutputInfo& OutputInfo = GetMediaOutputInfo(InMediaOutput);
			const IAvaMediaPlaybackClient& PlaybackClient = IAvaMediaModule::Get().GetMediaPlaybackClient();
			return PlaybackClient.GetMediaOutputIssueSeverity(GetMediaOutputServerName(InMediaOutput), GetChannelName().ToString(), OutputInfo.Guid);
		}

		const UMediaCapture* const MediaCapture = GetMediaCaptureForOutput(InMediaOutput);
		
		// For now, we don't have a way to determine if the capture has warnings.
		// We only know if it worked or not.
		if (IsValid(MediaCapture) && Avalanche::IsCapturing(MediaCapture))
		{
			return GetLocalSeverityForOutput(InMediaOutput);
		}

		// If the output is live but we don't have a media capture, that would be an error.
		return EAvaMediaIssueSeverity::Errors;
	}

	// If the state is "error", we override severity with "errors", but the local severity should be "errors" as well
	// and we expect error messages to have been recorded.
	return (InOutputState == EAvaMediaOutputState::Error) ?  EAvaMediaIssueSeverity::Errors : GetLocalSeverityForOutput(InMediaOutput);
}

const TArray<FString>& FAvaOutputChannel::GetMediaOutputIssueMessages(const UMediaOutput* InMediaOutput) const
{
	if (IAvaMediaModule::Get().IsMediaPlaybackClientStarted() && IsMediaOutputRemote(InMediaOutput))
	{
		const FAvaMediaOutputInfo& OutputInfo = GetMediaOutputInfo(InMediaOutput);
		const IAvaMediaPlaybackClient& PlaybackClient = IAvaMediaModule::Get().GetMediaPlaybackClient();
		return PlaybackClient.GetMediaOutputIssueMessages(GetMediaOutputServerName(InMediaOutput), GetChannelName().ToString(), OutputInfo.Guid);
	}
	else
	{
		if (const FLocalMediaOutputStatus* LocalMediaOutputStatus = LocalMediaOutputStatuses.Find(InMediaOutput))
		{
			return LocalMediaOutputStatus->Messages;
		}
	}
	static const TArray<FString> EmptyStringArray;
	return EmptyStringArray;
}

EAvaMediaOutputState FAvaOutputChannel::GetMediaOutputState(const UMediaOutput* InMediaOutput) const
{
	if (IsMediaOutputRemote(InMediaOutput))
	{
		if (IAvaMediaModule::Get().IsMediaPlaybackClientStarted())
		{
			const FAvaMediaOutputInfo& OutputInfo = GetMediaOutputInfo(InMediaOutput);
			if (!OutputInfo.IsValid())
			{
				UE_LOG(LogAvaOutputChannel, Error, TEXT("Channel \"%s\": MediaOutput \"%s\" has invalid info."),
					*GetNameSafe(InMediaOutput), *GetChannelName().ToString());
			}
			
			// If the output is broadcasting remote, fetch the status from the playback client (which is proxying that output's status).
			const IAvaMediaPlaybackClient& PlaybackClient = IAvaMediaModule::Get().GetMediaPlaybackClient();
			return PlaybackClient.GetMediaOutputState(GetMediaOutputServerName(InMediaOutput),GetChannelName().ToString(), OutputInfo.Guid);
		}
		else
		{
			// Remote output if offline if the client is not running.
			return EAvaMediaOutputState::Offline;
		}
	}
	
	const UMediaCapture* const MediaCapture = GetMediaCaptureForOutput(InMediaOutput);

	if (IsValid(MediaCapture))
	{
		// For now, we don't have a way to determine if the capture has warnings.
		// We only know if it worked or not.
		switch (MediaCapture->GetState())
		{
		case EMediaCaptureState::Capturing:
		case EMediaCaptureState::StopRequested:
			return EAvaMediaOutputState::Live;
		case EMediaCaptureState::Preparing:
			return EAvaMediaOutputState::Preparing;
		case EMediaCaptureState::Error:
			return EAvaMediaOutputState::Error;
		default:
			return EAvaMediaOutputState::Idle;
		}
	}
	
	// If no media capture is found for the given output and we are marked as broadcasting,
	// this is an error state.
	return bInternalStateBroadcasting ?  EAvaMediaOutputState::Error : EAvaMediaOutputState::Idle;
}

const FAvaMediaOutputInfo& FAvaOutputChannel::GetMediaOutputInfo(const UMediaOutput* InMediaOutput) const
{
	const int32 Index = MediaOutputs.Find(const_cast<UMediaOutput*>(InMediaOutput));
	if (MediaOutputInfos.IsValidIndex(Index))
	{
		return MediaOutputInfos[Index];
	}
	static FAvaMediaOutputInfo Empty;
	return Empty;
}

FAvaMediaOutputInfo* FAvaOutputChannel::GetMediaOutputInfoMutable(const UMediaOutput* InMediaOutput)
{
	const int32 Index = MediaOutputs.Find(const_cast<UMediaOutput*>(InMediaOutput));
	return (MediaOutputInfos.IsValidIndex(Index)) ? &MediaOutputInfos[Index] : nullptr; 
}

inline void UpdatePlaceHolderRenderTarget(TObjectPtr<UTextureRenderTarget2D>& InOutRenderTarget, FName InBaseName, const FIntPoint& InSize, EPixelFormat InFormat, const FLinearColor& InClearColor)
{
	if (!InOutRenderTarget)
	{
		InOutRenderTarget = AvaRenderTargetUtils::CreateDefaultRenderTarget(InBaseName);
	}

	AvaRenderTargetUtils::UpdateRenderTarget(InOutRenderTarget.Get(), InSize, InFormat, InClearColor);
}

bool FAvaOutputChannel::IsEditingEnabled() const
{
	return ChannelState != EAvaChannelState::Live;
}

void FAvaOutputChannel::SetState(EAvaChannelState InState, EAvaMediaIssueSeverity InIssueSeverity)
{
	if (ChannelState != InState || ChannelIssueSeverity != InIssueSeverity)
	{
		ChannelState = InState;
		ChannelIssueSeverity = InIssueSeverity;

		if (InState == EAvaChannelState::Live)
		{
			StartPlaceholderTick();
		}
		else
		{
			StopPlaceholderTick();
		}
		OnChannelChanged.Broadcast(*this, EAvaChannelChange::State);
	}
}

void FAvaOutputChannel::UpdatePlaceholder()
{
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}
	
	if (!PlaceholderWidget.IsValid())
	{
		PlaceholderWidget = SNew(SAvaPlaceholderWidget);
	}

	if (!WidgetRenderer.IsValid())
	{
		WidgetRenderer = MakeShared<FWidgetRenderer>();
		
		WidgetRenderer->SetIsPrepassNeeded(true);
		
		//Enable Gamma Correction for Widget Rendering
		WidgetRenderer->SetUseGammaCorrection(true);
		
		//Clear Target before rendering
		WidgetRenderer->SetShouldClearTarget(true);
	}

	UpdateOverridePlaceholder();

	PlaceholderWidget->SetChannelName(ChannelNameText);
	
	UpdatePlaceholderRenderTargets();
}

void FAvaOutputChannel::UpdatePlaceholderRenderTargets()
{
	const FIntPoint RenderTargetSize = DetermineRenderTargetSize();
	const EPixelFormat RenderTargetFormat = DetermineRenderTargetFormat();
	const FLinearColor ClearColor = IAvaMediaModule::Get().GetBroadcastSettings().GetChannelClearColor();
	static const FName PlaceholderRenderTargetName = TEXT("AvaOutputChannel_PlaceHolderRT");
	static const FName PlaceholderRenderTargetTmpName = TEXT("AvaOutputChannel_TmpPlaceHolderRT");

	UpdatePlaceHolderRenderTarget(PlaceholderRenderTarget, PlaceholderRenderTargetName, RenderTargetSize, RenderTargetFormat, ClearColor);
	UpdatePlaceHolderRenderTarget(PlaceholderRenderTargetTmp, PlaceholderRenderTargetTmpName, RenderTargetSize, RenderTargetFormat, ClearColor);
}

void FAvaOutputChannel::UpdateOverridePlaceholder()
{
	const IAvalancheBroadcastSettings& BroadcastSettings = IAvaMediaModule::Get().GetBroadcastSettings();

	if (!BroadcastSettings.IsDrawPlaceholderWidget())
	{
		OverridePlaceholderWidget.Reset();
		return;
	}
	
	UClass* const CurrentOverridePlaceholderClass = OverridePlaceholderUserWidget
		? OverridePlaceholderUserWidget->GetClass()
		: nullptr;

	// Remark: on the server, if this settings comes from a client, the server may not
	// have the asset and will need to sync it.
	// TODO: use sync service to sync asset and use it when ready.
	TSoftClassPtr<UUserWidget> PlaceholderWidgetClass(BroadcastSettings.GetPlaceholderWidgetClass());
	UClass* const TargetOverridePlaceholderClass = PlaceholderWidgetClass.LoadSynchronous();
	
	//Update only if the Current Placeholder Class is different from the one in Media Settings
	if (CurrentOverridePlaceholderClass != TargetOverridePlaceholderClass)
	{
		OverridePlaceholderUserWidget = nullptr;
		if (TargetOverridePlaceholderClass)
		{			
			OverridePlaceholderUserWidget = NewObject<UUserWidget>(GetTransientPackage()
				, TargetOverridePlaceholderClass
				, NAME_None
				, RF_Transient);
		}

		OverridePlaceholderWidget.Reset();
		if (OverridePlaceholderUserWidget)
		{
			OverridePlaceholderWidget = OverridePlaceholderUserWidget->TakeWidget();

			//Update Tick State
			if (OverridePlaceholderWidget.IsValid())
			{
				// Default to never tick, only recompute for auto
				bool bCanTick = false;

				//These rules are pretty much the same as UUserWidget::UpdateCanTick but without the UWorld Checks
				if (OverridePlaceholderUserWidget->GetDesiredTickFrequency() == EWidgetTickFrequency::Auto)
				{
					// Note: WidgetBPClass can be NULL in a cooked build.
					UWidgetBlueprintGeneratedClass* const WidgetBPClass = Cast<UWidgetBlueprintGeneratedClass>(OverridePlaceholderUserWidget->GetClass());
					bCanTick |= !WidgetBPClass || WidgetBPClass->ClassRequiresNativeTick();
					bCanTick |= OverridePlaceholderUserWidget->bHasScriptImplementedTick;
					if (!bCanTick)
					{
						for (UUserWidgetExtension* const Extension : OverridePlaceholderUserWidget->GetExtensions(UUserWidgetExtension::StaticClass()))
						{
							if (Extension->RequiresTick())
							{
								bCanTick = true;
								break;
							}
						}
					}
				}

				OverridePlaceholderWidget->SetCanTick(bCanTick);
			}
		}
	}

	if (OverridePlaceholderUserWidget && OverridePlaceholderUserWidget->Implements<UAvalancheOutputChannelViewInterface>())
	{
		IAvalancheOutputChannelViewInterface::Execute_SetChannelName(OverridePlaceholderUserWidget, ChannelNameText);
	}
}

inline bool AreDimensionsValid(const UTextureRenderTarget2D* InRenderTarget, const int32 InMaxAllowedDrawSize)
{
	return (InRenderTarget->SizeX <= 0
		|| InRenderTarget->SizeY <= 0
		|| InRenderTarget->SizeX > InMaxAllowedDrawSize
		|| InRenderTarget->SizeY > InMaxAllowedDrawSize) ? false : true;
}

void FAvaOutputChannel::DrawPlaceholderWidget() const
{
	if (!IsValid(PlaceholderRenderTarget) || !IsValid(PlaceholderRenderTargetTmp))
	{
		return;
	}
	
	const int32 MaxAllowedDrawSize = GetMax2DTextureDimension();
	
	if (!AreDimensionsValid(PlaceholderRenderTarget.Get(), MaxAllowedDrawSize) 
		|| !AreDimensionsValid(PlaceholderRenderTargetTmp.Get(), MaxAllowedDrawSize))
	{
		return;
	}

	const bool bHasWidgetResources = PlaceholderWidget.IsValid() && WidgetRenderer.IsValid();
	
	if (IAvaMediaModule::Get().GetBroadcastSettings().IsDrawPlaceholderWidget() && bHasWidgetResources)
	{
		TSharedRef<SWidget> Widget = OverridePlaceholderWidget.IsValid()
			? OverridePlaceholderWidget.ToSharedRef()
			: PlaceholderWidget.ToSharedRef();
	
		// Render the widget in the tmp render target.
		WidgetRenderer->DrawWidget(PlaceholderRenderTargetTmp.Get()
			, Widget
			, FVector2D(PlaceholderRenderTargetTmp->SizeX, PlaceholderRenderTargetTmp->SizeY)
			, FSlateApplication::Get().GetDeltaTime());

		// Invert the Alpha on the Placeholder Widget
		UTextureRenderTarget2D* SourceRT = PlaceholderRenderTargetTmp.Get();
		UTextureRenderTarget2D* DestinationRT = PlaceholderRenderTarget.Get();

		ENQUEUE_RENDER_COMMAND(FAvaOutputInvertAlpha)(
			[SourceRT, DestinationRT](FRHICommandListImmediate& RHICmdList)
			{
				Avalanche::ExecuteInvertAlphaConversionPass(RHICmdList, SourceRT, DestinationRT);
			});
	}
	else
	{
		UE::AvaRenderTargetMediaUtils::ClearRenderTarget(PlaceholderRenderTarget.Get());
	}
}

bool FAvaOutputChannel::IsValidChannel() const
{
	return this != &FAvaOutputChannel::NullChannel && GetChannelName() != NAME_None;
}

bool FAvaOutputChannel::StartChannelBroadcast()
{
	bInternalStateBroadcasting = true;
	
	IAvaMediaModule& Module = IAvaMediaModule::Get();
	if (Module.IsMediaPlaybackClientStarted())
	{
		const TArray<UMediaOutput*> RemoteMediaOutputs = GetRemoteMediaOutputs();
		if (!RemoteMediaOutputs.IsEmpty())
		{
			const FString ProfileName = GetProfileName().ToString();
			Module.GetMediaPlaybackClient().RequestBroadcast(ProfileName, GetChannelName(), RemoteMediaOutputs, EAvaMediaBroadcastAction::Start);
		}
	}

	const FString ChannelName = GetChannelName().ToString();
	
	if (GetState() == EAvaChannelState::Live)
	{
		UE_LOG(LogAvaOutputChannel, Warning
			, TEXT("Output Channel (%s) attempted to Start Channel Broadcast while already Running!")
			, *ChannelName);
		return true;
	}

	// Ensure placeholder render targets are compatible with media outputs.
	UpdateChannelResources(true);
	
	UTextureRenderTarget2D* const RenderTarget = GetCurrentRenderTarget(true);
	
	for (UMediaOutput* MediaOutput : MediaOutputs)
	{
		if (!IsValid(MediaOutput))
		{
			UE_LOG(LogAvaOutputChannel, Warning
				, TEXT("Output Channel (%s) has an Invalid Media Output.")
				, *ChannelName);
			continue;
		}

		ResetLocalMediaOutputStatus(MediaOutput);
		
		// Don't locally start remote media outputs.
		if (IsMediaOutputRemote(MediaOutput))
		{
			continue;
		}
		
		// Check if this MediaOutput can start with the given render target
		if (!IsMediaOutputCompatible(MediaOutput, RenderTarget))
		{
			StopCaptureForOutput(MediaOutput);	// Ensures no stale capture is running.
			continue;
		}

		UMediaCapture* const MediaCapture = MediaOutput->CreateMediaCapture();
		
		if (!IsValid(MediaCapture))
		{
			FString& Message = EmplaceMessageForOutput(MediaOutput, EAvaMediaIssueSeverity::Warnings);
			Message = FString::Printf(
				TEXT("Output Channel (%s) has a Media Output (%s) that could not create a valid Media Capture.")
				, *ChannelName, *MediaOutput->GetName());

			UE_LOG(LogAvaOutputChannel, Warning, TEXT("%s"), *Message);
			
			StopCaptureForOutput(MediaOutput);	// Ensures no stale capture is running.
			continue;
		}
		
		// TODO: Expose this along with MediaOutput.
		FMediaCaptureOptions CaptureOptions;
		
		// Don't stop Capture when Throttling the Engine
		CaptureOptions.bSkipFrameWhenRunningExpensiveTasks = false;
		// Allow the formats to be converted if different.
		CaptureOptions.bConvertToDesiredPixelFormat = true;
		
		check(RenderTarget);
		if (MediaCapture->CaptureTextureRenderTarget2D(RenderTarget, CaptureOptions))
		{
			FAvaMediaCapture& AvaMediaCapture = MediaCaptures.Emplace(MediaOutput);
			AvaMediaCapture.Reset(MediaCapture, MediaOutput, RenderTarget);
			AvaMediaCapture.OnMediaCaptureStateChanged.AddRaw(this, &FAvaOutputChannel::OnAvaMediaCaptureStateChanged); 
		}
	}

	RefreshState();
	return true;
}

void FAvaOutputChannel::StopChannelBroadcast()
{
	bInternalStateBroadcasting = false;

	StopPlaceholderTick();
	
	IAvaMediaModule& Module = IAvaMediaModule::Get();
	if (Module.IsMediaPlaybackClientStarted())
	{
		const TArray<UMediaOutput*> RemoteMediaOutputs = GetRemoteMediaOutputs();
		if (!RemoteMediaOutputs.IsEmpty())
		{
			const FString ProfileName = GetProfileName().ToString();
			Module.GetMediaPlaybackClient().RequestBroadcast(ProfileName, GetChannelName(), RemoteMediaOutputs, EAvaMediaBroadcastAction::Stop);
		}
	}

	for (const UMediaOutput* MediaOutput : MediaOutputs)
	{
		ResetLocalMediaOutputStatus(MediaOutput);
		StopCaptureForOutput(MediaOutput);
	}
	
	MediaCaptures.Reset();
	LastActivePlayableGroup.Reset();
	CurrentRenderTarget.Reset();
	
	if (IsValid(PlaceholderRenderTarget))
	{
		PlaceholderRenderTarget->UpdateResourceImmediate();
	}

	if (IsValid(PlaceholderRenderTargetTmp))
	{
		PlaceholderRenderTargetTmp->UpdateResourceImmediate();
	}

	SetState(EAvaChannelState::Idle, EAvaMediaIssueSeverity::None);
}

UMediaOutput* FAvaOutputChannel::AddMediaOutput(const UClass* InMediaOutputClass, const FAvaMediaOutputInfo& InOutputInfo)
{
	// Don't add a remote output to a local preview channel.
	if (InOutputInfo.IsValid() && InOutputInfo.IsRemote() && GetChannelType() == EAvaBroadcastChannelType::Preview)
	{
		return nullptr;
	}
	
	if (InMediaOutputClass && InMediaOutputClass->IsChildOf(UMediaOutput::StaticClass()))
	{
		UMediaOutput* const MediaOutput = NewObject<UMediaOutput>(UAvalancheBroadcast::GetAvalancheBroadcast()
			, InMediaOutputClass
			, NAME_None
			, RF_Transactional);

		// If this is a MediaOutput class that requires render targets to be resolved to cpu memory,
		// we need more buffers to avoid having the flush rendering commands.
		static const int32 MinimumNumberOfTextureBuffers = 3;
		if (MediaOutput->NumberOfTextureBuffers < MinimumNumberOfTextureBuffers)
		{
			MediaOutput->NumberOfTextureBuffers = MinimumNumberOfTextureBuffers;
		}

		// Set bInvertKeyOutput property to true by default since we always output a scene render with inverted alpha.
		{
			// Known implementations that have it: BlackMagic, Aja, NDI.
			// Ava Display and Rivermax don't have it, but don't support Key.
			FBoolProperty* const Property = FindFProperty<FBoolProperty>(MediaOutput->GetClass(), TEXT("bInvertKeyOutput"));
			if (Property)
			{
				Property->SetPropertyValue(Property->ContainerPtrToValuePtr<bool>(MediaOutput, 0), true);
			}
		}

		AddMediaOutput(MediaOutput, InOutputInfo);
		
		return MediaOutput;
	}
	return nullptr;
}

void FAvaOutputChannel::AddMediaOutput(UMediaOutput* InMediaOutput, const FAvaMediaOutputInfo& InOutputInfo)
{
	if (IsValid(InMediaOutput))
	{
		// Check that we don't have a remote output on a local preview.
		check(!(InOutputInfo.IsValid() && InOutputInfo.IsRemote() && GetChannelType() == EAvaBroadcastChannelType::Preview));
#if WITH_EDITOR
		InMediaOutput->OnOutputModified().AddRaw(this, &FAvaOutputChannel::OnMediaOutputModified);
#endif
		MediaOutputs.Add(InMediaOutput);
		MediaOutputInfos.Add(InOutputInfo);
		// Note: the media output may not be fully configured at that point.
		OnChannelChanged.Broadcast(*this, EAvaChannelChange::MediaOutputs);
		UpdateChannelResources(true);
	}
}

int32 FAvaOutputChannel::RemoveMediaOutput(UMediaOutput* InMediaOutput)
{
	int32 RemoveCount = 0;
	const int32 IndexToRemove = MediaOutputs.Find(InMediaOutput);
	if (IndexToRemove != INDEX_NONE)
	{
#if WITH_EDITOR
		InMediaOutput->OnOutputModified().RemoveAll(this);
#endif
		RemoveCount = 1;
		MediaOutputs.RemoveAt(IndexToRemove);
		MediaOutputInfos.RemoveAt(IndexToRemove);
		OnChannelChanged.Broadcast(*this, EAvaChannelChange::MediaOutputs);
		UpdateChannelResources(true);
	}
	return RemoveCount;
}

void FAvaOutputChannel::OnMediaOutputModified(UMediaOutput* InMediaOutput)
{
	// Update media output info's device name.
	if (const FString DeviceName = UE::AvaMediaOutputUtils::GetDeviceName(InMediaOutput); !DeviceName.IsEmpty())
	{
		if (FAvaMediaOutputInfo* OutputInfo = GetMediaOutputInfoMutable(InMediaOutput))
		{
			OutputInfo->DeviceName = FName(DeviceName);
		}
	}
	
	IAvaMediaModule& AvaMediaModule = IAvaMediaModule::Get();
	if (IsMediaOutputRemote(InMediaOutput) && AvaMediaModule.IsMediaPlaybackClientStarted())
	{
		const FString ProfileName = GetProfileName().ToString();
		IAvaMediaPlaybackClient& PlaybackClient = AvaMediaModule.GetMediaPlaybackClient();
		PlaybackClient.RequestBroadcast(ProfileName, GetChannelName(), GetRemoteMediaOutputs(), EAvaMediaBroadcastAction::UpdateConfig);
	}
	else
	{
		UpdateChannelResources(true);
		RefreshState();
	}
}

void FAvaOutputChannel::PostLoadMediaOutputs(bool bInIsProfileActive, FAvaBroadcastProfile* InProfile)
{
	Profile = InProfile;
	
	// Create missing output infos (legacy support)
	if (MediaOutputInfos.Num() != MediaOutputs.Num())
	{
		MediaOutputInfos.Empty(MediaOutputs.Num());
		for (const UMediaOutput* Output : MediaOutputs)
		{
			MediaOutputInfos.Add(Avalanche::MediaOutputInfoHelper::InitFrom(Output));
		}
	}

	// Remove invalid outputs and corresponding infos.
	for (int32 OutputIndex = 0; OutputIndex < MediaOutputs.Num();)
	{
		if (!IsValid(MediaOutputs[OutputIndex]))
		{
			MediaOutputs.RemoveAt(OutputIndex);
			MediaOutputInfos.RemoveAt(OutputIndex);
		}
		else
		{
			++OutputIndex;
		}
	}

	// Complete missing fields (legacy support)
	for (FAvaMediaOutputInfo& MediaOutputInfo : MediaOutputInfos)
	{
		MediaOutputInfo.PostLoad();
	}

#if WITH_EDITOR
	for (UMediaOutput* MediaOutput : MediaOutputs)
	{
		if (!MediaOutput->OnOutputModified().IsBoundToObject(this))
		{
			MediaOutput->OnOutputModified().AddRaw(this, &FAvaOutputChannel::OnMediaOutputModified);
		}
	}
#endif
	
	OnChannelChanged.Broadcast(*this, EAvaChannelChange::MediaOutputs);
	UpdateChannelResources(bInIsProfileActive);
	RefreshState();
}

void FAvaOutputChannel::UpdateRenderTarget(UAvaMediaPlayableGroup* InPlayableGroup, UTextureRenderTarget2D* InRenderTarget)
{
	// In the current design, the render target is associated with a "playable group".
	// It can only be associated with one group at a time.

	const bool bRenderTargetChanged = CurrentRenderTarget.Get() != InRenderTarget;
	
	LastActivePlayableGroup = InPlayableGroup;
	CurrentRenderTarget = InRenderTarget;

	if (bRenderTargetChanged)
	{
		UpdateViewportTarget();
	}
}

void FAvaOutputChannel::UpdateAudioDevice(const FAudioDeviceHandle& InAudioDeviceHandle)
{
	for (TPair<TObjectPtr<UMediaOutput>, FAvaMediaCapture>& MediaCapture : MediaCaptures)
	{
		UMediaCapture* MediaCapturePtr = MediaCapture.Value.MediaCapture;
		if (MediaCapturePtr->GetCaptureAudioDevice().GetDeviceID() != InAudioDeviceHandle.GetDeviceID())
		{
			MediaCapturePtr->SetCaptureAudioDevice(InAudioDeviceHandle);
		}
	}
}

UAvaMediaPlayableGroup* FAvaOutputChannel::GetLastActivePlayableGroup() const
{
	return LastActivePlayableGroup.Get();
}

UTextureRenderTarget2D* FAvaOutputChannel::GetCurrentRenderTarget(bool bInFallbackToPlaceholder) const
{
	if (CurrentRenderTarget.IsValid())
	{
		return CurrentRenderTarget.Get();
	}
	
	return bInFallbackToPlaceholder
		? GetPlaceholderRenderTarget()
		: nullptr;
}

UTextureRenderTarget2D* FAvaOutputChannel::GetPlaceholderRenderTarget() const
{
	return PlaceholderRenderTarget.Get();
}

bool FAvaOutputChannel::HasAnyLocalMediaOutputs() const
{
	for (const UMediaOutput* MediaOutput : MediaOutputs)
	{
		if (IsValid(MediaOutput) && !IsMediaOutputRemote(MediaOutput))
		{
			return true;
		}
	}
	return false;
}

bool FAvaOutputChannel::HasAnyRemoteMediaOutputs() const
{
	for (const UMediaOutput* MediaOutput : MediaOutputs)
	{
		if (IsValid(MediaOutput) && IsMediaOutputRemote(MediaOutput))
		{
			return true;
		}
	}
	return false;
}


bool FAvaOutputChannel::IsMediaOutputRemote(const UMediaOutput* InMediaOutput) const
{
	// Ideally, the output info is valid here, so we just need to see if the
	// output is associated with a remote server.
	const FAvaMediaOutputInfo& OutputInfo = GetMediaOutputInfo(InMediaOutput);
	if (OutputInfo.IsValid())
	{
		return OutputInfo.IsRemote();
	}

	// Fallback/Legacy, in case the output info is not valid,
	// if the playback client is started, we can try the legacy method
	// of checking the remote device providers. This is much less reliable.
	IAvaMediaModule& AvaMediaModule = IAvaMediaModule::Get();
	if (AvaMediaModule.IsMediaPlaybackClientStarted())
	{
		return AvaMediaModule.GetMediaPlaybackClient().IsMediaOutputRemoteFallback(InMediaOutput);
	}

	return false;
}

const FString& FAvaOutputChannel::GetMediaOutputServerName(const UMediaOutput* InMediaOutput) const
{
	const FAvaMediaOutputInfo& OutputInfo = GetMediaOutputInfo(InMediaOutput);
	if (OutputInfo.IsValid())
	{
		return OutputInfo.ServerName;
	}

	static const FString EmptyString;
	return EmptyString;
}

TArray<UMediaOutput*> FAvaOutputChannel::GetRemoteMediaOutputs() const
{
	TArray<UMediaOutput*> RemoteMediaOutputs;
	RemoteMediaOutputs.Reserve(MediaOutputs.Num());
	for (UMediaOutput* MediaOutput : MediaOutputs)
	{
		if (IsValid(MediaOutput) && IsMediaOutputRemote(MediaOutput))
		{
			RemoteMediaOutputs.Add(MediaOutput);
		}
	}
	return RemoteMediaOutputs;
}

bool FAvaOutputChannel::IsMediaOutputCompatible(UMediaOutput* InMediaOutput, UTextureRenderTarget2D* InRenderTarget) const
{
	// Check if this MediaOutput can start with the given render target
	if (IsValid(InRenderTarget) && IsValid(InMediaOutput))
	{
		// Size Match Check
		{
			const FIntPoint MediaOutputSize = InMediaOutput->GetRequestedSize();
			const FIntPoint RenderTargetSize = AvaRenderTargetUtils::GetRenderTargetSize(InRenderTarget);
			if (MediaOutputSize != UMediaOutput::RequestCaptureSourceSize && MediaOutputSize != RenderTargetSize)
			{
				FString& Message = EmplaceMessageForOutput(InMediaOutput, EAvaMediaIssueSeverity::Errors);
				Message = FString::Printf(
					TEXT("Media Output \"%s\" requested size (%d, %d) is not compatible with Channel \"%s\" Render Target \"%s\" size (%d, %d).")
					, *InMediaOutput->GetName(), MediaOutputSize.X, MediaOutputSize.Y
					, *GetChannelName().ToString()
					, *InRenderTarget->GetName(), RenderTargetSize.X, RenderTargetSize.Y);
				
				UE_LOG(LogAvaOutputChannel, Error, TEXT("%s"), *Message);
				return false;
			}
		}

		// Format Match Check
		{
			const EPixelFormat MediaOutputFormat = InMediaOutput->GetRequestedPixelFormat();
			const EPixelFormat RenderTargetFormat = InRenderTarget->GetFormat();
			if (MediaOutputFormat != PF_Unknown && MediaOutputFormat != RenderTargetFormat)
			{
				const int32 TargetNumColorBits = AvaRenderTargetUtils::GetNumColorChannelBits(RenderTargetFormat);
				const int32 OutputNumColorBits = AvaRenderTargetUtils::GetNumColorChannelBits(MediaOutputFormat);
				if (OutputNumColorBits > TargetNumColorBits)
				{
					FString& Message = EmplaceMessageForOutput(InMediaOutput, EAvaMediaIssueSeverity::Errors);
					Message = FString::Printf(
						TEXT("Media Output \"%s\" requested format %s is not compatible with Channel \"%s\" Render Target \"%s\" format %s")
						TEXT(" because it has higher color precision (%d vs %d bits).")
						, *InMediaOutput->GetName(), GetPixelFormatString(MediaOutputFormat)
						, *GetChannelName().ToString()
						, *InRenderTarget->GetName(), GetPixelFormatString(RenderTargetFormat)
						, OutputNumColorBits, TargetNumColorBits);

					UE_LOG(LogAvaOutputChannel, Error, TEXT("%s"), *Message);
					return false;
				}

				// RGBA8 vs RGB10A2, we will detect a conflict here.
				// RGB10A2 wins in color bits, but loose in alpha bits.
				const int32 TargetNumAlphaBits = AvaRenderTargetUtils::GetNumAlphaChannelBits(RenderTargetFormat);
				const int32 OutputNumAlphaBits = AvaRenderTargetUtils::GetNumAlphaChannelBits(MediaOutputFormat);
				if (OutputNumAlphaBits > TargetNumAlphaBits)
				{
					FString& Message = EmplaceMessageForOutput(InMediaOutput, EAvaMediaIssueSeverity::Warnings);
					Message = FString::Printf(
						TEXT("Media Output \"%s\" requested format %s is not compatible with Channel \"%s\" Render Target \"%s\" format %s")
						TEXT(" because it has higher alpha precision (%d vs %d bits).")
						, *InMediaOutput->GetName(), GetPixelFormatString(MediaOutputFormat)
						, *GetChannelName().ToString()
						, *InRenderTarget->GetName(), GetPixelFormatString(RenderTargetFormat)
						, OutputNumAlphaBits, TargetNumAlphaBits);

					UE_LOG(LogAvaOutputChannel, Warning, TEXT("%s"), *Message);
				}
			}
		}
		return true;
	}
	return false;
}

void FAvaOutputChannel::UpdateViewportTarget()
{
	//Only need to update when Broadcasting, since when starting Broadcast it will Capture and update it anyways
	if (GetState() == EAvaChannelState::Live)
	{
		UTextureRenderTarget2D* const RenderTarget = GetCurrentRenderTarget(true);

		bool bNeedStateRefresh = false;

		// Start all media outputs that have a compatible format 
		// and are not already started on another game instance.
		for (UMediaOutput* const MediaOutput : MediaOutputs)
		{
			if (!IsValid(MediaOutput))
			{
				continue;
			}

			if (IsMediaOutputRemote(MediaOutput))
			{
				continue;
			}

			// For now, we don't update devices that are not live.
			if (GetMediaOutputState(MediaOutput) != EAvaMediaOutputState::Live)
			{
				continue;
			}
		
			// Check if this MediaOutput can still run with the given render target.
			// Remark: this test maybe optional. It is normally done when channel start
			// and we don't allow configuration changes to the output while it is running.
			if (!IsMediaOutputCompatible(MediaOutput, RenderTarget))
			{
				// Ensure no stale capture is running for this output.
				StopCaptureForOutput(MediaOutput);
				bNeedStateRefresh = true;
				continue;
			}

			FAvaMediaCapture* AvaMediaCapture = MediaCaptures.Find(MediaOutput);
			UMediaCapture* const MediaCapture = AvaMediaCapture ? AvaMediaCapture->MediaCapture : nullptr;
			
			// Double Check and Skip Updating SceneViewport / RenderTarget if not Capturing
			// Remark: if the capture is an external/removable device, it can stop the capture.
			// We can the state to be refreshed at least.
			if (!Avalanche::IsCapturing(MediaCapture))
			{
				bNeedStateRefresh = true;
				continue;
			}
			
			check(RenderTarget);

			// We go out of our way to avoid updating the render target (if it is the same)
			// because UpdateTextureRenderTarget2D has side effects even if we are updating with the same render target.
			if (AvaMediaCapture && AvaMediaCapture->RenderTarget.IsValid() && AvaMediaCapture->RenderTarget == RenderTarget)
			{
				continue;
			}
			
			if (AvaMediaCapture && MediaCapture->UpdateTextureRenderTarget2D(RenderTarget))
			{
				AvaMediaCapture->RenderTarget = RenderTarget;
			}
		}
		
		if (bNeedStateRefresh)
		{
			RefreshState();
		}
	}
	OnChannelChanged.Broadcast(*this, EAvaChannelChange::RenderTarget);
}

UMediaCapture* FAvaOutputChannel::GetMediaCaptureForOutput(const UMediaOutput* InMediaOutput) const
{
	const FAvaMediaCapture* AvaMediaCapture = MediaCaptures.Find(InMediaOutput);
	return AvaMediaCapture ? AvaMediaCapture->MediaCapture : nullptr;
}

void FAvaOutputChannel::StopCaptureForOutput(const UMediaOutput* InMediaOutput)
{
	UMediaCapture* const MediaCapture = GetMediaCaptureForOutput(InMediaOutput);
	if (IsValid(MediaCapture) && Avalanche::IsCapturing(MediaCapture))
	{
		MediaCapture->StopCapture(false);
	}
	MediaCaptures.Remove(InMediaOutput);
}

void FAvaOutputChannel::TickPlaceholder(float)
{
	// Only Draw if Render Target is Invalid 
	if (GetState() == EAvaChannelState::Live && !IsValid(GetCurrentRenderTarget(false)))
	{
		DrawPlaceholderWidget();
	}
}

void FAvaOutputChannel::StartPlaceholderTick()
{
	if (!PlaceholderTickHandle.IsValid() && FSlateApplication::IsInitialized())
	{
		PlaceholderTickHandle = FSlateApplication::Get().OnPreTick().AddRaw(this, &FAvaOutputChannel::TickPlaceholder);
	}
}

void FAvaOutputChannel::StopPlaceholderTick()
{
	if (PlaceholderTickHandle.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().OnPreTick().Remove(PlaceholderTickHandle);
	}
	PlaceholderTickHandle.Reset();
	ReleasePlaceholderResources();
}

void FAvaOutputChannel::OnAvaMediaCaptureStateChanged(const UMediaCapture* InMediaCapture, const UMediaOutput* InMediaOutput)
{
	RefreshState();

	// Note: the overall channel state may not change.
	// So we need to broadcast a specific event for the
	// media output state change.
	OnMediaOutputStateChanged.Broadcast(*this, InMediaOutput);
}

void FAvaOutputChannel::UpdateChannelResources(bool bInIsProfileActive)
{
	ChannelNameText = FText::FromName(GetChannelName());
	
	if (bInIsProfileActive)
	{
		// Only update the placeholder resources (including render targets) if currently active profile.
		UpdatePlaceholder();
	}
	else
	{
		// Release all resources.
		ReleasePlaceholderResources();
		ReleasePlaceholderRenderTargets();
	}
}

FIntPoint FAvaOutputChannel::DetermineRenderTargetSize() const
{
	// Just go through the list of media output in order and returns the first
	// one that specified a valid resolution.
	for (UMediaOutput* const MediaOutput : MediaOutputs)
	{
		if (IsValid(MediaOutput))
		{
			const FIntPoint TargetSize = MediaOutput->GetRequestedSize();
			if (TargetSize != UMediaOutput::RequestCaptureSourceSize && TargetSize.X > 0 && TargetSize.Y > 0)
			{
				return TargetSize;
			}
		}
	}
	return GetDefaultMediaOutputSize(GetChannelType());
}

EPixelFormat FAvaOutputChannel::DetermineRenderTargetFormat() const
{
	// Determine render target format with the most bits per pixel.
	// Note: check for alpha bits (A2 vs A8).
	EPixelFormat HighestBitsPerColorChannelFormat = PF_Unknown;
	int32 HighestNumBitsPerColorChannel = 0;
	int32 HighestNumColorComponents = 0;
	EPixelFormat HighestAlphaBitsFormat = PF_Unknown;
	int32 HighestNumAlphaBits = 0;
	
	for (const UMediaOutput* const MediaOutput : MediaOutputs)
	{
		if (IsValid(MediaOutput))
		{
			const EPixelFormat TargetFormat = MediaOutput->GetRequestedPixelFormat();			
			if (TargetFormat != PF_Unknown)
			{
				const int32 NumBitsPerColorChannel = AvaRenderTargetUtils::GetNumColorChannelBits(TargetFormat);
				const int32 NumColorComponents = AvaRenderTargetUtils::GetNumColorComponents(TargetFormat);
				// Note: only select the format if it has at least as many color components.
				// Otherwise, we stick with the format that has less bits, but more components.
				if (NumBitsPerColorChannel > HighestNumBitsPerColorChannel && NumColorComponents >= HighestNumColorComponents)
				{
					HighestNumBitsPerColorChannel = NumBitsPerColorChannel;
					HighestNumColorComponents = NumColorComponents;
					HighestBitsPerColorChannelFormat = TargetFormat;
				}
				
				const int32 NumAlphaBits = AvaRenderTargetUtils::GetNumAlphaChannelBits(TargetFormat);
				if (NumAlphaBits > HighestNumAlphaBits)
				{
					HighestNumAlphaBits = NumAlphaBits;
					HighestAlphaBitsFormat = TargetFormat;
				}
			}
		}
	}

	// Note: this logic favors the number of color bits to the detriment of alpha bits.
	// In a RGBA8 vs RGB10A2 case, if we want 8 bits alpha, to the detriment of color bits,
	// the logic below will need to be changed.
	
	if (HighestNumAlphaBits > HighestNumBitsPerColorChannel && HighestAlphaBitsFormat != PF_Unknown)
	{
		return HighestAlphaBitsFormat;
	}
	
	if (HighestBitsPerColorChannelFormat != PF_Unknown)
	{
		return HighestBitsPerColorChannelFormat;
	}
	
	if (HighestAlphaBitsFormat != PF_Unknown)
	{
		return HighestAlphaBitsFormat;
	}
	
	return GetDefaultMediaOutputFormat();
}

FIntPoint FAvaOutputChannel::GetDefaultMediaOutputSize(EAvaBroadcastChannelType InChannelType)
{
	// Special case for local preview channel.
	if (InChannelType == EAvaBroadcastChannelType::Preview)
	{
		return UAvalancheMediaSettings::Get().PreviewDefaultResolution;
	}

	// Default for program channel.
	return IAvaMediaModule::Get().GetBroadcastSettings().GetDefaultResolution();
}

EPixelFormat FAvaOutputChannel::GetDefaultMediaOutputFormat()
{
	return IAvaMediaModule::Get().GetBroadcastSettings().GetDefaultPixelFormat();
}

void FAvaOutputChannel::SetViewportQualitySettings(const FAvaViewportQualitySettings& InQualitySettings)
{
	QualitySettings = InQualitySettings;
	OnChannelChanged.Broadcast(*this, EAvaChannelChange::Settings);
}

FString& FAvaOutputChannel::EmplaceMessageForOutput(const UMediaOutput* InMediaOutput, EAvaMediaIssueSeverity InSeverity) const
{
	FLocalMediaOutputStatus& LocalMediaOutputStatus = LocalMediaOutputStatuses.Emplace(InMediaOutput);
	LocalMediaOutputStatus.Severity = InSeverity;
	return LocalMediaOutputStatus.Messages.Emplace_GetRef();
}

EAvaMediaIssueSeverity FAvaOutputChannel::GetLocalSeverityForOutput(const UMediaOutput* InMediaOutput) const
{
	const FLocalMediaOutputStatus* LocalMediaOutputStatus = LocalMediaOutputStatuses.Find(InMediaOutput);
	return LocalMediaOutputStatus ? LocalMediaOutputStatus->Severity : EAvaMediaIssueSeverity::None;
}

void FAvaOutputChannel::ResetLocalMediaOutputStatus(const UMediaOutput* InMediaOutput)
{
	if (IsValid(InMediaOutput))
	{
		LocalMediaOutputStatuses.Add(InMediaOutput,FLocalMediaOutputStatus());
	}
}