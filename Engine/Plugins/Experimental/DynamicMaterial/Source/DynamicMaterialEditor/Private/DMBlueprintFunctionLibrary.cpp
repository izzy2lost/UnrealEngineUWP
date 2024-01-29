// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMBlueprintFunctionLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/DMMaterialLayer.h"
#include "Components/DMMaterialSlot.h"
#include "Components/DMMaterialStage.h"
#include "Components/DMMaterialStageBlend.h"
#include "Components/DMMaterialStageFunction.h"
#include "Components/DMMaterialStageGradient.h"
#include "Components/DMMaterialStageSource.h"
#include "Components/DMMaterialStageThroughput.h"
#include "Components/DMMaterialStageThroughputLayerBlend.h"
#include "Components/DMMaterialSubStage.h"
#include "Components/MaterialStageBlends/DMMSBNormal.h"
#include "Components/MaterialStageExpressions/DMMSETextureSample.h"
#include "Components/MaterialStageInputs/DMMSIExpression.h"
#include "Components/MaterialStageInputs/DMMSITextureUV.h"
#include "Components/MaterialStageInputs/DMMSIValue.h"
#include "Components/MaterialValues/DMMaterialValueTexture.h"
#include "Components/PrimitiveComponent.h"
#include "DMEDefs.h"
#include "DMObjectMaterialProperty.h"
#include "DMWorldSubsystem.h"
#include "DynamicMaterialEditorModule.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Material/DynamicMaterialInstance.h"
#include "Material/DynamicMaterialInstanceFactory.h"
#include "Materials/Material.h"
#include "Model/DynamicMaterialModel.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"
#include "Model/IDynamicMaterialModelEditorOnlyDataInterface.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

UDMMaterialStageInputValue* UDMBlueprintFunctionLibrary::FindDefaultStageOpacityInputValue(UDMMaterialStage* InStage)
{
	if (!IsValid(InStage))
	{
		return nullptr;
	}

	int32 OpacityInputIndex = -1;

	if (UDMMaterialStageThroughput* const ThroughputSource = Cast<UDMMaterialStageThroughput>(InStage->GetSource()))
	{
		const TArray<FDMMaterialStageConnector>& Connectors = ThroughputSource->GetInputConnectors();
		for (const FDMMaterialStageConnector& Connector : Connectors)
		{
			if (Connector.Name.ToString() == "Opacity")
			{
				OpacityInputIndex = Connector.Index;
			}
		}
	}

	const TArray<UDMMaterialStageInput*>& StageInputs = InStage->GetInputs();
	const TArray<FDMMaterialStageConnection>& InputConnectionMap = InStage->GetInputConnectionMap();

	if (InputConnectionMap.IsValidIndex(OpacityInputIndex) && InputConnectionMap[OpacityInputIndex].Channels.Num() > 0)
	{
		if (InputConnectionMap[OpacityInputIndex].Channels[0].SourceIndex >= FDMMaterialStageConnectorChannel::FIRST_STAGE_INPUT)
		{
			const int32 StageInputIndex = InputConnectionMap[OpacityInputIndex].Channels[0].SourceIndex - FDMMaterialStageConnectorChannel::FIRST_STAGE_INPUT;

			return Cast<UDMMaterialStageInputValue>(StageInputs[StageInputIndex]);
		}
	}

	return nullptr;
}

