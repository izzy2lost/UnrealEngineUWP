// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlayAnim/K2Node_PlayAnim.h"

#include "PlayAnim/PlayAnimCallbackProxy.h"

#define LOCTEXT_NAMESPACE "K2Node_PlayAnim"

UK2Node_PlayAnim::UK2Node_PlayAnim(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ProxyFactoryFunctionName = GET_FUNCTION_NAME_CHECKED(UPlayAnimCallbackProxy, CreateProxyObjectForPlayAnim);
	ProxyFactoryClass = UPlayAnimCallbackProxy::StaticClass();
	ProxyClass = UPlayAnimCallbackProxy::StaticClass();
}

FText UK2Node_PlayAnim::GetTooltipText() const
{
	return LOCTEXT("K2Node_PlayAnim_Tooltip", "Plays an Animation object on an AnimNextComponent");
}

FText UK2Node_PlayAnim::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("PlayAnim", "Play Animation");
}

FText UK2Node_PlayAnim::GetMenuCategory() const
{
	return LOCTEXT("PlayAnimCategory", "Animation|AnimNext");
}

#undef LOCTEXT_NAMESPACE
