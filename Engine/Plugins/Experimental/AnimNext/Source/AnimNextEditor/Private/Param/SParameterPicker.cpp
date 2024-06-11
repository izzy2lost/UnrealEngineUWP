// Copyright Epic Games, Inc. All Rights Reserved.

#include "SParameterPicker.h"

#include "AssetDefinitionDefault.h"
#include "UncookedOnlyUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Param/ParamType.h"
#include "DetailLayoutBuilder.h"
#include "EditorUtils.h"
#include "IAnimNextModuleInterface.h"
#include "IDetailTreeNode.h"
#include "IPropertyRowGenerator.h"
#include "IStructureDataProvider.h"
#include "SAddParametersDialog.h"
#include "Param/ParameterPickerArgs.h"
#include "Widgets/Input/SSearchBox.h"
#include "String/ParseTokens.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Framework/Application/SlateApplication.h"
#include "ScopedTransaction.h"
#include "SSimpleButton.h"
#include "UniversalObjectLocatorEditor.h"
#include "AnimNextUncookedOnly/Private/AnimNextUncookedOnlyModule.h"
#include "Component/AnimNextComponent.h"
#include "Editor/PropertyEditor/Private/SSingleProperty.h"
#include "Module/AnimNextModule.h"
#include "Param/AnimNextParam.h"
#include "Param/IParameterSourceType.h"
#include "Param/ParamUtils.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/PropertyViewer/SFieldName.h"
#include "Widgets/PropertyViewer/SPropertyViewer.h"
#include "Param/AnimNextParamUniversalObjectLocator.h"
#include "UniversalObjectLocators/AssetLocatorFragment.h"

#define LOCTEXT_NAMESPACE "SParameterPicker"

namespace UE::AnimNext::Editor
{

// IStructureDataProvider that allows us to display the instanced struct for instance IDs inline
class FInstanceIdProvider : public IStructureDataProvider
{
public:
	explicit FInstanceIdProvider(TInstancedStruct<FAnimNextParamInstanceIdentifier>& InInstanceId)
		: InstanceId(InInstanceId)
	{
	}

	virtual bool IsValid() const override
	{
		return InstanceId.IsValid();
	}
	
	virtual const UStruct* GetBaseStructure() const override
	{
		return InstanceId.GetScriptStruct();
	}

	virtual void GetInstances(TArray<TSharedPtr<FStructOnScope>>& OutInstances, const UStruct* ExpectedBaseStructure) const override
	{
		uint8* Memory = const_cast<uint8*>(InstanceId.GetMemory());
		OutInstances.Add(MakeShared<FStructOnScope>(InstanceId.GetScriptStruct(), Memory));
	}

	virtual bool IsPropertyIndirection() const override
	{
		return false;
	}

	virtual uint8* GetValueBaseAddress(uint8* ParentValueAddress, const UStruct* ExpectedBaseStructure) const override
	{
		if (!ParentValueAddress)
		{
			return nullptr;
		}

		FInstancedStruct& InstancedStruct = *reinterpret_cast<FInstancedStruct*>(ParentValueAddress);
		if (ExpectedBaseStructure && InstancedStruct.GetScriptStruct() && InstancedStruct.GetScriptStruct()->IsChildOf(ExpectedBaseStructure))
		{
			return InstancedStruct.GetMutableMemory();
		}
		
		return nullptr;
	}

