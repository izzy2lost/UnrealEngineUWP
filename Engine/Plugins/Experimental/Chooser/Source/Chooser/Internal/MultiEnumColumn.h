// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "EnumColumn.h"
#include "IChooserColumn.h"
#include "IChooserParameterEnum.h"
#include "StructUtils/InstancedStruct.h"
#include "ChooserPropertyAccess.h"
#include "Serialization/MemoryReader.h"
#include "MultiEnumColumn.generated.h"

struct FBindingChainElement;

USTRUCT()
struct FChooserMultiEnumRowData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Runtime)
	uint32 Value = 0;

	bool Evaluate(const uint32 LeftHandSide) const
	{
		return Value == 0 || Value & LeftHandSide;
	}
};


USTRUCT()
struct CHOOSER_API FMultiEnumColumn : public FChooserColumnBase
{
	GENERATED_BODY()
public:
	FMultiEnumColumn();

	UPROPERTY(EditAnywhere, NoClear, Meta = (ExcludeBaseStruct, BaseStruct = "/Script/Chooser.ChooserParameterEnumBase"), Category = "Data")
	FInstancedStruct InputValue;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "Data")
	FChooserMultiEnumRowData DefaultRowValue;
#endif
	
	UPROPERTY(EditAnywhere, Category = "Data")
	// array of results (cells for this column for each row in the table)
	// should match the length of the Results array
	TArray<FChooserMultiEnumRowData> RowValues;

	virtual void Filter(FChooserEvaluationContext& Context, const FChooserIndexArray& IndexListIn, FChooserIndexArray& IndexListOut) const override;
	
#if WITH_EDITOR
	mutable uint8 TestValue = 0;
	virtual bool EditorTestFilter(int32 RowIndex) const override
	{
		return RowValues.IsValidIndex(RowIndex) && RowValues[RowIndex].Evaluate(1 << TestValue);
	}
	
	virtual void SetTestValue(TArrayView<const uint8> Value) override
	{
		FMemoryReaderView Reader(Value);
		Reader << TestValue;
	}
	
	virtual void AddToDetails (FInstancedPropertyBag& PropertyBag, int32 ColumnIndex, int32 RowIndex);
	virtual void SetFromDetails(FInstancedPropertyBag& PropertyBag, int32 ColumnIndex, int32 RowIndex);
#endif
	
	CHOOSER_COLUMN_BOILERPLATE(FChooserParameterEnumBase);

	virtual void PostLoad() override;
};