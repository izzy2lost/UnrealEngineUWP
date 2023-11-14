// Copyright Epic Games, Inc. All Rights Reserved.

#include "OXRVisionOSSession.h"
#include "OXRVisionOSAction.h"
#include "OXRVisionOSActionSet.h"
#include "OXRVisionOSController.h"
#include "OXRVisionOSInstance.h"
#include "OXRVisionOSPlatformUtils.h"
#include "OXRVisionOS_RenderBridge.h"
#include "OXRVisionOSRuntimeSettings.h"
#include "OXRVisionOSSettingsTypes.h"
#include "OXRVisionOSSpace.h"
#include "OXRVisionOSSwapchain.h"
#include "OXRVisionOSTracker.h"
#include "OXRVisionOS_openxr_platform.h"
#include "OXRVisionOSAppDelegate.h"
#include "PixelFormat.h"
#include "StereoRenderTargetManager.h"
#include "HAL/RunnableThread.h"

#include "RenderingThread.h"

#include "CoreGlobals.h"
#include "RHICommandList.h"
#include "MetalRHIVisionOSBridge.h"

#define CHECK_API_RET_CREATE_FAIL(___function) {int __ret = RESULT_OK; __ret = (___function); if(__ret < RESULT_OK) { UE_LOG(LogOXRVisionOS, Warning, TEXT("Call to " #___function " failed, returned %d 0x%x"), __ret, __ret); UE_DEBUG_BREAK(); bCreateFailed = true; return; }}
#define CHECK_API_RET_RETURN_FAIL(___function) {int __ret = RESULT_OK; __ret = (___function); if(__ret < RESULT_OK) { UE_LOG(LogOXRVisionOS, Warning, TEXT("Call to " #___function " failed, returned %d 0x%x"), __ret, __ret); UE_DEBUG_BREAK(); return XrResult::XR_ERROR_RUNTIME_FAILURE; }}
#define CHECK_API_RET_WARN(___function) {int __ret = RESULT_OK; __ret = (___function); if(__ret < RESULT_OK) { UE_LOG(LogOXRVisionOS, Warning, TEXT("Call to " #___function " failed, returned %d 0x%x"), __ret, __ret);}}

#define OXRVISIONOS_REPROJECTION_ANALYSIS 0

// Helper macro for checking Api error codes. If the result of a function call is negative, causes a fatal log.
#define FA_CHECK_F(code, format, ...) do { int32 Result; { Result = (code); } if (Result < 0) { UE_LOG(LogOXRVisionOS, Fatal, TEXT("%s failed with error code: 0x%08x") format, TEXT(#code), Result, ##__VA_ARGS__); } } while(false)
#define FA_CHECK(code) FA_CHECK_F(code, TEXT(""))

namespace OXRVisionOSCVars
{
	static TAutoConsoleVariable<bool> CVarDisableReprojection(
		TEXT("vr.OXRVisionOS.DisableReprojection"),
		false,
		TEXT("Disables reprojection for debugging purposes"));
}

namespace OXRVisionOSSessionHelpers
{
	enum ELayerIndex : uint8
	{
		Background = 0,
		//CompositedOverlays,
		Num
	};

	constexpr uint32 OXRVisionOSBackbufferLength = 2;
	constexpr uint32 MaxOXRVisionOSLayers = 1; //TODO for now I only want the one stereo layer, 
}

XrResult FOXRVisionOSSession::Create(TSharedPtr<FOXRVisionOSSession, ESPMode::ThreadSafe>& OutSession, const XrSessionCreateInfo* createInfo, FOXRVisionOSInstance* Instance)
{
	if (createInfo == nullptr || Instance == nullptr)
	{
		return XrResult::XR_ERROR_VALIDATION_FAILURE;
	}

	if (createInfo->type != XR_TYPE_SESSION_CREATE_INFO)
	{
		return XrResult::XR_ERROR_VALIDATION_FAILURE;
	}

	const XrGraphicsBindingOXRVisionOSEPIC* GraphicsBinding = OpenXR::FindChainedStructByType<XrGraphicsBindingOXRVisionOSEPIC>(createInfo->next, (XrStructureType)XR_TYPE_GRAPHICS_BINDING_OXRVISIONOS_EPIC);
	if (GraphicsBinding)
	{
		//TODO use the graphics binding?
	}

	OutSession = MakeShared<FOXRVisionOSSession, ESPMode::ThreadSafe>(createInfo, Instance);
	if (OutSession->bCreateFailed)
	{
		OutSession = nullptr;
		return XrResult::XR_ERROR_RUNTIME_FAILURE;
	}
	return XrResult::XR_SUCCESS;
}


FOXRVisionOSSession::FOXRVisionOSSession(const XrSessionCreateInfo* createInfo, FOXRVisionOSInstance* InInstance)
{
	Instance = InInstance;

	if (FOXRVisionOSTracker::Create(Tracker, Instance->IsExtensionEnabled(XR_EXT_EYE_GAZE_INTERACTION_EXTENSION_NAME)) == false)
	{
		UE_LOG(LogOXRVisionOS, Warning, TEXT("FOXRVisionOSTracker::Create failed.  See logging above.")); 
		bCreateFailed = true;
		return;
	}

	//if (FOXRVisionOSController::Create(Controllers, this) == false)
	//{
	//	UE_LOG(LogOXRVisionOS, Warning, TEXT("FOXRVisionOSController::Create failed.  See logging above."));
	//	bCreateFailed = true;
	//	return;
	//}

	const UOXRVisionOSRuntimeSettings* Settings = GetDefault<UOXRVisionOSRuntimeSettings>();
	AimPoseAdjustment = FQuat(Settings->OXRVisionOSAimPoseAdjustment);
	GripPoseAdjustment = FQuat(Settings->OXRVisionOSGripPoseAdjustment);

	static_assert(OXRVisionOSSessionHelpers::ELayerIndex::Num <= OXRVisionOSSessionHelpers::MaxOXRVisionOSLayers);

	RenderBridge = FOXRVisionOS::Get()->GetRenderBridge();
	check(RenderBridge != nullptr);

	ARKitSession = ar_session_create();

	XrWaitFrameEvent = FPlatformProcess::GetSynchEventFromPool(false);
	XrWaitFrameEvent->Trigger();

	SetSessionState(XR_SESSION_STATE_IDLE);
	
	if (Instance->IsHMDAvailable())
	{
		SetSessionState(XR_SESSION_STATE_READY);
	}
	else
	{
	//	// else we need to wait for the hmd to become available, not relevant on this all-in-one hmd device
	//	OnWorldTickStartDelegateHandle = FWorldDelegates::OnWorldTickStart.AddRaw(this, &FOXRVisionOSSession::OnWorldTickStart);
	}
}

void FOXRVisionOSSession::OnWorldTickStart(UWorld* World, ELevelTick TickType, float DeltaTime)
{
	if (SessionState == XR_SESSION_STATE_IDLE)
	{
		if (Instance->IsHMDAvailable())
		{
			SetSessionState(XR_SESSION_STATE_READY);
		}
	}

	// Stop trying to transition to READY when no longer IDLE.
	if (SessionState != XR_SESSION_STATE_IDLE)
	{
		FWorldDelegates::OnWorldTickStart.Remove(OnWorldTickStartDelegateHandle);
	}
}

FOXRVisionOSSession::~FOXRVisionOSSession()
{
	FWorldDelegates::OnWorldTickStart.Remove(OnWorldTickStartDelegateHandle);

	//XrDestroySession can be called from any session state, so all of them must cleanup correctly.
	if (IsSessionRunning())
	{
		EndSessionInternal();
	}

	Controllers = nullptr;
	Tracker = nullptr;

	FPlatformProcess::ReturnSynchEventToPool(XrWaitFrameEvent);
	XrWaitFrameEvent = nullptr;

	if (bCreateFailed)
	{
		UE_LOG(LogOXRVisionOS, Warning, TEXT("Destructing FOXRVisionOSSession because session create failed.  You may see more warnings as api shutdown calls fail, depending on how the create failed."));
	}

	{
		// We need to unregister the swapchain buffers from the video out.  
		// First we have to make sure they aren't being used by flipping to blank and flushing it through.
		// Then we can unregister.

		// Completely drain all remaining GPU work	
		ENQUEUE_RENDER_COMMAND(FlipToBlank)([this](FRHICommandListImmediate& RHICmdList)
			{
				// Flip to blank and drain the GPU
				RHICmdList.EnqueueLambda([this](FRHICommandListImmediate& RHICmdList)
					{
						//TODO
						//MetalRHIOXRVisionOS::SetFlipToBlankAndFinalize(RHICmdList);

						//// Wait for all outstanding flips to clear, including our flip to blank.
					});

				// Perform a full flush
				RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThreadFlushResources);

				RHICmdList.BlockUntilGPUIdle();
			});

		FlushRenderingCommands();
	}
}

