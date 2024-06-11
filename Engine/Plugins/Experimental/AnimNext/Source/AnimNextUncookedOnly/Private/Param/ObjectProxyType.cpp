// Copyright Epic Games, Inc. All Rights Reserved.

#include "ObjectProxyType.h"

#include "AnimNextModuleImpl.h"
#include "IUniversalObjectLocatorEditorModule.h"
#include "UniversalObjectLocatorEditor.h"
#include "UniversalObjectLocatorFragmentType.h"
#include "Component/AnimNextComponent.h"
#include "Modules/ModuleManager.h"
#include "Param/AnimNextParamUniversalObjectLocator.h"
#include "Param/ClassProxy.h"
#include "Param/ObjectProxyFactory.h"

#define LOCTEXT_NAMESPACE "ObjectProxyType"

namespace UE::AnimNext::UncookedOnly
{

const UStruct* FObjectProxyType::GetStruct(const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InInstanceId) const
{
	using namespace UE::UniversalObjectLocator;

	const FAnimNextParamUniversalObjectLocator* Locator = InInstanceId.GetPtr<FAnimNextParamUniversalObjectLocator>();
	if(Locator == nullptr)
	{
		return nullptr;
	}

	IUniversalObjectLocatorEditorModule& UolEditorModule = FModuleManager::LoadModuleChecked<IUniversalObjectLocatorEditorModule>("UniversalObjectLocatorEditor");
	const FFragmentType* FragmentType = Locator->Locator.GetLastFragmentType();
	if(FragmentType == nullptr)
	{
		return nullptr;
	}
	
	TSharedPtr<ILocatorFragmentEditor> LocatorEditor = UolEditorModule.FindLocatorEditor(FragmentType->PrimaryEditorType);
	if(!LocatorEditor.IsValid())
	{
		return nullptr;
	}

	// TODO: This context needs to defer to project/schedule/workspace defaults similar to SParameterPicker/FAnimNextLocatorContext
	UObject* Context = UAnimNextComponent::StaticClass()->GetDefaultObject();
	return LocatorEditor->ResolveClass(*Locator->Locator.GetLastFragment(), Context);
}

FText FObjectProxyType::GetDisplayText(const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InInstanceId) const
{
	using namespace UE::UniversalObjectLocator;
	
	const FAnimNextParamUniversalObjectLocator* Locator = InInstanceId.GetPtr<FAnimNextParamUniversalObjectLocator>();
	if(Locator == nullptr)
	{
		return FText::GetEmpty();
	}

	TStringBuilder<256> StringBuilder;
	IUniversalObjectLocatorEditorModule& UolEditorModule = FModuleManager::LoadModuleChecked<IUniversalObjectLocatorEditorModule>("UniversalObjectLocatorEditor");
	Locator->Locator.ForEachFragment([&UolEditorModule, &StringBuilder](int32 FragmentIndex, int32 NumFragments, const FUniversalObjectLocatorFragment& InFragment)
	{
		if(const FFragmentType* FragmentType = InFragment.GetFragmentType())
		{
			TSharedPtr<ILocatorFragmentEditor> LocatorEditor = UolEditorModule.FindLocatorEditor(FragmentType->PrimaryEditorType);
			if(LocatorEditor.IsValid())
			{
				FText Text = LocatorEditor->GetDisplayText(&InFragment);
				if(FragmentIndex != 0)
				{
					StringBuilder.Append(TEXT("."));
				}
				StringBuilder.Append(Text.ToString());
				return true;
			}
		}

		return false;
	});

	return FText::FromStringView(StringBuilder);
}

FText FObjectProxyType::GetTooltipText(const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InInstanceId) const
{
	using namespace UE::UniversalObjectLocator;
	
	const FAnimNextParamUniversalObjectLocator* Locator = InInstanceId.GetPtr<FAnimNextParamUniversalObjectLocator>();
	if(Locator == nullptr)
	{
		return FText::GetEmpty();
	}

	TStringBuilder<256> StringBuilder;
	IUniversalObjectLocatorEditorModule& UolEditorModule = FModuleManager::LoadModuleChecked<IUniversalObjectLocatorEditorModule>("UniversalObjectLocatorEditor");
	bool bSuccess = Locator->Locator.ForEachFragment([&UolEditorModule, &StringBuilder](int32 FragmentIndex, int32 NumFragments, const FUniversalObjectLocatorFragment& InFragment)
	{
		if(const FFragmentType* FragmentType = InFragment.GetFragmentType())
		{
			TSharedPtr<ILocatorFragmentEditor> LocatorEditor = UolEditorModule.FindLocatorEditor(FragmentType->PrimaryEditorType);
			if(LocatorEditor.IsValid())
			{
				FText Text = LocatorEditor->GetDisplayText(&InFragment);
				if(FragmentIndex != 0)
				{
					StringBuilder.Append(TEXT("."));
				}
				StringBuilder.Append(Text.ToString());
				return true;
			}
		}

		return false;
	});

	FTextBuilder TextBuilder;
	if(bSuccess)
	{
		TextBuilder.AppendLine(FText::Format(LOCTEXT("ParameterInstanceTooltipFormat", "Instance: {0}"), FText::FromStringView(StringBuilder)));
	}

	TStringBuilder<256> ScopeStringBuilder;
	Locator->Locator.ToString(ScopeStringBuilder);
	TextBuilder.AppendLine(FText::Format(LOCTEXT("ParameterUOLTooltipFormat", "UOL: {0}"), FText::FromStringView(ScopeStringBuilder)));

	return TextBuilder.ToText();
}

bool FObjectProxyType::FindParameterInfo(const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InInstanceId, TConstArrayView<FName> InParameterNames, TArrayView<FParameterSourceInfo> OutInfo) const
{
	check(InParameterNames.Num() == OutInfo.Num());

	const UClass* Class = Cast<UClass>(GetStruct(InInstanceId));
	if(Class == nullptr)
	{
		return false;
	}

	UE::AnimNext::FAnimNextModuleImpl& AnimNextModule = FModuleManager::GetModuleChecked<UE::AnimNext::FAnimNextModuleImpl>("AnimNext");
	TSharedPtr<FObjectProxyFactory> ObjectProxyFactory = StaticCastSharedPtr<FObjectProxyFactory>(AnimNextModule.FindParameterSourceFactory("ObjectProxy"));
	if(!ObjectProxyFactory.IsValid())
	{
		return false;
	}

	// Extract parameter info if found
	bool bFound = false;
	TSharedRef<FClassProxy> ClassProxy = ObjectProxyFactory->FindOrCreateClassProxy(Class);
	for(int32 ParameterIndex = 0; ParameterIndex < InParameterNames.Num(); ++ParameterIndex)
	{
		FName ParameterName = InParameterNames[ParameterIndex];
		if(const int32* FoundIndexPtr = ClassProxy->ParameterNameMap.Find(ParameterName))
		{
			const FClassProxyParameter& FoundParameter = ClassProxy->Parameters[*FoundIndexPtr];
			FParameterSourceInfo& ParameterInfo = OutInfo[ParameterIndex];
			ParameterInfo.Type = FoundParameter.Type;
			ParameterInfo.DisplayName = FoundParameter.DisplayName;
			ParameterInfo.Tooltip = FoundParameter.Tooltip;
			ParameterInfo.Function = FoundParameter.Function.Get();
			ParameterInfo.Property = FoundParameter.Property.Get();
			ParameterInfo.bThreadSafe = FoundParameter.bThreadSafe;
			bFound = true;
		}
		else
		{
			OutInfo[ParameterIndex] = FParameterSourceInfo();
		}
	}

	return bFound;
}

void FObjectProxyType::ForEachParameter(const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InInstanceId, TFunctionRef<void(FName, const FParameterSourceInfo&)> InFunction) const
{
	using namespace UE::UniversalObjectLocator;

	const UClass* Class = Cast<UClass>(GetStruct(InInstanceId));
	if(Class == nullptr)
	{
		return;
	}

	UE::AnimNext::FAnimNextModuleImpl& AnimNextModule = FModuleManager::GetModuleChecked<UE::AnimNext::FAnimNextModuleImpl>("AnimNext");
	TSharedPtr<FObjectProxyFactory> ObjectProxyFactory = StaticCastSharedPtr<FObjectProxyFactory>(AnimNextModule.FindParameterSourceFactory("ObjectProxy"));
	if(!ObjectProxyFactory.IsValid())
	{
		return;
	}

	// Extract parameter info if found
	TSharedRef<FClassProxy> ClassProxy = ObjectProxyFactory->FindOrCreateClassProxy(Class);
	for(const FClassProxyParameter& Parameter : ClassProxy->Parameters)
	{
		FParameterSourceInfo SourceInfo;
		SourceInfo.Type = Parameter.Type;
		SourceInfo.DisplayName = Parameter.DisplayName;
		SourceInfo.Tooltip = Parameter.Tooltip;
		SourceInfo.Function = Parameter.Function.Get();
		SourceInfo.Property = Parameter.Property.Get();
		SourceInfo.bThreadSafe = Parameter.bThreadSafe;

		InFunction(Parameter.ParameterName, SourceInfo);
	}
}

}

#undef LOCTEXT_NAMESPACE