// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsControlBPLibrary.h"

#include "RigidBodyControlData.h"
#include "AnimNode_RigidBodyWithControl.h"

//UE_DISABLE_OPTIMIZATION

#if 1

//======================================================================================================================
template<typename TParameterType> const TArray<FName>* FindNamesInSet(
	const FRigidBodyNameRecords& NameRecords, const FName SetName)
{
	return nullptr;
}

//======================================================================================================================
template<> const TArray<FName>* FindNamesInSet<FRigidBodyControl>(
	const FRigidBodyNameRecords& NameRecords, const FName SetName)
{
	return NameRecords.ControlSets.Find(SetName);
}

//======================================================================================================================
template<> const TArray<FName>* FindNamesInSet<FRigidBodyModifier>(
	const FRigidBodyNameRecords& NameRecords, const FName SetName)
{
	return NameRecords.BodyModifierSets.Find(SetName);
}

//======================================================================================================================
template<typename TParameterType> TArray<FName> GetNamesInSet(
	const FRigidBodyWithControlReference& RigidBodyWithControl, const FName SetName)
{
	TArray<FName> OutputNames;

	RigidBodyWithControl.CallAnimNodeFunction<FAnimNode_RigidBodyWithControl>(
		TEXT("GetControlNamesInSet"),
		[&OutputNames, SetName](FAnimNode_RigidBodyWithControl& InRigidBodyWithControl)
		{
			if (const TArray<FName>* FoundNames = FindNamesInSet<TParameterType>(
				InRigidBodyWithControl.GetNameRecords(), SetName))
			{
				OutputNames = *FoundNames;
			}
		});

	return OutputNames;
}

//======================================================================================================================
template<typename TNamedParameters> void InterpolateParametersContainers(
	TArray<TNamedParameters>& TargetContainer, const TArray<TNamedParameters>& SourceContainer, const float Weight)
{
	for (const TNamedParameters& Source : SourceContainer)
	{
		const FName SourceName = Source.Name;
		if (TNamedParameters* const ExistingUpdate = TargetContainer.FindByPredicate(
			[SourceName](const TNamedParameters& Element) { return Element.Name == SourceName; }))
		{
			// Interpolate values if the target container includes an element with the source values name.
			ExistingUpdate->Data = Interpolate(ExistingUpdate->Data, Source.Data, Weight);
		}
		else
		{
			// Add this element to the target container.
			TargetContainer.Add(Source);
		}
	}
}

//======================================================================================================================
template<typename TNamedParameters> void BlendParametersThroughSet(
	const FRigidBodyControlAndModifierParameters& InParametersContainer, 
	const TNamedParameters&                       InStartParameters,
	const TNamedParameters&                       InEndParameters,
	const TArray<FName>&                          Names,
	FRigidBodyControlAndModifierParameters&       OutParametersContainer)
{
	// TODO - Limbs can include branches. Would need a more sophisticated approach to deal with
	// those properly, perhaps include a depth index in the control name ?

	OutParametersContainer = InParametersContainer;

	const float WeightDelta = 1.0f / static_cast<float>(Names.Num() - 1);
	float Weight = 0.0f;

	for (const FName& Name : Names)
	{
		OutParametersContainer.Add(
			TNamedParameters(Name, Interpolate(InStartParameters.Data, InEndParameters.Data, Weight)));
		Weight += WeightDelta;
	}
}

//======================================================================================================================
void UPhysicsControlBPLibrary::AddControlParameters(
	UPARAM(ref) FRigidBodyControlAndModifierParameters& InParametersContainer,
	FRigidBodyControlAndModifierParameters&             OutParametersContainer,
	const FName                                         Name, 
	const FRigidBodyControlSparseData&                  ControlData)
{
	OutParametersContainer = InParametersContainer;
	OutParametersContainer.Add(FRigidBodyNamedControlParameters(Name, ControlData));
}

//======================================================================================================================
void UPhysicsControlBPLibrary::AddMultipleControlParameters(
	UPARAM(ref) FRigidBodyControlAndModifierParameters& InParametersContainer,
	FRigidBodyControlAndModifierParameters&             OutParametersContainer,
	const TArray<FName>&                                Names,
	const FRigidBodyControlSparseData&                  ControlData)
{
	OutParametersContainer = InParametersContainer;
	OutParametersContainer.ControlParameters.Reserve(OutParametersContainer.ControlParameters.Num() + Names.Num());

	for (const FName Name : Names)
	{
		OutParametersContainer.Add(FRigidBodyNamedControlParameters(Name, ControlData));
	}	
}

