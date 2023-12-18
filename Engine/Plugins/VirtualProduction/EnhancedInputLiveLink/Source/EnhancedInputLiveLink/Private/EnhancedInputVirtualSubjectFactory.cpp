// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnhancedInputVirtualSubjectFactory.h"
#include "EnhancedInputVirtualSubject.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EnhancedInputVirtualSubjectFactory)

#define LOCTEXT_NAMESPACE "EnhancedInputVirtualSubjectFactory"


UEnhancedInputVirtualSubjectFactory::UEnhancedInputVirtualSubjectFactory()
{
	SupportedClass = UBlueprint::StaticClass();
	ParentClass = UEnhancedInputVirtualSubject::StaticClass();
}

FText UEnhancedInputVirtualSubjectFactory::GetDisplayName() const
{
	return LOCTEXT("EnhancedInputVirtualSubjectName", "Enhanced Input Virtual Subject");
}

#undef LOCTEXT_NAMESPACE