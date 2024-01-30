// Copyright Epic Games, Inc. All Rights Reserved.

#include "Font/AvaFont.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"

#define LOCTEXT_NAMESPACE "AvaFont"

FAvaFont::FAvaDefaultFontObjects FAvaFont::DefaultFontObjects;

UFont* FAvaFont::GetDefaultFont()
{
	if (IsRunningDedicatedServer())
	{
		return nullptr;
	}

	if (!DefaultFontObjects.AvaDefaultFont)
	{
		const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

		// Trying to load Roboto font
		const FSoftObjectPath RobotoPath(TEXT("/Script/Engine.Font'/Engine/EngineFonts/Roboto.Roboto'"));
		UFont* DefaultFont = Cast<UFont>(AssetRegistryModule.Get().GetAssetByObjectPath(RobotoPath).GetAsset());

		if (!DefaultFont)
		{
			// no Roboto was found, this is not expected. Trying to load an avalanche font
			TArray<FAssetData> AssetDataArray;
			// avalanche fonts path
			const FName Path = TEXT("/Game/SystemFonts/Fonts");
			AssetRegistryModule.Get().GetAssetsByPath(Path, AssetDataArray);

			// no avalanche font available, trying to load the first engine font available
			if (AssetDataArray.IsEmpty())
			{
				const UClass* Class = UFont::StaticClass();
				const FTopLevelAssetPath AssetPath(Class->GetPathName());
				AssetRegistryModule.Get().GetAssetsByClass(AssetPath, AssetDataArray);
			}

			for (const FAssetData& AssetData : AssetDataArray)
			{
				DefaultFont = CastChecked<UFont>(AssetData.GetAsset());

				if (DefaultFont)
				{
					break;
				}
			}
		}

		if (ensureMsgf(DefaultFont, TEXT("AvalancheFont: cannot load any font to be used as default.")))
		{
			DefaultFontObjects.AvaDefaultFont = DefaultFont;
		}
	}

	return DefaultFontObjects.AvaDefaultFont.Get();
}

FString FAvaFont::GenerateAvalancheFontFormattedString(const FString& InFontName, const FString& InFontObjectPathName)
{
	// e.g. (Property1=Value1,Property2=Value2,Property3=Value3,...)

	FString FormattedString = TEXT("(");
	FormattedString += TEXT("CurrentFont=None");
	FormattedString += TEXT(",AvalancheFontObject=/Script/Avalanche.AvalancheFontObject'") + InFontObjectPathName + TEXT("'");
	FormattedString += TEXT(",FontName=\"") + InFontName  + TEXT("\"");
	FormattedString += TEXT(")");

	return FormattedString;
}

bool FAvaFont::GenerateAvalancheFontFormattedString(const UAvaFontObject* InFontObject, FString& OutFormattedString)
{
	if (!InFontObject)
	{
		return false;
	}

	OutFormattedString = GenerateAvalancheFontFormattedString(InFontObject->GetFontName(), InFontObject->GetPathName());
	return true;
}

bool FAvaFont::AreSameFont(const FAvaFont* InFontA, const FAvaFont* InFontB)
{
	if (!InFontA || !InFontB)
	{
		return false;
	}

	return *InFontA == *InFontB;
}

void FAvaFont::InitDefaults()
{
	bIsFavorite = false;
	FontAssetState = EFontAssetState::DefaultFont;
}

FAvaFont::FAvaFont()
{
	InitDefaults();
	SetFontObject(GetDefaultAvaFontObject());
}

FAvaFont::FAvaFont(UAvaFontObject* InFontObject)
{
	InitDefaults();

	if (InFontObject)
	{
		SetFontObject(InFontObject);
	}
	else
	{
		SetFontObject(GetDefaultAvaFontObject());
	}
}

UFont* FAvaFont::GetFont()
{
	// make sure FontAssetState value is up to date
	RefreshAssetState();

	if (FontAssetState == EFontAssetState::SelectedFont)
	{
		EnsureUsingCurrentVersion();

		if (AvalancheFontObject)
		{
			return AvalancheFontObject->GetFont();
		}
	}

	// if this is the default font, or previously referenced resource is not available, return default font
	return GetDefaultFont();
}

