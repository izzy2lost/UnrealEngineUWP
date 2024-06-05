// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Internationalization/Text.h"

class FDragDropOperation;
class UWidget;

class UMGEDITOR_API IDragDropExtension
{
public:
	virtual ~IDragDropExtension() { }

	virtual bool CanDropOnTarget(const UWidget* Target, const TSharedPtr<FDragDropOperation>& DragDropOp) const = 0;
	virtual FText GetDropFailureText(const UWidget* Target, const TSharedPtr<FDragDropOperation>& DragDropOp) const = 0;
};

/**
 * Drag & drop extensibility manager holds a list of registered drag and drop extensions.
 */
class UMGEDITOR_API FDragDropExtensibilityManager
{
public:
	void AddExtension(const TSharedRef<IDragDropExtension>& Extension)
	{
		if (ensure(!Extensions.Contains(Extension)))
		{
			Extensions.Add(Extension);
		}
	}

	void RemoveExtension(const TSharedRef<IDragDropExtension>& Extension)
	{
		int32 NumRemoved = Extensions.RemoveSingleSwap(Extension);
		ensure(NumRemoved == 1);
	}

	TArrayView<const TSharedPtr<IDragDropExtension>> GetExtensions() const
	{
		return Extensions;
	}

private:
	TArray<TSharedPtr<IDragDropExtension>> Extensions;
};

/** Indicates that a class can extend drag & drop functionality */
class IHasDragDropExtensibility
{
public:
	virtual TSharedPtr<FDragDropExtensibilityManager> GetDragDropExtensibilityManager() = 0;
};
