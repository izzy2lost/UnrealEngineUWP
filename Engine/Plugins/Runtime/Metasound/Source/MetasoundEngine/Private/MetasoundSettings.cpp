// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundSettings.h"

#include "Algo/Count.h"
#include "HAL/IConsoleManager.h"
#include "MetasoundFrontendDocument.h"

#define LOCTEXT_NAMESPACE "MetaSound"


namespace Metasound::SettingsPrivate
{
	FAutoConsoleCommand CVarMetaSoundSetTargetPage(
		TEXT("au.MetaSound.Pages.SetTarget"),
		TEXT("Sets the target page to that with the given name. If name not specified or not found, command is ignored.\n"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (!Args.IsEmpty())
			{
				if (UMetaSoundSettings* Settings = GetMutableDefault<UMetaSoundSettings>())
				{
					Settings->SetTargetPage(FName { *Args.Last() });
				}
			}
		})
	);

#if WITH_EDITOR
	template<typename SettingsStructType>
	TSet<FName> GetStructNames(const TArray<SettingsStructType>& InSettings, int32 IgnoreIndex = INDEX_NONE)
	{
		TSet<FName> Names;
		for (int32 Index = 0; Index < InSettings.Num(); ++Index)
		{
			if (Index != IgnoreIndex)
			{
				Names.Add(InSettings[Index].Name);
			}
		}
		return Names;
	}

	/** Generate new name for the item. **/
	static FName GenerateUniqueName(const TSet<FName>& Names, const TCHAR* InBaseName)
	{
		FString NewName = InBaseName;
		for (int32 Postfix = 1; Names.Contains(*NewName); ++Postfix)
		{
			NewName = FString::Format(TEXT("{0}_{1}"), { InBaseName, Postfix });
		}
		return FName(*NewName);
	}

	template<typename SettingsStructType>
	void OnCreateNewSettingsStruct(const TArray<SettingsStructType>& InSettings, const FString& InBaseName, SettingsStructType& OutNewItem)
	{
		const TSet<FName> Names = GetStructNames(InSettings);
		OutNewItem.Name = GenerateUniqueName(Names, *InBaseName);
		OutNewItem.UniqueId = FGuid::NewGuid();
	}

	template<typename SettingsStructType>
	void OnRenameSettingsStruct(const TArray<SettingsStructType>& InSettings, int32 Index, const FString& InBaseName, SettingsStructType& OutRenamed)
	{
		if (OutRenamed.Name.IsNone())
		{
			const TSet<FName> Names = GetStructNames(InSettings);
			OutRenamed.Name = GenerateUniqueName(Names, *InBaseName);
		}
		else
		{
			const TSet<FName> Names = GetStructNames(InSettings, Index);
			if (Names.Contains(OutRenamed.Name))
			{
				OutRenamed.Name = GenerateUniqueName(Names, *OutRenamed.Name.ToString());
			}
		}
	}
#endif // WITH_EDITOR

	template<typename SettingsStructType>
	const SettingsStructType* FindSettingsStruct(const TArray<SettingsStructType>& Settings, const FGuid& InUniqueID)
	{
		auto MatchesIDPredicate = [&InUniqueID](const SettingsStructType& Struct) { return Struct.UniqueId == InUniqueID; };
		return Settings.FindByPredicate(MatchesIDPredicate);
	}

	template<typename SettingsStructType>
	const SettingsStructType* FindSettingsStruct(const TArray<SettingsStructType>& Settings, FName Name)
	{
		auto MatchesNamePredicate = [Name](const SettingsStructType& Struct) { return Struct.Name == Name; };
		return Settings.FindByPredicate(MatchesNamePredicate);
	}

#if WITH_EDITOR
	template<typename SettingsStructType>
	void PostEditChainChangedStructMember(FPropertyChangedChainEvent& PostEditChangeChainProperty, TArray<SettingsStructType>& StructSettings, FName PropertyName, const FString& NewItemName)
	{
		const int32 ItemIndex = PostEditChangeChainProperty.GetArrayIndex(PropertyName.ToString());

		if (TDoubleLinkedList<FProperty*>::TDoubleLinkedListNode* HeadNode = PostEditChangeChainProperty.PropertyChain.GetHead())
		{
			const FProperty* Prop = HeadNode->GetValue();
			if (Prop->GetName() != PropertyName)
			{
				return;
			}
		}

		// Item changed..
		if (ItemIndex != INDEX_NONE && StructSettings.IsValidIndex(ItemIndex))
		{
			SettingsStructType& Item = StructSettings[ItemIndex];
			if (PostEditChangeChainProperty.GetPropertyName() == "Name")
			{
				OnRenameSettingsStruct<SettingsStructType>(StructSettings, ItemIndex, NewItemName, Item);
			}
			else if (PostEditChangeChainProperty.GetPropertyName() == PropertyName)
			{
				// Array change add or duplicate
				if (PostEditChangeChainProperty.ChangeType == EPropertyChangeType::ArrayAdd
					|| PostEditChangeChainProperty.ChangeType == EPropertyChangeType::Duplicate)
				{
					OnCreateNewSettingsStruct<SettingsStructType>(StructSettings, NewItemName, Item);
				}
			}
		}

		// Handle pasting separately as we might not have a valid index in the case of pasting when array is empty.
		if (PostEditChangeChainProperty.GetPropertyName() == PropertyName)
		{
			// Paste...
			if (PostEditChangeChainProperty.ChangeType == EPropertyChangeType::ValueSet)
			{
				const int32 IndexOfPastedItem = ItemIndex != INDEX_NONE ? ItemIndex : 0;
				if (StructSettings.IsValidIndex(IndexOfPastedItem))
				{
					SettingsStructType& Item = StructSettings[IndexOfPastedItem];
					OnCreateNewSettingsStruct<SettingsStructType>(StructSettings, NewItemName, Item);
				}
			}
		}
	}
