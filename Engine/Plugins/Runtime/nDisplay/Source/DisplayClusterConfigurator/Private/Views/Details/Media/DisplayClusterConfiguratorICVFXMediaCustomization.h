// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Views/Details/DisplayClusterConfiguratorBaseTypeCustomization.h"

#include "Input/Reply.h"

#include "IPropertyTypeCustomization.h"

class UDisplayClusterConfigurationData;


/**
 * Details panel customization for the FDisplayClusterConfigurationMediaICVFX struct.
 */
class FDisplayClusterConfiguratorICVFXMediaCustomization
	: public FDisplayClusterConfiguratorBaseTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShared<FDisplayClusterConfiguratorICVFXMediaCustomization>();
	}

protected:
	//~ Begin IPropertyTypeCustomization
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	//~ End IPropertyTypeCustomization

private:

	/** Builds setup button widget. */
	void AddSetupButton(IDetailChildrenBuilder& ChildBuilder);

	/** Builds reset button widget. */
	void AddResetButton(IDetailChildrenBuilder& ChildBuilder);

	/** Handles setup button clicks. */
	FReply OnSetupButtonClicked();

	/** Handles reset button clicks. */
	FReply OnResetButtonClicked();

private:

	/** Returns configuration of a DCRA owning the camera being edited. */
	UDisplayClusterConfigurationData* GetConfig() const;

	/** Marks package as dirty */
	void MarkDirty();

private:

	/** Tiles layout property. */
	TSharedPtr<IPropertyHandle> TilesLayoutHandle;
};
