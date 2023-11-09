// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "IPropertyTypeCustomization.h"
#include "ControlRig.h"
#include "ModularRig.h"
#include "ControlRigBlueprint.h"
#include "Editor/ControlRigWrapperObject.h"
#include "Styling/SlateTypes.h"
#include "IPropertyUtilities.h"
#include "SSearchableComboBox.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Internationalization/FastDecimalFormat.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "Algo/Transform.h"

class IPropertyHandle;

class FRigModuleInstanceDetails : public IDetailCustomization
{
public:

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	virtual void BeginDestroy() {};

	// Makes a new instance of this detail layout class for a specific detail view requesting it
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShareable(new FRigModuleInstanceDetails);
	}

	FString GetModulePath() const;
	FText GetName() const;
	FText GetRigClassPath() const;
	FRigElementKeyRedirector GetConnections() const;
	TArray<FRigVMExternalVariable> GetConfigValues() const;

	void OnConfigValueChanged(const FName InVariableName);
	
	TArray<FString> GetModulePaths() const;

	TArray<FRigModuleInstance> GetModulesInDetailsView(const TArray<FString>& InFilter = TArray<FString>()) const
	{
		TArray<FRigModuleInstance> Elements;
		for(const FPerModuleInfo& Info : PerModuleInfos)
		{
			FRigModuleInstance Content = Info.WrapperObject->GetContent<FRigModuleInstance>();
			if(!InFilter.IsEmpty() && !InFilter.Contains(Content.GetPath()))
			{
				continue;
			}
			Elements.Add(Content);
		}
		return Elements;
	}

	struct FPerModuleInfo
	{
		FPerModuleInfo()
			: WrapperObject()
			, Module()
			, DefaultModule()
		{}

		bool IsValid() const { return Module.IsValid(); }
		operator bool() const { return IsValid(); }

		UModularRig* GetRig() const { return (UModularRig*)Module.GetRig(); }
		UModularRig* GetDefaultRig() const
		{
			if(DefaultModule.IsValid())
			{
				return (UModularRig*)DefaultModule.GetRig();
			}
			return GetRig();
		}

		UControlRigBlueprint* GetBlueprint() const
		{
			if(const UModularRig* ControlRig = GetRig()->GetTypedOuter<UModularRig>())
			{
				return Cast<UControlRigBlueprint>(ControlRig->GetClass()->ClassGeneratedBy);
			}
			return GetDefaultRig()->GetTypedOuter<UControlRigBlueprint>();
		}

		FRigModuleInstance* GetModule() const
		{
			return (FRigModuleInstance*)Module.Get();
		}

		FRigModuleInstance* GetDefaultElement() const
		{
			if(DefaultModule)
			{
				return (FRigModuleInstance*)DefaultModule.Get();
			}
			return GetModule();
		}

		TWeakObjectPtr<URigVMDetailsViewWrapperObject> WrapperObject;
		FModuleInstanceHandle Module;
		FModuleInstanceHandle DefaultModule;
	};

	const FPerModuleInfo& FindModule(const FString& InKey) const;
	const FPerModuleInfo* FindModuleByPredicate(const TFunction<bool(const FPerModuleInfo&)>& InPredicate) const;
	bool ContainsModuleByPredicate(const TFunction<bool(const FPerModuleInfo&)>& InPredicate) const;

	virtual void RegisterSectionMappings(FPropertyEditorModule& PropertyEditorModule, UClass* InClass);

protected:

	TArray<FPerModuleInfo> PerModuleInfos;
};
