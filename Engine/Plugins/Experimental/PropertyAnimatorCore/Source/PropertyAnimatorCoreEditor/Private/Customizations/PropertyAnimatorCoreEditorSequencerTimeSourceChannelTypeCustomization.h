// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"
#include "Templates/SharedPointer.h"

class FReply;
class IPropertyHandle;
struct EVisibility;

/** Type customization for FPropertyAnimatorCoreSequencerTimeSourceChannel */
class FPropertyAnimatorCoreEditorSequencerTimeSourceChannelTypeCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShared<FPropertyAnimatorCoreEditorSequencerTimeSourceChannelTypeCustomization>();
	}

	//~ Begin IPropertyTypeCustomization
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& InRow, IPropertyTypeCustomizationUtils& InUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle, IDetailChildrenBuilder& InBuilder, IPropertyTypeCustomizationUtils& InUtils) override;
	//~ End IPropertyTypeCustomization

protected:
	FReply OnCreateTrackButtonClicked();
	EVisibility GetCreateTrackButtonVisibility() const;
	bool IsCreateTrackButtonEnabled() const;

	TSharedPtr<IPropertyHandle> ChannelPropertyHandle;
};
