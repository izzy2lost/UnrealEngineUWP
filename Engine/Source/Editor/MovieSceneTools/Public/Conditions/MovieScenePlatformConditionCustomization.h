// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "Templates/SharedPointer.h"
#include "PropertyHandle.h"

class IDetailLayoutBuilder;

class MOVIESCENETOOLS_API FMovieScenePlatformConditionCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

	TSharedPtr<IPropertyHandle> ValidPlatformsPropertyHandle;
};