//======================================================================================================================
void UPhysicsControlBPLibrary::AddModifierParameters(
	UPARAM(ref) FRigidBodyControlAndModifierParameters& InParametersContainer,
	FRigidBodyControlAndModifierParameters&             OutParametersContainer,
	const FName                                         Name,
	const FRigidBodyModifierSparseData&                 ModifierData)
{
	OutParametersContainer = InParametersContainer;
	OutParametersContainer.Add(FRigidBodyNamedModifierParameters(Name, ModifierData));
}

//======================================================================================================================
void UPhysicsControlBPLibrary::AddMultipleModifierParameters(
	UPARAM(ref) FRigidBodyControlAndModifierParameters& InParametersContainer,
	FRigidBodyControlAndModifierParameters&             OutParametersContainer,
	const TArray<FName>&                                Names,
	const FRigidBodyModifierSparseData&                 ModifierData)
{
	OutParametersContainer = InParametersContainer;
	OutParametersContainer.ModifierParameters.Reserve(OutParametersContainer.ModifierParameters.Num() + Names.Num());

	for (const FName Name : Names)
	{
		OutParametersContainer.Add(FRigidBodyNamedModifierParameters(Name, ModifierData));
	}
}

//======================================================================================================================
void UPhysicsControlBPLibrary::BlendParameters(
	UPARAM(ref) FRigidBodyControlAndModifierParameters& InParametersContainerA, 
	UPARAM(ref) FRigidBodyControlAndModifierParameters& InParametersContainerB, 
	const float                                         InInterpolationWeight, 
	FRigidBodyControlAndModifierParameters&             OutParametersContainers)
{
	OutParametersContainers = InParametersContainerA;

	InterpolateParametersContainers(
		OutParametersContainers.ControlParameters, InParametersContainerB.ControlParameters, InInterpolationWeight);
	InterpolateParametersContainers(
		OutParametersContainers.ModifierParameters, InParametersContainerB.ModifierParameters, InInterpolationWeight);
}

//======================================================================================================================
void UPhysicsControlBPLibrary::BlendControlParametersThroughSet(
	UPARAM(ref) FRigidBodyControlAndModifierParameters& InParametersContainer,
	UPARAM(ref) const FRigidBodyNamedControlParameters& InStartParameters,
	UPARAM(ref) const FRigidBodyNamedControlParameters& InEndParameters,
	const TArray<FName>&                                InNames,
	FRigidBodyControlAndModifierParameters&             OutParametersContainer)
{
	BlendParametersThroughSet(InParametersContainer, InStartParameters, InEndParameters, InNames, OutParametersContainer);
}

//======================================================================================================================
void UPhysicsControlBPLibrary::BlendModifierParametersThroughSet(
	UPARAM(ref) FRigidBodyControlAndModifierParameters&  InParametersContainer, 
	UPARAM(ref) const FRigidBodyNamedModifierParameters& InStartParameters,
	UPARAM(ref) const FRigidBodyNamedModifierParameters& InEndParameters,
	const TArray<FName>&                                 InNames,
	FRigidBodyControlAndModifierParameters&              OutParametersContainer)
{
	BlendParametersThroughSet(InParametersContainer, InStartParameters, InEndParameters, InNames, OutParametersContainer);
}

//======================================================================================================================
FRigidBodyWithControlReference UPhysicsControlBPLibrary::ConvertToRigidBodyWithControl(
	const FAnimNodeReference&           Node,
	EAnimNodeReferenceConversionResult& Result)
{
	return FAnimNodeReference::ConvertToType<FRigidBodyWithControlReference>(Node, Result);
}

//======================================================================================================================
UFUNCTION(BlueprintPure, Category = "Animation|PhysicsControl", meta = (BlueprintThreadSafe))
TArray<FName> UPhysicsControlBPLibrary::GetControlNamesInSet(
	const FRigidBodyWithControlReference& RigidBodyWithControl, 
	const FName                           SetName)
{
	return GetNamesInSet<FRigidBodyControl>(RigidBodyWithControl, SetName);
}

//======================================================================================================================
UFUNCTION(BlueprintPure, Category = "Animation|PhysicsControl", meta = (BlueprintThreadSafe))
TArray<FName> UPhysicsControlBPLibrary::GetBodyModifierNamesInSet(
	const FRigidBodyWithControlReference& RigidBodyWithControl,
	const FName                           SetName)
{
	return GetNamesInSet<FRigidBodyModifier>(RigidBodyWithControl, SetName);
}

#endif
