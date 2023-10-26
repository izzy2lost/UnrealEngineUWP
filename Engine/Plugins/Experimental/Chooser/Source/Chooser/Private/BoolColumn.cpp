// Copyright Epic Games, Inc. All Rights Reserved.
#include "BoolColumn.h"
#include "ChooserIndexArray.h"
#include "ChooserPropertyAccess.h"
#include "Chooser.h"
#include "ChooserTrace.h"

bool FBoolContextProperty::GetValue(FChooserEvaluationContext& Context, bool& OutResult) const
{
	return Binding.GetValue(Context, OutResult);
}

bool FBoolContextProperty::SetValue(FChooserEvaluationContext& Context, bool InValue) const
{
	return Binding.SetValue(Context, InValue);
}

FBoolColumn::FBoolColumn()
{
	InputValue.InitializeAs(FBoolContextProperty::StaticStruct());
}

void FBoolColumn::Filter(FChooserEvaluationContext& Context, const FChooserIndexArray& IndexListIn, FChooserIndexArray& IndexListOut) const
{
	if (InputValue.IsValid())
	{
		bool Result = false;
		InputValue.Get<FChooserParameterBoolBase>().GetValue(Context,Result);

		TRACE_CHOOSER_VALUE(Context, ToCStr(InputValue.Get<FChooserParameterBase>().GetDebugName()), Result);

	#if WITH_EDITOR
		if (Context.DebuggingInfo.bCurrentDebugTarget)
		{
			TestValue = Result;
		}
	#endif
		
		for (uint32 Index : IndexListIn)
		{
			if (RowValuesWithAny.Num() > (int)Index)
			{
				
				if (RowValuesWithAny[Index] == EBoolColumnCellValue::MatchAny || Result == static_cast<bool>(RowValuesWithAny[Index]))
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