// Copyright Epic Games, Inc. All Rights Reserved.

#include "SimModule/SuspensionBaseInterface.h"

namespace Chaos
{

FSuspensionBaseInterface::FSuspensionBaseInterface()
	: WheelSimTreeIndex(INVALID_IDX)
	, TargetPos(FVector::ZeroVector)
	, ImpactNormal(FVector::ZeroVector)
	, HitDistance(0.f)
	, WheelInContact(false)
{
}

bool FSuspensionBaseInterface::IsBehaviourType(eSimModuleTypeFlags InType) const
{ 
	return (InType & Raycast); 
}

eSimType FSuspensionBaseInterface::GetSimType() const 
{ 
	return eSimType::Suspension; 
}

void FSuspensionBaseInterface::SetTargetPoint(const FVector& InTargetPoint, const FVector& InImpactNormal, float InHitDistance, bool InWheelInContact)
{
	TargetPos = InTargetPoint;
	ImpactNormal = InImpactNormal;
	HitDistance = InHitDistance;
	WheelInContact = InWheelInContact;
}


} // namespace Chaos
