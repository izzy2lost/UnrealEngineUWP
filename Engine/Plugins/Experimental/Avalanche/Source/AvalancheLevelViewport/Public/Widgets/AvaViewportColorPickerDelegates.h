// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaDefs.h"
#include "Delegates/Delegate.h"
#include "Delegates/DelegateCombinations.h"
#include "Templates/SharedPointerFwd.h"

class IToolkitHost;

DECLARE_MULTICAST_DELEGATE_TwoParams(FAvaOnColorPicked, const TSharedRef<IToolkitHost>& Editor, const FAvaColorChangeData& NewColorData)
DECLARE_MULTICAST_DELEGATE_TwoParams(FAvaOnColorSourceSelected, const TSharedRef<IToolkitHost>& Editor, const FAvaColorChangeData& NewColorData)

struct AVALANCHELEVELVIEWPORT_API FAvaViewportColorPickerDelegates
{
	static FAvaOnColorPicked& GetOnColorPicked();

	static void BroadcastColorPicked(const TSharedRef<IToolkitHost>& InEditor, const FAvaColorChangeData& InNewColorData);

	static FAvaOnColorPicked& GetOnColorSourceSelected();

	static void BroadcastColorSourceSelected(const TSharedRef<IToolkitHost>& InEditor, const FAvaColorChangeData& InNewColorData);

	static FAvaColorChangeData GetLastColorData();
};
