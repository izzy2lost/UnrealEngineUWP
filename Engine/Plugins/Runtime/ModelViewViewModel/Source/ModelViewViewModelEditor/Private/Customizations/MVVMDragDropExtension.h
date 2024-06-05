// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IHasDragDropExtensibility.h"

namespace UE::MVVM
{
class FDragDropExtension : public IDragDropExtension
{
	//~ Begin IDragDropExtension overrides
	virtual bool CanDropOnTarget(const UWidget* Target, const TSharedPtr<FDragDropOperation>& DragDropOp) const override;
	virtual FText GetDropFailureText(const UWidget* Target, const TSharedPtr<FDragDropOperation>& DragDropOp) const override;
	//~ End IDragDropExtension overrides
};
}
