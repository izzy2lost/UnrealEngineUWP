// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigModuleDetails.h"
#include "Widgets/SWidget.h"
#include "IDetailChildrenBuilder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Input/SVectorInputBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SButton.h"
#include "ControlRigBlueprint.h"
#include "ControlRigElementDetails.h"
#include "Graph/ControlRigGraph.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyEditorModule.h"
#include "SEnumCombo.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Styling/AppStyle.h"
#include "Editor/SModularRigTreeView.h"
#include "StructViewerFilter.h"
#include "StructViewerModule.h"
#include "Features/IModularFeatures.h"
#include "IPropertyAccessEditor.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "ControlRigModuleDetails"

static const FText ControlRigModuleDetailsMultipleValues = LOCTEXT("MultipleValues", "Multiple Values");

static void RigModuleDetails_GetCustomizedInfo(TSharedRef<IPropertyHandle> InStructPropertyHandle, UControlRigBlueprint*& OutBlueprint)
{
	TArray<UObject*> Objects;
	InStructPropertyHandle->GetOuterObjects(Objects);
	for (UObject* Object : Objects)
	{
		if (Object->IsA<UControlRigBlueprint>())
		{
			OutBlueprint = CastChecked<UControlRigBlueprint>(Object);
			break;
		}

		OutBlueprint = Object->GetTypedOuter<UControlRigBlueprint>();
		if(OutBlueprint)
		{
			break;
		}

		if(const UControlRig* ControlRig = Object->GetTypedOuter<UControlRig>())
		{
			OutBlueprint = Cast<UControlRigBlueprint>(ControlRig->GetClass()->ClassGeneratedBy);
			if(OutBlueprint)
			{
				break;
			}
		}
	}

	if (OutBlueprint == nullptr)
	{
		TArray<UPackage*> Packages;
		InStructPropertyHandle->GetOuterPackages(Packages);
		for (UPackage* Package : Packages)
		{
			if (Package == nullptr)
			{
				continue;
			}

			TArray<UObject*> SubObjects;
			Package->GetDefaultSubobjects(SubObjects);
			for (UObject* SubObject : SubObjects)
			{
				if (UControlRig* Rig = Cast<UControlRig>(SubObject))
				{
					UControlRigBlueprint* Blueprint = Cast<UControlRigBlueprint>(Rig->GetClass()->ClassGeneratedBy);
					if (Blueprint)
					{
						if(Blueprint->GetOutermost() == Package)
						{
							OutBlueprint = Blueprint;
							break;
						}
					}
				}
			}

			if (OutBlueprint)
			{
				break;
			}
		}
	}
}

static UControlRigBlueprint* RigModuleDetails_GetBlueprintFromRig(UModularRig* InRig)
{
	if(InRig == nullptr)
	{
		return nullptr;
	}

	UControlRigBlueprint* Blueprint = InRig->GetTypedOuter<UControlRigBlueprint>();
	if(Blueprint == nullptr)
	{
		Blueprint = Cast<UControlRigBlueprint>(InRig->GetClass()->ClassGeneratedBy);
	}
	return Blueprint;
}

void FRigModuleInstanceDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	PerModuleInfos.Reset();

	TArray<TWeakObjectPtr<UObject>> DetailObjects;
	DetailBuilder.GetObjectsBeingCustomized(DetailObjects);
	for(TWeakObjectPtr<UObject> DetailObject : DetailObjects)
	{
		if(UControlRig* ModuleInstance = Cast<UControlRig>(DetailObject))
		{
			if(const UModularRig* ModularRig = Cast<UModularRig>(ModuleInstance->GetOuter()))
			{
				if(const FRigModuleInstance* Module = ModularRig->FindModule(ModuleInstance))
				{
					const FString Path = Module->GetPath();

					FPerModuleInfo Info;
					Info.Path = Path;
					Info.Module = ModularRig->GetHandle(Path);
					if(!Info.Module.IsValid())
					{
						return;
					}
					
					if(const UControlRigBlueprint* Blueprint = Info.GetBlueprint())
					{
						if(const UModularRig* DefaultModularRig = Cast<UModularRig>(Blueprint->GeneratedClass->GetDefaultObject()))
						{
							Info.DefaultModule = DefaultModularRig->GetHandle(Path);
						}
					}

					PerModuleInfos.Add(Info);
				}
			}
		}
	}

	// don't customize if the 
	if(PerModuleInfos.IsEmpty())
	{
		return;
	}

	IDetailCategoryBuilder& GeneralCategory = DetailBuilder.EditCategory(TEXT("General"), LOCTEXT("General", "General"));
	{
		GeneralCategory.AddCustomRow(FText::FromString(TEXT("Name")))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("Name")))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.IsEnabled(true)
		]
		.ValueContent()
		[
			SNew(SInlineEditableTextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(this, &FRigModuleInstanceDetails::GetName)
			.IsEnabled(true)
		];

		GeneralCategory.AddCustomRow(FText::FromString(TEXT("RigClass")))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("RigClass")))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.IsEnabled(true)
		]
		.ValueContent()
		[
			SNew(SInlineEditableTextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(this, &FRigModuleInstanceDetails::GetRigClassPath)
			.IsEnabled(true)
		];
	}

	IDetailCategoryBuilder& ConnectionsCategory = DetailBuilder.EditCategory(TEXT("Connections"), LOCTEXT("Connections", "Connections"));
	{
		TArray<FRigModuleConnector> Connectors = GetConnectors();
		FRigElementKeyRedirector Redirector = GetConnections();
		for(FRigModuleConnector& Connector : Connectors)
		{
			const FText Label = FText::FromString(Connector.Name);
			FRigElementKey ConnectorKey(*Connector.Name, ERigElementType::Connector);
			const FRigElementKey* TargetKey = Redirector.FindExternalKey(ConnectorKey);
			if (TargetKey)
			{
				Connections.Add(ConnectorKey, *TargetKey);
			}
			else
			{
				Connections.Add(ConnectorKey, FRigElementKey());
			}
			ConnectionsCategory.AddCustomRow(Label)
				.NameContent()
				[
					SNew(STextBlock)
					.Text(Label)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.IsEnabled(true)
				]
				.ValueContent()
				[
					SAssignNew(RigElementKeyWidget, SRigElementKeyWidget)
					.Blueprint(PerModuleInfos[0].GetBlueprint())
					.IsEnabled_Lambda([this](){ return true; })
					.ActiveBackgroundColor(FSlateColor(FLinearColor(1.f, 1.f, 1.f, FRigElementKeyDetailsDefs::ActivePinBackgroundAlpha)))
					.ActiveForegroundColor(FSlateColor(FLinearColor(1.f, 1.f, 1.f, FRigElementKeyDetailsDefs::ActivePinForegroundAlpha)))
					.InactiveBackgroundColor(FSlateColor(FLinearColor(1.f, 1.f, 1.f, FRigElementKeyDetailsDefs::InactivePinBackgroundAlpha)))
					.InactiveForegroundColor(FSlateColor(FLinearColor(1.f, 1.f, 1.f, FRigElementKeyDetailsDefs::InactivePinForegroundAlpha)))
					.OnElementNameChanged(this, &FRigModuleInstanceDetails::OnElementNameChanged, ConnectorKey)
					.OnGetSelectedClicked(this, &FRigModuleInstanceDetails::OnGetSelectedClicked, ConnectorKey)
					.OnSelectInHierarchyClicked(this, &FRigModuleInstanceDetails::OnSelectInHierarchyClicked, ConnectorKey)
					.OnGetElementNameAsText_Raw(this, &FRigModuleInstanceDetails::GetElementNameAsText, ConnectorKey)
					.OnGetElementType(this, &FRigModuleInstanceDetails::GetElementType, ConnectorKey)
					.OnElementTypeChanged(this, &FRigModuleInstanceDetails::OnElementTypeChanged, ConnectorKey)
				];
		}
	}

	IDetailCategoryBuilder& ConfigValuesCategory = DetailBuilder.EditCategory(TEXT("Default"), LOCTEXT("ConfigValues", "Config Values"));
	{
		IPropertyAccessEditor& PropertyAccessEditor = IModularFeatures::Get().GetModularFeature<IPropertyAccessEditor>("PropertyAccessEditor");

		TArray<TSharedRef<IPropertyHandle>> DefaultProperties;
		ConfigValuesCategory.GetDefaultProperties(DefaultProperties, true, true);

		for(const TSharedRef<IPropertyHandle>& DefaultProperty : DefaultProperties)
		{
			const FProperty* Property = DefaultProperty->GetProperty();
			if(Property == nullptr)
			{
				DetailBuilder.HideProperty(DefaultProperty);
				continue;
			}

			// skip advanced properties for now
			const bool bAdvancedDisplay = Property->HasAnyPropertyFlags(CPF_AdvancedDisplay);
			if(bAdvancedDisplay)
			{
				DetailBuilder.HideProperty(DefaultProperty);
				continue;
			}

			// skip non-public properties for now
			const bool bIsPublic = Property->HasAnyPropertyFlags(CPF_Edit | CPF_EditConst);
			const bool bIsInstanceEditable = !Property->HasAnyPropertyFlags(CPF_DisableEditOnInstance);
			if(!bIsPublic || !bIsInstanceEditable)
			{
				DetailBuilder.HideProperty(DefaultProperty);
				continue;
			}

			const FSimpleDelegate OnValueChangedDelegate = FSimpleDelegate::CreateSP(this, &FRigModuleInstanceDetails::OnConfigValueChanged, Property->GetFName());
			DefaultProperty->SetOnPropertyValueChanged(OnValueChangedDelegate);
			DefaultProperty->SetOnChildPropertyValueChanged(OnValueChangedDelegate);

			FPropertyBindingWidgetArgs BindingArgs;
			BindingArgs.Property = (FProperty*)Property;
			BindingArgs.CurrentBindingText = TAttribute<FText>::CreateLambda([this, Property]()
			{
				return GetBindingText(Property);
			});
			BindingArgs.CurrentBindingImage = TAttribute<const FSlateBrush*>::CreateLambda([this, Property]()
			{
				return GetBindingImage(Property);
			});
			BindingArgs.CurrentBindingColor = TAttribute<FLinearColor>::CreateLambda([this, Property]()
			{
				return GetBindingColor(Property);
			});

			BindingArgs.OnCanBindProperty.BindLambda([](const FProperty* InProperty) -> bool { return true; });
			BindingArgs.OnCanBindToClass.BindLambda([](UClass* InClass) -> bool { return false; });
			BindingArgs.OnCanRemoveBinding.BindRaw(this, &FRigModuleInstanceDetails::CanRemoveBinding);
			BindingArgs.OnRemoveBinding.BindSP(this, &FRigModuleInstanceDetails::HandleRemoveBinding);

			BindingArgs.bGeneratePureBindings = true;
			BindingArgs.bAllowNewBindings = true;
			BindingArgs.bAllowArrayElementBindings = false;
			BindingArgs.bAllowStructMemberBindings = false;
			BindingArgs.bAllowUObjectFunctions = false;

			BindingArgs.MenuExtender = MakeShareable(new FExtender);
			BindingArgs.MenuExtender->AddMenuExtension(
				"Properties",
				EExtensionHook::After,
				nullptr,
				FMenuExtensionDelegate::CreateSPLambda(this, [this, Property](FMenuBuilder& MenuBuilder)
				{
					FillBindingMenu(MenuBuilder, Property);
				})
			);

			// todo: remove the original row.

			ConfigValuesCategory.AddCustomRow(DefaultProperty->GetPropertyDisplayName())
			.NameContent()
			[
				DefaultProperty->CreatePropertyNameWidget()
			]

			// note: this doesn't work for some reason. seeking help from the editor team
			.ValueContent()
			[
				DefaultProperty->CreatePropertyValueWidget()
				// todo: if the property is bound / or partially bound
				// mark the property value widget as disabled / read only.
			]
			
			.ExtensionContent()
			[
				PropertyAccessEditor.MakePropertyBindingWidget(nullptr, BindingArgs)
			];
		}
	}
}