XrResult FOXRVisionOSSession::XrDestroySession()
{
	// Instance->DestroySession() can delete this, so better just return after that.
	return Instance->DestroySession();
}

XrResult FOXRVisionOSSession::XrEnumerateReferenceSpaces(
	uint32_t                                    SpaceCapacityInput,
	uint32_t*									SpaceCountOutput,
	XrReferenceSpaceType*						OutSpaces)
{
	if (SpaceCountOutput == nullptr)
	{
		return XrResult::XR_ERROR_VALIDATION_FAILURE;
	}
	if (SpaceCapacityInput != 0 && OutSpaces == nullptr)
	{
		return XrResult::XR_ERROR_VALIDATION_FAILURE;
	}

	const int NumSupportedSpaceTypes = 2;//3;

	*SpaceCountOutput = NumSupportedSpaceTypes;
	if (SpaceCapacityInput != 0)
	{
		if (SpaceCapacityInput < *SpaceCountOutput)
		{
			return XrResult::XR_ERROR_SIZE_INSUFFICIENT;
		}

		OutSpaces[0] = XR_REFERENCE_SPACE_TYPE_VIEW;
		OutSpaces[1] = XR_REFERENCE_SPACE_TYPE_LOCAL;
		//OutSpaces[2] = XR_REFERENCE_SPACE_TYPE_STAGE;

		//CoordinateSystemType CurrentCoordinateSystem = COORDINATE_SYSTEM_LOCAL_FLOOR;
		// const int32 Ret = SetCoordinateSystem(COORDINATE_SYSTEM_STAGE);
		// if (Ret == RESULT_OK)
		// {
		// 	UE_LOG(LogOXRVisionOS, Log, TEXT("XrEnumerateReferenceSpaces called SetCoordinateSystem(COORDINATE_SYSTEM_STAGE)."));
		// }
		// else
		// {
		// 	UE_LOG(LogOXRVisionOS, Warning, TEXT("XrEnumerateReferenceSpaces call to SetCoordinateSystem(COORDINATE_SYSTEM_STAGE) failed!"));
		// }
	}
	return XrResult::XR_SUCCESS;
}

XrResult FOXRVisionOSSession::XrCreateReferenceSpace(
	const XrReferenceSpaceCreateInfo*			CreateInfo,
	XrSpace*									Space)
{
	if (!CreateInfo || CreateInfo->type != XR_TYPE_REFERENCE_SPACE_CREATE_INFO)
	{
		return XrResult::XR_ERROR_VALIDATION_FAILURE;
	}

	TSharedPtr<FOXRVisionOSSpace, ESPMode::ThreadSafe> NewSpace = nullptr;
	XrResult Ret = FOXRVisionOSSpace::CreateReferenceSpace(NewSpace, CreateInfo, this);
	if (Ret == XrResult::XR_SUCCESS)
	{
		Spaces.Add(NewSpace);
		*Space = (XrSpace)NewSpace.Get();
	}

	return Ret;
}

XrResult FOXRVisionOSSession::XrGetReferenceSpaceBoundsRect(
	XrReferenceSpaceType                    ReferenceSpaceType,
	XrExtent2Df*							Bounds)
{
	if (!Bounds)
	{
		return XrResult::XR_ERROR_VALIDATION_FAILURE;
	}

	if (ReferenceSpaceType != XR_REFERENCE_SPACE_TYPE_STAGE)
	{
		UE_LOG(LogOXRVisionOS, Log, TEXT("XrGetReferenceSpaceBoundsRect called with an reference space type that does not support it"));
		Bounds->width = 0.0f;
		Bounds->height = 0.0f;
		return XR_SPACE_BOUNDS_UNAVAILABLE;
	}

	// According to docs the play area is a 1.5m sphere around the hmd.
	// We will not return any bounds for now.
	return XR_SPACE_BOUNDS_UNAVAILABLE;

	// Bounds->width = ;
	// Bounds->height = ;
	// return XR_SUCCESS;
}

XrResult FOXRVisionOSSession::XrCreateActionSpace(
	const XrActionSpaceCreateInfo*	CreateInfo,
	XrSpace*						Space)
{
	if (!CreateInfo || CreateInfo->type != XR_TYPE_ACTION_SPACE_CREATE_INFO || !Space)
	{
		return XrResult::XR_ERROR_VALIDATION_FAILURE;
	}

	TSharedPtr<FOXRVisionOSSpace, ESPMode::ThreadSafe> NewSpace = nullptr;
	XrResult Ret = FOXRVisionOSSpace::CreateActionSpace(NewSpace, CreateInfo, this);
	if (Ret == XrResult::XR_SUCCESS)
	{
		Spaces.Add(NewSpace);
		*Space = (XrSpace)NewSpace.Get();
	}
	return Ret;
}

XrResult FOXRVisionOSSession::DestroySpace(FOXRVisionOSSpace* Space)
{
	uint32 ArrayIndex = Spaces.IndexOfByPredicate([Space](const TSharedPtr<FOXRVisionOSSpace, ESPMode::ThreadSafe>& Data) { return (Data.Get() == Space); });

	if (ArrayIndex == INDEX_NONE)
	{
		return  XrResult::XR_ERROR_HANDLE_INVALID;
	}

	Spaces.RemoveAtSwap(ArrayIndex);

	return XrResult::XR_SUCCESS;
}

XrResult FOXRVisionOSSession::XrEnumerateSwapchainFormats(
	uint32_t                                    FormatCapacityInput,
	uint32_t*									FormatCountOutput,
	int64_t*									Formats)
{
	if (FormatCountOutput == nullptr)
	{
		return XrResult::XR_ERROR_VALIDATION_FAILURE;
	}
	if (FormatCapacityInput != 0 && Formats == nullptr)
	{
		return XrResult::XR_ERROR_VALIDATION_FAILURE;
	}

	const int NumSupportedFormats = 4;

	*FormatCountOutput = NumSupportedFormats;
	if (FormatCapacityInput != 0)
	{
		if (FormatCapacityInput < *FormatCountOutput)
		{
			return XrResult::XR_ERROR_SIZE_INSUFFICIENT;
		}

		//TODO is this right?
		// HDR formats 
		Formats[0] = PF_B8G8R8A8;			//k8_8_8_8UNorm			// This is Unreal's default format
		Formats[1] = PF_A2B10G10R10; 		//k10_10_10_2UNorm		
		Formats[2] = PF_FloatRGB; 			//k11_11_10Float
		Formats[3] = PF_FloatRGBA; 			//k16_16_16_16Float

		//Depth formats - depth is not supported
		//formats[1] = PF_DepthStencil;		//k32Float
		//formats[2] = PF_D24;				//k32Float
	}
	return XrResult::XR_SUCCESS;
}

XrResult FOXRVisionOSSession::XrCreateSwapchain(
	const XrSwapchainCreateInfo*				CreateInfo,
	XrSwapchain*								Swapchain)
{
	if (CreateInfo == nullptr || CreateInfo->type != XR_TYPE_SWAPCHAIN_CREATE_INFO || Swapchain == nullptr)
	{
		return XrResult::XR_ERROR_VALIDATION_FAILURE;
	}

	TSharedPtr<FOXRVisionOSSwapchain, ESPMode::ThreadSafe> NewSwapchain = nullptr;
	XrResult Ret = FOXRVisionOSSwapchain::Create(NewSwapchain, CreateInfo, OXRVisionOSSessionHelpers::OXRVisionOSBackbufferLength, this);
	if (Ret == XrResult::XR_SUCCESS)
	{
		*Swapchain = (XrSwapchain)NewSwapchain.Get();
		Swapchains.Add(NewSwapchain);
	}
	return Ret;
}

XrResult FOXRVisionOSSession::DestroySwapchain(
	FOXRVisionOSSwapchain* Swapchain)
{
	uint32 ArrayIndex = Swapchains.IndexOfByPredicate([Swapchain](const TSharedPtr<FOXRVisionOSSwapchain, ESPMode::ThreadSafe>& Data) { return (Data.Get() == Swapchain); });

	if (ArrayIndex == INDEX_NONE)
	{
		return XrResult::XR_ERROR_HANDLE_INVALID;
	}

	Swapchains.RemoveAtSwap(ArrayIndex);

	return XrResult::XR_SUCCESS;
}

