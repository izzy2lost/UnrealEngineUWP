// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MVVM/ViewModels/OutlinerDecorators/IOutlinerDecorator.h"

namespace UE::Sequencer
{

class FOutlinerDecoratorBase
	: public IOutlinerDecorator
{
public:

	FOutlinerDecoratorBase()
	{
	}
	
public:

	bool IsItemCompatibleWithDecorator(const FCreateOutlinerColumnParams& InParams) const override { return false; }
	TSharedPtr<SWidget> CreateDecoratorWidget(const FCreateOutlinerColumnParams& InParams, const TSharedRef<ISequencerTreeViewRow>& TreeViewRow, const TSharedRef<IOutlinerColumn>& OutlinerColumn, const int32 NumCompatibleDecorators) override { return nullptr; }
};

} // namespace UE::Sequencer