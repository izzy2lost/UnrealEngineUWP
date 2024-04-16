// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/ParamUtils.h"

#include "UniversalObjectLocator.h"
#include "Component/AnimNextMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimSequence.h"
#include "Param/AnimNextClassExtensionLibrary.h"
#include "Param/ParamType.h"
#include "Param/ParamTypeHandle.h"
#include "Param/ParamCompatibility.h"

namespace UE::AnimNext
{

FParamCompatibility FParamUtils::GetCompatibility(const FParamTypeHandle& InLHS, const FParamTypeHandle& InRHS)
{
	auto CheckClassCastToCustom = [](const FParamTypeHandle& InLHS, const UClass* InRHSClass)
	{
		FAnimNextParamType::EValueType ValueTypeLHS;
		FAnimNextParamType::EContainerType ContainerTypeLHS;
		const UObject* ValueTypeObjectLHS;

		InLHS.GetCustomTypeInfo(ValueTypeLHS, ContainerTypeLHS, ValueTypeObjectLHS);
		if(ContainerTypeLHS == FAnimNextParamType::EContainerType::None && ValueTypeLHS == FAnimNextParamType::EValueType::Object)
		{
			if(const UClass* Class = Cast<UClass>(ValueTypeObjectLHS))
			{
				if(InRHSClass->IsChildOf(Class))
				{
					return true;
				}
			}
		}
		return false;
	};

	auto CheckClassCastFromCustom = [](const UClass* InLHSClass, const FParamTypeHandle& InRHS)
	{
		FAnimNextParamType::EValueType ValueTypeRHS;
		FAnimNextParamType::EContainerType ContainerTypeRHS;
		const UObject* ValueTypeObjectRHS;

		InRHS.GetCustomTypeInfo(ValueTypeRHS, ContainerTypeRHS, ValueTypeObjectRHS);
		if(ContainerTypeRHS == FAnimNextParamType::EContainerType::None && ValueTypeRHS == FAnimNextParamType::EValueType::Object)
		{
			if(const UClass* Class = Cast<UClass>(ValueTypeObjectRHS))
			{
				if(Class->IsChildOf(InLHSClass))
				{
					return true;
				}
			}
		}
		return false;
	};
	
	switch (InRHS.GetParameterType())
	{
	case FParamTypeHandle::EParamType::Bool:
		switch (InLHS.GetParameterType())
		{
		case FParamTypeHandle::EParamType::Bool:
			return EParamCompatibility::Compatible_Equal;
		}
		break;
	case FParamTypeHandle::EParamType::Byte:
		switch (InLHS.GetParameterType())
		{
		case FParamTypeHandle::EParamType::Byte:
			return EParamCompatibility::Compatible_Equal;
		case FParamTypeHandle::EParamType::Int32:
			return EParamCompatibility::Compatible_Promotion;
		case FParamTypeHandle::EParamType::Int64:
			return EParamCompatibility::Compatible_Promotion;
		case FParamTypeHandle::EParamType::Float:
			return EParamCompatibility::Compatible_Promotion;
		case FParamTypeHandle::EParamType::Double:
			return EParamCompatibility::Compatible_Promotion;
		}
		break;
	case FParamTypeHandle::EParamType::Int32:
		switch (InLHS.GetParameterType())
		{
		case FParamTypeHandle::EParamType::Byte:
			return EParamCompatibility::Incompatible_DataLoss;
		case FParamTypeHandle::EParamType::Int32:
			return EParamCompatibility::Compatible_Equal;
		case FParamTypeHandle::EParamType::Int64:
			return EParamCompatibility::Compatible_Promotion;
		case FParamTypeHandle::EParamType::Float:
			return EParamCompatibility::Incompatible_DataLoss;
		case FParamTypeHandle::EParamType::Double:
			return EParamCompatibility::Compatible_Promotion;
		}
		break;
	case FParamTypeHandle::EParamType::Int64:
		switch (InLHS.GetParameterType())
		{
		case FParamTypeHandle::EParamType::Byte:
			return EParamCompatibility::Incompatible_DataLoss;
		case FParamTypeHandle::EParamType::Int32:
			return EParamCompatibility::Incompatible_DataLoss;
		case FParamTypeHandle::EParamType::Int64:
			return EParamCompatibility::Compatible_Equal;
		case FParamTypeHandle::EParamType::Float:
			return EParamCompatibility::Incompatible_DataLoss;
		case FParamTypeHandle::EParamType::Double:
			return EParamCompatibility::Incompatible_DataLoss;
		}
		break;
	case FParamTypeHandle::EParamType::Float:
		switch (InLHS.GetParameterType())
		{
		case FParamTypeHandle::EParamType::Byte:
			return EParamCompatibility::Incompatible_DataLoss;
		case FParamTypeHandle::EParamType::Int32:
			return EParamCompatibility::Incompatible_DataLoss;
		case FParamTypeHandle::EParamType::Int64:
			return EParamCompatibility::Incompatible_DataLoss;
		case FParamTypeHandle::EParamType::Float:
			return EParamCompatibility::Compatible_Equal;
		case FParamTypeHandle::EParamType::Double:
			return EParamCompatibility::Compatible_Promotion;
		}
		break;
	case FParamTypeHandle::EParamType::Double:
		switch (InLHS.GetParameterType())
		{
		case FParamTypeHandle::EParamType::Byte:
			return EParamCompatibility::Incompatible_DataLoss;
		case FParamTypeHandle::EParamType::Int32:
			return EParamCompatibility::Incompatible_DataLoss;
		case FParamTypeHandle::EParamType::Int64:
			return EParamCompatibility::Incompatible_DataLoss;
		case FParamTypeHandle::EParamType::Float:
			return EParamCompatibility::Incompatible_DataLoss;
		case FParamTypeHandle::EParamType::Double:
			return EParamCompatibility::Compatible_Equal;
		}
		break;
	case FParamTypeHandle::EParamType::Name:
		switch (InLHS.GetParameterType())
		{
		case FParamTypeHandle::EParamType::Name:
			return EParamCompatibility::Compatible_Equal;
		}
		break;
	case FParamTypeHandle::EParamType::String:
		switch (InLHS.GetParameterType())
		{
		case FParamTypeHandle::EParamType::String:
			return EParamCompatibility::Compatible_Equal;
		}
		break;
	case FParamTypeHandle::EParamType::Text:
		switch (InLHS.GetParameterType())
		{
		case FParamTypeHandle::EParamType::Text:
			return EParamCompatibility::Compatible_Equal;
		}
		break;
	case FParamTypeHandle::EParamType::Vector:
		switch (InLHS.GetParameterType())
		{
		case FParamTypeHandle::EParamType::Vector:
			return EParamCompatibility::Compatible_Equal;
		}
		break;
	case FParamTypeHandle::EParamType::Vector4:
		switch (InLHS.GetParameterType())
		{
		case FParamTypeHandle::EParamType::Vector4:
			return EParamCompatibility::Compatible_Equal;
		}
		break;
	case FParamTypeHandle::EParamType::Quat:
		switch (InLHS.GetParameterType())
		{
		case FParamTypeHandle::EParamType::Quat:
			return EParamCompatibility::Compatible_Equal;
		}
		break;
	case FParamTypeHandle::EParamType::Transform:
		switch (InLHS.GetParameterType())
		{
		case FParamTypeHandle::EParamType::Transform:
			return EParamCompatibility::Compatible_Equal;
		}
		break;
	case FParamTypeHandle::EParamType::Object:
		switch (InLHS.GetParameterType())
		{
		case FParamTypeHandle::EParamType::Object:
			return EParamCompatibility::Compatible_Equal;
		case FParamTypeHandle::EParamType::Custom:
			if(CheckClassCastToCustom(InLHS, UObject::StaticClass()))
			{
				return EParamCompatibility::Compatible_Cast;
			}
		}
		break;
	case FParamTypeHandle::EParamType::CharacterMovementComponent:
		switch (InLHS.GetParameterType())
		{
		case FParamTypeHandle::EParamType::Object:
			return EParamCompatibility::Compatible_Cast;
		case FParamTypeHandle::EParamType::CharacterMovementComponent:
			return EParamCompatibility::Compatible_Equal;
		case FParamTypeHandle::EParamType::Custom:
			if(CheckClassCastToCustom(InLHS, UCharacterMovementComponent::StaticClass()))
			{
				return EParamCompatibility::Compatible_Cast;
			}
		}
		break;
	case FParamTypeHandle::EParamType::AnimNextMeshComponent:
		switch (InLHS.GetParameterType())
		{
		case FParamTypeHandle::EParamType::Object:
			return EParamCompatibility::Compatible_Cast;
		case FParamTypeHandle::EParamType::AnimNextMeshComponent:
			return EParamCompatibility::Compatible_Equal;
		case FParamTypeHandle::EParamType::Custom:
			if(CheckClassCastToCustom(InLHS, UAnimNextMeshComponent::StaticClass()))
			{
				return EParamCompatibility::Compatible_Cast;
			}
		}
		break;
	case FParamTypeHandle::EParamType::AnimSequence:
		switch (InLHS.GetParameterType())
		{
		case FParamTypeHandle::EParamType::Object:
			return EParamCompatibility::Compatible_Cast;
		case FParamTypeHandle::EParamType::AnimSequence:
			return EParamCompatibility::Compatible_Equal;
		case FParamTypeHandle::EParamType::Custom:
			if(CheckClassCastToCustom(InLHS, UAnimSequence::StaticClass()))
			{
				return EParamCompatibility::Compatible_Cast;
			}
		}
		break;
	case FParamTypeHandle::EParamType::AnimNextGraphLODPose:
		switch (InLHS.GetParameterType())
		{
		case FParamTypeHandle::EParamType::AnimNextGraphLODPose:
			return EParamCompatibility::Compatible_Equal;
		}
		break;
	case FParamTypeHandle::EParamType::AnimNextGraphReferencePose:
		switch (InLHS.GetParameterType())
		{
		case FParamTypeHandle::EParamType::AnimNextGraphReferencePose:
			return EParamCompatibility::Compatible_Equal;
		}
		break;
	case FParamTypeHandle::EParamType::Custom:
		switch (InLHS.GetParameterType())
		{
		case FParamTypeHandle::EParamType::Object:
			if(CheckClassCastFromCustom(UObject::StaticClass(), InRHS))
			{
				return EParamCompatibility::Compatible_Cast;
			}
			break;
		case FParamTypeHandle::EParamType::CharacterMovementComponent:
			if(CheckClassCastFromCustom(UCharacterMovementComponent::StaticClass(), InRHS))
			{
				return EParamCompatibility::Compatible_Cast;
			}
			break;
		case FParamTypeHandle::EParamType::AnimNextMeshComponent:
			if(CheckClassCastFromCustom(UAnimNextMeshComponent::StaticClass(), InRHS))
			{
				return EParamCompatibility::Compatible_Cast;
			}
			break;
		case FParamTypeHandle::EParamType::AnimSequence:
			if(CheckClassCastFromCustom(UAnimSequence::StaticClass(), InRHS))
			{
				return EParamCompatibility::Compatible_Cast;
			}
			break;
		case FParamTypeHandle::EParamType::Custom:
			{
				FAnimNextParamType::EValueType ValueTypeLHS, ValueTypeRHS;
				FAnimNextParamType::EContainerType ContainerTypeLHS, ContainerTypeRHS;
				const UObject* ValueTypeObjectLHS, *ValueTypeObjectRHS;

				InLHS.GetCustomTypeInfo(ValueTypeLHS, ContainerTypeLHS, ValueTypeObjectLHS);
				InRHS.GetCustomTypeInfo(ValueTypeRHS, ContainerTypeRHS, ValueTypeObjectRHS);

				if (ContainerTypeLHS == ContainerTypeRHS)
				{
					if (ContainerTypeLHS == FAnimNextParamType::EContainerType::Array)
					{
						if (ValueTypeLHS != ValueTypeRHS)
						{
							switch (ValueTypeLHS)
							{
							case FAnimNextParamType::EValueType::Float:
								if(ValueTypeRHS == FAnimNextParamType::EValueType::Double)
								{
									return EParamCompatibility::Incompatible_DataLoss;
								}
								break;
							case FAnimNextParamType::EValueType::Double:
								if (ValueTypeRHS == FAnimNextParamType::EValueType::Float)
								{
									return EParamCompatibility::Compatible_Promotion;
								}
								break;
							case FAnimNextParamType::EValueType::Struct:
							case FAnimNextParamType::EValueType::Object:
							case FAnimNextParamType::EValueType::SoftObject:
							case FAnimNextParamType::EValueType::Class:
							case FAnimNextParamType::EValueType::SoftClass:
								if (ValueTypeObjectLHS == ValueTypeObjectRHS)
								{
									return EParamCompatibility::Compatible_Equal;
								}
								break;
							}
						}
						else
						{
							switch (ValueTypeLHS)
							{
							case FAnimNextParamType::EValueType::Bool:
							case FAnimNextParamType::EValueType::Byte:
							case FAnimNextParamType::EValueType::Int32:
							case FAnimNextParamType::EValueType::Int64:
							case FAnimNextParamType::EValueType::Float:
							case FAnimNextParamType::EValueType::Double:
							case FAnimNextParamType::EValueType::Name:
							case FAnimNextParamType::EValueType::String:
							case FAnimNextParamType::EValueType::Text:
								return EParamCompatibility::Compatible_Equal;
							case FAnimNextParamType::EValueType::Enum:
							case FAnimNextParamType::EValueType::Struct:
							case FAnimNextParamType::EValueType::Object:
							case FAnimNextParamType::EValueType::SoftObject:
							case FAnimNextParamType::EValueType::Class:
							case FAnimNextParamType::EValueType::SoftClass:
								if (ValueTypeObjectLHS == ValueTypeObjectRHS)
								{
									return EParamCompatibility::Compatible_Equal;
								}
								break;
							}
						}
					}
					else
					{
						if (ValueTypeLHS == ValueTypeRHS)
						{
							switch (ValueTypeLHS)
							{
							default:
							case FAnimNextParamType::EValueType::None:
								return EParamCompatibility::Incompatible;
							case FAnimNextParamType::EValueType::Enum:
								if (ValueTypeObjectLHS == ValueTypeObjectRHS)
								{
									return EParamCompatibility::Compatible_Equal;
								}
								break;
							case FAnimNextParamType::EValueType::Struct:
								if (ValueTypeObjectLHS == ValueTypeObjectRHS)
								{
									return EParamCompatibility::Compatible_Equal;
								}
								else if (CastChecked<UScriptStruct>(ValueTypeObjectRHS)->IsChildOf(CastChecked<UScriptStruct>(ValueTypeObjectLHS)))
								{
									return EParamCompatibility::Compatible_Cast;
								}
								break;
							case FAnimNextParamType::EValueType::Object:
							case FAnimNextParamType::EValueType::SoftObject:
							case FAnimNextParamType::EValueType::Class:
							case FAnimNextParamType::EValueType::SoftClass:
								if (ValueTypeObjectLHS == ValueTypeObjectRHS)
								{
									return EParamCompatibility::Compatible_Equal;
								}
								else if (CastChecked<UClass>(ValueTypeObjectRHS)->IsChildOf(CastChecked<UClass>(ValueTypeObjectLHS)))
								{
									return EParamCompatibility::Compatible_Cast;
								}
								break;
							}
						}
					}
				}
			}
			break;
		}
		break;
	}

	return EParamCompatibility::Incompatible;
}

FParamCompatibility FParamUtils::GetCompatibility(const FAnimNextParamType& InLHS, const FAnimNextParamType& InRHS)
{
	return GetCompatibility(InLHS.GetHandle(), InRHS.GetHandle());
}

bool FParamUtils::CanUseFunction(const UFunction* InFunction)
{
	UClass* FunctionClass = InFunction->GetOuterUClass();
	if(FunctionClass->IsChildOf(UAnimNextClassExtensionLibrary::StaticClass()))
	{
		UAnimNextClassExtensionLibrary* CDO = FunctionClass->GetDefaultObject<UAnimNextClassExtensionLibrary>();
		const UClass* ExtendedClass = CDO->GetSupportedClass();
		
		// Check 'hoisted' functions on BPFLs
		if(!InFunction->HasAllFunctionFlags(FUNC_BlueprintCallable | FUNC_Static | FUNC_Native | FUNC_Public))
		{
			return false;
		}

		if(InFunction->NumParms != 2)
		{
			return false;
		}
		
		int32 ParamIndex = 0;
		for(TFieldIterator<FProperty> It(InFunction); It && (It->PropertyFlags & CPF_Parm); ++It, ++ParamIndex)
		{
			// Check first parameter is an object of the expected class
			if(ParamIndex == 0)
			{
				FObjectProperty* ObjectProperty = CastField<FObjectProperty>(*It);
				if(ObjectProperty == nullptr)
				{ 
					return false;
				}
				
				if(!ObjectProperty->PropertyClass->IsChildOf(ExtendedClass))
				{
					return false;
				}
			}

			// Check return value
			if(ParamIndex == 1 && !It->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				return false;
			}
		}
	}
	else
	{
		// We add only 'accessor' functions (no params apart from the return value) that have valid return types
		const FProperty* ReturnProperty = InFunction->GetReturnProperty();
		if(ReturnProperty == nullptr || InFunction->NumParms != 1 || !InFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable))
		{
			return false;
		}

		const FParamTypeHandle TypeHandle = FParamTypeHandle::FromProperty(ReturnProperty);
		if(!TypeHandle.IsValid())
		{
			return false;
		}
	}

	return true;
}

bool FParamUtils::CanUseProperty(const FProperty* InProperty)
{
	if(!InProperty->HasAnyPropertyFlags(CPF_Edit | CPF_EditConst | CPF_BlueprintVisible) || InProperty->HasAnyPropertyFlags(CPF_Deprecated | CPF_EditorOnly))
	{
		return false;
	}

	const FParamTypeHandle TypeHandle = FParamTypeHandle::FromProperty(InProperty);
	if(!TypeHandle.IsValid())
	{
		return false;
	}

	return true;
}

FName FParamUtils::LocatorToName(const FUniversalObjectLocator& InLocator)
{
	// By default the string representation of an empty UOL is "uobj://none", so we shortcut here for FName consistency 
	if(InLocator.IsEmpty())
	{
		return NAME_None; 
	}

	TStringBuilder<1024> StringBuilder;
	InLocator.ToString(StringBuilder);
	ensure(StringBuilder.Len() < NAME_SIZE);
	return FName(StringBuilder.ToView());
}


}