FText FRigModuleInstanceDetails::GetName() const
{
	if(PerModuleInfos.Num() > 1)
	{
		bool bSame = true;
		for (int32 i=1; i<PerModuleInfos.Num(); ++i)
		{
			if (PerModuleInfos[i].GetModule()->Name !=  PerModuleInfos[0].GetModule()->Name)
			{
				bSame = false;
				break;
			}
		}
		if (!bSame)
		{
			return ControlRigModuleDetailsMultipleValues;
		}
	}
	return FText::FromName(PerModuleInfos[0].GetModule()->Name);
}

FText FRigModuleInstanceDetails::GetRigClassPath() const
{
	if(PerModuleInfos.Num() > 1)
	{
		bool bSame = true;
		for (int32 i=1; i<PerModuleInfos.Num(); ++i)
		{
			if (PerModuleInfos[i].GetModule()->Rig->GetClass() !=  PerModuleInfos[0].GetModule()->Rig->GetClass())
			{
				bSame = false;
				break;
			}
		}
		if (!bSame)
		{
			return ControlRigModuleDetailsMultipleValues;
		}
	}

	if (FRigModuleInstance* Module = PerModuleInfos[0].GetModule())
	{
		if (TSoftObjectPtr<UControlRig> Rig = Module->Rig)
		{
			if (Rig.IsValid())
			{
				return FText::FromString(Module->Rig->GetClass()->GetClassPathName().ToString());
			}
		}
	}

	return FText();
}

