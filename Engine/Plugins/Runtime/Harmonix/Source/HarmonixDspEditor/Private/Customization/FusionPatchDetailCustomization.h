// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "DetailCategoryBuilder.h"
#include "UObject/NameTypes.h"
#include "Templates/SharedPointer.h"
#include "Layout/Visibility.h"

class SWidget;

//Fusion Patch detail customizatiaon class
class FFusionPatchDetailCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShareable(new FFusionPatchDetailCustomization);
	}

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;
private:
	int32 CurrentKeyzoneIndex = 0;
	
	void AddKeyzonesNameToMenuArray(TSharedPtr<IPropertyHandleArray> KeyzonesHandle, int32 NumKeyzones);

	//helper functions to paint customized properties
	FDetailWidgetRow& AddCustomMinMaxSliderRow(IDetailCategoryBuilder& FusionPatchDataCategory, const FText& DisplayName, TSharedPtr<IPropertyHandle> MinPropertyHandle, TSharedPtr<IPropertyHandle> MaxPropertyHandle);
	void DrawSelectedKeyzoneProperties(IDetailCategoryBuilder& FusionPatchDataCategory, TSharedPtr<IPropertyHandleArray> KeyzonesHandle, int32 NumKeyzones);

	TSharedPtr<FString> CurrentKeyzoneName;
	TArray<TSharedPtr<FString>> KeyzonesNameMenu;

	TSharedRef<SWidget> CreateMinMaxSliderWidget(TSharedPtr<IPropertyHandle> MinValuePropertyHandle, TSharedPtr<IPropertyHandle> MaxValuePropertyHandle);
};


