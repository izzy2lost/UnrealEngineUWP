// Copyright Epic Games, Inc. All Rights Reserved.
#include "OutputFloatColumn.h"
#include "ChooserPropertyAccess.h"
#include "FloatRangeColumn.h"

FOutputFloatColumn::FOutputFloatColumn()
{
	InputValue.InitializeAs(FFloatContextProperty::StaticStruct());
}

void FOutputFloatColumn::SetOutputs(FChooserEvaluationContext& Context, int RowIndex) const
{
	if (InputValue.IsValid())
	{
		double OutputValue = FallbackValue;
		if (RowValues.IsValidIndex(RowIndex))
		{
			OutputValue = RowValues[RowIndex];
		}
	
		InputValue.Get<FChooserParameterFloatBase>().SetValue(Context, OutputValue);
	}
	
#if WITH_EDITOR
	if (Context.DebuggingInfo.bCurrentDebugTarget)
	{
		TestValue = RowValues[RowIndex];
	}
#endif
}