void UDMBlueprintFunctionLibrary::SetDefaultStageSourceTexture(UDMMaterialStage* InStage, UTexture* InTexture)
{
	if (!IsValid(InStage) || !IsValid(InTexture))
	{
		return;
	}

	UDMMaterialStageSource* StageSource = InStage->GetSource();
	if (!StageSource)
	{
		return;
	}

	auto SetTextureValue = [InTexture](UDMMaterialStageInputValue* NewInputValue)
	{
		if (UDMMaterialValueTexture* TextureValue = Cast<UDMMaterialValueTexture>(NewInputValue->GetValue()))
		{
			TextureValue->SetValue(InTexture);
		}
	};

	if (GUndo)
	{
		InStage->Modify();
	}

	if (UDMMaterialStageBlend* const Blend = Cast<UDMMaterialStageBlend>(StageSource))
	{
		UDMMaterialStageInputExpression* NewInput = InStage->ChangeInput_Expression(UDMMaterialStageBlend::InputB, FDMMaterialStageConnectorChannel::WHOLE_CHANNEL,
			UDMMaterialStageExpressionTextureSample::StaticClass(), 0, FDMMaterialStageConnectorChannel::WHOLE_CHANNEL);

		UDMMaterialSubStage* SubStage = NewInput->GetSubStage();
		check(SubStage);
		UDMMaterialStageInputValue* NewInputValue = SubStage->ChangeInput_NewLocalValue(0, FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, EDMValueType::VT_Texture, FDMMaterialStageConnectorChannel::WHOLE_CHANNEL);
		SetTextureValue(NewInputValue);

		return;
	}

	if (UDMMaterialStageThroughputLayerBlend* const LayerBlend = Cast<UDMMaterialStageThroughputLayerBlend>(StageSource))
	{
		const bool bHasAlpha = UE::DynamicMaterial::Private::HasAlpha(InTexture);

		UDMMaterialStageInputExpression* NewInput = InStage->ChangeInput_Expression(2, FDMMaterialStageConnectorChannel::WHOLE_CHANNEL,
			UDMMaterialStageExpressionTextureSample::StaticClass(), bHasAlpha ? 1 : 0, FDMMaterialStageConnectorChannel::WHOLE_CHANNEL);

		UDMMaterialSubStage* SubStage = NewInput->GetSubStage();
		check(IsValid(SubStage));
		UDMMaterialStageInputValue* NewInputValue = SubStage->ChangeInput_NewLocalValue(0, FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, EDMValueType::VT_Texture, FDMMaterialStageConnectorChannel::WHOLE_CHANNEL);
		SetTextureValue(NewInputValue);

		return;
	}

	UDMMaterialStageExpression* NewExpression = InStage->ChangeSource_Expression(UDMMaterialStageExpressionTextureSample::StaticClass());
	UDMMaterialStageInputValue* NewInputValue = InStage->ChangeInput_NewLocalValue(0, FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, EDMValueType::VT_Texture, FDMMaterialStageConnectorChannel::WHOLE_CHANNEL);
	SetTextureValue(NewInputValue);
}

TArray<FDMObjectMaterialProperty> UDMBlueprintFunctionLibrary::GetActorDynamicMaterialSlots(AActor* InActor, const bool bInIncludeEmptySlots)
{
	TArray<FDMObjectMaterialProperty> OutSlots;

	if (!IsValid(InActor))
	{
		return OutSlots;
	}

	InActor->ForEachComponent<UPrimitiveComponent>(false, [bInIncludeEmptySlots, &OutSlots](UPrimitiveComponent* InPrimComp)
		{
			const int32 PrimitiveMaterialCount = InPrimComp->GetNumMaterials();

			for (int32 MaterialIndex = 0; MaterialIndex < PrimitiveMaterialCount; ++MaterialIndex)
			{
				if (UDynamicMaterialInstance* const MDI = Cast<UDynamicMaterialInstance>(InPrimComp->GetMaterial(MaterialIndex)))
				{
					if (UDynamicMaterialModel* const ThisMaterialModel = MDI->GetMaterialModel())
					{
						OutSlots.Add(FDMObjectMaterialProperty(InPrimComp, MaterialIndex));
						continue;
					}
				}

				if (bInIncludeEmptySlots)
				{
					OutSlots.Add(FDMObjectMaterialProperty(InPrimComp, MaterialIndex));
				}
			}
		});

	return OutSlots;
}

