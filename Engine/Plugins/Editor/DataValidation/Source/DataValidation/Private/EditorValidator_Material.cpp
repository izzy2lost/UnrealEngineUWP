// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorValidator_Material.h"
#include "AssetCompilingManager.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "DetailWidgetRow.h"
#include "EditorValidatorSubsystem.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Misc/DataValidation.h"
#include "Widgets/Input/SComboBox.h"

UEditorValidator_Material::UEditorValidator_Material()
	: Super()
{
	if (GetDefault<UDataValidationSettings>()->bEnableMaterialValidation)
	{
		for (const FMaterialEditorValidationPlatform& Config: GetDefault<UDataValidationSettings>()->MaterialValidationPlatforms)
		{
			FShaderValidationPlatform Platform = {};

			bool bValidShaderPlatform = false;
			if (Config.ShaderPlatform.Name == FMaterialEditorValidationShaderPlatform::MaxRHIShaderPlatformName)
			{
				Platform.ShaderPlatform = GMaxRHIShaderPlatform;
				bValidShaderPlatform = true;
			}
			else
			{
				for (int32 ShaderPlatformIndex = 0; ShaderPlatformIndex < SP_NumPlatforms; ++ShaderPlatformIndex)
				{
					const EShaderPlatform ShaderPlatform = static_cast<EShaderPlatform>(ShaderPlatformIndex);

					if (FDataDrivenShaderPlatformInfo::IsValid(ShaderPlatform)
						&& FDataDrivenShaderPlatformInfo::CanUseForMaterialValidation(ShaderPlatform)
						&& FDataDrivenShaderPlatformInfo::GetName(ShaderPlatform) == Config.ShaderPlatform.Name)
					{
						Platform.ShaderPlatform = ShaderPlatform;
						bValidShaderPlatform = true;
						break;
					}
				}
			}

			if (!bValidShaderPlatform)
			{
				UE_LOG(LogContentValidation, Warning, TEXT("Material asset validation shader platform '%s' is not available, skipping."), *Config.ShaderPlatform.Name.ToString());
				continue;
			}

			switch (Config.FeatureLevel)
			{
			case EMaterialEditorValidationFeatureLevel::CurrentMaxFeatureLevel: Platform.FeatureLevel = GMaxRHIFeatureLevel; break; 
			case EMaterialEditorValidationFeatureLevel::ES3_1: Platform.FeatureLevel = ERHIFeatureLevel::ES3_1; break;
			case EMaterialEditorValidationFeatureLevel::SM5: Platform.FeatureLevel = ERHIFeatureLevel::SM5; break;
			case EMaterialEditorValidationFeatureLevel::SM6: Platform.FeatureLevel = ERHIFeatureLevel::SM6; break;
			}

			switch (Config.MaterialQualityLevel)
			{
			case EMaterialEditorValidationQualityLevel::Low: Platform.MaterialQualityLevel = EMaterialQualityLevel::Low; break;
			case EMaterialEditorValidationQualityLevel::Medium: Platform.MaterialQualityLevel = EMaterialQualityLevel::Medium; break;
			case EMaterialEditorValidationQualityLevel::High: Platform.MaterialQualityLevel = EMaterialQualityLevel::High; break;
			case EMaterialEditorValidationQualityLevel::Epic: Platform.MaterialQualityLevel = EMaterialQualityLevel::Epic; break;
			}

			ValidationPlatforms.Add(Platform);
		}
	}
}

bool UEditorValidator_Material::CanValidateAsset_Implementation(const FAssetData& AssetData, UObject* InAsset, FDataValidationContext& InContext) const
{
	const bool bAnyValidationPlatforms = !ValidationPlatforms.IsEmpty();
	const bool bShouldAllowFullValidation = InContext.GetValidationUsecase() != EDataValidationUsecase::Save;
	return bAnyValidationPlatforms && bShouldAllowFullValidation && (Cast<UMaterial>(InAsset) || Cast<UMaterialInstance>(InAsset));
}

EDataValidationResult UEditorValidator_Material::ValidateLoadedAsset_Implementation(const FAssetData& AssetData, UObject* InAsset, FDataValidationContext& InContext)
{
	UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(InAsset);

	UMaterial* Material = MaterialInstance ? MaterialInstance->GetMaterial() : Cast<UMaterial>(InAsset);
	check(Material);

	TArray<FMaterialResource*> Resources;
	bool bAnyResources = false;

	for (const FShaderValidationPlatform& ValidationPlatform: ValidationPlatforms)
	{
		FMaterialResource* CurrentResource = FindOrCreateMaterialResource(Resources, Material, MaterialInstance, ValidationPlatform.FeatureLevel, ValidationPlatform.MaterialQualityLevel);
		check(CurrentResource);

		if (CurrentResource)
		{
			CurrentResource->CacheShaders(ValidationPlatform.ShaderPlatform);
			bAnyResources = true;
		}
	}

	if (!bAnyResources)
	{
		FMaterial::DeferredDeleteArray(Resources);
		return EDataValidationResult::NotValidated;
	}

	FAssetCompilingManager::Get().FinishAllCompilation();

	bool bCompileErrors = false;
	for (FMaterialResource* Resource: Resources)
	{
		if (!Resource->IsCompilationFinished())
		{
			UE_LOG(LogContentValidation, Warning, TEXT("Shader compilation was expected to be finished, but was not finished."));
		}

		bCompileErrors |= Resource->GetCompileErrors().Num() > 0;
	}

	FMaterial::DeferredDeleteArray(Resources);

	return !bCompileErrors ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}

