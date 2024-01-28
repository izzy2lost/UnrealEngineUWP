// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h"
#include "Templates/SharedPointer.h"

class IDetailKeyframeHandler;
class IPropertyHandle;
struct FPropertyRowExtensionButton;

struct CUSTOMDETAILSVIEW_API FCustomDetailsViewSequencerUtils
{
	static void CreateSequencerExtensionButton(TWeakPtr<IDetailKeyframeHandler> InKeyframeHandlerWeak, TSharedPtr<IPropertyHandle> InPropertyHandle,
		TArray<FPropertyRowExtensionButton>& OutExtensionButtons);
};
