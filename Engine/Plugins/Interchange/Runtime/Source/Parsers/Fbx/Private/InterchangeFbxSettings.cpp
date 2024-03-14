// Copyright Epic Games, Inc.All Rights Reserved.

#include "InterchangeFbxSettings.h"

UInterchangeFbxSettings::UInterchangeFbxSettings()
{
	if(HasAnyFlags(EObjectFlags::RF_ClassDefaultObject))
	{
		PredefinedPropertyTracks =
		{
			{ TEXT("Intensity"), { EInterchangePropertyTracks::LightIntensity, EInterchangeAnimationPayLoadType::CURVE }	},
			{ TEXT("Color"), {EInterchangePropertyTracks::LightColor, EInterchangeAnimationPayLoadType::CURVE} },
			{ TEXT("bUseTemperature"), {EInterchangePropertyTracks::LightUseTemperature, EInterchangeAnimationPayLoadType::STEPCURVE} },
			{ TEXT("IntensityUnits"), {EInterchangePropertyTracks::LightIntensityUnits, EInterchangeAnimationPayLoadType::STEPCURVE} },
			{ TEXT("bHidden"), {EInterchangePropertyTracks::Visibility, EInterchangeAnimationPayLoadType::STEPCURVE} },
			{ TEXT("CurrentFocalLength"), {EInterchangePropertyTracks::CameraCurrentFocalLength, EInterchangeAnimationPayLoadType::CURVE} },
			{ TEXT("CurrentAperture"), {EInterchangePropertyTracks::CameraCurrentAperture, EInterchangeAnimationPayLoadType::CURVE} },
			{ TEXT("AspectRatioAxisConstraint"), {EInterchangePropertyTracks::CameraAspectRatioAxisConstraint, EInterchangeAnimationPayLoadType::CURVE} },
		};
	}
}

FInterchangePropertyTracksSettings UInterchangeFbxSettings::GetPropertyTrack(const FString& PropertyName) const
{
	FInterchangePropertyTracksSettings Result;
	if(const FInterchangePropertyTracksSettings* Property = PredefinedPropertyTracks.Find(PropertyName))
	{
		Result = *Property;
	}
	else if(Property = CustomPropertyTracks.Find(PropertyName); Property != nullptr)
	{
		Result = *Property;
	}

	return Result;
}

FName FInterchangePropertyTracksSettings::GetPropertyName()
{
	FName Name;

	switch(Property)
	{
	case EInterchangePropertyTracks::Visibility :
		Name = UE::Interchange::Animation::PropertyTracks::Visibility;
		break;

	case EInterchangePropertyTracks::CameraAspectRatioAxisConstraint:
		Name = UE::Interchange::Animation::PropertyTracks::Camera::AspectRatioAxisConstraint;
		break;

	case EInterchangePropertyTracks::CameraAutoActivate:
		Name = UE::Interchange::Animation::PropertyTracks::Camera::AutoActivate;
		break;

	case EInterchangePropertyTracks::CameraConstrainAspectRatio:
		Name = UE::Interchange::Animation::PropertyTracks::Camera::ConstrainAspectRatio;
		break;

	case EInterchangePropertyTracks::CameraCurrentAperture:
		Name = UE::Interchange::Animation::PropertyTracks::Camera::CurrentAperture;
		break;

	case EInterchangePropertyTracks::CameraCurrentFocalLength:
		Name = UE::Interchange::Animation::PropertyTracks::Camera::CurrentFocalLength;
		break;

	case EInterchangePropertyTracks::CameraCustomNearClippingPlane:
		Name = UE::Interchange::Animation::PropertyTracks::Camera::CustomNearClippingPlane;
		break;

	case EInterchangePropertyTracks::CameraFieldOfView:
		Name = UE::Interchange::Animation::PropertyTracks::Camera::FieldOfView;
		break;

	case EInterchangePropertyTracks::CameraMobility:
		Name = UE::Interchange::Animation::PropertyTracks::Camera::Mobility;
		break;

	case EInterchangePropertyTracks::CameraOrthoFarClipPlane:
		Name = UE::Interchange::Animation::PropertyTracks::Camera::OrthoFarClipPlane;
		break;

	case EInterchangePropertyTracks::CameraOrthoWidth:
		Name = UE::Interchange::Animation::PropertyTracks::Camera::OrthoWidth;
		break;

	case EInterchangePropertyTracks::LightColor:
		Name = UE::Interchange::Animation::PropertyTracks::Light::Color;
		break;

	case EInterchangePropertyTracks::LightIntensity:
		Name = UE::Interchange::Animation::PropertyTracks::Light::Intensity;break;
		break;

	case EInterchangePropertyTracks::LightIntensityUnits:
		Name = UE::Interchange::Animation::PropertyTracks::Light::IntensityUnits;
		break;

	case EInterchangePropertyTracks::LightTemperature:
		Name = UE::Interchange::Animation::PropertyTracks::Light::Temperature;
		break;

	case EInterchangePropertyTracks::LightUseTemperature:
		Name = UE::Interchange::Animation::PropertyTracks::Light::UseTemperature;
		break;

	case EInterchangePropertyTracks::None:
	default:
		checkf(false, TEXT("Unknown Interchange Property Track"));
	}

	return Name;
}