	TInstancedStruct<FAnimNextParamInstanceIdentifier>& InstanceId;
};

void SParameterPicker::Construct(const FArguments& InArgs)
{
	Args = InArgs._Args;
	if(Args.Context == nullptr)
	{
		// TODO: This needs to defer to project/schedule/workspace defaults similar to FAnimNextLocatorContext
		Args.Context = UAnimNextComponent::StaticClass()->GetDefaultObject();
	}

	SelectedInstanceId = Args.InstanceId;
	if(!SelectedInstanceId.IsValid())
	{
		SelectedInstanceId = TInstancedStruct<FAnimNextParamUniversalObjectLocator>::Make();
	}

	FieldIterator = MakeUnique<FFieldIterator>(Args.OnFilterParameterType);

	if(Args.OnGetParameterBindings != nullptr)
	{
		Args.OnGetParameterBindings->BindSP(this, &SParameterPicker::HandleGetParameterBindings);
	}

	if(Args.OnSetInstanceId != nullptr)
	{
		Args.OnSetInstanceId->BindSP(this, &SParameterPicker::HandleSetInstanceId);
	}

	if(Args.bFocusSearchWidget)
	{
		RegisterActiveTimer( 0.f, FWidgetActiveTimerDelegate::CreateLambda([this](double InCurrentTime, float InDeltaTime)
		{
			if (SearchBox.IsValid())
			{
				FWidgetPath WidgetToFocusPath;
				FSlateApplication::Get().GeneratePathToWidgetUnchecked(SearchBox.ToSharedRef(), WidgetToFocusPath);
				if(WidgetToFocusPath.IsValid())
				{
					FSlateApplication::Get().SetKeyboardFocus(WidgetToFocusPath, EFocusCause::SetDirectly);
					WidgetToFocusPath.GetWindow()->SetWidgetToFocusOnActivate(SearchBox);
				}
				return EActiveTimerReturnType::Stop;
			}

			return EActiveTimerReturnType::Continue;
		}));
	}

	InstanceIdProvider = MakeShared<FInstanceIdProvider>(SelectedInstanceId);

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FPropertyRowGeneratorArgs PropertyRowGeneratorArgs;
	PropertyRowGeneratorArgs.NotifyHook = this;
	PropertyRowGenerator = PropertyEditorModule.CreatePropertyRowGenerator(PropertyRowGeneratorArgs);
	PropertyRowGenerator->SetStructure(InstanceIdProvider);

	check(PropertyRowGenerator->GetRootTreeNodes().Num() > 0);
	TSharedPtr<IDetailTreeNode> RootNode = PropertyRowGenerator->GetRootTreeNodes()[0];
	TArray<TSharedRef<IDetailTreeNode>> Children;
	RootNode->GetChildren(Children);
	TSharedPtr<IDetailTreeNode> ChildNode = Children[0];
	const FNodeWidgets NodeWidgets = ChildNode->CreateNodeWidgets();
	
	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(400.0f)
		.HeightOverride(400.0f)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(Args.bShowInstanceId ? FMargin(0.0f, 2.0f) : FMargin(0.0f))
			[
				SNew(SHorizontalBox)
				.Visibility(Args.bShowInstanceId ? EVisibility::Visible : EVisibility::Collapsed)
				// TODO: slot for picker to switch instance ID types here
				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					NodeWidgets.ValueWidget.ToSharedRef()
				]
			]
			+SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(2.0f)
			[
				SAssignNew(PropertyViewer, UE::PropertyViewer::SPropertyViewer)
				.OnSelectionChanged(this, &SParameterPicker::HandleFieldPicked)
				.OnGenerateContainer(this, &SParameterPicker::HandleGenerateContainer)
				.FieldIterator(FieldIterator.Get())
				.FieldExpander(&FieldExpander)
				.bShowSearchBox(true)
			]
		]
	];

	RefreshEntries();
}