XrResult FOXRVisionOSSession::XrAttachSessionActionSets(
	const XrSessionActionSetsAttachInfo* AttachInfo)
{
	if (AttachInfo == nullptr || AttachInfo->type != XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO)
	{
		return XrResult::XR_ERROR_VALIDATION_FAILURE;
	}

	if (ActionSetsAlreadyAttached)
	{
		return XrResult::XR_ERROR_ACTIONSETS_ALREADY_ATTACHED;
	}

	UE_LOG(LogOXRVisionOS, Verbose, TEXT("XrAttachSessionActionSets Attaching"));

	check(AttachInfo->next == nullptr);
	for (int i = 0; i < AttachInfo->countActionSets; i++)
	{
		XrActionSet ActionSet = AttachInfo->actionSets[i];
		check(ActionSet != XR_NULL_HANDLE);
		TSharedPtr<FOXRVisionOSActionSet, ESPMode::ThreadSafe> ActionSetPtr = Instance->GetActionSetSharedPtr(ActionSet);
		ActionSetPtr->Attach(this);
		ActionSets.Add(ActionSetPtr); 
	}

	// Choose the InteractionProfileBinding we will actually use.
	bool bBoundAnInteractionProfile = false;
	for (auto& InteractionProfilePath : Instance->GetPreferredInteractionProfileBindingList())
	{
		const FOXRVisionOSInstance::FInteractionProfileBinding* IPB = Instance->GetInteractionProfileBindings().Find(InteractionProfilePath);

		if (IPB != nullptr)
		{
			UE_LOG(LogOXRVisionOS, Verbose, TEXT("XrAttachSessionActionSets found interaction profile %s and will bind inputs using it."), ANSI_TO_TCHAR(Instance->PathToString(IPB->InteractionProfilePath)));
			bBoundAnInteractionProfile = true;

			for (TSharedPtr<FOXRVisionOSActionSet, ESPMode::ThreadSafe>& ActionSet : ActionSets)
			{
				// Each action may be bound to one or more source
				for (TSharedPtr<FOXRVisionOSAction, ESPMode::ThreadSafe> ActionSP : ActionSet->GetActions())
				{
					FOXRVisionOSAction* Action = ActionSP.Get();
					for (const FOXRVisionOSInstance::FInteractionProfileBinding::FBinding& Binding : IPB->Bindings)
					{
						if (Binding.Action == (XrAction)Action)
						{
							// If this interaction profile is the current interaction profile of the top level path of this binding bind it.
							XrPath TopLevelBindingPath = Instance->InputPathToTopLevelPath(Binding.Path);
							XrPath TopLevelPathInteractionProfile = Instance->TopLevelPathToInteractionProfile(TopLevelBindingPath);
							if (InteractionProfilePath == TopLevelPathInteractionProfile)
							{
								UE_LOG(LogOXRVisionOS, Verbose, TEXT("  Binding %s to %s"), ANSI_TO_TCHAR(Instance->PathToString(Binding.Path)), OXRVisionOS::OXRVisionOSControllerButtonToTCHAR(Binding.Button));
								Action->BindSource(Binding.Path, Binding.Button);
							}
							else
							{
								UE_LOG(LogOXRVisionOS, Verbose, TEXT("  NOT binding %s to %s because this is not the best interaction profile"), ANSI_TO_TCHAR(Instance->PathToString(Binding.Path)), OXRVisionOS::OXRVisionOSControllerButtonToTCHAR(Binding.Button));
							}
						}
					}
				}
			}
		}
	}
	
	if (!bBoundAnInteractionProfile)
	{
		UE_LOG(LogOXRVisionOS, Warning, TEXT("XrAttachSessionActionSets has been called, but no supported interaction profiles were suggested through xrSuggestInteractionProfileBindings!  None of the input actions will function."));
	}
	
	ActionSetsAlreadyAttached = true;
	return XrResult::XR_SUCCESS;
}

XrResult FOXRVisionOSSession::XrGetCurrentInteractionProfile(
	XrPath TopLevelUserPath,
	XrInteractionProfileState* InteractionProfile)
{
	if (ActionSetsAlreadyAttached == false)
	{
		return XrResult::XR_ERROR_ACTIONSET_NOT_ATTACHED;
	}

	InteractionProfile->interactionProfile = Instance->GetCurrentInteractionProfile(TopLevelUserPath);

	return XrResult::XR_SUCCESS;
}

XrResult FOXRVisionOSSession::XrGetActionStateBoolean(
	const XrActionStateGetInfo* GetInfo,
	XrActionStateBoolean* State)
{
	const FOXRVisionOSAction* Action = (const FOXRVisionOSAction*)GetInfo->action;
	if (Action)
	{
		return Action->GetActionStateBoolean(GetInfo->subactionPath, *State);
	}
	else
	{
		return XR_ERROR_HANDLE_INVALID;
	}
}

XrResult FOXRVisionOSSession::XrGetActionStateFloat(
	const XrActionStateGetInfo* GetInfo,
	XrActionStateFloat* State)
{
	const FOXRVisionOSAction* Action = (const FOXRVisionOSAction*)GetInfo->action;
	if (Action)
	{
		return Action->GetActionStateFloat(GetInfo->subactionPath, *State);
	}
	else
	{
		return XR_ERROR_HANDLE_INVALID;
	}
}

XrResult FOXRVisionOSSession::XrGetActionStateVector2f(
	const XrActionStateGetInfo* GetInfo,
	XrActionStateVector2f* State)
{
	const FOXRVisionOSAction* Action = (const FOXRVisionOSAction*)GetInfo->action;
	if (Action)
	{
		return Action->GetActionStateVector2f(GetInfo->subactionPath, *State);
	}
	else
	{
		return XR_ERROR_HANDLE_INVALID;
	}
}

XrResult FOXRVisionOSSession::XrGetActionStatePose(
	const XrActionStateGetInfo* GetInfo,
	XrActionStatePose* State)
{
	const FOXRVisionOSAction* Action = (const FOXRVisionOSAction*)GetInfo->action;
	if (Action)
	{
		return Action->GetActionStatePose(GetInfo->subactionPath, *State);
	}
	else
	{
		return XR_ERROR_HANDLE_INVALID;
	}
}

XrResult FOXRVisionOSSession::XrSyncActions(
	const XrActionsSyncInfo* SyncInfo)
{
	check(SyncInfo);

	//TODO XR_SESSION_LOSS_PENDING

	if (SessionState != XR_SESSION_STATE_FOCUSED)
	{
		return XrResult::XR_SESSION_NOT_FOCUSED;
	}

	// Collect the hardware data
	//Controllers->SyncActions();
	const FPipelinedFrameState& PipelinedFrameState = GetPipelinedFrameStateForThread();
	Tracker->SyncActions(PipelinedFrameState.FrameCounter, PipelinedFrameState.PredictedDisplayTime);

	// Clear active from all action sets
	for (TSharedPtr<FOXRVisionOSActionSet, ESPMode::ThreadSafe>& ActionSet : ActionSets)
	{
		ActionSet->ClearActive();
	}
	
	//TArray<FOXRVisionOSActionSet*> ActiveActionSetsByPriority; // Note ActionSetPriority is currently unsupported on OXRVisionOS.
	for (int32 i = 0; i < SyncInfo->countActiveActionSets; i++)
	{
		XrActiveActionSet ActiveXRActionSet = SyncInfo->activeActionSets[i];
		FOXRVisionOSActionSet& ActiveActionSet = *((FOXRVisionOSActionSet*)ActiveXRActionSet.actionSet);
		ActiveActionSet.SetActive(ActiveXRActionSet.subactionPath);
		//ActiveActionSetsByPriority.Add(&ActiveActionSet);
	}
	// Sort highest priority to the front.
	//ActiveActionSetsByPriority.Sort([](const FOXRVisionOSActionSet& A, const FOXRVisionOSActionSet& B) { return A.GetPriority() < B.GetPriority(); });

//UE_LOG(LogOXRVisionOS, Log, TEXT("XrSyncActions PipelinedFrameState.PredictedDisplayTime %lld"), PipelinedFrameState.PredictedDisplayTime);

	// Go through the action sets updating state per hardware.
//	for (TSharedPtr<FOXRVisionOSActionSet, ESPMode::ThreadSafe>& ActionSet : ActionSets)
//	{
//		ActionSet->SyncActions(Controllers, Tracker);
//	}

	return XR_SUCCESS;
}

