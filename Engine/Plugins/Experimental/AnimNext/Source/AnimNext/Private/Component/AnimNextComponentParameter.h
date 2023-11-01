// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Param/ParamId.h"
#include "Param/ParamType.h"
#include "Param/ParamTypeHandle.h"
#include "AnimNextComponentParameter.generated.h"

class UAnimNextGraph;

// Boilerplate macro that all derived types of UAnimNextComponentParameter should use.
// Usage is: IMPLEMENT_ANIMNEXT_COMPONENT_PARAMETER(UAnimNextComponentParameter_DerivedType)
#define IMPLEMENT_ANIMNEXT_COMPONENT_PARAMETER(Type) \
	virtual void CacheParamInfo() const override \
	{ \
		using namespace UE::AnimNext; \
		if(!ParameterId.IsValid() && Parameter != NAME_None) \
		{ \
			ParameterId = FParamId(Parameter); \
			ValuePtr = const_cast<uint8*>(reinterpret_cast<const uint8*>((&Value))); \
			ValueProperty = StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(Type, Value)); \
			ValueType = FParamTypeHandle::FromProperty(ValueProperty).GetType(); \
		} \
	} \

// Base class for static parameters that can be inserted into a schedule via a component
// Each parameter type needs its own derived type of this object
UCLASS(DefaultToInstanced, abstract, editinlinenew)
class UAnimNextComponentParameter : public UObject
{
	GENERATED_BODY()

public:
	// The scope to apply the parameter to. If this is empty, it is applied at the root scope of the schedule.
	UPROPERTY(EditAnywhere, Category = "Parameter", AdvancedDisplay, meta = (CustomWidget = "ParamName"))
	FName Scope;

	// Check validity
	bool IsValid() const
	{
		CacheParamInfo();

		return ValueType.IsValid() && ParameterId.IsValid() && ValuePtr != nullptr;
	}
	
	// Get the name, type and value for this parameter
	void GetParamInfo(UE::AnimNext::FParamId& OutParamId, FAnimNextParamType& OutType, uint8*& OutValue) const
	{
		CacheParamInfo();

		OutParamId = ParameterId;
		OutType = ValueType;
		OutValue = ValuePtr;
	}

private:
	virtual void CacheParamInfo() const PURE_VIRTUAL(UAnimNextComponentParameter::CacheParamInfo, )

protected:
	mutable UE::AnimNext::FParamId ParameterId;
	mutable FProperty* ValueProperty = nullptr;
	mutable FAnimNextParamType ValueType;
	mutable	uint8* ValuePtr = nullptr;
};

// An object parameter
UCLASS(MinimalAPI, meta = (DisplayName = "Anim Next Graph"))
class UAnimNextComponentParameter_AnimNextGraph : public UAnimNextComponentParameter
{
	GENERATED_BODY()

	IMPLEMENT_ANIMNEXT_COMPONENT_PARAMETER(UAnimNextComponentParameter_AnimNextGraph);

	// The parameter to set the value to
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (CustomWidget = "ParamName", AllowedParamType = "TObjectPtr<UAnimNextGraph>"))
	FName Parameter;

	// The value to set
	UPROPERTY(EditAnywhere, Category = "Parameter")
	TObjectPtr<UAnimNextGraph> Value;
};
