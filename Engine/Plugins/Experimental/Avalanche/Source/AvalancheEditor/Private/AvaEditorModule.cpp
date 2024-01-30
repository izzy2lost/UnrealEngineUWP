// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaEditorModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetTypeActions/AssetTypeActions_AvaBlueprint.h"
#include "AvaEditorActorUtils.h"
#include "AvaEditorCommands.h"
#include "AvaEditorIntegration.h"
#include "AvaEditorStyle.h"
#include "AvaShapeActor.h"
#include "Components/LightComponentBase.h"
#include "DynamicMeshes/AvaShapeDynMeshBase.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/Light.h"
#include "Engine/SkyLight.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "IAssetTools.h"
#include "IAvaOutlinerModule.h"
#include "Icon/AvaOutlinerObjectIconCustomization.h"
#include "Item/AvaOutlinerActor.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CoreDelegates.h"
#include "SVGImporter/AvaOutlinerSVGActorContextMenu.h"
#include "Styling/SlateIconFinder.h"
#include "UnrealEdGlobals.h"
#include "Viewport/AvaViewportQualitySettings.h"
#include "Widgets/AvaViewportColorPickerActorClassRegistry.h"

// Details View
#include "DetailsPanel/DMMaterialInterfaceTypeCustomizer.h"
#include "DetailView/Customizations/AvaAnchorAlignmentPropertyTypeCustomization.h"
#include "DetailView/Customizations/AvaCategoryHiderCustomization.h"
#include "DetailView/Customizations/AvaMeshesDetailCustomization.h"
#include "DetailView/Customizations/AvaVectorPropertyTypeCustomization.h"
#include "DetailView/Customizations/AvaViewportQualitySettingsPropertyTypeCustomization.h"

DEFINE_LOG_CATEGORY(AvaLog);

#define LOCTEXT_NAMESPACE "AvalancheEditor"

struct FAvaViewportColorPickerLightAdapter : public FAvaViewportColorPickerActorAdapter
{
	virtual ~FAvaViewportColorPickerLightAdapter() override = default;

	virtual FAvaColorChangeData GetColorData(const AActor* InActor) const override
	{
		if (IsValid(InActor))
		{
			if (const ULightComponentBase* Component = InActor->FindComponentByClass<ULightComponentBase>())
			{
				return {EAvaColorStyle::Solid, Component->GetLightColor(), FLinearColor::Black, /* bIsUnlit */ false};
			}
		}

		return FAvaColorChangeData();
	}

	virtual void SetColorData(AActor* InActor, const FAvaColorChangeData& InColorData) const override
	{
		if (IsValid(InActor))
		{
			if (ULightComponentBase* Component = InActor->FindComponentByClass<ULightComponentBase>())
			{
				FProperty* LightColorProperty = Component->GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(ULightComponentBase, LightColor));

				Component->PreEditChange(LightColorProperty);

				Component->LightColor = InColorData.PrimaryColor.ToFColorSRGB();

				FPropertyChangedEvent PropertyChangedEvent = {
					LightColorProperty,
					EPropertyChangeType::Interactive
				};

				Component->PostEditChangeProperty(PropertyChangedEvent);
			}
		}
	}
};

const FName AvalancheCategoryName(TEXT("AvalancheCategory"));

namespace UE::AvalancheEditor::Private
{
	static FString LevelTemplatesPath = FString::Printf(TEXT("/%hs/%s"), UE_PLUGIN_NAME, TEXT("LevelTemplates"));
	static FString DefaultLevelPath = FString::Printf(TEXT("/%hs/%s"), UE_PLUGIN_NAME, TEXT("DefaultMotionDesignLevel"));
	static FString DefaultLevelThumbnailPath = FString::Printf(TEXT("/%hs/%s"), UE_PLUGIN_NAME, TEXT("DefaultMotionDesignLevelThumbnail"));
}

