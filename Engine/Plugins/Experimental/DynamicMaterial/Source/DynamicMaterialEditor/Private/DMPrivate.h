// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "HAL/Platform.h"
#include "Slate/SDMSlot.h"
#include "UObject/WeakObjectPtr.h"

class UDMMaterialLayerObject;
class UDMMaterialSlot;
class UDMMaterialStage;
class UDMMaterialStageInput;
struct FExpressionInput;
struct FExpressionOutput;

namespace UE::DynamicMaterialEditor::Private
{
	struct FDMInputInputs
	{
		int32 InputIndex;
		TArray<UDMMaterialStageInput*> ChannelInputs;
	};

	void SetMask(FExpressionInput& InInputConnector, const FExpressionOutput& InOutputConnector, int32 InChannelOverride);
}

struct FDMMaterialLayerReference
{
	TWeakObjectPtr<UDMMaterialLayerObject> LayerWeak;

	FDMMaterialLayerReference();
	FDMMaterialLayerReference(UDMMaterialLayerObject* InLayer);

	UDMMaterialLayerObject* GetLayer() const;

	bool IsBaseEnabled() const;
	bool IsBaseBeingEdited() const;
	bool IsMaskEnabled() const;
	bool IsMaskBeingEdited() const;
};
