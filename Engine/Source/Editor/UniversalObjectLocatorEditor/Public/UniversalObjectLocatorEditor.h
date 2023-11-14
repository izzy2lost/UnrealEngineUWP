// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

struct FSlateIcon;

class FText;
class UObject;
class SWidget;
class FDragDropOperation;

namespace UE::UniversalObjectLocator
{

class IUniversalObjectLocatorCustomization;

class ILocatorEditor : public TSharedFromThis<ILocatorEditor>
{
public:
	virtual ~ILocatorEditor() = default;

	virtual bool IsDragSupported(TSharedPtr<FDragDropOperation> DragOperation, UObject* Context) const = 0;

	virtual UObject* ResolveDragOperation(TSharedPtr<FDragDropOperation> DragOperation, UObject* Context) const = 0;

	virtual TSharedPtr<SWidget> MakeEditUI(TSharedPtr<IUniversalObjectLocatorCustomization> Customization) = 0;

	virtual FText GetDisplayText() const = 0;
	virtual FText GetDisplayTooltip() const = 0;
	virtual FSlateIcon GetDisplayIcon() const = 0;
};


} // namespace UE::UniversalObjectLocator