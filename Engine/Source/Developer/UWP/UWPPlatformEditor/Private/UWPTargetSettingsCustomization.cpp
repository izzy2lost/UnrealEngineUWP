#include "UWPPlatformEditorPrivatePCH.h"

#include "UWPTargetSettingsCustomization.h"
#include "SExternalImageReference.h"
#include "IExternalImagePickerModule.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "PropertyHandle.h"
#include "EditorDirectories.h"
#include "SFilePathPicker.h"
#include "STextComboBox.h"
#include "ISourceControlModule.h"
#include "SNotificationList.h"
#include "NotificationManager.h"
#include "AllowWindowsPlatformTypes.h"
#include "Wincrypt.h"

#define LOCTEXT_NAMESPACE "UWPTargetSettingsCustomization"

TSharedRef<IDetailCustomization> FUWPTargetSettingsCustomization::MakeInstance()
{
	return MakeShareable(new FUWPTargetSettingsCustomization);
}

void FUWPTargetSettingsCustomization::InitSupportedPlatformVersions()
{
	PlatformVersionOptions.Empty();

	// Windows 10 RTM
	PlatformVersionOptions.Add(MakeShareable(new FString("10.0.10240.0")));
	// Windows 10 November 2015 update
	PlatformVersionOptions.Add(MakeShareable(new FString("10.0.10586.0")));
}

void FUWPTargetSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	InitSupportedPlatformVersions();

	TSharedRef<IPropertyHandle> MinVersionProperty = DetailBuilder.GetProperty("MinimumPlatformVersion");
	AddWidgetForPlatformVersion(DetailBuilder, MinVersionProperty);
	TSharedRef<IPropertyHandle> MaxVersionProperty = DetailBuilder.GetProperty("MaximumPlatformVersionTested");
	AddWidgetForPlatformVersion(DetailBuilder, MaxVersionProperty);

	// Not yet implemented - we're just forcing SM5 right now.
	//@todo: if/when we add support we should reuse code from WindowsTargetSettingsDetails (which means it needs to be broken out into an accessible location).
	TSharedRef<IPropertyHandle> RHIPropertyHandle = DetailBuilder.GetProperty("TargetedRHIs");
	DetailBuilder.HideProperty(RHIPropertyHandle);

	// Add UI to select signing certificate
	TSharedRef<IPropertyHandle> SigningProperty = DetailBuilder.GetProperty("SigningCertificate");
	IDetailCategoryBuilder& PackagingCategoryBuilder = DetailBuilder.EditCategory(FName(*SigningProperty->GetMetaData("Category")));
	DetailBuilder.HideProperty(SigningProperty);

	FString DefaultSigningSubPath = FString::Printf(TEXT("Build\\UWP\\%s.pfx"), *SigningProperty->GetPropertyDisplayName().ToString());
	FString SubPath;
	if (SigningProperty->GetValue(SubPath) == FPropertyAccess::Fail)
	{
		SubPath = DefaultSigningSubPath;
		SigningProperty->SetValue(SubPath);
	}
	if (SubPath.IsEmpty())
	{
		SubPath = DefaultSigningSubPath;
		SigningProperty->SetValue(SubPath);
	}

	FString ProjectPath = FPaths::GameDir() / SubPath;

	PackagingCategoryBuilder.AddCustomRow(SigningProperty->GetPropertyDisplayName())
	.NameContent()
	[
		SigningProperty->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MaxDesiredWidth(500.0f)
	.MinDesiredWidth(100.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SFilePathPicker)
			.BrowseButtonImage(FEditorStyle::GetBrush("PropertyWindow.Button_Ellipsis"))
			.BrowseButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
			.BrowseDirectory(FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_OPEN))
			.FileTypeFilter(TEXT("pfx files (*.pfx)|*.pfx"))
			.IsReadOnly(true)
			.OnPathPicked(FOnPathPicked::CreateLambda([ProjectPath](const FString &PickedPath) { OnCertificatePicked(PickedPath, ProjectPath); }))
			.FilePath(TAttribute<FString>::Create(TAttribute<FString>::FGetter::CreateLambda([ProjectPath]() { return GetNameForSigningCertificate(ProjectPath); })))
		]
	];

	// Add the packaging images customization
	AddWidgetForResourceImage(DetailBuilder, DetailBuilder.GetProperty("Logo"), FVector2D(150.0f, 150.0f));
	AddWidgetForResourceImage(DetailBuilder, DetailBuilder.GetProperty("SmallLogo"), FVector2D(44.0f, 44.0f));
	AddWidgetForResourceImage(DetailBuilder, DetailBuilder.GetProperty("WideLogo"), FVector2D(310.0f, 150.0f));
	AddWidgetForResourceImage(DetailBuilder, DetailBuilder.GetProperty("SplashScreen"), FVector2D(310.0f, 150.0f));

	// Add UI to select tile and splash colors.
	TSharedRef<IPropertyHandle> HexProperty = DetailBuilder.GetProperty("TileBackgroundColorHex");
	DetailBuilder.HideProperty(HexProperty);
	TSharedRef<IPropertyHandle> ColorProperty = DetailBuilder.GetProperty("TileBackgroundColor");
	ColorProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda([HexProperty, ColorProperty] { TransferColorToHexProperty(ColorProperty, HexProperty); }));

	HexProperty = DetailBuilder.GetProperty("SplashScreenBackgroundColorHex");
	DetailBuilder.HideProperty(HexProperty);
	ColorProperty = DetailBuilder.GetProperty("SplashScreenBackgroundColor");
	ColorProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda([HexProperty, ColorProperty] { TransferColorToHexProperty(ColorProperty, HexProperty); }));
}