TArray<FRigModuleConnector> FRigModuleInstanceDetails::GetConnectors() const
{
	if(PerModuleInfos.Num() > 1)
	{
		return TArray<FRigModuleConnector>();
	}

	if (FRigModuleInstance* Module = PerModuleInfos[0].GetModule())
	{
		if (TSoftObjectPtr<UControlRig> Rig = Module->Rig)
		{
			if (Rig.IsValid())
			{
				return Rig->GetRigModuleSettings().ExposedConnectors;
			}
		}
	}

	return TArray<FRigModuleConnector>();
}

FRigElementKeyRedirector FRigModuleInstanceDetails::GetConnections() const
{
	if(PerModuleInfos.Num() > 1)
	{
		return FRigElementKeyRedirector();
	}

	if (FRigModuleInstance* Module = PerModuleInfos[0].GetModule())
	{
		if (TSoftObjectPtr<UControlRig> Rig = Module->Rig)
		{
			if (Rig.IsValid())
			{
				return Rig->GetElementKeyRedirector();
			}
		}
	}

	return FRigElementKeyRedirector();
}

void FRigModuleInstanceDetails::OnConfigValueChanged(const FName InVariableName)
{
	for(const FPerModuleInfo& Info : PerModuleInfos)
	{
		if (const FRigModuleInstance* ModuleInstance = Info.GetModule())
		{
			TSoftObjectPtr<UControlRig> Rig = ModuleInstance->Rig;
			if (Rig.IsValid())
			{
				FString ValueStr = Rig->GetVariableAsString(InVariableName);
				if (UControlRigBlueprint* Blueprint = Info.GetBlueprint())
				{
					UModularRigController* Controller = Blueprint->GetModularRigController();
					Controller->SetConfigValueInModule(ModuleInstance->GetPath(), InVariableName, ValueStr);
				}
			}
		}
	}
}

const FRigModuleInstanceDetails::FPerModuleInfo& FRigModuleInstanceDetails::FindModule(const FString& InPath) const
{
	const FPerModuleInfo* Info = FindModuleByPredicate([InPath](const FPerModuleInfo& Info)
	{
		return Info.GetModule()->GetPath() == InPath;
	});

	if(Info)
	{
		return *Info;
	}

	static const FPerModuleInfo EmptyInfo;
	return EmptyInfo;
}

const FRigModuleInstanceDetails::FPerModuleInfo* FRigModuleInstanceDetails::FindModuleByPredicate(const TFunction<bool(const FPerModuleInfo&)>& InPredicate) const
{
	return PerModuleInfos.FindByPredicate(InPredicate);
}

bool FRigModuleInstanceDetails::ContainsModuleByPredicate(const TFunction<bool(const FPerModuleInfo&)>& InPredicate) const
{
	return PerModuleInfos.ContainsByPredicate(InPredicate);
}

void FRigModuleInstanceDetails::RegisterSectionMappings(FPropertyEditorModule& PropertyEditorModule, UClass* InClass)
{
	TSharedRef<FPropertySection> MetadataSection = PropertyEditorModule.FindOrCreateSection(InClass->GetFName(), "Metadata", LOCTEXT("Metadata", "Metadata"));
	MetadataSection->AddCategory("Metadata");
}