#endif // WITH_EDITOR
} // namespace Metasound::SettingsPrivate


#if WITH_EDITOR
void UMetaSoundSettings::ConformPageSettingsDefault(bool bNotifyDefaultConformed)
{
	using namespace Metasound;

	bool bContainsPageDefault = false;
	bool bDefaultConformed = false;
	for (int32 Index = PageSettings.Num() - 1; Index >= 0; --Index)
	{
		FMetaSoundPageSettings& Page = PageSettings[Index];
		const bool bIsDefaultName = Page.Name == Frontend::DefaultPageName;
		if (bIsDefaultName)
		{
			if (Page.UniqueId != Frontend::DefaultPageID)
			{
				Page.UniqueId = { };
				bDefaultConformed = true;
			}

			bContainsPageDefault = true;
		}
		else
		{
			if (Page.UniqueId == Frontend::DefaultPageID)
			{
				Page.UniqueId = FGuid::NewGuid();
				bDefaultConformed = true;
			}
		}
	}

	if (!bContainsPageDefault)
	{
		FMetaSoundPageSettings DefaultSettings;
		DefaultSettings.Name = Frontend::DefaultPageName;
		PageSettings.Insert(MoveTemp(DefaultSettings), 0);
		bDefaultConformed = true;
	}

	if (bNotifyDefaultConformed && bDefaultConformed)
	{
		OnDefaultConformed.Broadcast();
	}
}
#endif // WITH_EDITOR

const FMetaSoundPageSettings* UMetaSoundSettings::FindPageSettings(FName Name) const
{
	return Metasound::SettingsPrivate::FindSettingsStruct(PageSettings, Name);
}

const FMetaSoundPageSettings* UMetaSoundSettings::FindPageSettings(const FGuid& InPageID) const
{
	return Metasound::SettingsPrivate::FindSettingsStruct(PageSettings, InPageID);
}

const FMetaSoundQualitySettings* UMetaSoundSettings::FindQualitySettings(FName Name) const
{
	return Metasound::SettingsPrivate::FindSettingsStruct(QualitySettings, Name);
}

const FMetaSoundQualitySettings* UMetaSoundSettings::FindQualitySettings(const FGuid& InQualityID) const
{
	return Metasound::SettingsPrivate::FindSettingsStruct(QualitySettings, InQualityID);
}

const FGuid& UMetaSoundSettings::GetTargetPageID() const
{
	if (const FMetaSoundPageSettings* TargetSettings = FindPageSettings(TargetPageName))
	{
		return TargetSettings->UniqueId;
	}

	if (PageSettings.IsEmpty())
	{
		return Metasound::Frontend::DefaultPageID;
	}

	return PageSettings.Last().UniqueId;
}

#if WITH_EDITOR
void UMetaSoundSettings::PostEditChangeChainProperty(FPropertyChangedChainEvent& PostEditChangeChainProperty)
{
	using namespace Metasound::SettingsPrivate;

	PostEditChainChangedStructMember(PostEditChangeChainProperty, PageSettings, GetPageSettingPropertyName(), TEXT("New Page"));
	PostEditChainChangedStructMember(PostEditChangeChainProperty, QualitySettings, GetQualitySettingPropertyName(), TEXT("New Quality"));

	constexpr bool bNotifyDefaultConformed = true;
	ConformPageSettingsDefault(bNotifyDefaultConformed);

	Super::PostEditChangeChainProperty(PostEditChangeChainProperty);
}

void UMetaSoundSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{	
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	if (PropertyChangedEvent.MemberProperty->GetName() == GetPageSettingPropertyName())
	{
		OnPageSettingsUpdated.Broadcast();
	}

	DenyListCacheChangeID++;
}

void UMetaSoundSettings::PostInitProperties()
{
	constexpr bool bNotifyDefaultConformed = false;
	ConformPageSettingsDefault(bNotifyDefaultConformed);

	Super::PostInitProperties();
}
#endif // WITH_EDITOR

bool UMetaSoundSettings::SetTargetPage(FName PageName)
{
	if (const FMetaSoundPageSettings* PageSetting = FindPageSettings(PageName))
	{
		if (TargetPageName != PageSetting->Name)
		{
			TargetPageName = PageSetting->Name;
			return true;
		}
	}

	return false;
}

#if WITH_EDITORONLY_DATA
Metasound::Engine::FOnSettingsDefaultConformed& UMetaSoundSettings::GetOnDefaultConformedDelegate()
{
	return OnDefaultConformed;
}

Metasound::Engine::FOnPageSettingsUpdated& UMetaSoundSettings::GetOnPageSettingsUpdatedDelegate()
{
	return OnPageSettingsUpdated;
}

FName UMetaSoundSettings::GetPageSettingPropertyName()
{
	return GET_MEMBER_NAME_CHECKED(UMetaSoundSettings, PageSettings);
}

FName UMetaSoundSettings::GetQualitySettingPropertyName()
{
	return GET_MEMBER_NAME_CHECKED(UMetaSoundSettings, QualitySettings);
}
#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR
TArray<FName> UMetaSoundSettings::GetQualityList()
{
	TArray<FName> Names;

#if WITH_EDITORONLY_DATA
	if (const UMetaSoundSettings* Settings = GetDefault<UMetaSoundSettings>())
	{
		Algo::Transform(Settings->GetQualitySettings(), Names, [](const FMetaSoundQualitySettings& Quality) -> FName
		{
			return Quality.Name;
		});
	}
#endif //WITH_EDITORONLY_DATA

	return Names;
}
#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE // MetaSound
