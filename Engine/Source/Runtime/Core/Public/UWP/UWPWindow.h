// Copyright (C) Microsoft. All rights reserved.

#pragma once

#include "GenericWindow.h"
#include "SharedPointer.h"

/**
* A platform specific implementation of FNativeWindow.
*/
class CORE_API FUWPWindow
	: public FGenericWindow, public TSharedFromThis<FUWPWindow>
{
public:

	/** Destructor. */
	virtual ~FUWPWindow();

	/** Create a new FUWPWindow. */
	static TSharedRef<FUWPWindow> Make();

	/** Init a FUWPWindow. */
	void Initialize(class FUWPApplication* const Application, const TSharedRef<FGenericWindowDefinition>& InDefinition);

public:
	virtual void ReshapeWindow(int32 X, int32 Y, int32 Width, int32 Height) override;

	virtual void AdjustCachedSize(FVector2D& Size) const override;

	virtual void SetWindowMode(EWindowMode::Type InNewWindowMode) override;

	virtual EWindowMode::Type GetWindowMode() const override;

public:
	static int32 FUWPWindow::ConvertDipsToPixels(int32 Dips, float Dpi);
	static int32 FUWPWindow::ConvertPixelsToDips(int32 Pixels, float Dpi);

private:

	/** Protect the constructor; only TSharedRefs of this class can be made. */
	FUWPWindow();
};