void FRigModuleInstanceDetails::OnElementNameChanged(TSharedPtr<FString> InItem, ESelectInfo::Type InSelectionInfo, FRigElementKey Connector)
{
	for (FPerModuleInfo& Info : PerModuleInfos)
	{
		if (UControlRigBlueprint* Blueprint = RigModuleDetails_GetBlueprintFromRig(Info.GetModularRig()))
		{
			UModularRigController* Controller = Blueprint->GetModularRigController();
			FRigElementKey Key;
			FRigElementKey* TargetKey = Connections.Find(Connector);
			if (TargetKey)
			{
				if (InItem)
				{
					TargetKey->Name = **InItem;
				}

				FRigElementKey NamespacedConnector(*FString::Printf(TEXT("%s:%s"), *Info.GetModule()->GetPath(), *Connector.Name.ToString()), ERigElementType::Connector);
				Controller->ConnectConnectorToElement(NamespacedConnector, *TargetKey);
			}
		}
	}
}

void FRigModuleInstanceDetails::OnElementTypeChanged(ERigElementType InElementType, FRigElementKey Connector)
{
	for (FPerModuleInfo& Info : PerModuleInfos)
	{
		if (UControlRigBlueprint* Blueprint = RigModuleDetails_GetBlueprintFromRig(Info.GetModularRig()))
		{
			UModularRigController* Controller = Blueprint->GetModularRigController();
			FRigElementKey Key;
			FRigElementKey* TargetKey = Connections.Find(Connector);
			if (TargetKey)
			{
				TargetKey->Type = InElementType;

				FRigElementKey NamespacedConnector(*FString::Printf(TEXT("%s:%s"), *Info.GetModule()->GetPath(), *Connector.Name.ToString()), ERigElementType::Connector);
				Controller->ConnectConnectorToElement(NamespacedConnector, *TargetKey);
			}
		}
	}
}

FText FRigModuleInstanceDetails::GetElementNameAsText(FRigElementKey Connector) const
{
	if (const FRigElementKey* TargetKey = Connections.Find(Connector))
	{
		return FText::FromName(TargetKey->Name);
	}
	return FText();
}

FReply FRigModuleInstanceDetails::OnGetSelectedClicked(FRigElementKey Connector)
{
	if (UControlRigBlueprint* Blueprint = RigModuleDetails_GetBlueprintFromRig(PerModuleInfos[0].GetModularRig()))
	{
		const TArray<FRigElementKey>& Selected = Blueprint->Hierarchy->GetSelectedKeys();
		if (Selected.Num() > 0)
		{
			FRigElementKey NamespacedConnector(*FString::Printf(TEXT("%s:%s"), *PerModuleInfos[0].GetModule()->GetPath(), *Connector.Name.ToString()), ERigElementType::Connector);
			Blueprint->GetModularRigController()->ConnectConnectorToElement(NamespacedConnector, Selected[0]);
		}
	}
	return FReply::Handled();
}

FReply FRigModuleInstanceDetails::OnSelectInHierarchyClicked(FRigElementKey Connector)
{
	if (UControlRigBlueprint* Blueprint = RigModuleDetails_GetBlueprintFromRig(PerModuleInfos[0].GetModularRig()))
	{
		if (FRigElementKey* Target = Connections.Find(Connector))
		{
			if (Target->IsValid())
			{
				Blueprint->GetHierarchyController()->SetSelection({*Target});
			}
		}
	}
	return FReply::Handled();
}

ERigElementType FRigModuleInstanceDetails::GetElementType(FRigElementKey Connector) const
{
	if (const FRigElementKey* TargetKey = Connections.Find(Connector))
	{
		return TargetKey->Type;
	}
	return ERigElementType::None;
}

FText FRigModuleInstanceDetails::GetBindingText(const FProperty* InProperty) const
{
	const FName VariableName = InProperty->GetFName();
	FText FirstValue;
	for(const FPerModuleInfo& Info : PerModuleInfos)
	{
		if (const FRigModuleReference* ModuleReference = Info.GetReference())
		{
			if(ModuleReference->Bindings.Contains(VariableName))
			{
				const FText BindingText = FText::FromString(ModuleReference->Bindings.FindChecked(VariableName));
				if(FirstValue.IsEmpty())
				{
					FirstValue = BindingText;
				}
				else if(!FirstValue.EqualTo(BindingText))
				{
					return ControlRigModuleDetailsMultipleValues;
				}
			}
		}
	}
	return FirstValue;
}

const FSlateBrush* FRigModuleInstanceDetails::GetBindingImage(const FProperty* InProperty) const
{
	static FName TypeIcon(TEXT("Kismet.VariableList.TypeIcon"));
	static FName ArrayTypeIcon(TEXT("Kismet.VariableList.ArrayTypeIcon"));

	if(CastField<FArrayProperty>(InProperty))
	{
		return FAppStyle::GetBrush(ArrayTypeIcon);
	}
	return FAppStyle::GetBrush(TypeIcon);
}