EAvaFontSource FAvaFont::GetFontSource() const
{
	if (AvalancheFontObject)
	{
		return AvalancheFontObject->GetSource();
	}

	return EAvaFontSource::Invalid;
}

FName FAvaFont::GetFontName() const
{
	return FName(GetFontNameAsString());
}

FString FAvaFont::GetFontNameAsString() const
{
	if (AvalancheFontObject)
	{
		return AvalancheFontObject->GetFontName();
	}

	return FontName;
}

FText FAvaFont::GetFontNameAsText() const
{
	return FText::FromName(GetFontName());
}

bool FAvaFont::IsFavorite() const
{
	return bIsFavorite;
}

bool FAvaFont::IsDefaultFont() const
{
	if (AvalancheFontObject && AvalancheFontObject->GetFont())
	{
		return AvalancheFontObject->GetFont() == GetDefaultFont();
	}

	return true;
}

bool FAvaFont::IsFallbackFont() const
{
	return FontAssetState == EFontAssetState::FallbackFont;
}

bool FAvaFont::IsMonospaced() const
{
	if (AvalancheFontObject)
	{
		return AvalancheFontObject->IsMonospaced();
	}

	return false;
}

bool FAvaFont::IsBold() const
{
	if (AvalancheFontObject)
	{
		return AvalancheFontObject->IsBold();
	}

	return false;
}

bool FAvaFont::IsItalic() const
{
	if (AvalancheFontObject)
	{
		return AvalancheFontObject->IsItalic();
	}

	return false;
}

void FAvaFont::SetFavorite(const bool bFavorite)
{
	bIsFavorite = bFavorite;
}

void FAvaFont::EnsureUsingCurrentVersion()
{
	if (CurrentFont_DEPRECATED)
	{
		AvalancheFontObject = NewObject<UAvaFontObject>();
		AvalancheFontObject->InitProjectFont(CurrentFont_DEPRECATED, GetFontName().ToString());
		CurrentFont_DEPRECATED = nullptr;

		RefreshName();
	}
}

bool FAvaFont::HasValidFont() const
{
	const FCompositeFont* CompositeFont = nullptr;

	if (CurrentFont_DEPRECATED)
	{
		CompositeFont = CurrentFont_DEPRECATED->GetCompositeFont();
	}
	else if (AvalancheFontObject && AvalancheFontObject->GetFont())
	{
		CompositeFont = AvalancheFontObject->GetFont()->GetCompositeFont();
	}

	if (CompositeFont)
	{
		if (!CompositeFont->DefaultTypeface.Fonts.IsEmpty())
		{
			if (CompositeFont->DefaultTypeface.Fonts[0].Font.GetFontFaceAsset())
			{
				return true;
			}
		}
	}

	return false;
}

void FAvaFont::SetFontObject(UAvaFontObject* InFontObject)
{
	CurrentFont_DEPRECATED = nullptr;
	AvalancheFontObject = InFontObject;

	RefreshName();
	RefreshAssetState();
}

void FAvaFont::InitFromFont(UFont* InFont)
{
	if (InFont)
	{
		FString Name;
		UE::Avalanche::FontUtilities::Public::GetFontName(InFont, Name);

		AvalancheFontObject = NewObject<UAvaFontObject>();
		AvalancheFontObject->InitProjectFont(InFont, Name);

		RefreshName();
	}
}

