// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MVVM/ViewModels/OutlinerDecorators/OutlinerDecoratorBase.h"

namespace UE::Sequencer
{

struct FCreateOutlinerColumnParams;
class IOutlinerColumn;
class STimeWarpDecoratorWidget;

class FTimeWarpOutlinerDecorator
	: public FOutlinerDecoratorBase
{
public:
	FTimeWarpOutlinerDecorator();
	
	virtual FName GetDecoratorName() const override;
	virtual bool IsItemCompatibleWithDecorator(const FCreateOutlinerColumnParams& InParams) const override;
	virtual TSharedPtr<SWidget> CreateDecoratorWidget(const FCreateOutlinerColumnParams& InParams, const TSharedRef<ISequencerTreeViewRow>& TreeViewRow, const TSharedRef<IOutlinerColumn>& OutlinerColumn, const int32 NumCompatibleDecorators) override;

protected:
	friend class STimeWarpDecoratorWidget;
	mutable float Opacity;

private:
	TSharedPtr<STimeWarpDecoratorWidget> DecoratorWidget;
};

} // namespace UE::Sequencer

