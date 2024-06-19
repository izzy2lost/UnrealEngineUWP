// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/PropertyViewer/IFieldExpander.h"
#include "Framework/PropertyViewer/IFieldIterator.h"
#include "Param/AnimNextParam.h"
#include "Widgets/SCompoundWidget.h"
#include "Param/ParameterPickerArgs.h"
#include "Widgets/PropertyViewer/SPropertyViewer.h"
#include "Widgets/Views/STreeView.h"
#include "StructUtils/PropertyBag.h"
#include "Misc/NotifyHook.h"
#include "Param/AnimNextEditorParam.h"

class IPropertyRowGenerator;
class IStructureDataProvider;
class SSearchBox;

namespace UE::AnimNext::Editor
{

class SParameterPicker : public SCompoundWidget, public FNotifyHook
{
public:
	SLATE_BEGIN_ARGS(SParameterPicker) {}

	SLATE_ARGUMENT(FParameterPickerArgs, Args)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	void RefreshEntries();

	bool GetFieldInfo(UE::PropertyViewer::SPropertyViewer::FHandle InHandle, const FFieldVariant& InField, FName& OutName, TInstancedStruct<FAnimNextParamInstanceIdentifier>& OutInstanceId, FAnimNextParamType& OutType) const;

	void HandleGetParameterBindings(TArray<FParameterBindingReference>& OutParameterBindings) const;

	void HandleSetInstanceId(const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InInstanceId);

	void HandleFieldPicked(UE::PropertyViewer::SPropertyViewer::FHandle InHandle, TArrayView<const FFieldVariant> InFields, ESelectInfo::Type InSelectionType);

	TSharedRef<SWidget> HandleGenerateContainer(UE::PropertyViewer::SPropertyViewer::FHandle InHandle, TOptional<FText> InDisplayName);

	// FNotifyHook interface
	virtual void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FEditPropertyChain* PropertyThatChanged) override;
	
private:
	friend class SParameterPickerRow;

	FParameterPickerArgs Args;
	
	TSharedPtr<UE::PropertyViewer::SPropertyViewer> PropertyViewer;

	FText FilterText;

	TSharedPtr<SSearchBox> SearchBox;

	TInstancedStruct<FAnimNextParamInstanceIdentifier> SelectedInstanceId;

	TSharedPtr<IPropertyRowGenerator> PropertyRowGenerator;

	TSharedPtr<IStructureDataProvider> InstanceIdProvider;

	struct FFieldIterator : UE::PropertyViewer::IFieldIterator
	{
		FFieldIterator(FOnFilterParameterType InOnFilterParameterType)
			: OnFilterParameterType(InOnFilterParameterType)
		{}
		
		virtual TArray<FFieldVariant> GetFields(const UStruct* InStruct) const override;

		FOnFilterParameterType OnFilterParameterType;
		const UStruct* CurrentStruct = nullptr;
	};

	struct FFieldExpander : UE::PropertyViewer::IFieldExpander
	{
		virtual TOptional<const UClass*> CanExpandObject(const FObjectPropertyBase* Property, const UObject* Instance) const override;
		virtual bool CanExpandScriptStruct(const FStructProperty* StructProperty) const override;
		virtual TOptional<const UStruct*> GetExpandedFunction(const UFunction* Function) const override;
	} FieldExpander;

	TUniquePtr<FFieldIterator> FieldIterator;

	struct FContainerInfo
	{
		FContainerInfo(const FText& InDisplayName, const FText& InTooltipText, const UStruct* InStruct)
			: DisplayName(InDisplayName)
			, TooltipText(InTooltipText)
			, Struct(InStruct)
		{}

		FContainerInfo(const FText& InDisplayName, const FText& InTooltipText, const FAssetData& InAssetData, TUniquePtr<FInstancedPropertyBag>&& InPropertyBag)
			: DisplayName(InDisplayName)
			, TooltipText(InTooltipText)
			, Struct(InPropertyBag.Get()->GetPropertyBagStruct())
			, PropertyBag(MoveTemp(InPropertyBag))
			, AssetData(InAssetData)
		{}

		FText DisplayName;
		FText TooltipText;
		const UStruct* Struct = nullptr;
		TUniquePtr<FInstancedPropertyBag> PropertyBag;
		FAssetData AssetData;
	};
	
	TArray<FContainerInfo> CachedContainers;

	TMap<UE::PropertyViewer::SPropertyViewer::FHandle, int32> ContainerMap;
};

}