// Copyright Epic Games, Inc. All Rights Reserved.

#include "Stateless/Modules/NiagaraStatelessModule_AddVelocity.h"
#include "Stateless/NiagaraStatelessDrawDebugContext.h"

void UNiagaraStatelessModule_AddVelocity::BuildEmitterData(FNiagaraStatelessEmitterDataBuildContext& BuildContext) const
{
	if (!IsModuleEnabled())
	{
		return;
	}

	NiagaraStateless::FPhysicsBuildData& PhysicsBuildData = BuildContext.GetTransientBuildData<NiagaraStateless::FPhysicsBuildData>();
	if (VelocityType == ENSM_VelocityType::Linear)
	{
		PhysicsBuildData.VelocityMin += VelocityMin * VelocityScale;
		PhysicsBuildData.VelocityMax += VelocityMax * VelocityScale;
	}
	else if (VelocityType == ENSM_VelocityType::FromPoint)
	{
		ensureMsgf(PhysicsBuildData.bPointVelocity == false, TEXT("Only a single point force is supported at the moment."));

		PhysicsBuildData.bPointVelocity = true;
		PhysicsBuildData.PointVelocityMin = PointVelocityMin;
		PhysicsBuildData.PointVelocityMax = PointVelocityMax;
		PhysicsBuildData.PointOrigin = PointOrigin;
	}
	else if (VelocityType == ENSM_VelocityType::InCone)
	{
		ensureMsgf(PhysicsBuildData.bConeVelocity == false, TEXT("Only a single cone force is supported at the moment."));

		PhysicsBuildData.bConeVelocity = true;
		PhysicsBuildData.ConeQuat = FQuat4f(ConeRotation.Quaternion());
		PhysicsBuildData.ConeVelocityMin = ConeVelocityMin;
		PhysicsBuildData.ConeVelocityMax = ConeVelocityMax;
		PhysicsBuildData.ConeOuterAngle = ConeAngle;
		PhysicsBuildData.ConeInnerAngle = InnerCone;
		PhysicsBuildData.ConeVelocityFalloff = bSpeedFalloffFromConeAxisEnabled ? FMath::Clamp(SpeedFalloffFromConeAxis, 0.0f, 1.0f) : 0.0f;
	}
}

#if WITH_EDITOR
void UNiagaraStatelessModule_AddVelocity::DrawDebug(const FNiagaraStatelessDrawDebugContext& DrawDebugContext) const
{
	switch (VelocityType)
	{
		case ENSM_VelocityType::Linear:
		{
			const FVector MinDir = FVector(VelocityMin * VelocityScale);
			const FVector MaxDir = FVector(VelocityMax * VelocityScale);
			DrawDebugContext.DrawArrow(FVector::ZeroVector, MinDir);

			if (!FMath::IsNearlyEqual(MinDir.Length(), MaxDir.Length()))
			{
				DrawDebugContext.DrawArrow(FVector::ZeroVector, MaxDir);
			}
			break;
		}

		case ENSM_VelocityType::InCone:
		{
			const FQuat Quat = ConeRotation.Quaternion();
			const float ConeHAngle = ConeAngle / 2.0f;

			TOptional<float> InnerConeHAngle;
			if (InnerCone > 0.0f && !FMath::IsNearlyEqual(ConeAngle, InnerCone))
			{
				InnerConeHAngle = InnerCone / 2.0f;
			}

			DrawDebugContext.DrawCone(FVector::ZeroVector, Quat, ConeHAngle, ConeVelocityMin);
			if ( InnerConeHAngle.IsSet() )
			{
				DrawDebugContext.DrawCone(FVector::ZeroVector, Quat, InnerConeHAngle.GetValue(), ConeVelocityMin);
			}

			if (!FMath::IsNearlyEqual(ConeVelocityMin, ConeVelocityMax))
			{
				DrawDebugContext.DrawCone(FVector::ZeroVector, Quat, ConeHAngle, ConeVelocityMax);
				if (InnerConeHAngle.IsSet())
				{
					DrawDebugContext.DrawCone(FVector::ZeroVector, Quat, InnerConeHAngle.GetValue(), ConeVelocityMax);
				}
			}
			break;
		}

		case ENSM_VelocityType::FromPoint:
		{
			if (!FMath::IsNearlyEqual(PointVelocityMin, 0.0f))
			{
				DrawDebugContext.DrawSphere(FVector(PointOrigin), PointVelocityMin);
			}
			if (!FMath::IsNearlyEqual(PointVelocityMin, PointVelocityMax))
			{
				DrawDebugContext.DrawSphere(FVector(PointOrigin), PointVelocityMax);
			}
			break;
		}
	}
}
#endif
