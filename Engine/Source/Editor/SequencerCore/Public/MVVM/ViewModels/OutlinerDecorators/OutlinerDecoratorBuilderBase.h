// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MVVM/ViewModels/OutlinerDecorators/IOutlinerDecoratorBuilder.h"

namespace UE::Sequencer
{

class FOutlinerDecoratorBuilderBase
	: public IOutlinerDecoratorBuilder
{
public:

	FOutlinerDecoratorBuilderBase()
	{
	}
	
public:

	bool IsItemCompatibleWithDecorator(const FCreateOutlinerColumnParams& InParams) const override { return false; }
	TSharedPtr<SWidget> CreateDecoratorWidget(const FCreateOutlinerColumnParams& InParams, const TSharedRef<ISequencerTreeViewRow>& TreeViewRow, const TSharedRef<IOutlinerColumn>& OutlinerColumn, const int32 NumCompatibleDecorators) override { return nullptr; }
};

} // namespace UE::Sequencer