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
#include "Editor/SModularRigHierarchyTreeView.h"
#include "StructViewerFilter.h"
#include "StructViewerModule.h"

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
		URigVMDetailsViewWrapperObject* WrapperObject = CastChecked<URigVMDetailsViewWrapperObject>(DetailObject.Get());

		const FString Path = WrapperObject->GetContent<FRigModuleInstance>().GetPath();

		FPerModuleInfo Info;
		Info.WrapperObject = WrapperObject;
		Info.Module = Cast<UModularRig>(WrapperObject->GetSubject())->GetHandle(Path);

		if(!Info.Module.IsValid())
		{
			return;
		}
		if(const UControlRigBlueprint* Blueprint = Info.GetBlueprint())
		{
			if (UModularRig* ModularRig = Cast<UModularRig>(Blueprint->GetObjectBeingDebugged()))
			{
				Info.DefaultModule = ModularRig->GetHandle(Path);
			}
		}

		PerModuleInfos.Add(Info);
	}

	DetailBuilder.HideCategory(TEXT("RigModuleInstance"));

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
		FRigElementKeyRedirector Redirector = GetConnections();
		for(TPair<FRigElementKey, FCachedRigElement>& Connection : Redirector.InternalKeyToExternalKey)
		{
			if (!Connection.Key.IsTypeOf(ERigElementType::Connector))
			{
				continue;
			}
			const FText Label = FText::FromName(Connection.Key.Name);
			const FRigElementKey& Target = Connection.Value.GetKey();
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
					SNew(STextBlock)
					.Text(FText::FromName(Target.Name))
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.IsEnabled(true)
				];
		}
	}

	IDetailCategoryBuilder& ConfigValuesCategory = DetailBuilder.EditCategory(TEXT("Config Values"), LOCTEXT("ConfigValues", "Config Values"));
	{
		TArray<UObject*> DetailObjectsRaw;
		DetailObjectsRaw.Reserve(DetailObjects.Num());
		for (TWeakObjectPtr<UObject> Obj : DetailObjects)
		{
			if (Obj.IsValid())
			{
				DetailObjectsRaw.Add(Obj.Get());
			}
		}
		
		TArray<FRigVMExternalVariable> Variables = GetConfigValues();
		for(FRigVMExternalVariable& Variable : Variables)
		{
			if (!Variable.bIsPublic)
			{
				continue;
			}
			const FText Label = FText::FromName(Variable.Name);

			if (FRigModuleInstance* ModuleInstance = PerModuleInfos[0].GetModule())
			{
				TSoftObjectPtr<UControlRig> Rig = ModuleInstance->Rig;
				if (Rig.IsValid())
				{
					if (FProperty* Property = Rig.Get()->GetClass()->FindPropertyByName(Variable.Name))
					{
						uint8* Container = (uint8*)Rig.Get();

						TSharedPtr<FStructOnScope> StructOnScope = MakeShareable(new FStructOnScope(Rig.Get()->GetClass(), Container));
						IDetailPropertyRow* Row = ConfigValuesCategory.AddExternalObjectProperty({Rig.Get()}, Variable.Name);
						

						Row->DisplayName(FText::FromName(Variable.Name));

						const FSimpleDelegate OnValueChangedDelegate = FSimpleDelegate::CreateSP(this, &FRigModuleInstanceDetails::OnConfigValueChanged, Variable.Name);

						TSharedPtr<IPropertyHandle> Handle = Row->GetPropertyHandle();
						Handle->SetOnPropertyValueChanged(OnValueChangedDelegate);
						Handle->SetOnChildPropertyValueChanged(OnValueChangedDelegate);
						
					}
				}
			}
		}
	}
}

FString FRigModuleInstanceDetails::GetModulePath() const
{
	check(PerModuleInfos.Num() == 1);
	if (FRigModuleInstance* Module = PerModuleInfos[0].GetModule())
	{
		return Module->GetPath();
	}
	return FString();
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

TArray<FRigVMExternalVariable> FRigModuleInstanceDetails::GetConfigValues() const
{
	if(PerModuleInfos.Num() > 1)
	{
		return TArray<FRigVMExternalVariable>();
	}

	if (FRigModuleInstance* Module = PerModuleInfos[0].GetModule())
	{
		if (TSoftObjectPtr<UControlRig> Rig = Module->Rig)
		{
			if (Rig.IsValid())
			{
				return Rig->GetExternalVariables();
			}
		}
	}

	return TArray<FRigVMExternalVariable>();
}

void FRigModuleInstanceDetails::OnConfigValueChanged(const FName InVariableName)
{
	if (FRigModuleInstance* ModuleInstance = PerModuleInfos[0].GetModule())
	{
		TSoftObjectPtr<UControlRig> Rig = ModuleInstance->Rig;
		if (Rig.IsValid())
		{
			FString ValueStr = Rig->GetVariableAsString(InVariableName);
			if (UControlRigBlueprint* Blueprint = RigModuleDetails_GetBlueprintFromRig(PerModuleInfos[0].GetRig()))
			{
				UModularRigController* Controller = Blueprint->GetModularRigController();
				Controller->SetConfigValueInModule(ModuleInstance->GetPath(), InVariableName, ValueStr);
			}
		}
	}
}

TArray<FString> FRigModuleInstanceDetails::GetModulePaths() const
{
	TArray<FString> Paths;
	Algo::Transform(PerModuleInfos, Paths, [](const FPerModuleInfo& Info)
	{
		return Info.GetModule()->GetPath();
	});
	return Paths;
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


#undef LOCTEXT_NAMESPACE
