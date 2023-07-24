// Copyright Epic Games, Inc. All Rights Reserved.

#include "SimModule/SuspensionModule.h"
#include "SimModule/SimModuleTree.h"
#include "SimModule/WheelModule.h"
#include "VehicleUtility.h"

#if VEHICLE_DEBUGGING_ENABLED
UE_DISABLE_OPTIMIZATION
#endif

namespace Chaos
{

	FSuspensionSimModule::FSuspensionSimModule(const FSuspensionSettings& Settings)
		: TSimModuleSettings<FSuspensionSettings>(Settings)
		, SpringDisplacement(0.f)
		, LastDisplacement(0.f)
		, WheelSimTreeIndex(INVALID_IDX)
	{
		AccessSetup().MaxLength = FMath::Abs(Settings.MaxRaise + Settings.MaxDrop);
	}

	float FSuspensionSimModule::GetSpringLength() const
	{
		return  -(Setup().MaxLength - SpringDisplacement);
	}

	void FSuspensionSimModule::SetSpringLength(float InLength, float WheelRadius)
	{
		float DisplacementInput = InLength;
		DisplacementInput = FMath::Max(0.f, DisplacementInput);
		SpringDisplacement = Setup().MaxLength - DisplacementInput;
	}

	void FSuspensionSimModule::GetWorldRaycastLocation(const FTransform& BodyTransform, float WheelRadius, FSpringTrace& OutTrace)
	{
		FVector LocalDirection = Setup().SuspensionAxis;
		FVector Local = GetParentRelativeTransform().GetLocation(); // change to just a vector and GetLocalLocation
		FVector WorldLocation = BodyTransform.TransformPosition(GetRelativeOffsetTransform().TransformVector(Local/*Setup().LocalOffset*/));
		FVector WorldDirection = BodyTransform.TransformVector(GetRelativeOffsetTransform().TransformVector(LocalDirection));

		OutTrace.Start = WorldLocation - WorldDirection * (Setup().MaxRaise);
		OutTrace.End = WorldLocation + WorldDirection * (Setup().MaxDrop + WheelRadius);
		float TraceLength = OutTrace.Start.Z - OutTrace.End.Z;
	}

	void FSuspensionSimModule::Simulate(float DeltaTime, const FAllInputs& Inputs, FSimModuleTree& VehicleModuleSystem)
	{
		float ForceIntoSurface = 0.0f;
		if (SpringDisplacement > 0)
		{
			float Damping = (SpringDisplacement < LastDisplacement) ? Setup().CompressionDamping : Setup().ReboundDamping;
			float SpringSpeed = (LastDisplacement - SpringDisplacement) / DeltaTime;

			float StiffnessForce = SpringDisplacement * Setup().SpringRate;
			float DampingForce = SpringSpeed * Damping;
			float SuspensionForce = StiffnessForce - DampingForce;
			LastDisplacement = SpringDisplacement;

			if (SuspensionForce > 0)
			{
				ForceIntoSurface = SuspensionForce;
				AddLocalForce(Setup().SuspensionAxis * -SuspensionForce, true, false, true, FColor::Green);
			}
		}

		// tell wheels how much they are being pressed into the ground
		if (SimModuleTree)
		{
			if (Chaos::ISimulationModuleBase* Module = SimModuleTree->AccessSimModule(WheelSimTreeIndex))
			{
				check(Module->GetSimType() == eSimType::Wheel);
				Chaos::FWheelSimModule* Wheel = static_cast<Chaos::FWheelSimModule*>(Module);

				Wheel->SetForceIntoSurface(ForceIntoSurface);
			}
			

		}
	}

	void FSuspensionSimModuleDatas::FillSimState(ISimulationModuleBase* SimModule)
	{
		if (FSuspensionSimModule* Sim = static_cast<FSuspensionSimModule*>(SimModule))
		{
			Sim->SpringDisplacement = SpringDisplacement;
			Sim->LastDisplacement = LastDisplacement;
		}
	}

	void FSuspensionSimModuleDatas::FillNetState(const ISimulationModuleBase* SimModule)
	{
		if (const FSuspensionSimModule* Sim = static_cast<const FSuspensionSimModule*>(SimModule))
		{
			SpringDisplacement = Sim->SpringDisplacement;
			LastDisplacement = Sim->LastDisplacement;
		}
	}

	void FSuspensionSimModuleDatas::Lerp(const float LerpFactor, const FModuleNetData& Min, const FModuleNetData& Max)
	{
		const FSuspensionSimModuleDatas& MinData = static_cast<const FSuspensionSimModuleDatas&>(Min);
		const FSuspensionSimModuleDatas& MaxData = static_cast<const FSuspensionSimModuleDatas&>(Max);

		SpringDisplacement = FMath::Lerp(MinData.SpringDisplacement, MaxData.SpringDisplacement, LerpFactor);
		LastDisplacement = FMath::Lerp(MinData.LastDisplacement, MaxData.LastDisplacement, LerpFactor);
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	FString FSuspensionSimModuleDatas::ToString() const
	{
		return FString::Printf(TEXT("Module:%s SpringDisplacement:%f LastDisplacement:%f"),
			*DebugString, SpringDisplacement, LastDisplacement);
	}
#endif

} // namespace Chaos

#if VEHICLE_DEBUGGING_ENABLED
UE_ENABLE_OPTIMIZATION
#endif