void FAvaEditorModule::StartupModule()
{
	FAvaEditorStyle::Get(); // Initialises
	FAvaEditorCommands::Register();

	// Add the menu subsection
	FCoreDelegates::OnPostEngineInit.AddRaw(this, &FAvaEditorModule::PostEngineInit);
	FCoreDelegates::OnPreExit.AddRaw(this, &FAvaEditorModule::PreExit);

	RegisterAssetTools();
	RegisterCustomLayouts();

	// Register Palettes
	using namespace UE::AvalancheEditor::Private;

	// Register Icon Customization
	IAvaOutlinerModule::Get().RegisterOverriddenIcon<FAvaOutlinerActor, FAvaOutlinerObjectIconCustomization>(AAvaShapeActor::StaticClass())
		.SetOverriddenIcon(FOnGetOverriddenObjectIcon::CreateStatic(&FAvaEditorModule::GetOutlinerShapeActorIcon));

	IAvaOutlinerModule::Get().GetOnExtendOutlinerItemContextMenu()
    		.AddStatic(&FAvaOutlinerSVGActorContextMenu::OnExtendOutlinerContextMenu);

	FAvaViewportColorPickerActorClassRegistry::RegisterClassAdapter(ALight::StaticClass(), MakeShared<FAvaViewportColorPickerLightAdapter>());

	// Does not extend from ALight
	FAvaViewportColorPickerActorClassRegistry::RegisterClassAdapter(ASkyLight::StaticClass(), MakeShared<FAvaViewportColorPickerLightAdapter>());
}

void FAvaEditorModule::ShutdownModule()
{
	FAvaEditorStyle::Shutdown();
	FAvaEditorCommands::Unregister();

	FCoreDelegates::OnPostEngineInit.RemoveAll(this);
	FCoreDelegates::OnPreExit.RemoveAll(this);

	UnregisterAssetTools();
	if (UObjectInitialized() && !IsEngineExitRequested())
	{
		UnregisterCustomLayouts();

		if (IAvaOutlinerModule::IsLoaded())
		{
			IAvaOutlinerModule::Get().UnregisterOverriddenIcon<FAvaOutlinerActor>(AAvaShapeActor::StaticClass()->GetFName());
		}
	}
}

void FAvaEditorModule::CreateAvaLevelEditor()
{
	AvaLevelEditor = FAvaLevelEditorIntegration::BuildEditor();
}

void FAvaEditorModule::PostEngineInit()
{
	CreateAvaLevelEditor();

	if (FSlateApplication::IsInitialized())
	{
		RegisterPropertyEditorCategories();
		RegisterLevelTemplates();
	}
}

void FAvaEditorModule::PreExit()
{
	AvaLevelEditor.Reset();
}

FSlateIcon FAvaEditorModule::GetOutlinerShapeActorIcon(TSharedPtr<const FAvaOutlinerItem> InItem)
{
	if (!InItem.IsValid())
	{
		return FSlateIcon();
	}

	if (const FAvaOutlinerActor* ActorItem = InItem->CastTo<FAvaOutlinerActor>())
	{
		if (const AAvaShapeActor* ShapeActor = Cast<AAvaShapeActor>(ActorItem->GetActor()))
		{
			if (const UAvaShapeDynamicMeshBase* DynamicMesh = ShapeActor->GetDynamicMesh())
			{
				return FSlateIconFinder::FindIconForClass(DynamicMesh->GetClass());
			}
		}
	}

	return FSlateIcon();
}

void FAvaEditorModule::RegisterAssetTools()
{
	IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();

	AssetTools.RegisterAdvancedAssetCategory(AvalancheCategoryName
		, LOCTEXT("AvalancheCategoryName", "Motion Design"));

	TSharedRef<IAssetTypeActions> AvaBlueprintActions = MakeShared<FAssetTypeActions_AvaBlueprint>();
	AssetTools.RegisterAssetTypeActions(AvaBlueprintActions);
	AssetTypeActions.Add(AvaBlueprintActions);
}

void FAvaEditorModule::UnregisterAssetTools()
{
	// Unregister all the asset types that we registered
	if (FAssetToolsModule::IsModuleLoaded())
	{
		IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
		for (const TSharedPtr<IAssetTypeActions>& AssetTypeAction : AssetTypeActions)
		{
			AssetTools.UnregisterAssetTypeActions(AssetTypeAction.ToSharedRef());
		}
	}

	AssetTypeActions.Empty();
}