XrResult FOXRVisionOSSession::XrEnumerateBoundSourcesForAction(
	const XrBoundSourcesForActionEnumerateInfo*		EnumerateInfo,
	uint32_t										SourceCapacityInput,
	uint32_t*										SourceCountOutput,
	XrPath*											OutSources)
{
	if (SourceCountOutput == nullptr)
	{
		return XrResult::XR_ERROR_VALIDATION_FAILURE;
	}
	if (SourceCapacityInput != 0 && OutSources == nullptr)
	{
		return XrResult::XR_ERROR_VALIDATION_FAILURE;
	}

	if (EnumerateInfo == nullptr || EnumerateInfo->action == XR_NULL_HANDLE)
	{
		return XrResult::XR_ERROR_VALIDATION_FAILURE;
	}

	FOXRVisionOSAction* Action = (FOXRVisionOSAction*)EnumerateInfo->action;
	const TArray<XrPath>& Sources = Action->GetBoundSources();

	const int Num = Sources.Num();
	*SourceCountOutput = Num;
	if (SourceCapacityInput != 0)
	{
		if (SourceCapacityInput < *SourceCountOutput)
		{
			return XrResult::XR_ERROR_SIZE_INSUFFICIENT;
		}

		for (int32 i = 0; i < Num; ++i)
		{
			OutSources[i] = Sources[i];
		}
	}
	return XrResult::XR_SUCCESS;
}

XrResult FOXRVisionOSSession::XrApplyHapticFeedback(
	const XrHapticActionInfo* HapticActionInfo,
	const XrHapticBaseHeader* HapticFeedback)
{
	if (SessionState != XR_SESSION_STATE_FOCUSED)
	{
		return XrResult::XR_SESSION_NOT_FOCUSED;
	}

	UE_LOG(LogOXRVisionOS, Warning, TEXT("FOXRVisionOSSession XrApplyHapticFeedback returning XR_ERROR_PATH_UNSUPPORTED for path because there are no supported haptic paths on this platform."));
	return XrResult::XR_ERROR_PATH_UNSUPPORTED;
}

XrResult FOXRVisionOSSession::XrStopHapticFeedback(
	const XrHapticActionInfo* HapticActionInfo)
{
	if (SessionState != XR_SESSION_STATE_FOCUSED)
	{
		return XrResult::XR_SESSION_NOT_FOCUSED;
	}

	UE_LOG(LogOXRVisionOS, Warning, TEXT("FOXRVisionOSSession XrApplyHapticFeedback returning XR_ERROR_PATH_UNSUPPORTED for path because there are no supported haptic paths on this platform."));
	return XrResult::XR_ERROR_PATH_UNSUPPORTED;
}

XrResult FOXRVisionOSSession::XrBeginSession(
	const XrSessionBeginInfo* BeginInfo)
{
	if (BeginInfo == nullptr || BeginInfo->type != XR_TYPE_SESSION_BEGIN_INFO)
	{
		return XrResult::XR_ERROR_VALIDATION_FAILURE;
	}

	if (SessionState == XrSessionState::XR_SESSION_STATE_UNKNOWN  || SessionState == XrSessionState::XR_SESSION_STATE_IDLE)
	{
		return XrResult::XR_ERROR_SESSION_NOT_READY;
	}

	if (IsSessionRunning())
	{
		return XrResult::XR_ERROR_SESSION_RUNNING;
	}

	if (BeginInfo->primaryViewConfigurationType != XrViewConfigurationType::XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO)
	{
		return XrResult::XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED;
	}
    
    VOSLayerRenderer = OXRVisionOS::GetSwiftLayerRenderer();

	// Create World Tracking data provider
	ar_world_tracking_configuration_t ArKitWorldTrackingConfiguration = ar_world_tracking_configuration_create();
	ARKitWorldTrackingProvider = ar_world_tracking_provider_create(ArKitWorldTrackingConfiguration);
	// Create Data providers
	ar_data_providers_t ARKitDataProviders = ar_data_providers_create();
	// Add
	ar_data_providers_add_data_provider(ARKitDataProviders, ARKitWorldTrackingProvider);
	
	// Start the arkit session
	ar_session_run(ARKitSession, ARKitDataProviders);

	// If we failed to get access to the hmd we could fail here like this.
	// if (???)
	// {
	// 	UE_LOG(LogOXRVisionOS, Warning, TEXT("Open failed code 0x%x.  Session going to XR_SESSION_STATE_LOSS_PENDING."), HMDHandle);
	// 	SetSessionState(XrSessionState::XR_SESSION_STATE_LOSS_PENDING);
	// 	return XrResult::XR_SESSION_LOSS_PENDING;
	// }

	if (Tracker->RegisterDevice(TrackerDeviceType::DEVICE_VR_HMD, HMDHandle) == false)
	{
		UE_LOG(LogOXRVisionOS, Error, TEXT("Tracker->RegisterDevice failed.  Bug."));
		return XrResult::XR_ERROR_RUNTIME_FAILURE;
	}

    // Fill the game threads view info buffer with temporary data, some of which was collected in FSwiftAppBootstrap::KickoffWithCompositingLayer
	// XrLocateViews will overwrite this with real data fetched from the VisionOS API, but we will use this temp fov data in the first two game thread frames.
    LocateViewInfoBuffer[0].NumViews = OXRVisionOS::GetSwiftNumViewports();
    LocateViewInfoBuffer[0].ViewTransforms[0] = matrix_identity_float4x4;
    LocateViewInfoBuffer[0].ViewTransforms[1] = matrix_identity_float4x4;
    simd_float4x4 ViewTransforms[2];
    const float HalfFov = 2.19686294f / 2.f;
    LocateViewInfoBuffer[0].HmdFovs[0].angleLeft = -HalfFov - 0.1f;
    LocateViewInfoBuffer[0].HmdFovs[0].angleRight = HalfFov - 0.1f;
    LocateViewInfoBuffer[0].HmdFovs[0].angleUp = HalfFov;
    LocateViewInfoBuffer[0].HmdFovs[0].angleDown = -HalfFov;
    LocateViewInfoBuffer[0].HmdFovs[1].angleLeft = -HalfFov + 0.1f;
    LocateViewInfoBuffer[0].HmdFovs[1].angleRight = HalfFov + 0.1f;
    LocateViewInfoBuffer[0].HmdFovs[1].angleUp = HalfFov;
    LocateViewInfoBuffer[0].HmdFovs[1].angleDown = HalfFov;
    LocateViewInfoBuffer[0].HmdFovs[1] = LocateViewInfoBuffer[0].HmdFovs[0];
    LocateViewInfoBuffer[1] = LocateViewInfoBuffer[0];
    LocateViewInfoBuffer[2] = LocateViewInfoBuffer[0];

	bIsRunning = true;
	PipelinedFrameStateGame.bSynchronizing = true;
	PipelinedFrameStateRendering.bSynchronizing = true;
	PipelinedFrameStateRHI.bSynchronizing = true;

	//Controllers->OnBeginSession(Tracker.Get());

	return XrResult::XR_SUCCESS;
}

XrResult FOXRVisionOSSession::XrEndSession()
{
	if (!IsSessionRunning())
	{
		return XrResult::XR_ERROR_SESSION_NOT_RUNNING;
	}

	if (SessionState != XrSessionState::XR_SESSION_STATE_STOPPING)
	{
		return XrResult::XR_ERROR_SESSION_NOT_STOPPING;
	}

	EndSessionInternal();

	SetSessionState(XrSessionState::XR_SESSION_STATE_IDLE);
	// Start polling to go back to XR_SESSION_STATE_READY as soon as the hmd is available
	OnWorldTickStartDelegateHandle = FWorldDelegates::OnWorldTickStart.AddRaw(this, &FOXRVisionOSSession::OnWorldTickStart);

	return XrResult::XR_SUCCESS;
}

void FOXRVisionOSSession::EndSessionInternal()
{
	UE_LOG(LogOXRVisionOS, Warning, TEXT("FOXRVisionOSSession::EndSessionInternal()"));

	//Controllers->OnEndSession(Tracker.Get());

	Tracker->UnregisterDevice(HMDHandle);
	//Close(HMDHandle);
	HMDHandle = INVALID_DEVICE_HANDLE;

	PipelinedFrameStateGame.CommandListContext = PipelinedFrameStateRendering.CommandListContext = PipelinedFrameStateRHI.CommandListContext = nullptr;

	if (bExitRequested)
	{
		SetSessionState(XrSessionState::XR_SESSION_STATE_EXITING);
	}

	bExitRequested = false;
	bIsRunning = false;
}

