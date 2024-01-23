// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stateless/NiagaraStatelessModule.h"

namespace NiagaraStateless
{
	//-TODO: Examine structure padding, FQuat4f / UObject*
	struct FPhysicsBuildData
	{
		static FName GetName() { return FName("FPhysicsBuildData"); }

		float		MassMin = 1.0f;
		float		MassMax = 1.0f;
		float		DragMin = 0.0f;
		float		DragMax = 0.0f;
		FVector3f	VelocityMin = FVector3f::ZeroVector;
		FVector3f	VelocityMax = FVector3f::ZeroVector;
		FVector3f	WindMin = FVector3f::ZeroVector;
		FVector3f	WindMax = FVector3f::ZeroVector;
		FVector3f	AccelerationMin = FVector3f::ZeroVector;
		FVector3f	AccelerationMax = FVector3f::ZeroVector;

		bool		bConeVelocity = false;
		FQuat4f		ConeQuat = FQuat4f::Identity;
		float		ConeVelocityMin = 0.0f;
		float		ConeVelocityMax = 0.0f;
		float		ConeOuterAngle = 0.0f;
		float		ConeInnerAngle = 0.0f;
		float		ConeVelocityFalloff = 0.0f;

		bool		bPointVelocity = false;
		float		PointVelocityMin = 0.0f;
		float		PointVelocityMax = 0.0f;
		FVector3f	PointOrigin = FVector3f::ZeroVector;

		bool		bNoiseEnabled = false;
		float		NoiseAmplitude = 0.0f;
		float		NoiseFrequency = 0.0f;
		UObject*	NoiseTexture = nullptr;
		int32		NoiseMode = 0;
		int32		NoiseLUTOffset = 0;
		int32		NoiseLUTNumChannel = 0;
		int32		NoiseLUTChannelWidth = 0;
	};
}
