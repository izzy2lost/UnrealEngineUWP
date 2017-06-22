#pragma once

#include "PropertyEditorModule.h"
#include "IDetailCustomization.h"
#include "DetailLayoutBuilder.h"

class FUWPTargetSettingsCustomization : public IDetailCustomization
{
public:

	// Makes a new instance of this detail layout class for a specific detail view requesting it
	static TSharedRef<IDetailCustomization> MakeInstance();

	// IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;
	// End of IDetailCustomization interface
private:

	void AddWidgetForCapability(IDetailLayoutBuilder& DetailBuilder, TSharedRef<IPropertyHandle> CapabilityList, const FString& CapabilityName, const FText& CapabilityCaption, const FText& CapabilityTooltip, bool bForAdvanced);
	void AddWidgetForPlatformVersion(IDetailLayoutBuilder& DetailBuilder, TSharedRef<IPropertyHandle> PropertyHandle);
	void AddWidgetForResourceImage(IDetailLayoutBuilder& DetailBuilder, TSharedRef<IPropertyHandle> PropertyHandle, const FVector2D& ImageDimensions);
	void AddWidgetForTargetDeviceFamily(IDetailLayoutBuilder& DetailBuilder, TSharedRef<IPropertyHandle> PropertyHandle);
	static FString GetNameForSigningCertificate(const FString &CertificatePath);
	FString GetPickerPath();
	bool HandlePostExternalIconCopy(const FString & InChosenImage);
	void InitSupportedPlatformVersions();
	void InitTargetDeviceFamilyOptions();
	ECheckBoxState IsCapabilityChecked(TSharedRef<IPropertyHandle> CapabilityList, const FString CapabilityName) const;
	void OnCapabilityStateChanged(ECheckBoxState CheckState, TSharedRef<IPropertyHandle> CapabilityList, const FString CapabilityName);
	static void OnCertificatePicked(const FString& PickedPath, const FString &TargetPath);
	void OnSelectedItemChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo, TSharedRef<IPropertyHandle> HandlePtr);
	static void TransferColorToHexProperty(TSharedRef<IPropertyHandle> ColorProperty, TSharedRef<IPropertyHandle> HexProperty);

	TArray<TSharedPtr<FString>> PlatformVersionOptions;
	TArray<TSharedPtr<FString>> TargetDeviceFamilyOptions;
};