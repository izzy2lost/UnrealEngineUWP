
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
#include "SourceControlHelpers.h"
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
	// Windows 10 Anniversary update
	PlatformVersionOptions.Add(MakeShareable(new FString("10.0.14393.0")));
	// Windows 10 Creators update
	PlatformVersionOptions.Add(MakeShareable(new FString("10.0.15063.0")));
}

void FUWPTargetSettingsCustomization::InitTargetDeviceFamilyOptions()
{
	TargetDeviceFamilyOptions.Empty();

	TargetDeviceFamilyOptions.Add(MakeShareable(new FString("Windows.Universal")));
	TargetDeviceFamilyOptions.Add(MakeShareable(new FString("Windows.Holographic")));
	TargetDeviceFamilyOptions.Add(MakeShareable(new FString("Windows.Desktop")));
	TargetDeviceFamilyOptions.Add(MakeShareable(new FString("Windows.Xbox")));
}

void FUWPTargetSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	InitTargetDeviceFamilyOptions();
	InitSupportedPlatformVersions();

	// Add UI for selecting the TargetDeviceFamily.
	TSharedRef<IPropertyHandle> TargetDeviceFamilyProperty = DetailBuilder.GetProperty("TargetDeviceFamily");
	AddWidgetForTargetDeviceFamily(DetailBuilder, TargetDeviceFamilyProperty);

	// Add UI for select min/max platform version.
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
	SigningProperty->NotifyPreChange();

	FString DefaultSigningSubPath = FString::Printf(TEXT("Build/UWP/%s.pfx"), *SigningProperty->GetProperty()->GetName());
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

	SigningProperty->NotifyPostChange();

	// Add the packaging images customization
	AddWidgetForResourceImage(DetailBuilder, DetailBuilder.GetProperty("Logo"), FVector2D(150.0f, 150.0f));
	AddWidgetForResourceImage(DetailBuilder, DetailBuilder.GetProperty("SmallLogo"), FVector2D(44.0f, 44.0f));
	AddWidgetForResourceImage(DetailBuilder, DetailBuilder.GetProperty("WideLogo"), FVector2D(310.0f, 150.0f));
	AddWidgetForResourceImage(DetailBuilder, DetailBuilder.GetProperty("SplashScreen"), FVector2D(310.0f, 150.0f));
	AddWidgetForResourceImage(DetailBuilder, DetailBuilder.GetProperty("StoreLogo"), FVector2D(50.0f, 50.0f));

	// Add UI to select tile and splash colors.
	TSharedRef<IPropertyHandle> TileHexProperty = DetailBuilder.GetProperty("TileBackgroundColorHex");
	DetailBuilder.HideProperty(TileHexProperty);
	TileHexProperty->NotifyPreChange();
	TSharedRef<IPropertyHandle> ColorProperty = DetailBuilder.GetProperty("TileBackgroundColor");
	ColorProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda([TileHexProperty, ColorProperty] { TransferColorToHexProperty(ColorProperty, TileHexProperty); }));
	TileHexProperty->NotifyPostChange();

	TSharedRef<IPropertyHandle> SplashHexProperty = DetailBuilder.GetProperty("SplashScreenBackgroundColorHex");
	DetailBuilder.HideProperty(SplashHexProperty);
	SplashHexProperty->NotifyPreChange();
	ColorProperty = DetailBuilder.GetProperty("SplashScreenBackgroundColor");
	ColorProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda([SplashHexProperty, ColorProperty] { TransferColorToHexProperty(ColorProperty, SplashHexProperty); }));
	SplashHexProperty->NotifyPostChange();

	// Add capability support.
	TSharedRef<IPropertyHandle> CapabilityList = DetailBuilder.GetProperty("CapabilityList");
	DetailBuilder.HideProperty(CapabilityList);
	AddWidgetForCapability(DetailBuilder, DetailBuilder.GetProperty("bInternetClient"), CapabilityList, TEXT("internetClient"));
	AddWidgetForCapability(DetailBuilder, DetailBuilder.GetProperty("bInternetClientServer"), CapabilityList, TEXT("internetClientServer"));
	AddWidgetForCapability(DetailBuilder, DetailBuilder.GetProperty("bPrivateNetworkClientServer"), CapabilityList, TEXT("privateNetworkClientServer"));
	AddWidgetForCapability(DetailBuilder, DetailBuilder.GetProperty("bAllJoyn"), CapabilityList, TEXT("allJoyn"));
	AddWidgetForCapability(DetailBuilder, DetailBuilder.GetProperty("bCodeGeneration"), CapabilityList, TEXT("codeGeneration"));

	TSharedRef<IPropertyHandle> DeviceCapabilityList = DetailBuilder.GetProperty("DeviceCapabilityList");
	DetailBuilder.HideProperty(DeviceCapabilityList);
	AddWidgetForCapability(DetailBuilder, DetailBuilder.GetProperty("bMicrophone"), DeviceCapabilityList, TEXT("microphone"));
	AddWidgetForCapability(DetailBuilder, DetailBuilder.GetProperty("bWebcam"), DeviceCapabilityList, TEXT("webcam"));
	AddWidgetForCapability(DetailBuilder, DetailBuilder.GetProperty("bProximity"), DeviceCapabilityList, TEXT("proximity"));
	AddWidgetForCapability(DetailBuilder, DetailBuilder.GetProperty("bLocation"), DeviceCapabilityList, TEXT("location"));
	AddWidgetForCapability(DetailBuilder, DetailBuilder.GetProperty("bBluetooth"), DeviceCapabilityList, TEXT("bluetooth"));

	TSharedRef<IPropertyHandle> UapCapabilityList = DetailBuilder.GetProperty("UapCapabilityList");
	DetailBuilder.HideProperty(UapCapabilityList);
	AddWidgetForCapability(DetailBuilder, DetailBuilder.GetProperty("bMusicLibrary"), UapCapabilityList, TEXT("musicLibrary"));
	AddWidgetForCapability(DetailBuilder, DetailBuilder.GetProperty("bPicturesLibrary"), UapCapabilityList, TEXT("picturesLibrary"));
	AddWidgetForCapability(DetailBuilder, DetailBuilder.GetProperty("bVideosLibrary"), UapCapabilityList, TEXT("videosLibrary"));
	AddWidgetForCapability(DetailBuilder, DetailBuilder.GetProperty("bBlockedChatMessages"), UapCapabilityList, TEXT("blockedChatMessages"));
	AddWidgetForCapability(DetailBuilder, DetailBuilder.GetProperty("bChat"), UapCapabilityList, TEXT("chat"));
	AddWidgetForCapability(DetailBuilder, DetailBuilder.GetProperty("bEnterpriseAuthentication"), UapCapabilityList, TEXT("enterpriseAuthentication"));
	AddWidgetForCapability(DetailBuilder, DetailBuilder.GetProperty("bObjects3D"), UapCapabilityList, TEXT("objects3D"));
	AddWidgetForCapability(DetailBuilder, DetailBuilder.GetProperty("bPhoneCall"), UapCapabilityList, TEXT("phoneCall"));
	AddWidgetForCapability(DetailBuilder, DetailBuilder.GetProperty("bRemovableStorage"), UapCapabilityList, TEXT("removableStorage"));
	AddWidgetForCapability(DetailBuilder, DetailBuilder.GetProperty("bSharedUserCertificates"), UapCapabilityList, TEXT("sharedUserCertificates"));
	AddWidgetForCapability(DetailBuilder, DetailBuilder.GetProperty("bUserAccountInformation"), UapCapabilityList, TEXT("userAccountInformation"));
	AddWidgetForCapability(DetailBuilder, DetailBuilder.GetProperty("bVoipCall"), UapCapabilityList, TEXT("voipCall"));

	TSharedRef<IPropertyHandle> Uap2CapabilityList = DetailBuilder.GetProperty("Uap2CapabilityList");
	DetailBuilder.HideProperty(Uap2CapabilityList);
	AddWidgetForCapability(DetailBuilder, DetailBuilder.GetProperty("bSpatialPerception"), Uap2CapabilityList, TEXT("spatialPerception"));

	// If this is the first time capabilities are being accessed for the project, enable defaults.
	TSharedRef<IPropertyHandle> SetDefaultCapabilitiesProperty = DetailBuilder.GetProperty("bSetDefaultCapabilities");
	DetailBuilder.HideProperty(SetDefaultCapabilitiesProperty);
	bool bSetDefaults;
	SetDefaultCapabilitiesProperty->GetValue(bSetDefaults);
	if (bSetDefaults)
	{
		OnCapabilityStateChanged(ECheckBoxState::Checked, CapabilityList, TEXT("internetClientServer"));
		OnCapabilityStateChanged(ECheckBoxState::Checked, CapabilityList, TEXT("privateNetworkClientServer"));
		SetDefaultCapabilitiesProperty->NotifyPreChange();
		SetDefaultCapabilitiesProperty->SetValue(false);
		SetDefaultCapabilitiesProperty->NotifyPostChange();
	}
}

