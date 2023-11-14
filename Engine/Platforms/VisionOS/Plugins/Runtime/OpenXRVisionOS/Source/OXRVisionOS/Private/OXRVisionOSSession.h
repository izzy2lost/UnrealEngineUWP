// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OXRVisionOS.h"

#include "Engine/EngineBaseTypes.h"
#include "Epic_openxr.h"
#include "OXRVisionOS_openxr_platform.h"
#include "HAL/Runnable.h"
#include "HeadMountedDisplayTypes.h"

#if PLATFORM_VISIONOS
#import <ARKit/ARKit.h>
#import <CompositorServices/CompositorServices.h>
#endif


class FOXRVisionOSInstance;
class FOXRVisionOSActionSet;
class FOXRVisionOSSpace;
class FOXRVisionOSSwapchain;
class FOXRVisionOSTracker;
class FOXRVisionOSController;

class FRHICommandListImmediate;
enum class EOXRVisionOSControllerButton : int32;

class FOXRVisionOSSession
{
public:
	static XrResult Create(TSharedPtr<FOXRVisionOSSession, ESPMode::ThreadSafe>& OutSession, const XrSessionCreateInfo* createInfo, FOXRVisionOSInstance* Instance);
	FOXRVisionOSSession(const XrSessionCreateInfo* createInfo, FOXRVisionOSInstance* Instance);
	~FOXRVisionOSSession();
	XrResult XrDestroySession();

	XrResult XrEnumerateReferenceSpaces(
		uint32_t spaceCapacityInput,
		uint32_t* spaceCountOutput,
		XrReferenceSpaceType* spaces);

	XrResult XrCreateReferenceSpace(
		const XrReferenceSpaceCreateInfo* createInfo,
		XrSpace* space);

	XrResult XrGetReferenceSpaceBoundsRect(
		XrReferenceSpaceType referenceSpaceType,
		XrExtent2Df* bounds);

	XrResult XrCreateActionSpace(
		const XrActionSpaceCreateInfo* createInfo,
		XrSpace* space);

	XrResult DestroySpace(
		FOXRVisionOSSpace* Space);

	XrResult XrEnumerateSwapchainFormats(
		uint32_t formatCapacityInput,
		uint32_t* formatCountOutput,
		int64_t* formats);

	XrResult XrCreateSwapchain(
		const XrSwapchainCreateInfo* createInfo,
		XrSwapchain* swapchain);

	XrResult DestroySwapchain(
		FOXRVisionOSSwapchain* Swapchain);

	XrResult XrAttachSessionActionSets(
		const XrSessionActionSetsAttachInfo* attachInfo);

	XrResult XrGetCurrentInteractionProfile(
		XrPath topLevelUserPath,
		XrInteractionProfileState* interactionProfile);

	XrResult XrGetActionStateBoolean(
		const XrActionStateGetInfo* getInfo,
		XrActionStateBoolean* state);

	XrResult XrGetActionStateFloat(
		const XrActionStateGetInfo* getInfo,
		XrActionStateFloat* state);

	XrResult XrGetActionStateVector2f(
		const XrActionStateGetInfo* getInfo,
		XrActionStateVector2f* state);

	XrResult XrGetActionStatePose(
		const XrActionStateGetInfo* getInfo,
		XrActionStatePose* state);

	XrResult XrSyncActions(
		const XrActionsSyncInfo* syncInfo);

	XrResult XrEnumerateBoundSourcesForAction(
		const XrBoundSourcesForActionEnumerateInfo* enumerateInfo,
		uint32_t sourceCapacityInput,
		uint32_t* sourceCountOutput,
		XrPath* sources);

	XrResult XrApplyHapticFeedback(
		const XrHapticActionInfo* hapticActionInfo,
		const XrHapticBaseHeader* hapticFeedback);

	XrResult XrStopHapticFeedback(
		const XrHapticActionInfo* hapticActionInfo);

	XrResult XrBeginSession(
		const XrSessionBeginInfo* beginInfo);

	XrResult XrEndSession();

	void EndSessionInternal();

	XrResult XrRequestExitSession();

	XrResult XrWaitFrame(
		const XrFrameWaitInfo* frameWaitInfo,
		XrFrameState* frameState);

	void OnBeginRendering_GameThread();

	void OnBeginRendering_RenderThread();

	XrResult XrBeginFrame(
		const XrFrameBeginInfo* frameBeginInfo);

	XrResult XrEndFrame(
		const XrFrameEndInfo* frameEndInfo);

	XrResult XrLocateViews(
		const XrViewLocateInfo* viewLocateInfo,
		XrViewState* viewState,
		uint32_t viewCapacityInput,
		uint32_t* viewCountOutput,
		XrView* views);

	XrTime GetCurrentTime() const;

	void SetSessionState(XrSessionState NewState);
	XrSessionState GetSessionState() const { return SessionState; }

	bool IsSessionRunning() const;

	FOXRVisionOSTracker& GetTrackerChecked() const { check(Tracker.IsValid()); return *Tracker; }

