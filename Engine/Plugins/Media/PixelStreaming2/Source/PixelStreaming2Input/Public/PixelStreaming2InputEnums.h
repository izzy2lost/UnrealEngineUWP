// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * @brief The data types supported when sending messages across the data channel to/from peers
 *
 */
enum class PIXELSTREAMING2INPUT_API EPixelStreaming2MessageTypes
{
	Uint8 = 0,
	Uint16 = 1,
	Int16 = 2,
	Float = 3,
	Double = 4,
	String = 5,
	Undefined = 6
};

/**
 * @brief The message directions
 *
 */
enum class PIXELSTREAMING2INPUT_API EPixelStreaming2MessageDirection : uint8
{
	ToStreamer = 0,
	FromStreamer = 1
};

/**
 * @brief The possible actions pixel streaming supports. These actions help differentiate input received from the browser
 *
 */
enum class PIXELSTREAMING2INPUT_API EPixelStreaming2InputAction : uint8
{
	X = 0,
	Y = 1,
	Axis = 2,
	Click = 3,
	Touch = 4,
	None = 5,
};

/**
 * @brief The possible control schemes pixel streaming supports. RouteToWindow routes input at an application level. RouteToWidget routes input to a specific widget, ignoring the rest of the application.
 *
 */
enum class PIXELSTREAMING2INPUT_API EPixelStreaming2InputType : uint8
{
	RouteToWindow = 0,
	RouteToWidget = 1
};

/**
 * @brief Known message types for the default `ToStreamer` message protocol of Pixel Streaming.
 */
namespace EPixelStreaming2ToStreamerMessage
{
	static FString IFrameRequest = FString(TEXT("IFrameRequest"));
	static FString RequestQualityControl = FString(TEXT("RequestQualityControl"));
	static FString FpsRequest = FString(TEXT("FpsRequest"));
	static FString AverageBitrateRequest = FString(TEXT("AverageBitrateRequest"));
	static FString StartStreaming = FString(TEXT("StartStreaming"));
	static FString StopStreaming = FString(TEXT("StopStreaming"));
	static FString LatencyTest = FString(TEXT("LatencyTest"));
	static FString RequestInitialSettings = FString(TEXT("RequestInitialSettings"));
	static FString TestEcho = FString(TEXT("TestEcho"));
	static FString UIInteraction = FString(TEXT("UIInteraction"));
	static FString Command = FString(TEXT("Command"));
	static FString TextboxEntry = FString(TEXT("TextboxEntry"));
	static FString KeyDown = FString(TEXT("KeyDown"));
	static FString KeyUp = FString(TEXT("KeyUp"));
	static FString KeyPress = FString(TEXT("KeyPress"));
	static FString MouseEnter = FString(TEXT("MouseEnter"));
	static FString MouseLeave = FString(TEXT("MouseLeave"));
	static FString MouseDown = FString(TEXT("MouseDown"));
	static FString MouseUp = FString(TEXT("MouseUp"));
	static FString MouseMove = FString(TEXT("MouseMove"));
	static FString MouseWheel = FString(TEXT("MouseWheel"));
	static FString MouseDouble = FString(TEXT("MouseDouble"));
	static FString TouchStart = FString(TEXT("TouchStart"));
	static FString TouchEnd = FString(TEXT("TouchEnd"));
	static FString TouchMove = FString(TEXT("TouchMove"));
	static FString GamepadButtonPressed = FString(TEXT("GamepadButtonPressed"));
	static FString GamepadButtonReleased = FString(TEXT("GamepadButtonReleased"));
	static FString GamepadAnalog = FString(TEXT("GamepadAnalog"));
	static FString GamepadConnected = FString(TEXT("GamepadConnected"));
	static FString GamepadDisconnected = FString(TEXT("GamepadDisconnected"));
	static FString XREyeViews = FString(TEXT("XREyeViews"));
	static FString XRHMDTransform = FString(TEXT("XRHMDTransform"));
	static FString XRControllerTransform = FString(TEXT("XRControllerTransform"));
	static FString XRButtonPressed = FString(TEXT("XRButtonPressed"));
	static FString XRButtonTouched = FString(TEXT("XRButtonTouched"));
	static FString XRButtonReleased = FString(TEXT("XRButtonReleased"));
	static FString XRAnalog = FString(TEXT("XRAnalog"));
	static FString XRSystem = FString(TEXT("XRSystem"));
	static FString XRButtonTouchReleased = FString(TEXT("XRButtonTouchReleased"));
} // namespace EPixelStreaming2ToStreamerMessage

/**
 * @brief Known message types for the default `FromStreamer` message protocol of Pixel Streaming.
 */
namespace EPixelStreaming2FromStreamerMessage
{
	static FString QualityControlOwnership = FString(TEXT("QualityControlOwnership"));
	static FString Response = FString(TEXT("Response"));
	static FString Command = FString(TEXT("Command"));
	static FString FreezeFrame = FString(TEXT("FreezeFrame"));
	static FString UnfreezeFrame = FString(TEXT("UnfreezeFrame"));
	static FString VideoEncoderAvgQP = FString(TEXT("VideoEncoderAvgQP"));
	static FString LatencyTest = FString(TEXT("LatencyTest"));
	static FString InitialSettings = FString(TEXT("InitialSettings"));
	static FString FileExtension = FString(TEXT("FileExtension"));
	static FString FileMimeType = FString(TEXT("FileMimeType"));
	static FString FileContents = FString(TEXT("FileContents"));
	static FString TestEcho = FString(TEXT("TestEcho"));
	static FString InputControlOwnership = FString(TEXT("InputControlOwnership"));
	static FString GamepadResponse = FString(TEXT("GamepadResponse"));
	static FString Protocol = FString(TEXT("Protocol"));
} // namespace EPixelStreaming2FromStreamerMessage