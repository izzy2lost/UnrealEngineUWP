// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MVVM/ViewModels/OutlinerDecorators/OutlinerDecoratorBase.h"

namespace UE::Sequencer
{

struct FCreateOutlinerColumnParams;
class IOutlinerColumn;
class SConditionDecoratorWidget;

class FConditionOutlinerDecorator
	: public FOutlinerDecoratorBase
{
public:
	FConditionOutlinerDecorator();
	
	virtual FName GetDecoratorName() const override;
	virtual bool IsItemCompatibleWithDecorator(const FCreateOutlinerColumnParams& InParams) const override;
	virtual TSharedPtr<SWidget> CreateDecoratorWidget(const FCreateOutlinerColumnParams& InParams, const TSharedRef<ISequencerTreeViewRow>& TreeViewRow, const TSharedRef<IOutlinerColumn>& OutlinerColumn, const int32 NumCompatibleDecorators) override;

protected:
	friend class SConditionDecoratorWidget;
	mutable float Opacity;

private:
	TSharedPtr<SConditionDecoratorWidget> DecoratorWidget;
};

} // namespace UE::Sequencer