void FAvaFont::PostSerialize(const FArchive& Ar)
{
	if (Ar.IsLoading())
	{
		RefreshName();

		if ((AvalancheFontObject && AvalancheFontObject->GetFont()) || CurrentFont_DEPRECATED)
		{
			FontAssetState = EFontAssetState::SelectedFont;
		}
		else
		{
			/*
			 * AvalancheFontObject might be invalid due to a previously existing issue.
			 * That would cause the Editor only version of AvalancheFontObject to be serialized, instead of the proper one.
			 * For assets using the font saved like that, the AvalancheFontObject had the wrong Outer, which worked fine in Editor, but not for Game/Runtime.
			 * The following code tries to find a font based on the font name, and then to create a new AvalancheFontObject, with the proper Outer
			 */

			bool bFontRecoverySuccess = false;

			TArray<FProperty*> OutProperties;
			Ar.GetSerializedPropertyChain(OutProperties);

			// this should just contain the FAvaFont
			for (const FProperty* Property : OutProperties)
			{
				// let's get the Outer we need to create the AvalancheFontObject
				if (UObject* const Outer = Property->GetOwner<UObject>())
				{
					// try to get a font just using the name (this might fail!)
					if (UFont* const Font = GetFontByName(FontName))
					{
						AvalancheFontObject = nullptr;

						// we create and assign a new UAvaFontObject with the proper Outer
						AvalancheFontObject = NewObject<UAvaFontObject>(Outer, GetFontName(), RF_Public | RF_Standalone);
						AvalancheFontObject->InitProjectFont(Font, FontName);
						bFontRecoverySuccess = true;
						break;
					}
				}
			}

			// if we managed to retrieve the font, just act as if it was properly loaded
			if (bFontRecoverySuccess)
			{
				FontAssetState = EFontAssetState::SelectedFont;
			}
			else
			{
				MissingFontName = FontName;
				FontAssetState = EFontAssetState::FallbackFont;
			}
		}
	}
}

UAvaFontObject* FAvaFont::GetDefaultAvaFontObject()
{
	if (!DefaultFontObjects.AvaDefaultFontObject)
	{
		if (UFont* DefaultFont = GetDefaultFont())
		{
			DefaultFontObjects.AvaDefaultFontObject = NewObject<UAvaFontObject>(DefaultFont);
			DefaultFontObjects.AvaDefaultFontObject->InitProjectFont(DefaultFont, DefaultFont->GetName());
		}
	}

	return DefaultFontObjects.AvaDefaultFontObject.Get();
}

void FAvaFont::RefreshName()
{
	if (AvalancheFontObject)
	{
		const FString& FontObjectName = AvalancheFontObject->GetFontName();

		if (FontName != FontObjectName)
		{
			FontName = FontObjectName;
		}
	}
}

void FAvaFont::RefreshAssetState()
{
	if (!IsDefaultFont())
	{
		FontAssetState = EFontAssetState::SelectedFont;
	}
}

UFont* FAvaFont::GetFontByName(const FString& InFontName)
{
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	// no Roboto was found, this is not expected. Trying to load an avalanche font
	TArray<FAssetData> AssetDataArray;

	// no avalanche font available, trying to load the first engine font available
	if (AssetDataArray.IsEmpty())
	{
		const UClass* Class = UFont::StaticClass();
		const FTopLevelAssetPath AssetPath(Class->GetPathName());
		AssetRegistryModule.Get().GetAssetsByClass(AssetPath, AssetDataArray);
	}

	for (const FAssetData& AssetData : AssetDataArray)
	{
		if (UFont* const CurrFont = CastChecked<UFont>(AssetData.GetAsset()))
		{
			FString CurrFontName;
			UE::Avalanche::FontUtilities::Public::GetFontName(CurrFont, CurrFontName);

			if (CurrFontName == InFontName)
			{
				return CurrFont;
			}
		}
	}

	return nullptr;
}

// note: this function used to be in the AvalancheEditor module, since it was not needed at Runtime before 
void UE::Avalanche::FontUtilities::Public::GetFontName(const UFont* InFont, FString& OutFontName)
{
	if (IsValid(InFont))
	{
		FString FontAssetName;
		FString FontImportName = InFont->ImportOptions.FontName;

		if (InFont->GetFName() == NAME_None)
		{
			FontAssetName = InFont->LegacyFontName.ToString();
		}
		else
		{
			FontAssetName = InFont->GetName();
		}

		// Roboto fonts are actually from the Arial family and their import name is "Arial", so we try to list them as well
		// this will likely lead to missing spaces in their names
		if (FontAssetName.Contains(TEXT("Roboto")) || FontImportName == TEXT("Arial"))
		{
			OutFontName = FontAssetName;
		}
		else
		{
			OutFontName = FontImportName;
		}
	}
}

#undef LOCTEXT_NAMESPACE