XrResult FOXRVisionOSSession::XrRequestExitSession()
{
	if (!IsSessionRunning())
	{
		return XrResult::XR_ERROR_SESSION_NOT_RUNNING;
	}

	SetSessionState(XrSessionState::XR_SESSION_STATE_STOPPING);
	bExitRequested = true;
	return XrResult::XR_SUCCESS;
}

// FOpenXRHMD::OnBeginSimulation_GameThread()
XrResult FOXRVisionOSSession::XrWaitFrame(
	const XrFrameWaitInfo* frameWaitInfo,
	XrFrameState* frameState)
{
	//UE_LOG(LogOXRVisionOS, Log, TEXT("FOXRVisionOSSession XrWaitFrame called"));

	SCOPED_NAMED_EVENT_TEXT("FOXRVisionOSSession::XrWaitFrame", FColor::Turquoise);

	// frameWaitInfo can be null
	if (frameWaitInfo != nullptr && frameWaitInfo->type != XR_TYPE_FRAME_WAIT_INFO)
	{
		return XrResult::XR_ERROR_VALIDATION_FAILURE;
	}

	if (frameState == nullptr || frameState->type != XR_TYPE_FRAME_STATE)
	{
		return XrResult::XR_ERROR_VALIDATION_FAILURE;
	}

	if (!IsSessionRunning())
	{
		UE_LOG(LogOXRVisionOS, Log, TEXT("FOXRVisionOSSession XrWaitFrame XR_ERROR_SESSION_NOT_RUNNING"));
		return XrResult::XR_ERROR_SESSION_NOT_RUNNING;
	}

	if (GetSessionState() == XrSessionState::XR_SESSION_STATE_READY)
	{
		SetSessionState(XrSessionState::XR_SESSION_STATE_SYNCHRONIZED);
		SetSessionState(XrSessionState::XR_SESSION_STATE_VISIBLE);
		SetSessionState(XrSessionState::XR_SESSION_STATE_FOCUSED);

		SessionFrameCounter += OXRVisionOSSessionHelpers::OXRVisionOSBackbufferLength;  // Jump ahead in case the current frame counter was partially used while the previous session was being ended.  
		PipelinedFrameStateGame.bSynchronizing = false;
	}

	// Block until the previous frame's xrBeginFrame has happened.
	{
		SCOPED_NAMED_EVENT_TEXT("FOXRVisionOSSession::XrWaitFrame XrWaitFrameEvent->Wait()", FColor::Turquoise);
		XrWaitFrameEvent->Wait();
	}

	PipelinedFrameStateGame.FrameCounter = SessionFrameCounter++;

	//UE_LOG(LogOXRVisionOS, Log, TEXT("FOXRVisionOSSession XrWaitFrame proceeds Game fliparg: %d BBIndex: %d"), PipelinedFrameStateGame.FrameCounter, PipelinedFrameStateGame.BackBufferIndex);

	// {
	// 	const int32 ret = ReprojectionBeginFrame(PipelinedFrameStateGame.FrameCounter);
	// 	if (ret != RESULT_OK)
	// 	{
	// 		UE_LOG(LogOXRVisionOS, Warning, TEXT("ReprojectionBeginFrame(%d) failed: %d (%u, 0x%08X)"), PipelinedFrameStateGame.FrameCounter, ret, ret, ret);
	// 	}
	// }
    
    PipelinedFrameStateGame.SwiftFrame = cp_layer_renderer_query_next_frame(VOSLayerRenderer);
    UE_LOG(LogTemp, Log, TEXT("SwiftLayerFrame(0x%x) from cp_layer_renderer_query_next_frame in FOXRVisionOSSession::XrWaitFrame"), PipelinedFrameStateGame.SwiftFrame);
    
    // Fetch the predicted timing information.
    PipelinedFrameStateGame.SwiftFrameTiming = cp_frame_predict_timing(PipelinedFrameStateGame.SwiftFrame);
    if ( PipelinedFrameStateGame.SwiftFrameTiming == nullptr)
    {
        //TODO ??? failed frame?
        // This means the layer is not in the correct state  Perhaps we are either running too early or we need to end the session.
        // we might want to return XR_ERROR_SESSION_NOT_RUNNING here, but only if we actually end the session first.
        assert(false);
        //return;
    }

    // Mark the beginnng of our game thread update work.  All the stuff before rendering.
    UE_LOG(LogTemp, Log, TEXT("SwiftLayerFrame(0x%x) from cp_frame_start_update in FOXRVisionOSSession::XrWaitFrame"), PipelinedFrameStateGame.SwiftFrame);
    cp_frame_start_update(PipelinedFrameStateGame.SwiftFrame);

    const cp_time_t PresentationTime = cp_frame_timing_get_presentation_time(PipelinedFrameStateGame.SwiftFrameTiming);
    const CFTimeInterval TargetRenderTime = cp_time_to_cf_time_interval(PresentationTime); // CRTimeInterval is double time in seconds.
    
	XrTime DisplayPeriod = 6 * 1000000; //TODO other frame sequences?  This is 90fps
	frameState->predictedDisplayPeriod = DisplayPeriod;
	frameState->predictedDisplayTime = OXRVisionOS::APITimeToXrTime(TargetRenderTime);
	PipelinedFrameStateGame.PredictedDisplayTime = frameState->predictedDisplayTime;
    
    PipelinedFrameStateGame.DeviceAnchor = ar_device_anchor_create();
    check(ARKitWorldTrackingProvider)
    auto anchor_status = ar_world_tracking_provider_query_device_anchor_at_timestamp(ARKitWorldTrackingProvider, TargetRenderTime, PipelinedFrameStateGame.DeviceAnchor);
    if (anchor_status == ar_device_anchor_query_status_success)
    {
        // Get the HMD transform and cache it
        PipelinedFrameStateGame.HeadTransform = ar_anchor_get_origin_from_anchor_transform(PipelinedFrameStateGame.DeviceAnchor);
    }
    else
    {
        // Cache identity as the HMD transform as a fallback
        PipelinedFrameStateGame.HeadTransform = matrix_identity_float4x4;
    }
    
    LocateViewInfoIndexAdvance();

	//TODO XrWaitFrame shouldRender based on visible & focused state
	frameState->shouldRender = XR_TRUE;

	// We have to pipeline state updates in order to prevent task overlap (esp during map loading where we kick extra renders)
	ENQUEUE_RENDER_COMMAND(UpdatePipelinedFrameState)([this, NewRenderingFrameState = PipelinedFrameStateGame](FRHICommandListImmediate& RHICmdList)
	{
		check(IsInRenderingThread());
		PipelinedFrameStateRendering = NewRenderingFrameState;
		RHICmdList.EnqueueLambda([this, NewRHIFrameState = PipelinedFrameStateRendering](FRHICommandListImmediate&)
		{
			PipelinedFrameStateRHI = NewRHIFrameState;
		});
	});

	// Update the STAGE space
	//Tracker->UpdateReferenceSpaces(Instance, this, HMDHandle, PipelinedFrameStateGame.PredictedDisplayTime, PipelinedFrameStateGame.FrameCounter);

	return XrResult::XR_SUCCESS;
}

void FOXRVisionOSSession::OnBeginRendering_GameThread()
{
    // Mark the end of our previous game frame
    if (PipelinedFrameStateGame.SwiftFrame)
    {
        UE_LOG(LogTemp, Log, TEXT("SwiftLayerFrame(0x%x) from cp_frame_end_update in FOXRVisionOSSession::OnBeginRendering_GameThread"), PipelinedFrameStateGame.SwiftFrame);
        cp_frame_end_update(PipelinedFrameStateGame.SwiftFrame);
        PipelinedFrameStateGame.SwiftFrame = nullptr;
    }
}

// FOpenXRHMD::OnBeginRendering_RenderThread
void FOXRVisionOSSession::OnBeginRendering_RenderThread()
{
    const MetalRHIVisionOS::BeginRenderingImmersiveParams Params{ PipelinedFrameStateRendering.SwiftFrame };
    MetalRHIVisionOS::BeginRenderingImmersive(Params);
    WaitUntil();
    StartSubmission();
}