void FAvaEditorModule::RegisterPropertyEditorCategories()
{
	static const FName PropertyEditor("PropertyEditor");
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(PropertyEditor);

	TSharedRef<FPropertySection> Section = PropertyModule.FindOrCreateSection("Object", "General", LOCTEXT("General", "General"));
	Section->AddCategory("Transform");
	Section->AddCategory("TransformCommon");
	Section->AddCategory("Mobility");

	Section = PropertyModule.FindOrCreateSection("Actor", "General", LOCTEXT("General", "General"));
	Section->AddCategory("Transform");
	Section->AddCategory("TransformCommon");
	Section->AddCategory("Mobility");

	// AvaShapeActor Sections
	{
		Section = PropertyModule.FindOrCreateSection("AvaShapeActor", "Shape", LOCTEXT("Shape", "Shape"));
		Section->AddCategory("Shape");

		Section = PropertyModule.FindOrCreateSection("AvaShapeActor", "Material", LOCTEXT("Material", "Material"));
		Section->AddCategory("Material");

		Section = PropertyModule.FindOrCreateSection("AvaShapeActor", "DynamicMesh", LOCTEXT("DynamicMesh", "Dynamic Mesh"));
		Section->AddCategory("DynamicMeshComponent");

		Section = PropertyModule.FindOrCreateSection("AvaShapeActor", "Rendering", LOCTEXT("Rendering", "Rendering"));
		Section->AddCategory("Rendering");
		Section->RemoveCategory("Lighting");
		Section->RemoveCategory("VirtualTexture");
		Section->RemoveCategory("MaterialParameters");
		Section->RemoveCategory("TextureStreaming");

		Section = PropertyModule.FindOrCreateSection("AvaShapeActor", "Lighting", LOCTEXT("Lighting", "Lighting"));
		Section->AddCategory("Lighting");
	}

	// AvaTextActor Sections
	{
		Section = PropertyModule.FindOrCreateSection("AvaTextActor", "Text", LOCTEXT("Text", "Text"));
		Section->AddCategory("Text");
		Section->AddCategory("TextAnimation");

		Section = PropertyModule.FindOrCreateSection("AvaTextActor", "Lighting", LOCTEXT("Lighting", "Lighting"));
		Section->AddCategory("Lighting");

		Section = PropertyModule.FindOrCreateSection("AvaTextActor", "Rendering", LOCTEXT("Rendering", "Rendering"));
		Section->AddCategory("Rendering");

		Section = PropertyModule.FindOrCreateSection("AvaTextActor", "Style", LOCTEXT("Style", "Style"));
		Section->AddCategory("Style");

		Section = PropertyModule.FindOrCreateSection("AvaTextActor", "Geometry", LOCTEXT("Geometry", "Geometry"));
		Section->AddCategory("Geometry");

		Section = PropertyModule.FindOrCreateSection("AvaTextActor", "Layout", LOCTEXT("Layout", "Layout"));
		Section->AddCategory("Layout");

		Section = PropertyModule.FindOrCreateSection("AvaTextActor", "Materials", LOCTEXT("Materials", "Text Materials"));
		Section->AddCategory("Materials");
	}

	// AvaCineCameraActor Sections
	{
		Section = PropertyModule.FindOrCreateSection("AvaCineCameraActor", "Camera", LOCTEXT("Camera", "Camera"));
		Section->AddCategory("Camera");
		Section->AddCategory("CameraOptions");
		Section->AddCategory("CurrentCameraSettings");
	}
}

void FAvaEditorModule::RegisterCustomLayouts()
{
	static FName PropertyEditor("PropertyEditor");

	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(PropertyEditor);

	// Generic
	PropertyModule.RegisterCustomClassLayout(AAvaShapeActor::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(
		&FAvaCategoryHiderCustomization::MakeInstance));
	PropertyModule.RegisterCustomClassLayout(UAvaShapeDynamicMeshBase::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(
		&FAvaMeshesDetailCustomization::MakeInstance));

	const TSharedRef<FDMMaterialInterfaceTypeIdentifier> MaterialPropertyTypeIdentifier = MakeShared<FDMMaterialInterfaceTypeIdentifier>();
	
	PropertyModule.RegisterCustomPropertyTypeLayout(UMaterialInterface::StaticClass()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(
		&FDMMaterialInterfaceTypeCustomizer::MakeInstance), MaterialPropertyTypeIdentifier);

	const TSharedRef<FAvaVectorPropertyTypeIdentifier> VectorPropertyTypeIdentifier = MakeShared<FAvaVectorPropertyTypeIdentifier>();

	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("Vector"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(
		&FAvaVectorPropertyTypeCustomization::MakeInstance), VectorPropertyTypeIdentifier);
	PropertyModule.RegisterCustomPropertyTypeLayout(TEXT("Vector2D"), FOnGetPropertyTypeCustomizationInstance::CreateStatic(
		&FAvaVectorPropertyTypeCustomization::MakeInstance), VectorPropertyTypeIdentifier);
	
	PropertyModule.RegisterCustomPropertyTypeLayout(FAvaViewportQualitySettings::StaticStruct()->GetFName()
		, FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FAvaViewportQualitySettingsPropertyTypeCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(FAvaAnchorAlignment::StaticStruct()->GetFName()
		, FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FAvaAnchorAlignmentPropertyTypeCustomization::MakeInstance));
}