TArray<FDMObjectMaterialProperty> UDMBlueprintFunctionLibrary::GetActorMaterialProperties(AActor* InActor)
{
	TArray<FDMObjectMaterialProperty> ActorProperties;

	if (!IsValid(InActor))
	{
		return ActorProperties;
	}

	FDMGetObjectMaterialPropertiesDelegate PropertyGenerator = FDynamicMaterialEditorModule::GetCustomMaterialPropertyGenerator(InActor->GetClass());

	if (PropertyGenerator.IsBound())
	{
		ActorProperties = PropertyGenerator.Execute(InActor);

		if (!ActorProperties.IsEmpty())
		{
			return ActorProperties;
		}
	}

	InActor->ForEachComponent<UPrimitiveComponent>(false, [&ActorProperties](UPrimitiveComponent* InComp)
		{
			for (int32 MaterialIdx = 0; MaterialIdx < InComp->GetNumMaterials(); ++MaterialIdx)
			{
				ActorProperties.Add({InComp, MaterialIdx});
			}
		});

	auto IterateProperies = [&ActorProperties](UObject* Outer)
	{
		if (!Outer || !Outer->GetClass())
		{
			return;
		}

		for (FProperty* OuterProperty : TFieldRange<FProperty>(Outer->GetClass()))
		{
			const EPropertyFlags Flags = OuterProperty->GetPropertyFlags();
			const bool bEditable = !!(Flags & CPF_Edit);
			const bool bEditConst = !!(Flags & CPF_EditConst);
			const bool bNoEditInstance = !!(Flags & CPF_DisableEditOnInstance);

			if (!bEditable || bEditConst || bNoEditInstance)
			{
				continue;;
			}

			if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(OuterProperty))
			{
				if (ObjectProperty->PropertyClass && ObjectProperty->PropertyClass->IsChildOf(UMaterialInterface::StaticClass()))
				{
					ActorProperties.Add(FDMObjectMaterialProperty(Outer, ObjectProperty));
				}
			}
			else if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(OuterProperty))
			{
				if (FObjectPropertyBase* ArrayObjectProperty = CastField<FObjectPropertyBase>(ArrayProperty->Inner))
				{
					if (ArrayObjectProperty->PropertyClass && ArrayObjectProperty->PropertyClass->IsChildOf(UMaterialInterface::StaticClass()))
					{
						FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(Outer));

						for (int Idx = 0, Count = ArrayHelper.Num(); Idx < Count; ++Idx)
						{
							ActorProperties.Add(FDMObjectMaterialProperty(Outer, OuterProperty, Idx));
						}
					}
				}
			}
		}
	};

	IterateProperies(InActor);

	InActor->ForEachComponent<UActorComponent>(false, [&IterateProperies](UActorComponent* InComp)
		{
			IterateProperies(InComp);
		});

	return ActorProperties;
}

UDynamicMaterialModel* UDMBlueprintFunctionLibrary::FindFirstValidActorMaterialSlot(const TArray<AActor*>& InActors)
{
	// Loop through selected actors to find first one with a valid material model on a primitive component.
	for (AActor* Actor : InActors)
	{
		if (!IsValid(Actor))
		{
			continue;
		}

		const TArray<FDMObjectMaterialProperty> ActorMaterialProperties = GetActorMaterialProperties(Actor);
		for (const FDMObjectMaterialProperty& ActorMaterialProperty : ActorMaterialProperties)
		{
			if (UDynamicMaterialModel* const ThisMaterialModel = ActorMaterialProperty.GetMaterialModel())
			{
				return ThisMaterialModel;
			}
		}
	}

	return nullptr;
}

FDMObjectMaterialProperty UDMBlueprintFunctionLibrary::MakePrimitiveComponentSlot(UPrimitiveComponent* InComponent, int32 InSlot)
{
	return FDMObjectMaterialProperty(InComponent, InSlot);
}

