// Copyright Epic Games, Inc. All Rights Reserved.
#include "OutputEnumColumn.h"
#include "EnumColumn.h"
#include "ChooserPropertyAccess.h"

FOutputEnumColumn::FOutputEnumColumn()
{
#if WITH_EDITOR
	InputValue.InitializeAs(FEnumContextProperty::StaticStruct());
#endif
}

void FOutputEnumColumn::SetOutputs(FChooserEvaluationContext& Context, int RowIndex) const
{
	if (InputValue.IsValid())
	{
		uint8 OutputValue = FallbackValue.Value;
		if (RowValues.IsValidIndex(RowIndex))
		{
			OutputValue = RowValues[RowIndex].Value;
		}
		InputValue.Get<FChooserParameterEnumBase>().SetValue(Context, OutputValue);
	}
	
#if WITH_EDITOR
	if (Context.DebuggingInfo.bCurrentDebugTarget)
	{
		TestValue = RowValues[RowIndex].Value;
	}
#endif
}