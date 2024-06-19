// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SimModule/SimulationModuleBase.h"

namespace Chaos
{

/** Suspension world ray/shape trace start and end positions */
struct CHAOSVEHICLESCORE_API FSpringTrace
{
	FVector Start;
	FVector End;

	FVector TraceDir()
	{
		FVector Dir(End - Start);
		return Dir.FVector::GetSafeNormal();
	}

	float Length()
	{
		FVector Dir(End - Start);
		return Dir.Size();
	}
};


class CHAOSVEHICLESCORE_API FSuspensionBaseInterface : public ISimulationModuleBase
{
public:
	FSuspensionBaseInterface();

	virtual ~FSuspensionBaseInterface() {}

	virtual bool IsBehaviourType(eSimModuleTypeFlags InType) const override;

	virtual eSimType GetSimType() const override;

	virtual float GetMaxSpringLength() const = 0;
	virtual float GetSpringLength() const = 0;
	virtual void SetSpringLength(float InLength, float WheelRadius) = 0;
	virtual void GetWorldRaycastLocation(const FTransform& BodyTransform, float WheelRadius, FSpringTrace& OutTrace) = 0;

	void SetTargetPoint(const FVector& InTargetPoint, const FVector& InImpactNormal, float InHitDistance, bool InWheelInContact);
	bool IsWheelInContact() const { return WheelInContact; }
	void SetWheelSimTreeIndex(int WheelTreeIndexIn) { WheelSimTreeIndex = WheelTreeIndexIn; }
	int GetWheelSimTreeIndex() const { return WheelSimTreeIndex; }

protected:
	int WheelSimTreeIndex;

	FVector TargetPos;
	FVector ImpactNormal;
	float HitDistance;
	bool WheelInContact;

};


} // namespace Chaos
