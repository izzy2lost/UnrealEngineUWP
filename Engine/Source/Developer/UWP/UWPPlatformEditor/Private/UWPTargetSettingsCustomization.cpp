
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
#include "FileHelper.h"
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
	AddWidgetForCapability(DetailBuilder, CapabilityList, TEXT("internetClient"), LOCTEXT("InternetClientCaption", "Internet Client"), LOCTEXT("InternetClientTooltip", "Provides outbound access to the Internet and networks in public places like airports and cofee shops."), false);
	AddWidgetForCapability(DetailBuilder, CapabilityList, TEXT("internetClientServer"), LOCTEXT("InternetClientServerCaption", "Internet Client Server"), LOCTEXT("InternetClientServerTooltip", "Provides inbound and outbound access to the Internet and networks in public places like airports and cofee shops"), false);
	AddWidgetForCapability(DetailBuilder, CapabilityList, TEXT("privateNetworkClientServer"), LOCTEXT("PrivateNetworkCaption", "Private Network Client Server"), LOCTEXT("PrivateNetworkTooltip", "Provides inbound and outbound access to the Internet and networks that have an authenticated domain controller, or that the user has designated as either home or work networks.Inbound access to critical ports is always blocked."), false);
	AddWidgetForCapability(DetailBuilder, CapabilityList, TEXT("allJoyn"), LOCTEXT("AllJoynCaption", "AllJoyn"), LOCTEXT("AllJoynTooltip", "Allows AllJoyn-enabled apps and devices on a network to discover and interact with each other."), true);
	AddWidgetForCapability(DetailBuilder, CapabilityList, TEXT("codeGeneration"), LOCTEXT("CodeGenCaption", "Code Generation"), LOCTEXT("CodeGenTooltip", "Allows apps to generate code dynamically."), true);

	TSharedRef<IPropertyHandle> DeviceCapabilityList = DetailBuilder.GetProperty("DeviceCapabilityList");
	DetailBuilder.HideProperty(DeviceCapabilityList);
	AddWidgetForCapability(DetailBuilder, DeviceCapabilityList, TEXT("microphone"), LOCTEXT("MicrophoneCaption", "Microphone"), LOCTEXT("MicrophoneTooltip", "Provides access to the microphone's audio feed, which allows the app to record audio from connected microphones. Required for Windows.Media.SpeechRecognition APIs."), false);
	AddWidgetForCapability(DetailBuilder, DeviceCapabilityList, TEXT("webcam"), LOCTEXT("WebcamCaption", "Webcam"), LOCTEXT("WebcamTooltip", "Provides access to the webcam's video feed, which allows the app to capture snapshots and movies from connected webcams."), true);
	AddWidgetForCapability(DetailBuilder, DeviceCapabilityList, TEXT("proximity"), LOCTEXT("ProximityCaption", "Proximity"), LOCTEXT("ProximityTooltip", "Provides capability to connect devices in close proximity to the PC via near field proximity radio or Wi-FI Direct."), true);
	AddWidgetForCapability(DetailBuilder, DeviceCapabilityList, TEXT("location"), LOCTEXT("LocationCaption", "Location"), LOCTEXT("LocationTooltip", "Provides access to the current location, which is obtained from dedicated hardware like a GPS sensor in the PC or derived from available network information."), true);
	AddWidgetForCapability(DetailBuilder, DeviceCapabilityList, TEXT("bluetooth"), LOCTEXT("BluetoothCaption", "Bluetooth"), LOCTEXT("BluetoothTooltip", "Allows communication with paired Bluetooth devices over the Generic Attribute (GATT) or Classic Basic Rate (RFCOMM) protocols."), true);

	TSharedRef<IPropertyHandle> UapCapabilityList = DetailBuilder.GetProperty("UapCapabilityList");
	DetailBuilder.HideProperty(UapCapabilityList);
	AddWidgetForCapability(DetailBuilder, UapCapabilityList, TEXT("musicLibrary"), LOCTEXT("MusicLibCaption", "Music Library"), LOCTEXT("MusicLibTooltip", "Provides capability to add, change, or delete files in the Music Library for the local PC and HomeGroup PCs."), true);
	AddWidgetForCapability(DetailBuilder, UapCapabilityList, TEXT("picturesLibrary"), LOCTEXT("PicturesLibCaption", "Pictures Library"), LOCTEXT("PicturesLibTooltip", "Provides capability to add, change, or delete files in the Pictures Library for the local PC and HomeGroup PCs."), true);
	AddWidgetForCapability(DetailBuilder, UapCapabilityList, TEXT("videosLibrary"), LOCTEXT("VideosLibCaption", "Videos Library"), LOCTEXT("VideosLibTooltip", "Provides capability to add, change, or delete files in the Videos Library for the local PC and HomeGroup PCs."), true);
	AddWidgetForCapability(DetailBuilder, UapCapabilityList, TEXT("blockedChatMessages"), LOCTEXT("BlockedChatCaption", "Blocked Chat Messages"), LOCTEXT("BlockedChatTooltip", "Allows apps to read SMS and MMS messages that have been blocked by the Spam Filter app."), true);
	AddWidgetForCapability(DetailBuilder, UapCapabilityList, TEXT("chat"), LOCTEXT("ChatCaption", "Chat"), LOCTEXT("ChatTooltip", "Allows apps to read and delete Text Messages. It also allows apps to store chat messages in the system data store."), true);
	AddWidgetForCapability(DetailBuilder, UapCapabilityList, TEXT("enterpriseAuthentication"), LOCTEXT("EnterpriseAuthCaption", "Enterprise Authentication"), LOCTEXT("EnterpriseAuthTooltip", "Subject to Store policy. Provides ability to connect to enterprise intranet resources that require domain credentials.This capability is typically not needed for most apps."), true);
	AddWidgetForCapability(DetailBuilder, UapCapabilityList, TEXT("objects3D"), LOCTEXT("Objects3DCaption", "Objects 3D"), LOCTEXT("Objects3DTooltip", "Provides access to the user's 3D Objects, allowing the app to enumerate and access all files in the library without user interaction."), true);
	AddWidgetForCapability(DetailBuilder, UapCapabilityList, TEXT("phoneCall"), LOCTEXT("PhoneCallCaption", "Phone Call"), LOCTEXT("PhoneCallTooltip", "Allows apps to access all phone lines on the device and perform the following functions: place a call, access line-related metadata, access line-related triggers, set and check block list and call origination information."), true);
	AddWidgetForCapability(DetailBuilder, UapCapabilityList, TEXT("removableStorage"), LOCTEXT("RemovableStorageCaption", "Removable Storage"), LOCTEXT("RemovableStorageTooltip", "Provides capability to add, change, or delete files on removable storage devices."), true);
	AddWidgetForCapability(DetailBuilder, UapCapabilityList, TEXT("sharedUserCertificates"), LOCTEXT("ShareCertsCaption", "Shared User Certificates"), LOCTEXT("SharedCertsTooltip", "Subject to Store policy. Provides capability to access software and hardware certificates for validating a user's identity."), true);
	AddWidgetForCapability(DetailBuilder, UapCapabilityList, TEXT("userAccountInformation"), LOCTEXT("UserAccountCaption", "User Account Information"), LOCTEXT("UserAccountTooltip", "Gives apps the ability to access user's name and picture. Required to access Windows.System.UserProfile APIs."), true);
	AddWidgetForCapability(DetailBuilder, UapCapabilityList, TEXT("voipCall"), LOCTEXT("VoipCallCaption", "VOIP Call"), LOCTEXT("VoipCallTooltip", "Allows access to the VOIP calling APIs in Windows.ApplicationModel.Calls."), true);

	TSharedRef<IPropertyHandle> Uap2CapabilityList = DetailBuilder.GetProperty("Uap2CapabilityList");
	DetailBuilder.HideProperty(Uap2CapabilityList);
	AddWidgetForCapability(DetailBuilder, Uap2CapabilityList, TEXT("spatialPerception"), LOCTEXT("SpatialPerceptionCaption", "Spatial Perception"), LOCTEXT("SpatialPerceptionTooltip", "Provides access to environment data, which will be used to generate spatial maps or stages. Required to access Windows.Perception.Spatial APIs."), false);

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

	const FString DefaultEngineImageSubPath = FString::Printf(TEXT("Build/UWP/DefaultImages/%s.png"), *PropertyHandle->GetProperty()->GetName());
	const FString DefaultGameImageSubPath = FString::Printf(TEXT("Build/UWP/Resources/%s.png"), *PropertyHandle->GetProperty()->GetName());
	FString ImageSubPath;
	if (PropertyHandle->GetValue(ImageSubPath) == FPropertyAccess::Fail)
	{
		ImageSubPath = DefaultGameImageSubPath;
		PropertyHandle->SetValue(ImageSubPath);
	}
	if (ImageSubPath.IsEmpty())
	{
		ImageSubPath = DefaultGameImageSubPath;
		PropertyHandle->SetValue(ImageSubPath);
	}

	const FString EngineImagePath = FPaths::EngineDir() / DefaultEngineImageSubPath;
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

void FUWPTargetSettingsCustomization::AddWidgetForCapability(IDetailLayoutBuilder& DetailBuilder, TSharedRef<IPropertyHandle> CapabilityList, const FString& CapabilityName, const FText& CapabilityCaption, const FText& CapabilityTooltip, bool bForAdvanced)
{
	IDetailCategoryBuilder& CapabilityBuilder = DetailBuilder.EditCategory(FName("Capabilities"));

	// Initialize checkbox state based on whether or not the capability currently exists in the CapabilityList.
	ECheckBoxState currentState = IsCapabilityChecked(CapabilityList, CapabilityName);
	if (currentState == ECheckBoxState::Checked)
	{
		OnCapabilityStateChanged(currentState, CapabilityList, CapabilityName);
	}

	CapabilityBuilder.AddCustomRow(CapabilityCaption, bForAdvanced)
	.NameContent()
	[
		SNew(STextBlock)
		.Text(CapabilityCaption)
		.ToolTipText(CapabilityTooltip)
		.Font(IDetailLayoutBuilder::GetDetailFont())
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
