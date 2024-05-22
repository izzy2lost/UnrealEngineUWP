// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNodeMesh.h"
#include "Engine/SkeletalMesh.h"

#include "CustomizableObjectNodePassThroughMesh.generated.h"


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodePassThroughMesh : public UCustomizableObjectNodeMesh
{
public:
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Mesh, Meta = (DisplayName = Mesh, AllowedClasses = "/Script/Engine.StaticMesh, /Script/Engine.SkeletalMesh"))
	FSoftObjectPath Mesh;

	// UCustomizableObjectNodeMesh interface
	virtual TArray<UCustomizableObjectLayout*> GetLayouts(const UEdGraphPin& OutPin) const override { return {}; }
	virtual UTexture2D* FindTextureForPin(const UEdGraphPin* Pin) const override { return nullptr; }
	virtual void GetUVChannelForPin(const UEdGraphPin* Pin, TArray<FVector2f>& OutSegments, int32 UVIndex) const override {}
	virtual UStreamableRenderAsset* GetMesh() const override { return Cast<UStreamableRenderAsset>(Mesh.TryLoad()); }
	virtual UEdGraphPin* GetMeshPin(int32 LOD, int32 SectionIndex) const override { return {}; }
	virtual UEdGraphPin* GetLayoutPin(int32 LODIndex, int32 SectionIndex, int32 LayoutIndex) const override { return {}; }
	virtual void GetPinSection(const UEdGraphPin& Pin, int32& OutLODIndex, int32& OutSectionIndex, int32& OutLayoutIndex) const override {}

	// UCustomizableObjectNode interface
	virtual void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;

	// Begin EdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;
};