	struct FPipelinedFrameState
	{
		int32 FrameCounter = -1;
		XrTime PredictedDisplayTime = 0;
		IRHICommandContext* CommandListContext = nullptr;
		//FTransform HMDPoseInTrackerSpace = FTransform::Identity;
		//PoseData HmdPose = {};
		cp_frame_t SwiftFrame = nullptr;
        cp_frame_timing_t SwiftFrameTiming = nullptr;
        simd_float4x4 HeadTransform = matrix_identity_float4x4;
        cp_drawable_t SwiftDrawable = nullptr;
        cp_frame_timing_t SwiftFinalFrameTiming = nullptr; // TODO should we do this or should we overwrite SwifFrameTiming???
        ar_device_anchor_t DeviceAnchor = nullptr;
		bool bSynchronizing = true; // when true we are in the process of starting up a new session and the new frame state has not yet propagated.
		XrFovf HmdFovs[2];
        int32 LocateViewInfoBufferIndex = 0;
	};
	const FPipelinedFrameState& GetPipelinedFrameStateForThread() const;
	FPipelinedFrameState& GetPipelinedFrameStateForThread();
	void GetHMDTransform(XrTime DisplayTime, FTransform& OutTransform, XrSpaceLocationFlags& OutLocationFlags);
	void GetTransformForButton(EOXRVisionOSControllerButton Button, XrTime DisplayTime, FTransform& OutTransform, XrSpaceLocationFlags& OutLocationFlags, FVector& OutLinearVelocity, FVector& OutAngularVelocity, XrSpaceVelocityFlags& OutVelocityFlags);
	void SetTriggerEffectVRControllers(int32 ControllerId, const FInputDeviceProperty* Property);
	// FOpenXRInput::Tick, game thread, after xrSyncActions
	void OnPostSyncActions(); 

	struct FLocateViewInfo
    {
        uint32_t NumViews = 0;
        simd_float4x4 ViewTransforms[2];
        XrFovf HmdFovs[2];
    };
    static constexpr int LocateViewInfoBufferLength = 3;
    FLocateViewInfo LocateViewInfoBuffer[LocateViewInfoBufferLength];
    void LocateViewInfoIndexAdvance() { ++PipelinedFrameStateGame.LocateViewInfoBufferIndex; }
    const FLocateViewInfo& GetLocateViewInfo_GameThread() { return LocateViewInfoBuffer[(PipelinedFrameStateGame.LocateViewInfoBufferIndex - 1) % LocateViewInfoBufferLength]; }
    FLocateViewInfo& GetLocateViewInfo_RenderThread() { return LocateViewInfoBuffer[PipelinedFrameStateRendering.LocateViewInfoBufferIndex % LocateViewInfoBufferLength]; }

private:
	bool bCreateFailed = false;
	bool bIsRunning = false;
	bool bExitRequested = false;
	FOXRVisionOSInstance* Instance = nullptr;
	uint64_t XRTimeScale = 1;
	XrSessionState SessionState = XR_SESSION_STATE_UNKNOWN;
	bool ActionSetsAlreadyAttached = false;
	TArray<TSharedPtr<FOXRVisionOSActionSet, ESPMode::ThreadSafe>> ActionSets;
	TSharedPtr<FOXRVisionOSController, ESPMode::ThreadSafe> Controllers;
	TArray<TSharedPtr<FOXRVisionOSSpace, ESPMode::ThreadSafe>> Spaces;
	FQuat GripPoseAdjustment;
	FQuat AimPoseAdjustment;

	FPlatformMemory::FPlatformVirtualMemoryBlock m_reprojectionData;
	FPlatformMemory::FPlatformVirtualMemoryBlock m_displayData;


	TArray<TSharedPtr<FOXRVisionOSSwapchain, ESPMode::ThreadSafe>> Swapchains;

	int32 NextBackBufferIndex = 0;
	int32 PreviousBackBufferIndexInBegin = -1;
	int32 SessionFrameCounter = 0;

	// Because OpenXR works with view poses, but works with hmd pose we cache an hmd pose when we get the trackign data.
	// If the OpenXR Application modifies the view poses such that they are not relative to the hmd pose this will fail and the altered view poses will not behave as expected.
	// This should work correctly if api modifies the interpupillary distance, provided that is properly pipelined on the api side (meaning it is constant for each fliparg).
	//void StoreHMDPoseOffsetData(const PoseData& HmdPose);

	static const int32 INVALID_DEVICE_HANDLE = -1;
	int32 HMDHandle = INVALID_DEVICE_HANDLE;

	TSharedPtr <FOXRVisionOSTracker, ESPMode::ThreadSafe> Tracker;

	//TEMP
	static const int32 RESULT_OK = 0;


	void OnWorldTickStart(UWorld* World, ELevelTick TickType, float DeltaTime);
	FDelegateHandle OnWorldTickStartDelegateHandle;

	void WaitUntil();
    void StartSubmission();
	FEvent* XrWaitFrameEvent = nullptr;
	FPipelinedFrameState	PipelinedFrameStateGame;
	FPipelinedFrameState	PipelinedFrameStateRendering;
	FPipelinedFrameState	PipelinedFrameStateRHI;
	int32 CachedBeginFlipFrameCounter = INT32_MAX;

	// TODO Reorganize this class?

	FOpenXRRenderBridge* RenderBridge = nullptr;

	CP_OBJECT_cp_layer_renderer* VOSLayerRenderer = nullptr;
	ar_session_t ARKitSession = nullptr;
	ar_world_tracking_provider_t ARKitWorldTrackingProvider = nullptr;
	ar_device_anchor_t ARKitHMDAnchor = nullptr;
};
