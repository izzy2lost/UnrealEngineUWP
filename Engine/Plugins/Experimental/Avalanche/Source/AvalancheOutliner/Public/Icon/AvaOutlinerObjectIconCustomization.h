// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaOutlinerIconCustomization.h"
#include "Delegates/Delegate.h"
#include "Delegates/DelegateCombinations.h"
#include "Templates/SharedPointer.h"

class UClass;

DECLARE_DELEGATE_RetVal_OneParam(FSlateIcon /** OutIconForItem */, FOnGetOverriddenObjectIcon, TSharedPtr<const FAvaOutlinerItem> /** InItemToCustomize */)

class AVALANCHEOUTLINER_API FAvaOutlinerObjectIconCustomization : public IAvaOutlinerIconCustomization
{
public:
	FAvaOutlinerObjectIconCustomization(const UClass* InSupportedClass);

	//~ Begin IAvaOutlinerIconCustomization
	virtual FName GetOutlinerItemIdentifier() const override { return SupportedClassName; }
	virtual bool HasOverrideIcon(TSharedPtr<const FAvaOutlinerItem> InOutlinerItem) const override;
	virtual FSlateIcon GetOverrideIcon(TSharedPtr<const FAvaOutlinerItem> InOutlinerItem) const override;
	//~ End IAvaOutlinerIconCustomization

	void SetOverriddenIcon(const FOnGetOverriddenObjectIcon& InOverriddenIcon);

protected:
	FName SupportedClassName;

	FOnGetOverriddenObjectIcon OnGetOverriddenIcon;
};