void FUWPTargetSettingsCustomization::AddWidgetForResourceImage(IDetailLayoutBuilder& DetailBuilder, TSharedRef<IPropertyHandle> PropertyHandle, const FVector2D& ImageDimensions)
{
	IDetailCategoryBuilder& PackagingCategoryBuilder = DetailBuilder.EditCategory(FName(*PropertyHandle->GetMetaData("Category")));
	DetailBuilder.HideProperty(PropertyHandle);

	FString DefaultImageSubPath = FString::Printf(TEXT("Build\\UWP\\Resources\\%s.png"), *PropertyHandle->GetPropertyDisplayName().ToString());
	FString ImageSubPath;
	if (PropertyHandle->GetValue(ImageSubPath) == FPropertyAccess::Fail)
	{
		ImageSubPath = DefaultImageSubPath;
		PropertyHandle->SetValue(ImageSubPath);
	}
	if (ImageSubPath.IsEmpty())
	{
		ImageSubPath = DefaultImageSubPath;
		PropertyHandle->SetValue(ImageSubPath);
	}

	FString ProjectLogoPath = FPaths::GameDir() / ImageSubPath;

	PackagingCategoryBuilder.AddCustomRow(PropertyHandle->GetPropertyDisplayName())
	.NameContent()
	[
		PropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MaxDesiredWidth(500.0f)
	.MinDesiredWidth(100.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SExternalImageReference, FString(), ProjectLogoPath)
			.FileDescription(PropertyHandle->GetPropertyDisplayName())
			.MaxDisplaySize(ImageDimensions)
			.OnGetPickerPath(FOnGetPickerPath::CreateSP(this, &FUWPTargetSettingsCustomization::GetPickerPath))
			.OnPostExternalImageCopy(FOnPostExternalImageCopy::CreateSP(this, &FUWPTargetSettingsCustomization::HandlePostExternalIconCopy))
		]
	];
}

FString FUWPTargetSettingsCustomization::GetPickerPath()
{
	return FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_OPEN);
}


bool FUWPTargetSettingsCustomization::HandlePostExternalIconCopy(const FString& InChosenImage)
{
	FEditorDirectories::Get().SetLastDirectory(ELastDirectory::GENERIC_OPEN, FPaths::GetPath(InChosenImage));
	return true;
}

