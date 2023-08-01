// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/RigUnit.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit)

DEFINE_LOG_CATEGORY_STATIC(LogRigUnit, Log, All);

#if WITH_EDITOR

bool FRigUnit::GetDirectManipulationTargets(const URigVMUnitNode* InNode, TSharedPtr<FStructOnScope> InInstance, URigHierarchy* InHierarchy, TArray<FRigDirectManipulationTarget>& InOutTargets, FString* OutFailureReason) const
{
	const UScriptStruct* ScriptStruct = InNode->GetScriptStruct();
	if(ScriptStruct == nullptr)
	{
		if(OutFailureReason)
		{
			*OutFailureReason = TEXT("Node is not resolved yet.");
		}
		return false;
	}
	
	for(const URigVMPin* Pin : InNode->GetPins())
	{
		(void)AddDirectManipulationTarget_Internal(InOutTargets, Pin, ScriptStruct);
	}

	return true;
}

bool FRigUnit::AddDirectManipulationTarget_Internal(TArray<FRigDirectManipulationTarget>& InOutTargets, const URigVMPin* InPin, const UScriptStruct* InScriptStruct)
{
	check(InPin);

	if(InPin->GetDirection() == ERigVMPinDirection::Input ||
		InPin->GetDirection() == ERigVMPinDirection::IO ||
		InPin->GetDirection() == ERigVMPinDirection::Visible)
	{
		if(InPin->GetCPPTypeObject() == TBaseStructure<FTransform>::Get() ||
			InPin->GetCPPTypeObject() == TBaseStructure<FEulerTransform>::Get() ||
			InPin->GetCPPTypeObject() == TBaseStructure<FVector>::Get() ||
			InPin->GetCPPTypeObject() == TBaseStructure<FQuat>::Get())
		{
			if(const URigVMPin* ParentPin = InPin->GetParentPin())
			{
				if(ParentPin->GetCPPTypeObject() == TBaseStructure<FTransform>::Get() ||
					ParentPin->GetCPPTypeObject() == TBaseStructure<FEulerTransform>::Get())
				{
					return false;
				}
			}

			ERigControlType ControlType = ERigControlType::EulerTransform;
			if(InPin->GetCPPTypeObject() == TBaseStructure<FVector>::Get())
			{
				ControlType = ERigControlType::Position;
			}
			else if(InPin->GetCPPTypeObject() == TBaseStructure<FQuat>::Get())
			{
				ControlType = ERigControlType::Rotator;
			}
			InOutTargets.AddUnique({InPin->GetSegmentPath(true), ControlType});
		}

		for(const URigVMPin* SubPin : InPin->GetSubPins())
		{
			AddDirectManipulationTarget_Internal(InOutTargets, SubPin, InScriptStruct);
		}
	}

	return false;
}

TTuple<const FProperty*, FRigVMPropertyPath> FRigUnit::FindPropertyFromPinPath(const UScriptStruct* InStruct, const FString& InPinPath)
{
	FString PinPath = InPinPath;
	PinPath = PinPath.Replace(TEXT("["), TEXT("."));
	PinPath = PinPath.Replace(TEXT("]"), TEXT("."));
	PinPath = PinPath.Replace(TEXT(".."), TEXT("."));
	PinPath.TrimCharInline('.', nullptr);

	FString Left, Right;
	if(!PinPath.Split(TEXT("."), &Left, &Right))
	{
		Left = PinPath;
		Right.Reset();
	}

	const FProperty* Property = InStruct->FindPropertyByName(*Left);
	if(Property == nullptr)
	{
		return {nullptr, FRigVMPropertyPath() };
	}

	if(Right.IsEmpty())
	{
		return {Property, FRigVMPropertyPath() };
	}

	return {Property, FRigVMPropertyPath(Property, Right) };
}

void FRigUnit::ConfigureDirectManipulationControl(const URigVMUnitNode* InNode, TSharedPtr<FRigDirectManipulationInfo> InInfo, FRigControlSettings& InOutSettings, FRigControlValue& InOutValue) const
{
	if(InInfo->Target.ControlType == ERigControlType::Transform)
	{
		InOutSettings.ControlType = ERigControlType::EulerTransform;
		InOutValue = FRigControlValue::Make<FEulerTransform>(FEulerTransform::Identity);
	}
	else if(InInfo->Target.ControlType == ERigControlType::Position)
	{
		InOutSettings.ControlType = ERigControlType::Position;
		InOutValue = FRigControlValue::Make<FVector>(FVector::ZeroVector);
	}
	else if(InInfo->Target.ControlType == ERigControlType::Rotator)
	{
		InOutSettings.ControlType = ERigControlType::Rotator;
		InOutValue = FRigControlValue::Make<FRotator>(FRotator::ZeroRotator);
	}
}

