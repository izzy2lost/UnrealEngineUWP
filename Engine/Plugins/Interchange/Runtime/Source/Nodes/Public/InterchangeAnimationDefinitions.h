// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/NameTypes.h"
#include "Types/AttributeStorage.h"

#include "InterchangeAnimationDefinitions.generated.h"

UENUM()
enum class EInterchangePropertyTracks : int32
{
	/** Common Properties*/
	Visibility,
	AutoActivate,

	/** Light Properties */
	LightColor,
	LightIntensity,
	LightIntensityUnits,
	LightTemperature,
	LightUseTemperature,

	/** Camera Properties*/
	CameraAspectRatio,
	CameraAutoCalculateOrthoPlanes,
	CameraAspectRatioAxisConstraint,
	CameraConstrainAspectRatio,
	CameraCurrentAperture,
	CameraCurrentFocalLength,
	CameraCustomNearClippingPlane,
	CameraFieldOfView,
	CameraFilmbackSensorAspectRatio,
	CameraFilmbackSensorHeight,
	CameraFilmbackSensorWidth,
	CameraFocusSettingsManualFocusDistance,
	CameraMobility,
	CameraOrthoFarClipPlane,
	CameraOrthoNearClipPlane,
	CameraOrthoWidth,
	CameraPostProcessBlendWeight,
	CameraProjectionMode,
	CameraShouldUpdatePhysicsVolume,
	CameraUseFieldOfViewForLOD,

	None = -1 UMETA(hidden),
};

namespace UE
{
	namespace Interchange
	{
		template<> struct TAttributeTypeTraits<EInterchangePropertyTracks>
		{
			static constexpr EAttributeTypes GetType()
			{
				return EAttributeTypes::Int32;
			}
			static FString ToString(const uint16& Value)
			{
				int32 ValueConv = Value;
				FStringFormatOrderedArguments OrderedArguments;
				OrderedArguments.Add(FStringFormatArg(ValueConv));
				return FString::Format(TEXT("{0}"), OrderedArguments);
			}
		};
	}
}