// FOpenXRHMD::OnBeginRendering_RHIThread
XrResult FOXRVisionOSSession::XrBeginFrame(
	const XrFrameBeginInfo* FrameBeginInfo)
{
	check(!IsRHIThreadRunning() || IsInRHIThread());

	//UE_LOG(LogOXRVisionOS, Log, TEXT("FOXRVisionOSSession XrBeginFrame fliparg: %d BBIndex: %d"), PipelinedFrameStateRendering.FrameCounter, PipelinedFrameStateRendering.BackBufferIndex); // Note frame state not copied yet

	SCOPED_NAMED_EVENT_TEXT("FOXRVisionOSSession::XrBeginFrame", FColor::Turquoise);

	// frameBeginInfo can be null
	if (FrameBeginInfo != nullptr && FrameBeginInfo->type != XR_TYPE_FRAME_BEGIN_INFO)
	{
		XrWaitFrameEvent->Trigger();
		return XrResult::XR_ERROR_VALIDATION_FAILURE;
	}

	if (!IsSessionRunning())
	{
		XrWaitFrameEvent->Trigger();
		return XrResult::XR_ERROR_SESSION_NOT_RUNNING;
	}

	if (PipelinedFrameStateRHI.FrameCounter == CachedBeginFlipFrameCounter)
	{
		// We don't trigger in this case because there wasn't a call to xrWaitFrame that corresponds
		// xrWaitFrame should have incremented the counter
		UE_LOG(LogOXRVisionOS, Log, TEXT("XrWaitFrameEvent not triggered because call order invalid  RHIFrameCounter %i != RenderFrameCounter %i"), PipelinedFrameStateRHI.FrameCounter, CachedBeginFlipFrameCounter);
		return XrResult::XR_ERROR_CALL_ORDER_INVALID;
	}
    CachedBeginFlipFrameCounter = PipelinedFrameStateRHI.FrameCounter;
       
	PipelinedFrameStateRHI.CommandListContext = RHIGetDefaultContext();
	
	if (PipelinedFrameStateRHI.bSynchronizing)
	{
		XrWaitFrameEvent->Trigger();
		//UE_LOG(LogOXRVisionOS, Log, TEXT("FOXRVisionOSSession XrBeginFrame bSynchronizing=true RHI fliparg: %d BBIndex: %d"), PipelinedFrameStateRHI.FrameCounter, PipelinedFrameStateRHI.BackBufferIndex);
		return XrResult::XR_SUCCESS;
	}

	XrWaitFrameEvent->Trigger();

	const XrResult BeginFrameResult = (NextBackBufferIndex != PreviousBackBufferIndexInBegin) ? XrResult::XR_SUCCESS : XrResult::XR_FRAME_DISCARDED;

	//if (PipelinedFrameStateRHI.CommandListContext)
    {
        // TODO
        // I think there's gonna be some trickiness when we have accurate VISIBLE + FOCUSED session states.
        // If we return XrFrameState.shouldRender == false (headset off), we still could deploy WaitUntilSafeForRendering
        // either here or in xrWaitSwapchainImage. In that case, we will hang because the flip won't deploy.
        // We might want to reconsider using WaitUntilSafeForRendering. If we use xrWaitFrame or xrWaitSwapchainImage
        // to wait for a CPU-signal for swapchain safety, then...we don't really need to check on the GPU again.
        // But the docs infer that ReprojectionBeginFrame and waitUntilSafeForRendering are tied :/
    }
    
    {
        //TODO idk, doing this twice once for render once for rhi...
        FPipelinedFrameState& FrameState = PipelinedFrameStateRHI;
        
        UE_LOG(LogTemp, Log, TEXT("SwiftLayerFrame(0x%x) cp_frame_query_drawable in FOXRVisionOSSession::XrBeginFrame"), FrameState.SwiftFrame);
        FrameState.SwiftDrawable = cp_frame_query_drawable(FrameState.SwiftFrame);
        check(FrameState.SwiftDrawable);
        UE_LOG(LogTemp, Log, TEXT("SwiftLayerFrame(0x%x) cp_frame_query_drawable got SwiftDrawable(0x%x) in FOXRVisionOSSession::StartSubmission()"), FrameState.SwiftFrame, FrameState.SwiftDrawable);

        FrameState.SwiftFinalFrameTiming = cp_drawable_get_frame_timing(FrameState.SwiftDrawable);
        
        CFTimeInterval TimeInterval = cp_time_to_cf_time_interval(cp_frame_timing_get_presentation_time(FrameState.SwiftFinalFrameTiming));
        check(ARKitWorldTrackingProvider)
        auto anchor_status = ar_world_tracking_provider_query_device_anchor_at_timestamp(ARKitWorldTrackingProvider, TimeInterval, FrameState.DeviceAnchor);
        if (anchor_status == ar_device_anchor_query_status_success)
        {
            // Get the HMD transform and cache it
            FrameState.HeadTransform = ar_anchor_get_origin_from_anchor_transform(FrameState.DeviceAnchor);
        }
        else
        {
            // Leave FrameState.HeadTransform untouched, same as game thread. It may simply be identity.  But whatever it is we will go ahead and render with it.
        }
    }
    
    //MetalRHIOXRVisionOS::WaitUntilSafeForRendering(PipelinedFrameStateRHI.CommandListContext, NextBackBufferIndex);
    PreviousBackBufferIndexInBegin = NextBackBufferIndex;

	return BeginFrameResult;
}

void FOXRVisionOSSession::WaitUntil()
{
    FPipelinedFrameState& FrameState = GetPipelinedFrameStateForThread();
    
    UE_LOG(LogTemp, Log, TEXT("SwiftLayerFrame(0x%x) cp_time_wait_until starting in FOXRVisionOSSession::WaitUntil"), FrameState.SwiftFrame);
    cp_time_wait_until(cp_frame_timing_get_optimal_input_time(FrameState.SwiftFrameTiming));
    UE_LOG(LogTemp, Log, TEXT("SwiftLayerFrame(0x%x) cp_time_wait_until completed in FOXRVisionOSSession::WaitUntil"), FrameState.SwiftFrame);
}

void FOXRVisionOSSession::StartSubmission()
{
    FPipelinedFrameState& FrameState = GetPipelinedFrameStateForThread();
    
    // tell the compositor service we are going to submit soon and need the drawable
    UE_LOG(LogTemp, Log, TEXT("SwiftLayerFrame(0x%x) cp_frame_start_submission in FOXRVisionOSSession::XrBeginFrame"), FrameState.SwiftFrame);
    cp_frame_start_submission(FrameState.SwiftFrame);

    UE_LOG(LogTemp, Log, TEXT("SwiftLayerFrame(0x%x) cp_frame_query_drawable in FOXRVisionOSSession::XrBeginFrame"), FrameState.SwiftFrame);
    FrameState.SwiftDrawable = cp_frame_query_drawable(FrameState.SwiftFrame);
    check(FrameState.SwiftDrawable);
    UE_LOG(LogTemp, Log, TEXT("SwiftLayerFrame(0x%x) cp_frame_query_drawable got SwiftDrawable(0x%x) in FOXRVisionOSSession::StartSubmission()"), FrameState.SwiftFrame, FrameState.SwiftDrawable);

    FrameState.SwiftFinalFrameTiming = cp_drawable_get_frame_timing(FrameState.SwiftDrawable);
    
    CFTimeInterval TimeInterval = cp_time_to_cf_time_interval(cp_frame_timing_get_presentation_time(FrameState.SwiftFinalFrameTiming));
    check(ARKitWorldTrackingProvider)
    auto anchor_status = ar_world_tracking_provider_query_device_anchor_at_timestamp(ARKitWorldTrackingProvider, TimeInterval, FrameState.DeviceAnchor);
    if (anchor_status == ar_device_anchor_query_status_success)
    {
        cp_drawable_set_device_anchor(FrameState.SwiftDrawable, FrameState.DeviceAnchor);
        // Get the HMD transform and cache it
        FrameState.HeadTransform = ar_anchor_get_origin_from_anchor_transform(FrameState.DeviceAnchor);
    }
    else
    {
        //TODO: this is also how you disable reprojection.  Hook that up to a cvar.
        cp_drawable_set_device_anchor(FrameState.SwiftDrawable, nullptr);
        // Leave FrameState.HeadTransform untouched, same as game thread. It may simply be identity.  But whatever it is we will go ahead and render with it.
    }
    
    //cp_drawable_set_ar_pose(FrameState.SwiftDrawable, )  // I think this is may be a deprecated way of specifying the frame's transform.  Replaced with cp_drawable_set_device_anchor
}

