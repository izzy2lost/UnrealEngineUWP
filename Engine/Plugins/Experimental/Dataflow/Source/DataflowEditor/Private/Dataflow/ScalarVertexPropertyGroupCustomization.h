// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"

class IPropertyHandle;
class ITableRow;
class SComboButton;
class STableViewBase;
class SWidget;

namespace Dataflow
{

	class FScalarVertexPropertyGroupCustomization : public IPropertyTypeCustomization
	{
	public:

		static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	private:
		virtual void CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
		virtual void CustomizeChildren(TSharedRef<IPropertyHandle> /*InPropertyHandle*/, IDetailChildrenBuilder& /*ChildBuilder*/, IPropertyTypeCustomizationUtils& /*CustomizationUtils*/) override {}

		FText GetText() const;
		void OnTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo);
		void OnSelectionChanged(TSharedPtr<FText> ItemSelected, ESelectInfo::Type SelectInfo);
		TSharedRef<ITableRow> MakeCategoryViewWidget(TSharedPtr<FText> Item, const TSharedRef<STableViewBase>& OwnerTable);
		TSharedRef<SWidget> OnGetMenuContent();
		
		template<typename T>
		T* GetOwnerStruct() const;

		TSharedPtr<IPropertyHandle> ChildPropertyHandle;
		TWeakPtr<SComboButton> ComboButton;
		TArray<TSharedPtr<FText>> GroupNames;
	};
}
