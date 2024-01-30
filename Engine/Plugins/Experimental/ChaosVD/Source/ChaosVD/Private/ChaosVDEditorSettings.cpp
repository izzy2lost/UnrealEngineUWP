// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDEditorSettings.h"
#include "DataWrappers/ChaosVDParticleDataWrapper.h"
#include "Chaos/ImplicitObjectType.h"

FColor FChaosDebugDrawColorsByState::GetColorFromState(EChaosVDObjectStateType State) const
{
	switch (State)
	{
		case EChaosVDObjectStateType::Sleeping:
			return SleepingColor;
		case EChaosVDObjectStateType::Kinematic:
			return KinematicColor;
		case EChaosVDObjectStateType::Static:
			return StaticColor;
		case EChaosVDObjectStateType::Dynamic:
			return DynamicColor;
		default:
			return FColor::Purple;
	}
}

FColor FChaosDebugDrawColorsByShapeType::GetColorFromShapeType(Chaos::EImplicitObjectType ShapeType) const
{
	switch(ShapeType)
	{
		case Chaos::ImplicitObjectType::Sphere:
			return SimpleTypeColor;
		case Chaos::ImplicitObjectType::Box:
			return SimpleTypeColor;
		case Chaos::ImplicitObjectType::Plane:
			return SimpleTypeColor;
		case Chaos::ImplicitObjectType::Capsule:
			return SimpleTypeColor;
		case Chaos::ImplicitObjectType::TaperedCylinder:
			return SimpleTypeColor;
		case Chaos::ImplicitObjectType::Cylinder:
			return SimpleTypeColor;
		case Chaos::ImplicitObjectType::Convex:
			return ConvexColor;
		case Chaos::ImplicitObjectType::HeightField:
			return HeightFieldColor;
		case Chaos::ImplicitObjectType::TriangleMesh:
			return TriangleMeshColor;
		case Chaos::ImplicitObjectType::LevelSet:
			return LevelSetColor;			
		default:
			return FColor::Purple; 
	}
}

FColor FChaosDebugDrawColorsByClientServer::GetColorFromState(bool bIsServer, EChaosVDObjectStateType State) const
{
	switch (State)
	{
	case EChaosVDObjectStateType::Sleeping:
		return bIsServer ? ServerSleepingColor : ClientSleepingColor;
	case EChaosVDObjectStateType::Kinematic:
		return bIsServer ? ServerColor : ClientColor;
	case EChaosVDObjectStateType::Static:
		return bIsServer ? ServerColor : ClientColor;
	case EChaosVDObjectStateType::Dynamic:
		return bIsServer ? ServerDynamicColor : ClientDynamicColor;
	default:
		return FColor::Purple;
	}
}

void UChaosVDEditorSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UChaosVDEditorSettings, GeometryVisibilityFlags))
	{
		VisibilitySettingsChangedDelegate.Broadcast(this);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UChaosVDEditorSettings, ParticleColorMode)
			|| PropertyName == GET_MEMBER_NAME_CHECKED(UChaosVDEditorSettings, ColorsByParticleState)
			|| PropertyName == GET_MEMBER_NAME_CHECKED(UChaosVDEditorSettings, ColorsByShapeType))
	{
		ColorsSettingsChangedDelegate.Broadcast(this);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UChaosVDEditorSettings, FarClippingOverride))
	{
		FarClippingOverrideChangedDelegate.Broadcast(this);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UChaosVDEditorSettings, bPlaybackAtRecordedFrameRate) ||
			 PropertyName == GET_MEMBER_NAME_CHECKED(UChaosVDEditorSettings, TargetFrameRateOverride))
	{
		PlaybackSettingsChangedDelegate.Broadcast(this);
	}

	// TODO: If we keep this object as the main setting object,
	// we should have a single event for what changed and an enum flags that the listener could use to decide if cares about the change
}
