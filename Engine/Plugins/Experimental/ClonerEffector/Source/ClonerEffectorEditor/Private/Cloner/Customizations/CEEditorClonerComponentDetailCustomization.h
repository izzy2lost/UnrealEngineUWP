// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "IDetailCustomization.h"
#include "UObject/NameTypes.h"
#include "UObject/WeakObjectPtr.h"

class FReply;
class UFunction;
class UObject;

/** Used to customize cloner component properties in details panel */
class FCEEditorClonerComponentDetailCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShared<FCEEditorClonerComponentDetailCustomization>();
	}

	explicit FCEEditorClonerComponentDetailCustomization()
	{
		RemoveEmptySections();
	}

	//~ Begin IDetailCustomization
	virtual void CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder) override;
	//~ End IDetailCustomization

protected:
	static void RemoveEmptySections();

	/** Execute ufunction with that name on selected objects */
	FReply OnFunctionButtonClicked(FName InFunctionName);

	TMap<FName, TMap<TWeakObjectPtr<UObject>, TWeakObjectPtr<UFunction>>> LayoutFunctionNames;
};