void SParameterPicker::RefreshEntries()
{
	using namespace UE::UniversalObjectLocator;

	PropertyViewer->RemoveAll();
	CachedContainers.Reset();
	ContainerMap.Reset();

	UncookedOnly::IAnimNextUncookedOnlyModule& Module = FModuleManager::GetModuleChecked<UncookedOnly::IAnimNextUncookedOnlyModule>("AnimNextUncookedOnly");
	const UStruct* Struct = nullptr;
	if(SelectedInstanceId.IsValid() && SelectedInstanceId.Get().IsValid())
	{
		TSharedPtr<UncookedOnly::IParameterSourceType> SourceType = Module.FindParameterSourceType(SelectedInstanceId.GetScriptStruct());
		if(SourceType.IsValid())
		{
			Struct = SourceType->GetStruct(SelectedInstanceId);
		}
	}

	if(Struct == nullptr || Struct == UAnimNextModule::StaticClass())
	{
		// For AnimNext graphs, we add the structs that are exposed via parameters
		TMap<FAssetData, FAnimNextParameterProviderAssetRegistryExports> Exports;
		if(UncookedOnly::FUtils::GetExportedParametersFromAssetRegistry(Exports))
		{
			for(const TPair<FAssetData, FAnimNextParameterProviderAssetRegistryExports>& ExportPair : Exports)
			{
				if(ExportPair.Value.Parameters.Num() > 0)
				{
					// Add a placeholder struct for this asset's properties
					TArray<FPropertyBagPropertyDesc> PropertyDescs;
					PropertyDescs.Reserve(ExportPair.Value.Parameters.Num());
					for(const FAnimNextParameterAssetRegistryExportEntry& ParameterEntry : ExportPair.Value.Parameters)
					{
						if(EnumHasAllFlags(ParameterEntry.GetFlags(), EAnimNextParameterFlags::Declared | EAnimNextParameterFlags::Public) && ParameterEntry.Name != NAME_None)
						{
							FParameterBindingReference ParameterBinding;
							ParameterBinding.Type = ParameterEntry.Type;
							ParameterBinding.Parameter = UncookedOnly::FUtils::GetParameterNameFromQualifiedName(ParameterEntry.Name);
							ParameterBinding.Graph = ExportPair.Key;

							if (!Args.OnFilterParameter.IsBound() || Args.OnFilterParameter.Execute(ParameterBinding) == EFilterParameterResult::Include)
							{
								if (!Args.OnFilterParameterType.IsBound() || Args.OnFilterParameterType.Execute(ParameterEntry.Type) == EFilterParameterResult::Include)
								{
									PropertyDescs.Emplace(ParameterBinding.Parameter, ParameterEntry.Type.GetContainerType(), ParameterEntry.Type.GetValueType(), ParameterEntry.Type.GetValueTypeObject());
								}
							}
						}
					}

					if(PropertyDescs.Num() > 0)
					{
						const FText DisplayName = FText::FromName(ExportPair.Key.AssetName);
						const FText TooltipText = FText::FromString(ExportPair.Key.GetObjectPathString());
						FContainerInfo& ContainerInfo = CachedContainers.Emplace_GetRef(DisplayName, TooltipText, ExportPair.Key, MakeUnique<FInstancedPropertyBag>());
						ContainerInfo.PropertyBag->AddProperties(PropertyDescs);
						UE::PropertyViewer::SPropertyViewer::FHandle Handle = PropertyViewer->AddContainer(ContainerInfo.PropertyBag.Get()->GetPropertyBagStruct(), DisplayName);
						ContainerMap.Add(Handle, CachedContainers.Num() - 1);
					}
				}
			}
		}
	}
	else if(Struct != nullptr)
	{
		FieldIterator->CurrentStruct = Struct;
		CachedContainers.Emplace_GetRef(Struct->GetDisplayNameText(), Struct->GetToolTipText(), Struct);
		if(const UScriptStruct* ScriptStruct = Cast<UScriptStruct>(Struct))
		{
			UE::PropertyViewer::SPropertyViewer::FHandle Handle = PropertyViewer->AddContainer(ScriptStruct);
			ContainerMap.Add(Handle, CachedContainers.Num() - 1);
		}
		else if(const UClass* Class = Cast<UClass>(Struct))
		{
			// Find any native UBlueprintFunctionLibrary classes to extend this class
			TArray<UClass*> Classes;
			GetDerivedClasses(UBlueprintFunctionLibrary::StaticClass(), Classes, true);
			Classes.Add(const_cast<UClass*>(Class));
			for (const UClass* LibraryClass : Classes)
			{
				if(LibraryClass->HasAnyClassFlags(CLASS_Abstract) || !LibraryClass->HasAnyClassFlags(CLASS_Native))
				{
					continue;
				}

				auto PassesFilterChecks = [this](const FProperty* InProperty)
				{
					if(InProperty && Args.OnFilterParameterType.IsBound())
					{
						FAnimNextParamType Type = FParamTypeHandle::FromProperty(InProperty).GetType();
						return Args.OnFilterParameterType.Execute(Type) == EFilterParameterResult::Include;
					}

					return false;
				};

				for (TFieldIterator<UFunction> FieldIt(LibraryClass); FieldIt; ++FieldIt)
				{
					if (FParamUtils::CanUseFunction(*FieldIt, Class))
					{
						if(PassesFilterChecks(FieldIt->GetReturnProperty()))
						{
							CachedContainers.Emplace_GetRef(LibraryClass->GetDisplayNameText(), LibraryClass->GetToolTipText(), LibraryClass);
							UE::PropertyViewer::SPropertyViewer::FHandle Handle = PropertyViewer->AddContainer(LibraryClass);
							ContainerMap.Add(Handle, CachedContainers.Num() - 1);
							break;
						}
					}
				}
			}
		}
	}
}