void FAvaEditorModule::UnregisterCustomLayouts()
{
	static FName PropertyEditor("PropertyEditor");

	if (FModuleManager::Get().IsModuleLoaded(PropertyEditor))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(PropertyEditor);

		// Generic
		PropertyModule.UnregisterCustomClassLayout(AActor::StaticClass()->GetFName());
		PropertyModule.UnregisterCustomClassLayout(AAvaShapeActor::StaticClass()->GetFName());
		PropertyModule.UnregisterCustomClassLayout(UAvaShapeDynamicMeshBase::StaticClass()->GetFName());

		PropertyModule.UnregisterCustomPropertyTypeLayout(UMaterialInterface::StaticClass()->GetFName());

		PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("Vector"));
		PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("Vector2D"));

		PropertyModule.UnregisterCustomPropertyTypeLayout(FAvaViewportQualitySettings::StaticStruct()->GetFName());
		PropertyModule.UnregisterCustomPropertyTypeLayout(FAvaAnchorAlignment::StaticStruct()->GetFName());
	}
}

void FAvaEditorModule::RegisterLevelTemplates()
{
	if (GUnrealEd)
	{
		// Register original template map,
		// @todo: Deprecate post-beta 0.1.4
		if (!GUnrealEd->IsTemplateMap(UE::AvalancheEditor::Private::DefaultLevelPath))
		{
			FTemplateMapInfo AvalancheMapTemplate;
			AvalancheMapTemplate.Category = TEXT("Motion Design");
			AvalancheMapTemplate.DisplayName = LOCTEXT("Map Template", "Motion Design");
			AvalancheMapTemplate.ThumbnailTexture = UE::AvalancheEditor::Private::DefaultLevelThumbnailPath;			
			AvalancheMapTemplate.Map = UE::AvalancheEditor::Private::DefaultLevelPath;
				
			GUnrealEd->AppendTemplateMaps({ AvalancheMapTemplate });	
		}

		IAssetRegistry* AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).TryGet();
		if (AssetRegistry)
		{
			TArray<FString> LevelTemplatePaths;
			AssetRegistry->GetSubPaths(UE::AvalancheEditor::Private::LevelTemplatesPath, LevelTemplatePaths, false);

			TArray<FTemplateMapInfo> TemplateMaps;
			
			for (const FString& LevelTemplatePath : LevelTemplatePaths)
			{
				FString LevelTemplateName = FPaths::GetPathLeaf(LevelTemplatePath);

				TArray<FAssetData> LevelTemplateAssetData;				
				if (AssetRegistry->GetAssetsByPath(FName(LevelTemplatePath), LevelTemplateAssetData, true))
				{
					FAssetData LevelAsset;
					FAssetData ThumbnailAsset;
					
					for (FAssetData& AssetData : LevelTemplateAssetData)
					{
						if (LevelAsset.IsValid() && ThumbnailAsset.IsValid())
						{
							break;
						}

						if (!AssetData.AssetName.ToString().Contains(LevelTemplateName))
						{
							continue;
						}
						
						if (AssetData.AssetClassPath == UWorld::StaticClass()->GetClassPathName())
						{
							LevelAsset = AssetData;
						}
						else if (AssetData.AssetClassPath == UTexture2D::StaticClass()->GetClassPathName())
						{
							ThumbnailAsset = AssetData;
						}
					}

					if (LevelAsset.IsValid() && ThumbnailAsset.IsValid())
					{
						if (!GUnrealEd->IsTemplateMap(LevelAsset.GetObjectPathString()))
						{
							FString LevelTemplateDisplayName = FName::NameToDisplayString(LevelTemplateName, false);
							
							FTemplateMapInfo& MapTemplate = TemplateMaps.Emplace_GetRef();
							MapTemplate.Category = TEXT("Motion Design");
							MapTemplate.DisplayName = FText::FromString(LevelTemplateDisplayName);
							MapTemplate.ThumbnailTexture = ThumbnailAsset.GetSoftObjectPath();			
							MapTemplate.Map = LevelAsset.GetSoftObjectPath();
						}
					}
				}
			}

			GUnrealEd->AppendTemplateMaps(TemplateMaps);
		}
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAvaEditorModule, AvalancheEditor)
