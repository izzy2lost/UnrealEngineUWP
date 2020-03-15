#include "UWPImageResourcesCustomization.h"

#include "DetailLayoutBuilder.h"
#include "IDetailChildrenBuilder.h"
#include "DetailWidgetRow.h"
#include "SExternalImageReference.h"
#include "Widgets/Text/STextBlock.h"
#include "IExternalImagePickerModule.h"
#include "ISourceControlModule.h"
#include "PropertyHandle.h"
#include "EditorDirectories.h"
#include "Interfaces/IPluginManager.h"
#include "Settings/ProjectPackagingSettings.h"

namespace
{
	FString GetPickerPath()
	{
		return FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_OPEN);
	}


	bool HandlePostExternalIconCopy(const FString& InChosenImage)
	{
		FEditorDirectories::Get().SetLastDirectory(ELastDirectory::GENERIC_OPEN, FPaths::GetPath(InChosenImage));
		return true;
	}

	void AddWidgetForResourceImage(IDetailChildrenBuilder& ChildrenBuilder, const FString& RootPath, const FString& Culture, const FString& ImageFileName, const FText& ImageCaption, const FVector2D& ImageDimensions)
	{
		const FString DefaultEngineImageSubPath = FString::Printf(TEXT("Build/UWP/DefaultImages/%s.png"), *ImageFileName);
		const FString DefaultGameImageSubPath = FString::Printf(TEXT("Build/UWP/Resources/%s/%s.png"), *Culture, *ImageFileName);

		const FString EngineImagePath = FPaths::EngineDir() / DefaultEngineImageSubPath;
		const FString ProjectImagePath = RootPath / DefaultGameImageSubPath;

		TArray<FString> ImageExtensions;
		ImageExtensions.Add(TEXT("png"));

		ChildrenBuilder.AddCustomRow(ImageCaption)
		.NameContent()
		[
			SNew(STextBlock)
			.Text(ImageCaption)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		.MinDesiredWidth(ImageDimensions.X)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SExternalImageReference, EngineImagePath, ProjectImagePath)
				.FileDescription(ImageCaption)
				.MaxDisplaySize(ImageDimensions)
				.OnGetPickerPath(FOnGetPickerPath::CreateStatic(&GetPickerPath))
				.OnPostExternalImageCopy(FOnPostExternalImageCopy::CreateStatic(&HandlePostExternalIconCopy))
				.DeleteTargetWhenDefaultChosen(true)
				.FileExtensions(ImageExtensions)
				.DeletePreviousTargetWhenExtensionChanges(true)
			]
		];
	}
}

#define LOCTEXT_NAMESPACE "UWPDlcImagesCustomization"

TSharedRef<IPropertyTypeCustomization> FUWPDlcImagesCustomization::MakeInstance()
{
	return MakeShared<FUWPDlcImagesCustomization>();
}

void FUWPDlcImagesCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TSharedRef<IPropertyHandle> PluginNameProperty = InStructPropertyHandle->GetParentHandle()->GetChildHandle(FName("AppliesToDlcPlugin")).ToSharedRef();
	FString CurrentPluginName;
	PluginNameProperty->GetValue(CurrentPluginName);

	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(CurrentPluginName);
	if (Plugin.IsValid())
	{
		TSharedRef<IPropertyHandle> CultureIdProperty = InStructPropertyHandle->GetParentHandle()->GetChildHandle(FName("CultureId")).ToSharedRef();
		FString CultureId;
		CultureIdProperty->GetValue(CultureId);

		AddWidgetForResourceImage(ChildBuilder, Plugin->GetBaseDir(), CultureId, TEXT("StoreLogo"), LOCTEXT("StoreLogo", "Store Logo"), FVector2D(50.0f, 50.0f));
	}
}

TSharedRef<IPropertyTypeCustomization> FUWPCorePackageImagesCustomization::MakeInstance()
{
	return MakeShared<FUWPCorePackageImagesCustomization>();
}

void FUWPCorePackageImagesCustomization::CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	HeaderRow.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	];
}

void FUWPCorePackageImagesCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TSharedRef<IPropertyHandle> CultureIdProperty = InStructPropertyHandle->GetParentHandle()->GetChildHandle(FName("CultureId")).ToSharedRef();
	FString CultureId;
	CultureIdProperty->GetValue(CultureId);

	AddWidgetForResourceImage(ChildBuilder, FPaths::ProjectDir(), CultureId, TEXT("Logo"), LOCTEXT("Square150x150Logo", "Square 150x150 Logo"), FVector2D(150.0f, 150.0f));
	AddWidgetForResourceImage(ChildBuilder, FPaths::ProjectDir(), CultureId, TEXT("SmallLogo"), LOCTEXT("Square44x44Logo", "Square 44x44 Logo"), FVector2D(44.0f, 44.0f));
	AddWidgetForResourceImage(ChildBuilder, FPaths::ProjectDir(), CultureId, TEXT("WideLogo"), LOCTEXT("Wide310x150Logo", "Wide 310x150 Logo"), FVector2D(310.0f, 150.0f));
	AddWidgetForResourceImage(ChildBuilder, FPaths::ProjectDir(), CultureId, TEXT("SplashScreen"), LOCTEXT("SplashScreen", "Splash Screen"), FVector2D(620.0f, 300.0f));
	AddWidgetForResourceImage(ChildBuilder, FPaths::ProjectDir(), CultureId, TEXT("StoreLogo"), LOCTEXT("StoreLogo", "Store Logo"), FVector2D(50.0f, 50.0f));
}

#undef LOCTEXT_NAMESPACE