FLinearColor FRigModuleInstanceDetails::GetBindingColor(const FProperty* InProperty) const
{
	if(InProperty)
	{
		FEdGraphPinType PinType;
		const UEdGraphSchema_K2* Schema_K2 = GetDefault<UEdGraphSchema_K2>();
		if (Schema_K2->ConvertPropertyToPinType(InProperty, PinType))
		{
			const URigVMEdGraphSchema* Schema = GetDefault<URigVMEdGraphSchema>();
			return Schema->GetPinTypeColor(PinType);
		}
	}
	return FLinearColor::White;
}

void FRigModuleInstanceDetails::FillBindingMenu(FMenuBuilder& MenuBuilder, const FProperty* InProperty) const
{
	if(PerModuleInfos.IsEmpty())
	{
		return;
	}

	UControlRigBlueprint* Blueprint = PerModuleInfos[0].GetBlueprint();
	UModularRigController* Controller = Blueprint->GetModularRigController();

	TArray<FString> CombinedBindings;
	for(int32 Index = 0; Index < PerModuleInfos.Num(); Index++)
	{
		const FPerModuleInfo& Info  = PerModuleInfos[Index];
		const TArray<FString> Bindings = Controller->GetPossibleBindings(Info.GetPath(), InProperty->GetFName());
		if(Index == 0)
		{
			CombinedBindings = Bindings;
		}
		else
		{
			// reduce the set of bindings to the overall possible bindings
			CombinedBindings.RemoveAll([Bindings](const FString& Binding)
			{
				return !Bindings.Contains(Binding);
			});
		}
	}

	if(CombinedBindings.IsEmpty())
	{
		MenuBuilder.AddMenuEntry(
			FUIAction(FExecuteAction()),
			SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("NoBindingAvailable", "No bindings available for this property."))
					.ColorAndOpacity(FLinearColor::White)
				]
			);
		return;
	}

	// sort lexically
	CombinedBindings.Sort();

	// create a map of all of the variables per menu prefix (the module path the variables belong to)
	struct FPerMenuData
	{
		FString Name;
		FString ParentMenuPath;
		TArray<FString> SubMenuPaths;
		TArray<FString> Variables;

		static void SetupMenu(
			TSharedRef<FRigModuleInstanceDetails const> ThisDetails,
			const FProperty* InProperty,
			FMenuBuilder& InMenuBuilder,
			const FString& InMenuPath,
			TSharedRef<TMap<FString, FPerMenuData>> PerMenuData)
		{
			FPerMenuData& Data = PerMenuData->FindChecked((InMenuPath));

			Data.SubMenuPaths.Sort();
			Data.Variables.Sort();

			for(const FString& VariablePath : Data.Variables)
			{
				FString VariableName = VariablePath;
				(void)VariablePath.Split(UModularRig::NamespaceSeparator, nullptr, &VariableName, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
				
				InMenuBuilder.AddMenuEntry(
					FUIAction(FExecuteAction::CreateLambda([ThisDetails, InProperty, VariablePath]()
					{
						ThisDetails->HandleChangeBinding(InProperty, VariablePath);
					})),
					SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(1.0f, 0.0f)
						[
							SNew(SImage)
							.Image(ThisDetails->GetBindingImage(InProperty))
							.ColorAndOpacity(ThisDetails->GetBindingColor(InProperty))
						]
						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(4.0f, 0.0f)
						[
							SNew(STextBlock)
							.Text(FText::FromString(VariableName))
							.ColorAndOpacity(FLinearColor::White)
						]
					);
			}

			for(const FString& SubMenuPath : Data.SubMenuPaths)
			{
				const FPerMenuData& SubMenuData = PerMenuData->FindChecked(SubMenuPath);

				const FText Label = FText::FromString(SubMenuData.Name);
				static const FText TooltipFormat = LOCTEXT("BindingMenuTooltipFormat", "Access to all variables of the {0} module");
				const FText Tooltip = FText::Format(TooltipFormat, Label);  
				InMenuBuilder.AddSubMenu(Label, Tooltip, FNewMenuDelegate::CreateLambda([ThisDetails, InProperty, SubMenuPath, PerMenuData](FMenuBuilder& SubMenuBuilder)
				{
					SetupMenu(ThisDetails, InProperty, SubMenuBuilder, SubMenuPath, PerMenuData);
				}));
			}
		}
	};
	
	// define the root menu
	const TSharedRef<TMap<FString, FPerMenuData>> PerMenuData = MakeShared<TMap<FString, FPerMenuData>>();
	PerMenuData->FindOrAdd(FString());

	// make sure all levels of the menu are known and we have the variables available
	for(const FString& BindingPath : CombinedBindings)
	{
		FString MenuPath;
		(void)BindingPath.Split(UModularRig::NamespaceSeparator, &MenuPath, nullptr, ESearchCase::CaseSensitive, ESearchDir::FromEnd);

		FString PreviousMenuPath = MenuPath;
		FString ParentMenuPath = MenuPath, RemainingPath;
		while(ParentMenuPath.Split(UModularRig::NamespaceSeparator, &ParentMenuPath, &RemainingPath, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
		{
			// scope since the map may change at the end of this block
			{
				FPerMenuData& Data = PerMenuData->FindOrAdd(MenuPath);
				if(Data.Name.IsEmpty())
				{
					Data.Name = RemainingPath;
				}
			}
			
			PerMenuData->FindOrAdd(ParentMenuPath).SubMenuPaths.AddUnique(PreviousMenuPath);
			PerMenuData->FindOrAdd(PreviousMenuPath).ParentMenuPath = ParentMenuPath;
			PerMenuData->FindOrAdd(PreviousMenuPath).Name = RemainingPath;
			if(!ParentMenuPath.Contains(UModularRig::NamespaceSeparator))
			{
				PerMenuData->FindOrAdd(FString()).SubMenuPaths.AddUnique(ParentMenuPath);
				PerMenuData->FindOrAdd(ParentMenuPath).Name = ParentMenuPath;
			}
			PreviousMenuPath = ParentMenuPath;
		}

		FPerMenuData& Data = PerMenuData->FindOrAdd(MenuPath);
		if(Data.Name.IsEmpty())
		{
			Data.Name = MenuPath;
		}

		Data.Variables.Add(BindingPath);
		if(!MenuPath.IsEmpty())
		{
			PerMenuData->FindChecked(Data.ParentMenuPath).SubMenuPaths.AddUnique(MenuPath);
		}
	}

	// build the menu
	FPerMenuData::SetupMenu(SharedThis(this), InProperty, MenuBuilder, FString(), PerMenuData);
}

bool FRigModuleInstanceDetails::CanRemoveBinding(FName InPropertyName) const
{
	// offer the "removing binding" button if any of the selected module instances
	// has a binding for the given variable
	for(const FPerModuleInfo& Info : PerModuleInfos)
	{
		if (const FRigModuleInstance* ModuleInstance = Info.GetModule())
		{
			if(ModuleInstance->VariableBindings.Contains(InPropertyName))
			{
				return true;
			}
		}
	}
	return false; 
}

void FRigModuleInstanceDetails::HandleRemoveBinding(FName InPropertyName) const
{
	FScopedTransaction Transaction(LOCTEXT("RemoveModuleVariableTransaction", "Remove Binding"));
	for(const FPerModuleInfo& Info : PerModuleInfos)
	{
		if (UControlRigBlueprint* Blueprint = Info.GetBlueprint())
		{
			if (const FRigModuleInstance* ModuleInstance = Info.GetModule())
			{
				UModularRigController* Controller = Blueprint->GetModularRigController();
				Controller->UnBindModuleVariable(ModuleInstance->GetPath(), InPropertyName);
			}
		}
	}
}

void FRigModuleInstanceDetails::HandleChangeBinding(const FProperty* InProperty, const FString& InNewVariablePath) const
{
	FScopedTransaction Transaction(LOCTEXT("BindModuleVariableTransaction", "Bind Module Variable"));
	for(const FPerModuleInfo& Info : PerModuleInfos)
	{
		if (UControlRigBlueprint* Blueprint = Info.GetBlueprint())
		{
			if (const FRigModuleInstance* ModuleInstance = Info.GetModule())
			{
				UModularRigController* Controller = Blueprint->GetModularRigController();
				Controller->BindModuleVariable(ModuleInstance->GetPath(), InProperty->GetFName(), InNewVariablePath);
			}
		}
	}
}


#undef LOCTEXT_NAMESPACE