void FUWPTargetSettingsCustomization::AddWidgetForResourceImage(IDetailLayoutBuilder& DetailBuilder, TSharedRef<IPropertyHandle> PropertyHandle, const FVector2D& ImageDimensions)
{
	PropertyHandle->NotifyPreChange();
	IDetailCategoryBuilder& PackagingCategoryBuilder = DetailBuilder.EditCategory(FName(*PropertyHandle->GetMetaData("Category")));
	DetailBuilder.HideProperty(PropertyHandle);

	const FString DefaultImageSubPath = FString::Printf(TEXT("Build/UWP/Resources/%s.png"), *PropertyHandle->GetProperty()->GetName());
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

	const FString EngineImagePath = FPaths::EngineDir() / DefaultImageSubPath;
	const FString ProjectImagePath = FPaths::GameDir() / ImageSubPath;

	// If the project image does not exist, copy the default image over from the engine directory so we have something to display in the UI.
	if (!FPaths::FileExists(ProjectImagePath))
	{
		FText ErrorMessage;
		SourceControlHelpers::CopyFileUnderSourceControl(ProjectImagePath, EngineImagePath, PropertyHandle->GetPropertyDisplayName(), ErrorMessage);
		PropertyHandle->SetValue(ImageSubPath);
	}

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
			SNew(SExternalImageReference, FString(), ProjectImagePath)
			.FileDescription(PropertyHandle->GetPropertyDisplayName())
			.MaxDisplaySize(ImageDimensions)
			.OnGetPickerPath(FOnGetPickerPath::CreateSP(this, &FUWPTargetSettingsCustomization::GetPickerPath))
			.OnPostExternalImageCopy(FOnPostExternalImageCopy::CreateSP(this, &FUWPTargetSettingsCustomization::HandlePostExternalIconCopy))
		]
	];

	PropertyHandle->NotifyPostChange();
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
	if (CurrentSelectedVersion.IsEmpty())
	{
		OnSelectedItemChanged(PlatformVersionOptions[CurrentSelectedIndex], ESelectInfo::Direct, PropertyHandle);
	}
	else
	{
		for (int32 i = 0; i < PlatformVersionOptions.Num(); ++i)
		{
			if (*PlatformVersionOptions[i] == CurrentSelectedVersion)
			{
				CurrentSelectedIndex = i;
				break;
			}
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
			.OnSelectionChanged(this, &FUWPTargetSettingsCustomization::OnSelectedItemChanged, PropertyHandle)
		]
	];
}