bool SParameterPicker::GetFieldInfo(UE::PropertyViewer::SPropertyViewer::FHandle InHandle, const FFieldVariant& InField, FName& OutName, TInstancedStruct<FAnimNextParamInstanceIdentifier>& OutInstanceId, FAnimNextParamType& OutType) const
{
	if(const int32* ContainerIndexPtr = ContainerMap.Find(InHandle))
	{
		const FContainerInfo& ContainerInfo = CachedContainers[*ContainerIndexPtr];
		if(UFunction* Function = InField.Get<UFunction>())
		{
			check(Function->GetReturnProperty());
			OutType = FParamTypeHandle::FromProperty(Function->GetReturnProperty()).GetType();
			OutName = *Function->GetPathName();
			OutInstanceId = SelectedInstanceId;
		}
		else if(FProperty* Property = InField.Get<FProperty>())
		{
			OutType = FParamTypeHandle::FromProperty(Property).GetType();
			if(ContainerInfo.PropertyBag.IsValid())
			{
				// Properties from property bags are assumed to use the asset that they come from
				ensure(ContainerInfo.AssetData.IsValid());
				TStringBuilder<256> StringBuilder;
				ContainerInfo.AssetData.AppendObjectPath(StringBuilder);
				StringBuilder.Append(TEXT(":"));
				Property->GetFName().AppendString(StringBuilder);
				OutName = FName(StringBuilder.ToView());
				OutInstanceId = TInstancedStruct<FAnimNextParamUniversalObjectLocator>::Make();
				OutInstanceId.GetMutable<FAnimNextParamUniversalObjectLocator>().Locator.Reset();
				OutInstanceId.GetMutable<FAnimNextParamUniversalObjectLocator>().Locator.AddFragment<FAssetLocatorFragment>(ContainerInfo.AssetData);
			}
			else
			{
				OutName = *Property->GetPathName();
				OutInstanceId = SelectedInstanceId;
			}
		}

		return true;
	}

	return false;
}

void SParameterPicker::HandleGetParameterBindings(TArray<FParameterBindingReference>& OutParameterBindings) const
{
	TArray<UE::PropertyViewer::SPropertyViewer::FSelectedItem> SelectedItems = PropertyViewer->GetSelectedItems();

	for(UE::PropertyViewer::SPropertyViewer::FSelectedItem& SelectedItem : SelectedItems)
	{
		if(SelectedItem.Fields.Num() > 0 && SelectedItem.Fields.Last().Num() > 0)
		{
			if(const int32* ContainerIndexPtr = ContainerMap.Find(SelectedItem.Handle))
			{
				const FContainerInfo& ContainerInfo = CachedContainers[*ContainerIndexPtr];
				FAnimNextParamType Type;
				FName Name;
				TInstancedStruct<FAnimNextParamInstanceIdentifier> InstanceId;
				if(GetFieldInfo(SelectedItem.Handle, SelectedItem.Fields.Last().Last(), Name, InstanceId, Type))
				{
					if(ensure(Type.IsValid() && !Name.IsNone()))
					{
						FParameterBindingReference NewReference;
						NewReference.Parameter = Name;
						NewReference.InstanceId = InstanceId;
						NewReference.Type = Type;
						NewReference.Graph = ContainerInfo.AssetData;
						OutParameterBindings.Emplace(NewReference);
					}
				}
			}
		}
	}
}

void SParameterPicker::HandleSetInstanceId(const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InInstanceId)
{
	SelectedInstanceId = InInstanceId;
	RefreshEntries();
}

