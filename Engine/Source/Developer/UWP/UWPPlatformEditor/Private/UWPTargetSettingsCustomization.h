#pragma once

#include "EditorStyle.h"
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

	void AddWidgetForResourceImage(IDetailLayoutBuilder& DetailBuilder, TSharedRef<IPropertyHandle> PropertyHandle, const FVector2D& ImageDimensions);
	FString GetPickerPath();
	bool HandlePostExternalIconCopy(const FString & InChosenImage);
	static void OnCertificatePicked(const FString& PickedPath, const FString &TargetPath);
	static FString GetNameForSigningCertificate(const FString &CertificatePath);
	static void TransferColorToHexProperty(TSharedRef<IPropertyHandle> ColorProperty, TSharedRef<IPropertyHandle> HexProperty);
	void OnPlatformVersionChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo, TSharedRef<IPropertyHandle> HandlePtr);
	void AddWidgetForPlatformVersion(IDetailLayoutBuilder& DetailBuilder, TSharedRef<IPropertyHandle> PropertyHandle);
	void InitSupportedPlatformVersions();

	TArray<TSharedPtr<FString>> PlatformVersionOptions;
};