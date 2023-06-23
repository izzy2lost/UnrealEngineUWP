// Copyright Epic Games, Inc. All Rights Reserved.
#include "EnumColumn.h"
#include "ChooserPropertyAccess.h"

#if WITH_EDITOR
#include "IPropertyAccessEditor.h"
#endif

bool FEnumContextProperty::GetValue(FChooserEvaluationContext& Context, uint8& OutResult) const
{
	return Binding.GetValue(Context, OutResult);
}

bool FEnumContextProperty::SetValue(FChooserEvaluationContext& Context, uint8 InValue) const
{
	return Binding.SetValue(Context, InValue);
}

FEnumColumn::FEnumColumn()
{
#if WITH_EDITOR
	InputValue.InitializeAs(FEnumContextProperty::StaticStruct());
#endif
}

bool FChooserEnumRowData::Evaluate(const uint8 LeftHandSide) const
{
	bool Equal = LeftHandSide == Value;
	return Equal ^ CompareNotEqual;
}

void FEnumColumn::Filter(FChooserEvaluationContext& Context, const TArray<uint32>& IndexListIn, TArray<uint32>& IndexListOut) const
{
	uint8 Result = 0;
	if (InputValue.IsValid() &&
		InputValue.Get<FChooserParameterEnumBase>().GetValue(Context, Result))
	{
#if WITH_EDITOR
		if (Context.DebuggingInfo.bCurrentDebugTarget)
		{
			TestValue = Result;
		}
#endif
		
		for (const uint32 Index : IndexListIn)
		{
			if (RowValues.IsValidIndex(Index))
			{
				const FChooserEnumRowData& RowValue = RowValues[Index];
				if (RowValue.Evaluate(Result))
				{
					IndexListOut.Emplace(Index);
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