class FMaterialEditorValidationPlatformCustomization : public IPropertyTypeCustomization
{
public:
	FMaterialEditorValidationPlatformCustomization()
		: MaxRHIShaderPlatform(MakeShared<EShaderPlatform>(SP_NumPlatforms))
	{
		for (int32 ShaderPlatformIndex = 0; ShaderPlatformIndex < SP_NumPlatforms; ++ShaderPlatformIndex)
		{
			const EShaderPlatform ShaderPlatform = static_cast<EShaderPlatform>(ShaderPlatformIndex);

			if (FDataDrivenShaderPlatformInfo::IsValid(ShaderPlatform) && FDataDrivenShaderPlatformInfo::CanUseForMaterialValidation(ShaderPlatform))
			{
				ValidationShaderPlatforms.Add(MakeShared<EShaderPlatform>(ShaderPlatform));
			}
		}

		ValidationShaderPlatforms.StableSort([this](const TSharedPtr<EShaderPlatform>& A, const TSharedPtr<EShaderPlatform>& B)
		{
			return GetShaderPlatformFriendlyName(A).CompareTo(GetShaderPlatformFriendlyName(B)) < 0;
		});

		ValidationShaderPlatforms.Insert(MaxRHIShaderPlatform, 0);
	}

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override
	{
		TSharedPtr<IPropertyHandle> PropertyHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FMaterialEditorValidationShaderPlatform, Name));
		ensure(PropertyHandle.IsValid());
		if (!PropertyHandle.IsValid())
		{
			return;
		}

		HeaderRow
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			SNew(SComboBox<TSharedPtr<EShaderPlatform>>)
			.OptionsSource(&ValidationShaderPlatforms)
			.InitiallySelectedItem(GetCurrentShaderPlatform(PropertyHandle))
			.OnSelectionChanged_Lambda([this, PropertyHandle](TSharedPtr<EShaderPlatform> ShaderPlatformOpt, ESelectInfo::Type SelectionInfo)
			{
				if (ShaderPlatformOpt.IsValid() && PropertyHandle.IsValid())
				{
					PropertyHandle->NotifyPreChange();
					if (ShaderPlatformOpt == MaxRHIShaderPlatform)
					{
						PropertyHandle->SetValue(FMaterialEditorValidationShaderPlatform::MaxRHIShaderPlatformName);
					}
					else
					{
						PropertyHandle->SetValue(FDataDrivenShaderPlatformInfo::GetName(*ShaderPlatformOpt.Get()));
					}
					PropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
				}
			})
			.OnGenerateWidget_Lambda([this](const TSharedPtr<EShaderPlatform>& ShaderPlatformOpt)
			{
				return SNew(STextBlock).Text(GetShaderPlatformFriendlyName(ShaderPlatformOpt));
			})
			.Content()
			[
				SNew(STextBlock)
				.Font(StructCustomizationUtils.GetRegularFont())
				.Text_Lambda([this, PropertyHandle]()
				{
					return GetShaderPlatformFriendlyName(GetCurrentShaderPlatform(PropertyHandle)); 
				})
			]
		];
	}

	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override
	{
	}

private:
	TArray<TSharedPtr<EShaderPlatform>> ValidationShaderPlatforms;
	const TSharedPtr<EShaderPlatform> MaxRHIShaderPlatform;

	FName GetShaderPlatformName(const TSharedPtr<EShaderPlatform>& ShaderPlatformOpt)
	{
		if (ShaderPlatformOpt == MaxRHIShaderPlatform)
		{
			return FMaterialEditorValidationShaderPlatform::MaxRHIShaderPlatformName;
		}
		else if (ShaderPlatformOpt.IsValid())
		{
			return FDataDrivenShaderPlatformInfo::GetName(*ShaderPlatformOpt.Get());
		}
		else
		{
			return FName("Invalid");
		}
	}

	FText GetShaderPlatformFriendlyName(const TSharedPtr<EShaderPlatform>& ShaderPlatformOpt)
	{
		if (ShaderPlatformOpt == MaxRHIShaderPlatform)
		{
			return NSLOCTEXT("AssetValidation", "ShaderPlatform_MaxRHIShaderPlatform", "Current RHI Max Shader Platform");
		}
		else if (ShaderPlatformOpt.IsValid())
		{
			const FText Name = FDataDrivenShaderPlatformInfo::GetFriendlyName(*ShaderPlatformOpt.Get());
			if (!Name.IsEmpty())
			{
				return Name;
			}

			return FText::FromName(FDataDrivenShaderPlatformInfo::GetName(*ShaderPlatformOpt.Get()));
		}
		else
		{
			return NSLOCTEXT("AssetValidation", "ShaderPlatform_Invalid", "Invalid");
		}
	}

	TSharedPtr<EShaderPlatform> GetCurrentShaderPlatform(const TSharedPtr<IPropertyHandle>& PropertyHandle)
	{
		FName CurrentShaderPlatformName;
		if (PropertyHandle->GetValue(CurrentShaderPlatformName))
		{
			for (const TSharedPtr<EShaderPlatform>& ValidationShaderPlatform : ValidationShaderPlatforms)
			{
				if (GetShaderPlatformName(ValidationShaderPlatform) == CurrentShaderPlatformName)
				{
					return ValidationShaderPlatform;
				}
			}
		}

		return TSharedPtr<EShaderPlatform>();
	}
};

FName FMaterialEditorValidationShaderPlatform::MaxRHIShaderPlatformName = MaxRHIShaderPlatformNameView.GetData();

void FMaterialEditorValidationShaderPlatform::RegisterCustomPropertyTypeLayout()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomPropertyTypeLayout(
		StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateLambda([](){return MakeShared<FMaterialEditorValidationPlatformCustomization>();})
	);
}

void FMaterialEditorValidationShaderPlatform::UnregisterCustomPropertyTypeLayout()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.UnregisterCustomPropertyTypeLayout(StaticStruct()->GetFName());
}
