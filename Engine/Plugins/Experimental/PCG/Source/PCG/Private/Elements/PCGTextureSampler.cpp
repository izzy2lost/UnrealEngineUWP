// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGTextureSampler.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGCrc.h"
#include "Helpers/PCGBlueprintHelpers.h"
#include "Helpers/PCGHelpers.h"
#include "Helpers/PCGSettingsHelpers.h"

#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGTextureSampler)

#define LOCTEXT_NAMESPACE "PCGTextureSamplerElement"

TArray<FPCGPinProperties> UPCGTextureSamplerSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;
	Properties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Texture);

	return Properties;
}

FPCGElementPtr UPCGTextureSamplerSettings::CreateElement() const
{
	return MakeShared<FPCGTextureSamplerElement>();
}

bool FPCGTextureSamplerElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGTextureSamplerElement::Execute);

	FPCGTextureSamplerContext* Context = static_cast<FPCGTextureSamplerContext*>(InContext);

	if (Context->bIsPaused)
	{
		return false;
	}

	if (Context->bTextureReadbackDone)
	{
		return true;
	}

	const UPCGTextureSamplerSettings* Settings = Context->GetInputSettings<UPCGTextureSamplerSettings>();
	check(Settings);

	if (Settings->Texture.IsNull())
	{
		return true;
	}

	UTexture2D* Texture = Settings->Texture.LoadSynchronous();

	if (!Texture)
	{
		PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("CouldNotResolveTexture", "Texture at path '{0}' could not be loaded"), FText::FromString(Settings->Texture.ToString())));
		return true;
	}

	if (!UPCGTextureData::IsSupported(Texture))
	{
		PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("UnsupportedTextureFormat", "Texture '{0}' has unsupported settings"), FText::FromName(Texture->GetFName())));
		return true;
	}

	const FTransform& Transform = Settings->Transform;
	const bool bUseAbsoluteTransform = Settings->bUseAbsoluteTransform;
	const EPCGTextureDensityFunction DensityFunction = Settings->DensityFunction;
	const EPCGTextureColorChannel ColorChannel = Settings->ColorChannel;
	const float TexelSize = Settings->TexelSize;
	const bool bUseAdvancedTiling = Settings->bUseAdvancedTiling;
	const FVector2D& Tiling = Settings->Tiling;
	const FVector2D& CenterOffset = Settings->CenterOffset;
	const float Rotation = Settings->Rotation;
	const bool bUseTileBounds = Settings->bUseTileBounds;
	const FVector2D& TileBoundsMin = Settings->TileBoundsMin;
	const FVector2D& TileBoundsMax = Settings->TileBoundsMax;

	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
	FPCGTaggedData& Output = Outputs.Emplace_GetRef();

	UPCGTextureData* TextureData = NewObject<UPCGTextureData>();
	Output.Data = TextureData;

	AActor* OriginalActor = UPCGBlueprintHelpers::GetOriginalComponent(*Context)->GetOwner();
	FTransform FinalTransform = Transform;
	if (!bUseAbsoluteTransform)
	{
		FTransform OriginalActorTransform = OriginalActor->GetTransform();
		FinalTransform = Transform * OriginalActorTransform;

		FBox OriginalActorLocalBounds = PCGHelpers::GetActorLocalBounds(OriginalActor);
		FinalTransform.SetScale3D(FinalTransform.GetScale3D() * 0.5 * (OriginalActorLocalBounds.Max - OriginalActorLocalBounds.Min));
	}

	// Initialize & set properties
	Context->bIsPaused = true;

	auto PostInitializeCallback = [Context, TextureData]()
	{
		Context->bIsPaused = false;
		Context->bTextureReadbackDone = true;

		if (!TextureData->IsValid())
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, LOCTEXT("TextureDataInitFailed", "Texture data failed to initialize, check log for more information"));
		}
	};

	TextureData->Initialize(Texture, FinalTransform, PostInitializeCallback);

	TextureData->DensityFunction = DensityFunction;
	TextureData->ColorChannel = ColorChannel;
	TextureData->TexelSize = TexelSize;
	TextureData->bUseAdvancedTiling = bUseAdvancedTiling;
	TextureData->Tiling = Tiling;
	TextureData->CenterOffset = CenterOffset;
	TextureData->Rotation = Rotation;
	TextureData->bUseTileBounds = bUseTileBounds;
	TextureData->TileBounds = FBox2D(TileBoundsMin, TileBoundsMax);

	return false;
}

FPCGContext* FPCGTextureSamplerElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGTextureSamplerContext* Context = new FPCGTextureSamplerContext();
	Context->InputData = InputData;
	Context->SourceComponent = SourceComponent;
	Context->Node = Node;

	return Context;
}

void FPCGTextureSamplerElement::GetDependenciesCrc(const FPCGDataCollection& InInput, const UPCGSettings* InSettings, UPCGComponent* InComponent, FPCGCrc& OutCrc) const
{
	FPCGCrc Crc;
	FSimplePCGElement::GetDependenciesCrc(InInput, InSettings, InComponent, Crc);

	if (const UPCGTextureSamplerSettings* Settings = Cast<UPCGTextureSamplerSettings>(InSettings))
	{
		// If not using absolute transform, depend on actor transform and bounds, and therefore take dependency on actor data.
		bool bUseAbsoluteTransform;
		PCGSettingsHelpers::GetOverrideValue(InInput, Settings, GET_MEMBER_NAME_CHECKED(UPCGTextureSamplerSettings, bUseAbsoluteTransform), Settings->bUseAbsoluteTransform, bUseAbsoluteTransform);
		if (!bUseAbsoluteTransform && InComponent)
		{
			if (const UPCGData* Data = InComponent->GetActorPCGData())
			{
				Crc.Combine(Data->GetOrComputeCrc(/*bFullDataCrc=*/false));
			}
		}
	}

	OutCrc = Crc;
}

#undef LOCTEXT_NAMESPACE
