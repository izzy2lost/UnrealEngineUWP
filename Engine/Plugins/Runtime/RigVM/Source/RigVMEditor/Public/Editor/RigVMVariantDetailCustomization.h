// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "RigVMBlueprint.h"
#include "Widgets/SRigVMLogWidget.h"

class IPropertyHandle;

class FRigVMVariantDetailCustomization : public IPropertyTypeCustomization
{
public:

	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShareable(new FRigVMVariantDetailCustomization);
	}

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<class IPropertyHandle> InStructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<class IPropertyHandle> InStructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

	bool IsAssetVariant() const;
	FText GetVariantGuidText() const;
	void RefreshVariantLog();
protected:

	URigVMBlueprint* BlueprintBeingCustomized;

	/** The log widget used for function variants */
	TSharedPtr<SRigVMLogWidget> VariantLog;
};