void FUWPTargetSettingsCustomization::AddWidgetForTargetDeviceFamily(IDetailLayoutBuilder& DetailBuilder, TSharedRef<IPropertyHandle> PropertyHandle)
{
	IDetailCategoryBuilder& TargetCategoryBuilder = DetailBuilder.EditCategory(FName(*PropertyHandle->GetMetaData("Category")));
	DetailBuilder.HideProperty(PropertyHandle);

	FString CurrentSelectedDeviceFamily;
	PropertyHandle->GetValue(CurrentSelectedDeviceFamily);

	// Default to first option when not set explicitly.
	int32 CurrentSelectedIndex = 0;
	if (CurrentSelectedDeviceFamily.IsEmpty())
	{
		OnSelectedItemChanged(TargetDeviceFamilyOptions[CurrentSelectedIndex], ESelectInfo::Direct, PropertyHandle);
	}
	else
	{
		for (int32 i = 0; i < TargetDeviceFamilyOptions.Num(); i++)
		{
			if (*TargetDeviceFamilyOptions[i] == CurrentSelectedDeviceFamily)
			{
				CurrentSelectedIndex = i;
				break;
			}
		}
	}

	TargetCategoryBuilder.AddCustomRow(PropertyHandle->GetPropertyDisplayName())
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
			.OptionsSource(&TargetDeviceFamilyOptions)
			.InitiallySelectedItem(TargetDeviceFamilyOptions[CurrentSelectedIndex])
			.OnSelectionChanged(this, &FUWPTargetSettingsCustomization::OnSelectedItemChanged, PropertyHandle)
		]
	];
}