void SParameterPicker::HandleFieldPicked(UE::PropertyViewer::SPropertyViewer::FHandle InHandle, TArrayView<const FFieldVariant> InFields, ESelectInfo::Type InSelectionType)
{
	if(InFields.Num() == 1)
	{
		FAnimNextParamType Type;
		FName Name;
		TInstancedStruct<FAnimNextParamInstanceIdentifier> InstanceId;
		if(GetFieldInfo(InHandle, InFields.Last(), Name, InstanceId, Type))
		{
			if(ensure(Type.IsValid() && !Name.IsNone()))
			{
				UncookedOnly::IAnimNextUncookedOnlyModule& Module = FModuleManager::GetModuleChecked<UncookedOnly::IAnimNextUncookedOnlyModule>("AnimNextUncookedOnly");
				if(InstanceId.IsValid() && InstanceId.Get().IsValid())
				{
					TSharedPtr<UncookedOnly::IParameterSourceType> SourceType = Module.FindParameterSourceType(InstanceId.GetScriptStruct());
					if(SourceType.IsValid())
					{
						const UStruct* Struct = SourceType->GetStruct(InstanceId);
						if(Struct && Struct == UAnimNextModule::StaticClass())
						{
							// Invalidate the instance ID if this is an AnimNext graph, as they dont have instances
							InstanceId.Reset();
						}
					}
				}

				FParameterBindingReference Reference(Name, Type, InstanceId);
				Args.OnParameterPicked.ExecuteIfBound(Reference);
			}
		}
	}
}

TSharedRef<SWidget> SParameterPicker::HandleGenerateContainer(UE::PropertyViewer::SPropertyViewer::FHandle InHandle, TOptional<FText> InDisplayName)
{
	if(int32* ContainerIndexPtr = ContainerMap.Find(InHandle))
	{
		if(CachedContainers.IsValidIndex(*ContainerIndexPtr))
		{
			FContainerInfo& ContainerInfo = CachedContainers[*ContainerIndexPtr];

			return SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("ClassIcon.Object"))
			]
			+SHorizontalBox::Slot()
			.Padding(4.0f)
			[
				SNew(STextBlock)
				.Text(ContainerInfo.DisplayName)
				.ToolTipText(ContainerInfo.TooltipText)
			];
		}
	}

	return SNullWidget::NullWidget;
}

void SParameterPicker::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FEditPropertyChain* PropertyThatChanged)
{
	Args.OnInstanceIdChanged.ExecuteIfBound(SelectedInstanceId);
	RefreshEntries();
}

TArray<FFieldVariant> SParameterPicker::FFieldIterator::GetFields(const UStruct* InStruct) const
{
	auto PassesFilterChecks = [this](const FProperty* InProperty)
	{
		if(InProperty && OnFilterParameterType.IsBound())
		{
			FAnimNextParamType Type = FParamTypeHandle::FromProperty(InProperty).GetType();
			return OnFilterParameterType.Execute(Type) == EFilterParameterResult::Include;
		}

		return false;
	};
	
	TArray<FFieldVariant> Result;
	for (TFieldIterator<FProperty> PropertyIt(InStruct, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
	{
		FProperty* Property = *PropertyIt;
		if (FParamUtils::CanUseProperty(Property))
		{
			if(PassesFilterChecks(Property))
			{
				Result.Add(FFieldVariant(Property));
			}
		}
	}
	for (TFieldIterator<UFunction> FunctionIt(InStruct, EFieldIteratorFlags::IncludeSuper); FunctionIt; ++FunctionIt)
	{
		UFunction* Function = *FunctionIt;
		if (FParamUtils::CanUseFunction(Function, CastChecked<UClass>(CurrentStruct)))
		{
			if(PassesFilterChecks(Function->GetReturnProperty()))
			{
				Result.Add(FFieldVariant(Function));
			}
		}
	}
	return Result;
}

TOptional<const UClass*> SParameterPicker::FFieldExpander::CanExpandObject(const FObjectPropertyBase* Property, const UObject* Instance) const
{
	return TOptional<const UClass*>();
}

bool SParameterPicker::FFieldExpander::CanExpandScriptStruct(const FStructProperty* StructProperty) const
{
	return false;
}

TOptional<const UStruct*> SParameterPicker::FFieldExpander::GetExpandedFunction(const UFunction* Function) const
{
	return TOptional<const UStruct*>();
}

}

#undef LOCTEXT_NAMESPACE