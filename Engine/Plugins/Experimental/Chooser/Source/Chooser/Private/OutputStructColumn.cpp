// Copyright Epic Games, Inc. All Rights Reserved.
#include "OutputStructColumn.h"
#include "ChooserPropertyAccess.h"
#include "OutputStructColumn.h"

#if WITH_EDITOR
	#include "IPropertyAccessEditor.h"
#endif

bool FStructContextProperty::SetValue(FChooserEvaluationContext& Context, const FInstancedStruct& InValue) const
{
	void* TargetData;
	if (Binding.GetValuePtr(Context, TargetData))
	{
		InValue.GetScriptStruct()->CopyScriptStruct(TargetData, InValue.GetMemory());
		return true;
	}
	return false;
}

#if WITH_EDITOR

void FOutputStructColumn::StructTypeChanged()
{
	if (InputValue.IsValid())
	{
		const UScriptStruct* Struct = InputValue.Get<FChooserParameterStructBase>().GetStructType();

		if (DefaultRowValue.GetScriptStruct() != Struct)
		{
			DefaultRowValue.InitializeAs(Struct);
		}

		for (FInstancedStruct& RowValue : RowValues)
		{
			if (RowValue.GetScriptStruct() != Struct)
			{
				RowValue.InitializeAs(Struct);
			}
		}
	}
}

#endif // WITH_EDITOR

FOutputStructColumn::FOutputStructColumn()
{
	InputValue.InitializeAs(FStructContextProperty::StaticStruct());
}

void FOutputStructColumn::SetOutputs(FChooserEvaluationContext& Context, int RowIndex) const
{
	if (InputValue.IsValid())
	{
		InputValue.Get<FChooserParameterStructBase>().SetValue(Context, RowValues[RowIndex]);
	}
	
#if WITH_EDITOR
	if (Context.DebuggingInfo.bCurrentDebugTarget)
	{
		TestValue = RowValues[RowIndex];
	}
#endif
}