void FUWPTargetSettingsCustomization::OnSelectedItemChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo, TSharedRef<IPropertyHandle> Handle)
{
	Handle->SetValue(*NewValue);
}

void FUWPTargetSettingsCustomization::AddWidgetForCapability(IDetailLayoutBuilder& DetailBuilder, TSharedRef<IPropertyHandle> CapabilityProperty, TSharedRef<IPropertyHandle> CapabilityList, const FString& CapabilityName)
{
	IDetailCategoryBuilder& CapabilityBuilder = DetailBuilder.EditCategory(FName(*CapabilityProperty->GetMetaData("Category")));
	DetailBuilder.HideProperty(CapabilityProperty);

	// Initialize checkbox state based on whether or not the capability currently exists in the CapabilityList.
	ECheckBoxState currentState = IsCapabilityChecked(CapabilityList, CapabilityName);
	if (currentState == ECheckBoxState::Checked)
	{
		OnCapabilityStateChanged(currentState, CapabilityList, CapabilityName);
	}

	CapabilityBuilder.AddCustomRow(CapabilityProperty->GetPropertyDisplayName())
	.NameContent()
	[
		CapabilityProperty->CreatePropertyNameWidget()
	]
	.ValueContent()
	.VAlign(VAlign_Center)
	[
		SNew(SCheckBox)
		.IsChecked(this, &FUWPTargetSettingsCustomization::IsCapabilityChecked, CapabilityList, CapabilityName)
		.OnCheckStateChanged(this, &FUWPTargetSettingsCustomization::OnCapabilityStateChanged, CapabilityList, CapabilityName)
	];
}

ECheckBoxState FUWPTargetSettingsCustomization::IsCapabilityChecked(TSharedRef<IPropertyHandle> CapabilityList, const FString CapabilityName) const
{
	TArray<void*> RawData;
	CapabilityList->AccessRawData(RawData);
	TArray<FString>* RawCapabilityStringArray = reinterpret_cast<TArray<FString>*>(RawData[0]);

	int32 Index;
	bool Found = RawCapabilityStringArray->Find(CapabilityName, Index);
	return (Found ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
}

void FUWPTargetSettingsCustomization::OnCapabilityStateChanged(ECheckBoxState CheckState, TSharedRef<IPropertyHandle> CapabilityList, const FString CapabilityName)
{
	bool IsEnabled = (CheckState == ECheckBoxState::Checked);
	TArray<void*> RawData;
	CapabilityList->AccessRawData(RawData);
	TArray<FString>* RawCapabilityStringArray = reinterpret_cast<TArray<FString>*>(RawData[0]);

	CapabilityList->NotifyPreChange();
	int32 Index;
	bool Found = RawCapabilityStringArray->Find(CapabilityName, Index);

	if (Found && !IsEnabled)
	{
		// Remove existing capability from the list
		RawCapabilityStringArray->RemoveAt(Index);
	}
	else if (!Found && IsEnabled)
	{
		//Add new capability to the list
		RawCapabilityStringArray->AddUnique(*CapabilityName);
	}

	// Save settings to Ini
	CapabilityList->NotifyPostChange();
}

#undef LOCTEXT_NAMESPACE

#include "HideWindowsPlatformTypes.h"
