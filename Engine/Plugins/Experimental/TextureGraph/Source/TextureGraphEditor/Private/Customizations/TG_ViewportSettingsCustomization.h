// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "TG_Parameter.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "IPropertyTypeCustomization.h"
//#include "IPropertyUtilities.h"
//#include "PropertyHandle.h"
#include "DetailWidgetRow.h"
//#include "DetailLayoutBuilder.h"
//#include "ISinglePropertyView.h"
#include "IDetailChildrenBuilder.h"
//#include "IDetailCustomization.h"
#include "IDetailGroup.h"
#include "PropertyCustomizationHelpers.h"
#include "TG_Pin.h"
#include "Expressions/Output/TG_Expression_Output.h"
#include "TG_Node.h"
#include "TG_Graph.h"
#include "TextureGraph.h"
#include "Model/Mix/ViewportSettings.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "FTextureGraphEditorModule"

class FTG_ViewportSettingsCustomization : public IPropertyTypeCustomization
{

private:
	UMixSettings* MixSettings;
	
	TSharedPtr<IPropertyHandle> MaterialPropertyHandle;
	TSharedPtr<IPropertyHandle> MaterialMappingInfosPropertyHandle;
	
public:
	static TSharedRef<IPropertyTypeCustomization> Create()
	{
		return MakeShareable(new FTG_ViewportSettingsCustomization);
	}

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override
	{
		MaterialPropertyHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FViewportSettings, Material));
		MaterialMappingInfosPropertyHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FViewportSettings, MaterialMappingInfos));
	}

	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		
		if (PropertyHandle->IsValidHandle())
		{
			ChildBuilder.AddProperty(MaterialPropertyHandle.ToSharedRef());

			// In this case, we'll add the array elements directly without the header for the array name.
			uint32 NumChildren;
			MaterialMappingInfosPropertyHandle->GetNumChildren(NumChildren);

			for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
			{
				TSharedRef<IPropertyHandle> ChildHandle = MaterialMappingInfosPropertyHandle->GetChildHandle(ChildIndex).ToSharedRef();
				if (ChildHandle->IsValidHandle())
				{
					ChildBuilder.AddProperty(ChildHandle);
				}
			}
		}
	}
};

#undef LOCTEXT_NAMESPACE