void FUWPTargetSettingsCustomization::OnCertificatePicked(const FString& PickedPath, const FString &TargetPath)
{
	FText FailReason;
	if (!SourceControlHelpers::CopyFileUnderSourceControl(TargetPath, PickedPath, LOCTEXT("CertificateDescription", "Certificate"), FailReason))
	{
		FNotificationInfo Info(FailReason);
		Info.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
}

FString FUWPTargetSettingsCustomization::GetNameForSigningCertificate(const FString &CertificatePath)
{
	// Display the friendly name of the certificate instead of the file name.
	FString CertificateName = LOCTEXT("CertificateNoCertificate", "No valid certificate selected").ToString();
	TArray<uint8> CertBytes;
	if (FFileHelper::LoadFileToArray(CertBytes, *CertificatePath))
	{
		CRYPT_DATA_BLOB CertBlob;
		CertBlob.cbData = CertBytes.Num();
		CertBlob.pbData = CertBytes.GetData();
		HCERTSTORE CertStore = PFXImportCertStore(&CertBlob, nullptr, 0);
		if (CertStore != nullptr)
		{
			const CERT_CONTEXT *CertContext = CertEnumCertificatesInStore(CertStore, nullptr);
			if (CertContext != nullptr)
			{
				DWORD NumCharacters = CertGetNameString(CertContext, CERT_NAME_FRIENDLY_DISPLAY_TYPE, 0, nullptr, nullptr, 0);
				CertificateName.GetCharArray().SetNumZeroed(NumCharacters);
				CertGetNameString(CertContext, CERT_NAME_FRIENDLY_DISPLAY_TYPE, 0, nullptr, CertificateName.GetCharArray().GetData(), CertificateName.GetAllocatedSize());
				CertFreeCertificateContext(CertContext);
			}
			CertCloseStore(CertStore, 0);
		}
		else
		{
			DWORD FailureReason = GetLastError();
			switch (FailureReason)
			{
			case ERROR_INVALID_PASSWORD:
				CertificateName = LOCTEXT("CertificatePasswordProtected", "Certificate is password protected").ToString();
				break;

			default:
				break;
			}
		}
	}

	return CertificateName;
}

void FUWPTargetSettingsCustomization::TransferColorToHexProperty(TSharedRef<IPropertyHandle> ColorProperty, TSharedRef<IPropertyHandle> HexProperty)
{
	// UI property is FColor to enable color picker, but we need to serialize as 3 byte hex string for manifest generation.
	FColor SelectedColor;
	FString SelectedColorAsString;
	ColorProperty->GetValueAsFormattedString(SelectedColorAsString);
	SelectedColor.InitFromString(SelectedColorAsString);
	FString SelectedColorAsManifestHex = FString::Printf(TEXT("#%02X%02X%02X"), SelectedColor.R, SelectedColor.G, SelectedColor.B);
	HexProperty->SetValue(SelectedColorAsManifestHex);
}

void FUWPTargetSettingsCustomization::AddWidgetForPlatformVersion(IDetailLayoutBuilder& DetailBuilder, TSharedRef<IPropertyHandle> PropertyHandle)
{
	IDetailCategoryBuilder& VersionCategoryBuilder = DetailBuilder.EditCategory(FName(*PropertyHandle->GetMetaData("Category")));
	DetailBuilder.HideProperty(PropertyHandle);

	FString CurrentSelectedVersion;
	PropertyHandle->GetValue(CurrentSelectedVersion);
	// Default to latest version when not set explicitly.  With Windows 10 automatic updates
	// it's a reasonably safe choice for users, and much more developer friendly than defaulting
	// to the oldest version.
	int32 CurrentSelectedIndex = PlatformVersionOptions.Num() - 1;
	for (int32 i = 0; i < PlatformVersionOptions.Num(); ++i)
	{
		if (*PlatformVersionOptions[i] == CurrentSelectedVersion)
		{
			CurrentSelectedIndex = i;
			break;
		}
	}

	VersionCategoryBuilder.AddCustomRow(PropertyHandle->GetPropertyDisplayName())
	.NameContent()
	[
		PropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MaxDesiredWidth(500.0f)
	.MinDesiredWidth(100.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextComboBox)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.OptionsSource(&PlatformVersionOptions)
			.InitiallySelectedItem(PlatformVersionOptions[CurrentSelectedIndex])
			.OnSelectionChanged(this, &FUWPTargetSettingsCustomization::OnPlatformVersionChanged, PropertyHandle)
		]
	];
}

void FUWPTargetSettingsCustomization::OnPlatformVersionChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo, TSharedRef<IPropertyHandle> Handle)
{
	Handle->SetValue(*NewValue);
}

#undef LOCTEXT_NAMESPACE

#include "HideWindowsPlatformTypes.h"
