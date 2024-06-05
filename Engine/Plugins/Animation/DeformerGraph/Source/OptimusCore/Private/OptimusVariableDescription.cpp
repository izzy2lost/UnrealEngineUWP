// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusVariableDescription.h"

#include "OptimusDataTypeRegistry.h"
#include "OptimusDeformer.h"
#include "OptimusHelpers.h"
#include "OptimusValueContainer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(OptimusVariableDescription)


void UOptimusVariableDescription::EnsureValueContainer()
{
	// Check if the current default value storage matches, otherwise create a matching default value storage, otherwise
	// if the variable type changes, we end up with mismatch in storage vs type.
	const UClass* RequiredClass = UOptimusValueContainerGeneratorClass::GetClassForType(GetPackage(), DataType);

	if (!DefaultValue || DefaultValue->GetClass() != RequiredClass)
	{
		DefaultValue = UOptimusValueContainer::MakeValueContainer(this, DataType);
	}

	CachedShaderValue = DefaultValue->GetShaderValue();
}


UOptimusDeformer* UOptimusVariableDescription::GetOwningDeformer() const
{
	const UOptimusVariableContainer* Container = CastChecked<UOptimusVariableContainer>(GetOuter());
	return Container ? CastChecked<UOptimusDeformer>(Container->GetOuter()) : nullptr;
}


int32 UOptimusVariableDescription::GetIndex() const
{
	const UOptimusVariableContainer* Container = CastChecked<UOptimusVariableContainer>(GetOuter());
	return Container->Descriptions.IndexOfByKey(this);
}


void UOptimusVariableDescription::PostLoad()
{
	Super::PostLoad();

	// 32-bit float data type is not supported for variables although they were allowed before. Do an in-place upgrade here. 
	const FOptimusDataTypeHandle FloatDataType = FOptimusDataTypeRegistry::Get().FindType(*FFloatProperty::StaticClass());
	const FOptimusDataTypeHandle DoubleDataType = FOptimusDataTypeRegistry::Get().FindType(*FDoubleProperty::StaticClass());
	
	if (DataType == FloatDataType)
	{
		DataType = DoubleDataType;
	}

	EnsureValueContainer();

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (!ValueData_DEPRECATED.IsEmpty())
	{
		CachedShaderValue.ShaderValue = ValueData_DEPRECATED;
		ValueData_DEPRECATED.Reset();
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}


#if WITH_EDITOR

void UOptimusVariableDescription::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = PropertyChangedEvent.GetPropertyName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UOptimusVariableDescription, VariableName))
	{
		UOptimusDeformer* Deformer = GetOwningDeformer();
		if (ensure(Deformer))
		{
			VariableName = Optimus::GetUniqueNameForScope(GetOuter(), VariableName);
			Rename(*VariableName.ToString(), nullptr);

			constexpr bool bForceChange = true;
			Deformer->RenameVariable(this, VariableName, bForceChange);
		}
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(FOptimusDataType, TypeName))
	{
		UOptimusDeformer* Deformer = GetOwningDeformer();
		if (ensure(Deformer))
		{
			// Set the variable type again, so that we can remove any links that are now type-incompatible.
			constexpr bool bForceChange = true;
			Deformer->SetVariableDataType(this, DataType, bForceChange);
		}
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UOptimusVariableDescription, DefaultValue))
	{
		// Store the default shader value.
		CachedShaderValue = DefaultValue->GetShaderValue();
	}
}


void UOptimusVariableDescription::PreEditUndo()
{
	UObject::PreEditUndo();

	VariableNameForUndo = VariableName;
}


void UOptimusVariableDescription::PostEditUndo()
{
	UObject::PostEditUndo();

	if (VariableNameForUndo != VariableName)
	{
		const UOptimusDeformer *Deformer = GetOwningDeformer();
		Deformer->Notify(EOptimusGlobalNotifyType::VariableRenamed, this);
	}
}
#endif