FDMObjectMaterialProperty UDMBlueprintFunctionLibrary::MakeObjectMaterialProperty(UObject* InObject, FName PropertyName)
{
	FProperty* Property = nullptr;

	if (IsValid(InObject))
	{
		Property = InObject->GetClass()->FindPropertyByName(PropertyName);
	}

	return FDMObjectMaterialProperty(InObject, Property, INDEX_NONE);
}

FDMObjectMaterialProperty UDMBlueprintFunctionLibrary::MakeObjectMaterialPropertyArray(UObject* InObject, FName PropertyName, int32 ArrayIndex)
{
	FProperty* Property = nullptr;

	if (IsValid(InObject))
	{
		Property = InObject->GetClass()->FindPropertyByName(PropertyName);
	}

	return FDMObjectMaterialProperty(InObject, Property, ArrayIndex);
}

UDynamicMaterialModel* UDMBlueprintFunctionLibrary::CreateDynamicMaterialInObject(FDMObjectMaterialProperty& InMaterialProperty)
{
	if (!InMaterialProperty.IsValid())
	{
		return nullptr;
	}

	UObject* const Outer = InMaterialProperty.OuterWeak.Get();

	UDynamicMaterialInstanceFactory* const InstanceFactory = NewObject<UDynamicMaterialInstanceFactory>();
	check(InstanceFactory);

	UDynamicMaterialInstance* const NewInstance = Cast<UDynamicMaterialInstance>(InstanceFactory->FactoryCreateNew(UDynamicMaterialInstance::StaticClass(),
		Outer, NAME_None, RF_Transactional, nullptr, GWarn));
	check(NewInstance);

	bool bSubsystemTakenOver = false;

	if (const UWorld* const World = Outer->GetWorld())
	{
		if (IsValid(World))
		{
			UDMWorldSubsystem* const WorldSubsystem = World->GetSubsystem<UDMWorldSubsystem>();

			if (IsValid(WorldSubsystem))
			{
				if (WorldSubsystem->GetMaterialValueSetterDelegate().IsBound())
				{
					bSubsystemTakenOver = WorldSubsystem->GetMaterialValueSetterDelegate().Execute(InMaterialProperty, NewInstance);
				}
			}
		}
	}

	if (!bSubsystemTakenOver)
	{
		InMaterialProperty.SetMaterial(NewInstance);
	}

	return NewInstance->GetMaterialModel();
}

UDynamicMaterialModel* UDMBlueprintFunctionLibrary::GetObjectPropertyMaterialModel(const FDMObjectMaterialProperty& InMaterialProperty) const
{
	return InMaterialProperty.GetMaterialModel();
}

UDynamicMaterialInstance* UDMBlueprintFunctionLibrary::GetObjectPropertyMaterial(const FDMObjectMaterialProperty& InMaterialProperty) const
{
	return InMaterialProperty.GetMaterial();
}

void UDMBlueprintFunctionLibrary::SetObjectPropertyMaterial(FDMObjectMaterialProperty& InMaterialProperty, UDynamicMaterialInstance* InDynamicMaterial)
{
	InMaterialProperty.SetMaterial(InDynamicMaterial);
}

bool UDMBlueprintFunctionLibrary::IsObjectPropertyValid(const FDMObjectMaterialProperty& InMaterialProperty) const
{
	return InMaterialProperty.IsValid();
}

FText UDMBlueprintFunctionLibrary::GetObjectPropertyName(const FDMObjectMaterialProperty& InMaterialProperty, bool bInIgnoreNewStatus) const
{
	return InMaterialProperty.GetPropertyName(bInIgnoreNewStatus);
}

