// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "IDetailCustomNodeBuilder.h"
#include "Containers/Set.h"
#include "PropertyPath.h"
#include "Widgets/SWidget.h"

class IDetailCategoryBuilder;
class FInstanceDataObjectFixupPanel;

/**
 * 
 */
class INSTANCEDATAOBJECTFIXUPTOOL_API FInstanceDataObjectFixupDetailCustomization : public IDetailCustomization, public TSharedFromThis<FInstanceDataObjectFixupDetailCustomization>
{
public:
	FInstanceDataObjectFixupDetailCustomization(const TSharedRef<FInstanceDataObjectFixupPanel>& DiffPanel);
	virtual ~FInstanceDataObjectFixupDetailCustomization() override;
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	TWeakPtr<FInstanceDataObjectFixupPanel> DiffPanel;
};

class INSTANCEDATAOBJECTFIXUPTOOL_API FHideLoosePropertiesCustomization : public IDetailCustomization, public TSharedFromThis<FInstanceDataObjectFixupDetailCustomization>
{
public:
	FHideLoosePropertiesCustomization() = default;
	virtual ~FHideLoosePropertiesCustomization() override;
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

	static void CustomizeHandle(const TSharedRef<IPropertyHandle>& Handle, IDetailLayoutBuilder& DetailBuilder);

private:
	TWeakPtr<FInstanceDataObjectFixupPanel> DiffPanel;
};

class INSTANCEDATAOBJECTFIXUPTOOL_API FInstanceDataObjectFixupDetailNodeBuilder : public IDetailCustomNodeBuilder, public TSharedFromThis<FInstanceDataObjectFixupDetailNodeBuilder>
{
public:
	FInstanceDataObjectFixupDetailNodeBuilder(const TSharedRef<FInstanceDataObjectFixupPanel>& DiffPanel, const TSharedRef<IPropertyHandle>& PropertyHandle);
	virtual void GenerateHeaderRowContent(FDetailWidgetRow& NodeRow) override;
	virtual void GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder) override;
	virtual FName GetName() const override;
	virtual TSharedPtr<IPropertyHandle> GetPropertyHandle() const override;

private:
	int32 GetNameWidgetIndex() const;
	TSharedRef<SWidget> GeneratePropertyRedirectMenu() const;
	EVisibility DeletionSymbolVisibility() const;
	EVisibility ValueContentVisibility() const;
	bool IsHidden() const;
	
	TSet<FPropertyPath> GetRedirectOptions(const UStruct* Struct, void* Value) const;
	void GetRedirectOptions(const UStruct* Struct, void* Value, const FPropertyPath& Path, TSet<FPropertyPath>& OutPaths) const;
	void GetRedirectOptions(const FProperty* Property, void* Value, const FPropertyPath& Path, TSet<FPropertyPath>& OutPaths) const;
	
	TWeakPtr<FInstanceDataObjectFixupPanel> DiffPanel;
	TSharedRef<IPropertyHandle> PropertyHandle;

	enum ENameWidgetIndex : uint8
	{
		DisplayRegularName = 0,
		DisplayRedirectMenu = 1
	};
};
