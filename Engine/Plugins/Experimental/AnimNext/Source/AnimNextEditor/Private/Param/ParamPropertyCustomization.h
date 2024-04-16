// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"
#include "PropertyEditorModule.h"
#include "Param/AnimNextEditorParam.h"

namespace UE::AnimNext::Editor
{

class FParamPropertyCustomization : public IPropertyTypeCustomization
{
private:
	// IPropertyTypeCustomization interface
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

	void Refresh();

	void HandleCopy();

private:
	TSharedPtr<IPropertyHandle> PropertyHandle;
	FAnimNextEditorParam CachedParam;
	FText CachedNameText;
	UScriptStruct* ParamStruct = nullptr;
	FExecuteAction DefaultCopyAction;
};

}
