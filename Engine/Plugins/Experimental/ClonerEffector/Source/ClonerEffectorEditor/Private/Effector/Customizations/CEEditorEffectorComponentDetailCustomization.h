// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"

/** Used to customize effector component properties in details panel */
class FCEEditorEffectorComponentDetailCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShared<FCEEditorEffectorComponentDetailCustomization>();
	}

	explicit FCEEditorEffectorComponentDetailCustomization()
	{
		RemoveEmptySections();
	}

	//~ Begin IDetailCustomization
	virtual void CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder) override;
	//~ End IDetailCustomization

protected:
	static void RemoveEmptySections();
};
