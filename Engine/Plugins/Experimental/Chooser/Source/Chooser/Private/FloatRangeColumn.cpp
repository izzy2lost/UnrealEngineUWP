// Copyright Epic Games, Inc. All Rights Reserved.
#include "FloatRangeColumn.h"
#include "ChooserIndexArray.h"
#include "ChooserPropertyAccess.h"
#include "ChooserTrace.h"



bool FFloatContextProperty::SetValue(FChooserEvaluationContext& Context, double InValue) const
{
	return Binding.SetValue(Context, InValue);
}

bool FFloatContextProperty::GetValue(FChooserEvaluationContext& Context, double& OutResult) const
{
	return Binding.GetValue(Context, OutResult);
}

FFloatRangeColumn::FFloatRangeColumn()
{
	InputValue.InitializeAs(FFloatContextProperty::StaticStruct());
}

void FFloatRangeColumn::Filter(FChooserEvaluationContext& Context, const FChooserIndexArray& IndexListIn, FChooserIndexArray& IndexListOut) const
{
	if (InputValue.IsValid())
	{
		double Result = 0.0f;
		InputValue.Get<FChooserParameterFloatBase>().GetValue(Context, Result);
		
		TRACE_CHOOSER_VALUE(Context, ToCStr(InputValue.Get<FChooserParameterBase>().GetDebugName()), Result);

#if WITH_EDITOR
		if (Context.DebuggingInfo.bCurrentDebugTarget)
		{
			TestValue = Result;
		}
#endif

		for(uint32 Index : IndexListIn)
		{
			if (RowValues.Num() > (int)Index)
			{
				const FChooserFloatRangeRowData& RowValue = RowValues[Index];
				if (Result >= RowValue.Min && Result <= RowValue.Max)
				{
					IndexListOut.Push(Index);
				}
			}
		}
	}
	else
	{
		// passthrough fallback (behaves better during live editing)
		IndexListOut = IndexListIn;
	}
}