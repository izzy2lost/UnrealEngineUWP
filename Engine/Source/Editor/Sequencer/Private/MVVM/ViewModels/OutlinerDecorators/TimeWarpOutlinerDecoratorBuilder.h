// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MVVM/ViewModels/OutlinerDecorators/OutlinerDecoratorBuilderBase.h"

namespace UE::Sequencer
{

struct FCreateOutlinerColumnParams;
class IOutlinerColumn;
class STimeWarpDecoratorWidget;

class FTimeWarpOutlinerDecoratorBuilder
	: public FOutlinerDecoratorBuilderBase
{
public:
	FTimeWarpOutlinerDecoratorBuilder();
	
	virtual FName GetDecoratorName() const override;
	virtual bool IsItemCompatibleWithDecorator(const FCreateOutlinerColumnParams& InParams) const override;
	virtual TSharedPtr<SWidget> CreateDecoratorWidget(const FCreateOutlinerColumnParams& InParams, const TSharedRef<ISequencerTreeViewRow>& TreeViewRow, const TSharedRef<IOutlinerColumn>& OutlinerColumn, const int32 NumCompatibleDecorators) override;
};

} // namespace UE::Sequencer

