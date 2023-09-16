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
}
