// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Containers/UnrealString.h"

enum class EDMMaterialPropertyType : uint8;
enum class EDMTextureSetMaterialProperty : uint8;
enum EMaterialProperty : int;

struct DYNAMICMATERIAL_API FDMUtils
{
#if WITH_EDITOR
	/** Designed to be used with preprocessor macros. */
	static FString CreateNodeComment(const ANSICHAR* InFile, int InLine, const ANSICHAR* InFunction, const FString* InComment = nullptr);
#endif

	static EDMTextureSetMaterialProperty MaterialPropertyTypeToTextureSetMaterialProperty(EDMMaterialPropertyType InPropertyType);

	static EDMMaterialPropertyType TextureSetMaterialPropertyToMaterialPropertyType(EDMTextureSetMaterialProperty InPropertyType);

	static EMaterialProperty MaterialPropertyTypeToMaterialProperty(EDMMaterialPropertyType InPropertyType);

	static EDMMaterialPropertyType MaterialPropertyToMaterialPropertyType(EMaterialProperty InPropertyType);
};

#if WITH_EDITOR
#define UE_DM_NodeComment_Default FDMUtils::CreateNodeComment(__FILE__, __LINE__, __FUNCTION__)
#define UE_DM_NodeComment(Comment) FDMUtils::CreateNodeComment(__FILE__, __LINE__, __FUNCTION__, &Comment)
#endif