bool UDMBlueprintFunctionLibrary::ExportMaterialInstance(UDynamicMaterialModel* InMaterialModel, const FString& InSavePath)
{
	if (!IsValid(InMaterialModel))
	{
		UE_LOG(LogDynamicMaterialEditor, Error, TEXT("Invalid material to export."));
		return false;
	}

	if (InSavePath.IsEmpty())
	{
		UE_LOG(LogDynamicMaterialEditor, Error, TEXT("Invalid material save path to export."));
		return false;
	}

	UDynamicMaterialInstance* Instance = InMaterialModel->GetDynamicMaterialInstance();
	if (!Instance)
	{
		UE_LOG(LogDynamicMaterialEditor, Error, TEXT("Failed to find a Material Designer Instance to export."));
		return false;
	}

	const FString PackagePath = FPaths::GetBaseFilename(*InSavePath, false);

	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package)
	{
		UE_LOG(LogDynamicMaterialEditor, Error, TEXT("Failed to create package for Material Designer Instance (%s)."), *PackagePath);
		return false;
	}

	const FString AssetName = FPaths::GetBaseFilename(*InSavePath, true);

	FObjectDuplicationParameters Params = InitStaticDuplicateObjectParams(Instance, Package, *AssetName,
		RF_Public | RF_Standalone, nullptr, EDuplicateMode::Normal, EInternalObjectFlags::None);

	UObject* NewAsset = StaticDuplicateObjectEx(Params);
	if (!NewAsset)
	{
		UE_LOG(LogDynamicMaterialEditor, Error, TEXT("Failed to create new Material Designer Instance asset."));
		return false;
	}

	// Not sure why these need to be set again!
	NewAsset->SetFlags(RF_Public | RF_Standalone);

	if (UDynamicMaterialInstance* NewInstance = Cast<UDynamicMaterialInstance>(NewAsset))
	{
		if (UDynamicMaterialModel* NewModel = NewInstance->GetMaterialModel())
		{
			if (IDynamicMaterialModelEditorOnlyDataInterface* ModelEditorOnlyData = NewModel->GetEditorOnlyData())
			{
				ModelEditorOnlyData->RequestMaterialBuild();
			}
		}
	}

	FAssetRegistryModule::AssetCreated(NewAsset);
	Package->FullyLoad();

	return true;
}

bool UDMBlueprintFunctionLibrary::ExportGeneratedMaterial(UDynamicMaterialModel* InMaterialModel, const FString& InSavePath)
{
	if (!IsValid(InMaterialModel))
	{
		UE_LOG(LogDynamicMaterialEditor, Error, TEXT("Invalid material to export."));
		return false;
	}

	if (InSavePath.IsEmpty())
	{
		UE_LOG(LogDynamicMaterialEditor, Error, TEXT("Invalid material save path to export."));
		return false;
	}

	UMaterial* GeneratedMaterial = InMaterialModel->GetGeneratedMaterial();
	if (!GeneratedMaterial)
	{
		UE_LOG(LogDynamicMaterialEditor, Error, TEXT("Failed to find a generated material to export."));
		return false;
	}

	const FString PackagePath = FPaths::GetBaseFilename(*InSavePath, false);

	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package)
	{
		UE_LOG(LogDynamicMaterialEditor, Error, TEXT("Failed to create package for exported material (%s)."), *PackagePath);
		return false;
	}

	const FString AssetName = FPaths::GetBaseFilename(*InSavePath, true);

	FObjectDuplicationParameters Params = InitStaticDuplicateObjectParams(GeneratedMaterial, Package, *AssetName,
		RF_Public | RF_Standalone, nullptr, EDuplicateMode::Normal, EInternalObjectFlags::None);

	UObject* NewAsset = StaticDuplicateObjectEx(Params);
	if (!NewAsset)
	{
		UE_LOG(LogDynamicMaterialEditor, Error, TEXT("Failed to create new material asset."));
		return false;
	}

	// Not sure why these need to be set again!
	NewAsset->SetFlags(RF_Public | RF_Standalone);

	FAssetRegistryModule::AssetCreated(NewAsset);
	Package->FullyLoad();

	return true;
}