bool FRigUnit::UpdateHierarchyForDirectManipulation(const URigVMUnitNode* InNode, TSharedPtr<FStructOnScope> InInstance, FControlRigExecuteContext& InContext, TSharedPtr<FRigDirectManipulationInfo> InInfo)
{
	const UScriptStruct* Struct = InNode->GetScriptStruct();
	check(Struct);
	check(InInstance.IsValid() && InInstance->IsValid());
	check(InInstance->GetStruct() == Struct);

	if(!InInfo.IsValid())
	{
		return false;
	}

	auto PropertyAndPath = FindPropertyFromPinPath(Struct, InInfo->Target.Name);
	const FStructProperty* StructProperty = CastField<FStructProperty>(PropertyAndPath.Get<0>());
	if(StructProperty == nullptr)
	{
		return false;
	}

	const uint8* Memory = StructProperty->ContainerPtrToValuePtr<uint8>(InInstance->GetStructMemory());
	if(PropertyAndPath.Get<1>().IsValid())
	{
		Memory = PropertyAndPath.Get<1>().GetData<uint8>((uint8*)Memory, StructProperty);
	}

	URigHierarchy* Hierarchy = InContext.Hierarchy;
	if (Hierarchy == nullptr)
	{
		return false;
	}

	FTransform Transform = Hierarchy->GetGlobalTransform(InInfo->ControlKey, false);

	if(StructProperty->Struct == TBaseStructure<FTransform>::Get())
	{
		const FTransform& Result = *(const FTransform*)Memory;
		Transform = Result;
	}
	else if(StructProperty->Struct == TBaseStructure<FEulerTransform>::Get())
	{
		const FEulerTransform& Result = *(const FEulerTransform*)Memory;
		Transform = Result.ToFTransform();
	}
	else if(StructProperty->Struct == TBaseStructure<FQuat>::Get())
	{
		const FQuat& Result = *(const FQuat*)Memory;
		Transform.SetRotation(Result);
	}
	else if(StructProperty->Struct == TBaseStructure<FVector>::Get())
	{
		const FVector& Result = *(const FVector*)Memory;
		Transform.SetTranslation(Result);
	}
	else
	{
		return false;
	}

	Hierarchy->SetControlOffsetTransform(InInfo->ControlKey, FTransform::Identity, false);
	Hierarchy->SetGlobalTransform(InInfo->ControlKey, Transform, false);

	if(!InInfo->bInitialized)
	{
		Hierarchy->SetGlobalTransform(InInfo->ControlKey, Transform, true);
	}
	return true;
}

bool FRigUnit::UpdateDirectManipulationFromHierarchy(const URigVMUnitNode* InNode, TSharedPtr<FStructOnScope> InInstance, FControlRigExecuteContext& InContext, TSharedPtr<FRigDirectManipulationInfo> InInfo)
{
	const UScriptStruct* Struct = InNode->GetScriptStruct();
	check(Struct);
	check(InInstance.IsValid() && InInstance->IsValid());
	check(InInstance->GetStruct() == Struct);

	if(!InInfo.IsValid())
	{
		return false;
	}

	auto PropertyAndPath = FindPropertyFromPinPath(Struct, InInfo->Target.Name);
	const FStructProperty* StructProperty = CastField<FStructProperty>(PropertyAndPath.Get<0>());
	if(StructProperty == nullptr)
	{
		return false;
	}

	uint8* Memory = StructProperty->ContainerPtrToValuePtr<uint8>(InInstance->GetStructMemory());
	if(PropertyAndPath.Get<1>().IsValid())
	{
		Memory = PropertyAndPath.Get<1>().GetData<uint8>(Memory, StructProperty);
	}

	URigHierarchy* Hierarchy = InContext.Hierarchy;
	if (Hierarchy == nullptr)
	{
		return false;
	}
	
	const FTransform Transform = Hierarchy->GetLocalTransform(InInfo->ControlKey, false);

	if(StructProperty->Struct == TBaseStructure<FTransform>::Get())
	{
		FTransform& Result = *(FTransform*)Memory;
		Result = Transform;
	}
	else if(StructProperty->Struct == TBaseStructure<FEulerTransform>::Get())
	{
		FEulerTransform& Result = *(FEulerTransform*)Memory;
		Result = FEulerTransform(Transform);
	}
	else if(StructProperty->Struct == TBaseStructure<FQuat>::Get())
	{
		FQuat& Result = *(FQuat*)Memory;
		Result = Transform.GetRotation();
	}
	else if(StructProperty->Struct == TBaseStructure<FVector>::Get())
	{
		FVector& Result = *(FVector*)Memory;
		Result = Transform.GetTranslation();
	}
	else
	{
		return false;
	}
	
	return true;
}

TArray<const URigVMPin*> FRigUnit::GetPinsForDirectManipulation(const URigVMUnitNode* InNode, const FRigDirectManipulationTarget& InTarget) const
{
	// the default is to return the pin matching the target
	TArray<const URigVMPin*> Pins;
	if(const URigVMPin* Pin = InNode->FindPin(InTarget.Name))
	{
		Pins.Add(Pin);
	}
	return Pins;
}

void FRigUnit::PerformDebugDrawingForDirectManipulation(const URigVMUnitNode* InNode, TSharedPtr<FStructOnScope> InInstance, FControlRigExecuteContext& InContext, TSharedPtr<FRigDirectManipulationInfo> InInfo) const
{
	FRigVMDrawInterface* DrawInterface = InContext.GetDrawInterface();
	if(DrawInterface == nullptr)
	{
		return;
	}
	
	if(FRigControlElement* Control = InContext.Hierarchy->Find<FRigControlElement>(InInfo->ControlKey))
	{
		const FTransform Transform = InContext.Hierarchy->GetLocalTransform(InInfo->ControlKey, false);
		const FVector Location = Transform.GetTranslation();
		if(!Location.IsNearlyZero())
		{
			const FTransform OffsetTransform = InContext.Hierarchy->GetControlOffsetTransform(Control, ERigTransformType::CurrentGlobal);
			DrawInterface->DrawLine(OffsetTransform, FVector::ZeroVector, Location, FLinearColor::Green);
		}
	}	
}

#endif