// FOpenXRHMD::OnFinishRendering_RHIThread()
XrResult FOXRVisionOSSession::XrEndFrame(
	const XrFrameEndInfo* InFrameEndInfo)
{
	//UE_LOG(LogOXRVisionOS, Log, TEXT("FOXRVisionOSSession XrEndFrame RHI fliparg: %d BBIndex: %d"), PipelinedFrameStateRHI.FrameCounter, PipelinedFrameStateRHI.BackBufferIndex);

	SCOPED_NAMED_EVENT_TEXT("FOXRVisionOSSession::XrEndFrame", FColor::Turquoise);

	if (InFrameEndInfo == nullptr || InFrameEndInfo->type != XR_TYPE_FRAME_END_INFO)
	{
		return XrResult::XR_ERROR_VALIDATION_FAILURE;
	}

	if (!IsSessionRunning())
	{
		return XrResult::XR_ERROR_SESSION_NOT_RUNNING;
	}

	if (PipelinedFrameStateRHI.bSynchronizing)
	{
		//UE_LOG(LogOXRVisionOS, Log, TEXT("FOXRVisionOSSession XrEndFrame bSynchronizing=true fliparg=%i buffer=%i"), PipelinedFrameStateRHI.FrameCounter, PipelinedFrameStateRHI.BackBufferIndex);
		return XrResult::XR_SUCCESS;
	}

	const XrFrameEndInfo& FrameEndInfo = *InFrameEndInfo;
	check(FrameEndInfo.environmentBlendMode == XR_ENVIRONMENT_BLEND_MODE_OPAQUE); // only opaque is supported, see XrEnumerateEnvironmentBlendModes
	check(FrameEndInfo.layerCount <= OXRVisionOSSessionHelpers::MaxOXRVisionOSLayers);
    
    {
        //TODO??? supporting only one layer now.  This means we do not have a non-reprojected layer.  Maybe we could have one as a separate swiftui?
        check(FrameEndInfo.layerCount == 1);
        check(FrameEndInfo.layers != nullptr);
        const XrCompositionLayerBaseHeader& LayerBaseHeader = *FrameEndInfo.layers[0];
        check(LayerBaseHeader.type == XR_TYPE_COMPOSITION_LAYER_PROJECTION);
        const XrCompositionLayerProjection& LayerProjection = *(reinterpret_cast<const XrCompositionLayerProjection*>(FrameEndInfo.layers[0]));
        check(LayerProjection.viewCount == 2); // Must be the number of views returned by xrLocateViews
        const XrCompositionLayerProjectionView& View0 = LayerProjection.views[0];
        const XrCompositionLayerProjectionView& View1 = LayerProjection.views[1];

        check(View0.subImage.swapchain == View1.subImage.swapchain); //TODO: should we do it this way or should we have two separate swapchains?  If we did we would need to handle both below.
        
        const FOXRVisionOSSwapchain& SwapchainData0 = *(reinterpret_cast<FOXRVisionOSSwapchain*>(View0.subImage.swapchain));
        const FOXRVisionOSSwapchain::FSwapchainImage& ReleasedImage = SwapchainData0.GetLastReleasedImage();
        
        //TODO feed in all necessary cp_frame_end_submission data
        const MetalRHIVisionOS::PresentImmersiveParams Params{ReleasedImage.Image, PipelinedFrameStateRHI.SwiftDrawable};
        MetalRHIVisionOS::PresentImmersive(Params);
    }

	 	NextBackBufferIndex = (NextBackBufferIndex + 1) % OXRVisionOSSessionHelpers::OXRVisionOSBackbufferLength;
	
	// else
	// {
	// 	UE_LOG(LogOXRVisionOS, Log, TEXT("XrEndFrame no layers, discarding, FrameCounter: %d, BufferIndex: %d"), PipelinedFrameStateRHI.FrameCounter, NextBackBufferIndex);
	// }

	return XrResult::XR_SUCCESS;
}

XrResult FOXRVisionOSSession::XrLocateViews(
	const XrViewLocateInfo*		ViewLocateInfo,
	XrViewState*				ViewState,
	uint32_t                    ViewCapacityInput,
	uint32_t*					ViewCountOutput,
	XrView*						Views)
{
    if (ViewLocateInfo == nullptr || ViewLocateInfo->type != XR_TYPE_VIEW_LOCATE_INFO)
    {
        return XrResult::XR_ERROR_VALIDATION_FAILURE;
    }
    
    if (ViewState == nullptr)
    {
        return XrResult::XR_ERROR_VALIDATION_FAILURE;
    }
    
    if (ViewCapacityInput != 0 && Views == nullptr)
    {
        return XrResult::XR_ERROR_VALIDATION_FAILURE;
    }
    
    if (ViewLocateInfo->viewConfigurationType != XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO)
    {
        return XrResult::XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED;
    }
    
    FPipelinedFrameState& FrameState = GetPipelinedFrameStateForThread();
    
    FOXRVisionOSSpace& BaseSpace = *(FOXRVisionOSSpace*)ViewLocateInfo->space;
    const int32 FrameCounter = FrameState.FrameCounter;
    FTransform BaseSpaceInverseTransform;
    XrSpaceLocationFlags BaseSpaceFlags;
    FVector OutLinearVelocity; FVector OutAngularVelocity; XrSpaceVelocityFlags OutVelocityFlags; // These are unused here.
    BaseSpace.GetInverseTransform(ViewLocateInfo->displayTime, BaseSpaceInverseTransform, BaseSpaceFlags, OutLinearVelocity, OutAngularVelocity, OutVelocityFlags);
    
    uint32_t NumViews = 0;
    if (IsInGameThread())
    {
        // We will use the NumViews from a previous render frame.
        const FLocateViewInfo& ViewInfoCache = GetLocateViewInfo_GameThread();
        NumViews = ViewInfoCache.NumViews;
    }
    else
    {
        check(IsInRenderingThread());
        check(FrameState.SwiftDrawable);
        NumViews = cp_drawable_get_view_count(FrameState.SwiftDrawable);
        FLocateViewInfo& ViewInfoCache = GetLocateViewInfo_RenderThread();
        ViewInfoCache.NumViews = NumViews;
    }

    //TODO probably all of this should happen only once per frame in xrBeginFrame and once in xrWaitFrame.
    
	*ViewCountOutput = NumViews;
	if (ViewCapacityInput != 0)
    {
        if (ViewCapacityInput < *ViewCountOutput)
        {
            return XrResult::XR_ERROR_SIZE_INSUFFICIENT;
        }
        
        //TODO make this more real???
        ViewState->viewStateFlags = (XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_POSITION_TRACKED_BIT);
        
        for (int Index = 0; Index < NumViews; ++Index)
        {
            if (IsInGameThread())
            {
                const FLocateViewInfo& ViewInfoCache = GetLocateViewInfo_GameThread();
                
                // Adjust the camera transform for the current eye position.
                simd_float4x4 CameraTransform = simd_mul(PipelinedFrameStateGame.HeadTransform, ViewInfoCache.ViewTransforms[Index]);
                
                Views[Index].pose = OXRVisionOS::ToXrPose(CameraTransform);
                Views[Index].fov = ViewInfoCache.HmdFovs[Index];
            }
            else
            {
                check(IsInRenderingThread());
                check(FrameState.SwiftDrawable);
                
                cp_view_t View = cp_drawable_get_view(FrameState.SwiftDrawable, Index);
                simd_float4 Tangents = cp_view_get_tangents(View);
                //simd_float2 DepthRange = cp_drawable_get_depth_range(FrameState.SwiftDrawable);  //TODO ??? do we need this?  There is an openxrextension for it, probably should use that.
    //                                                                          DepthRange[1], /* nearZ */
    //                                                                          DepthRange[0], /* farZ */
    //                                                                          true); /* reverseZ */
                
                FrameState.HmdFovs[Index].angleLeft     = -FMath::Atan(Tangents[0]);
                FrameState.HmdFovs[Index].angleRight    =  FMath::Atan(Tangents[1]);
                FrameState.HmdFovs[Index].angleUp       =  FMath::Atan(Tangents[2]);
                FrameState.HmdFovs[Index].angleDown     = -FMath::Atan(Tangents[3]);

                // Adjust the camera transform for the current eye position.
                simd_float4x4 ViewTransform = cp_view_get_transform(View);
                simd_float4x4 CameraTransform = simd_mul(PipelinedFrameStateRHI.HeadTransform, ViewTransform);
                
                Views[Index].pose = OXRVisionOS::ToXrPose(CameraTransform);
                Views[Index].fov = FrameState.HmdFovs[Index];
                
                // Cache the render thread data for the game thread
                FLocateViewInfo& ViewInfoCache = GetLocateViewInfo_RenderThread();
                ViewInfoCache.ViewTransforms[Index] = ViewTransform;
                ViewInfoCache.HmdFovs[Index] = FrameState.HmdFovs[Index];
            }
        }

		//StoreHMDPoseOffsetData(hmdInfo.devicePose);
	}

//	if (IsInGameThread())
//	{
//		ENQUEUE_RENDER_COMMAND(UpdateHMDPoseFromGT)([this, NewRenderingFrameState = PipelinedFrameStateGame](FRHICommandListImmediate& RHICmdList)
//		{
//			PipelinedFrameStateRendering.HMDPoseInTrackerSpace = NewRenderingFrameState.HMDPoseInTrackerSpace;
//			RHICmdList.EnqueueLambda([this, NewRHIFrameState = PipelinedFrameStateRendering](FRHICommandListImmediate&)
//			{
//				PipelinedFrameStateRHI.HMDPoseInTrackerSpace = NewRHIFrameState.HMDPoseInTrackerSpace;
//			});
//		});
//	}
//	else
//	{
//		check(IsInRenderingThread());
//		FRHICommandListImmediate& RHICmdList = GetImmediateCommandList_ForRenderCommand();
//		RHICmdList.EnqueueLambda([this, NewRHIFrameState = PipelinedFrameStateRendering](FRHICommandListImmediate&)
//		{
//			PipelinedFrameStateRHI.HMDPoseInTrackerSpace = NewRHIFrameState.HMDPoseInTrackerSpace;
//		});
//	}

	return XrResult::XR_SUCCESS;
}

// void FOXRVisionOSSession::StoreHMDPoseOffsetData(const PoseData& HmdPose)
// {
// 	FTransform HmdTransform = OXRVisionOS::ToFTransform(HmdPose);
// 	// After a tracking disruption hmd transform often becomes slightly denormalized, more than our checkslow threshold.
// 	HmdTransform.NormalizeRotation();

// 	FPipelinedFrameState& FrameStateForThread = GetPipelinedFrameStateForThread();
// 	FrameStateForThread.HMDPoseInTrackerSpace = HmdTransform;
// }

void FOXRVisionOSSession::GetHMDTransform(XrTime DisplayTime, FTransform& OutTransform, XrSpaceLocationFlags& OutLocationFlags)
{
    //TODO just ignoring XrTime right now!!!
    
    FPipelinedFrameState& FrameState = GetPipelinedFrameStateForThread();
    
    OutLocationFlags = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT
        | XR_SPACE_LOCATION_POSITION_VALID_BIT
        | XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT
    | XR_SPACE_LOCATION_POSITION_TRACKED_BIT;
    
    OutTransform = OXRVisionOS::ToFTransform(FrameState.HeadTransform);
}

void FOXRVisionOSSession::GetTransformForButton(EOXRVisionOSControllerButton Button, XrTime DisplayTime, FTransform& OutTransform, XrSpaceLocationFlags& OutLocationFlags, FVector& OutLinearVelocity, FVector& OutAngularVelocity, XrSpaceVelocityFlags& OutVelocityFlags)
{
	OutLocationFlags = 0;
	OutVelocityFlags = 0;

//	bool bGaze = false;
	int32 DeviceHandle = INVALID_DEVICE_HANDLE;
	switch (Button)
	{
	case EOXRVisionOSControllerButton::GripL:
	case EOXRVisionOSControllerButton::AimL:
		DeviceHandle = Controllers->GetLeftControllerHandle();
		break;
	case EOXRVisionOSControllerButton::GripR:
	case EOXRVisionOSControllerButton::AimR:
		DeviceHandle = Controllers->GetRightControllerHandle();
		break;
	case EOXRVisionOSControllerButton::HMDPose:
        check(false); // should be using GetHMDTransform
		DeviceHandle = HMDHandle;
		break;
	// case EOXRVisionOSControllerButton::GazePose:
	// 	DeviceHandle = HMDHandle;
	// 	bGaze = true;
	// 	break;
	default:
		break;
	}
	if (DeviceHandle == INVALID_DEVICE_HANDLE)
	{
		OutTransform = FTransform::Identity;
		return;
	}

	// if (bGaze)
	// {
	// 	// We get the gaze origin from the hmd.
	// 	bool bSuccess = Tracker->GetGazeResult(DeviceHandle, DisplayTime, FrameNumber, GazeResultData, HMDResultData);
	// 	if (!bSuccess)
	// 	{
	// 		OutTransform = FTransform::Identity;
	// 		return;
	// 	}
	// 	OutLocationFlags = OXRVisionOS::ToXrSpaceLocationFlags(HMDResultData);
	// 	OutVelocityFlags = 0; // gaze never provides velocity data

	// 	// gazeDirection is in gazeOriginPose space.
	// 	PoseData& HMDGazeOriginPose = HMDResultData.Hmd.GazeOriginPose;
	// 	FQuat GazeOriginQuat =
	// 	FVector GazeOriginVector =
	// 	FQuat GazeQuat =

	// 	OutTransform = FTransform(GazeQuat) * FTransform(GazeOriginQuat, GazeOriginVector);
	// }
	// else
	{
		OXRVisionOS::TrackerResultData ResultData = {};
		bool bSuccess = Tracker->GetResult(DeviceHandle, DisplayTime, ResultData);
		if (!bSuccess)
		{
			OutTransform = FTransform::Identity;
			return;
		}

		//OutLocationFlags = OXRVisionOS::ToXrSpaceLocationFlags(ResultData);
		//OutVelocityFlags = OXRVisionOS::ToXrSpaceVelocityFlags(ResultData);

		if ((OutLocationFlags & (XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)))
		{
			//PoseData& Pose = DeviceHandle == HMDHandle ? hmdInfo.Pose : vrControllerInfo.Pose;
			OXRVisionOS::TrackerPoseData Pose; //TEMP

			// If part of the transform is invalid send along identity/zero.
			FQuat Quat = (OutLocationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) ? OXRVisionOS::ToFQuat(Pose.orientation) : FQuat::Identity;
			FVector Vector = (OutLocationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) ? OXRVisionOS::ToFVector(Pose.position) : FVector(0.0f, 0.0f, 0.0f);

			OutTransform = FTransform(Quat, Vector);

			// api provides just one controller pose, which is a reasonable aim pose, we shall make both adjustible.
			if (Button == EOXRVisionOSControllerButton::GripL || Button == EOXRVisionOSControllerButton::GripR)
			{
				OutTransform.SetRotation(OutTransform.TransformRotation(GripPoseAdjustment));
			}
			else if (Button == EOXRVisionOSControllerButton::AimL || Button == EOXRVisionOSControllerButton::AimR)
			{
				OutTransform.SetRotation(OutTransform.TransformRotation(AimPoseAdjustment));
			}
		}

		// if (OutVelocityFlags & XR_SPACE_VELOCITY_LINEAR_VALID_BIT)
		// {
		// 	OutLinearVelocity = OXRVisionOS::ToFVector(ResultData.velocity);
		// }
		// if (OutVelocityFlags & XR_SPACE_VELOCITY_ANGULAR_VALID_BIT)
		// {
		// 	OutAngularVelocity = OXRVisionOS::ToFVector(ResultData.angularVelocity);
		// }
	}
}

// FOpenXRInput::Tick, game thread, after xrSyncActions
void FOXRVisionOSSession::OnPostSyncActions()
{
}

XrTime FOXRVisionOSSession::GetCurrentTime() const
{	
    const CFAbsoluteTime CurrentTime = CFAbsoluteTimeGetCurrent();
	return OXRVisionOS::APITimeToXrTime(CurrentTime);
}

void FOXRVisionOSSession::SetSessionState(XrSessionState NewState)
{
	UE_LOG(LogOXRVisionOS, Log, TEXT("FOXRVisionOSSession %s -> %s"), OpenXRSessionStateToString(SessionState), OpenXRSessionStateToString(NewState));

	SessionState = NewState;

	XrEventDataSessionStateChanged Event;
	Event.type = XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED;
	Event.next = nullptr;
	Event.session = (XrSession)this;
	Event.state = SessionState;
	Event.time = GetCurrentTime();

	Instance->EnqueueEvent(Event);
}

bool FOXRVisionOSSession::IsSessionRunning() const
{
	return bIsRunning;
}

const FOXRVisionOSSession::FPipelinedFrameState& FOXRVisionOSSession::GetPipelinedFrameStateForThread() const
{
	if (IsInRHIThread())
	{
		return PipelinedFrameStateRHI;
	}
	else if (IsInRenderingThread())
	{
		return PipelinedFrameStateRendering;
	}
	else
	{
		check(IsInGameThread());
		return PipelinedFrameStateGame;
	}
}

FOXRVisionOSSession::FPipelinedFrameState& FOXRVisionOSSession::GetPipelinedFrameStateForThread()
{
	return const_cast<FOXRVisionOSSession::FPipelinedFrameState&>(static_cast<const FOXRVisionOSSession&>(*this).GetPipelinedFrameStateForThread());
}
