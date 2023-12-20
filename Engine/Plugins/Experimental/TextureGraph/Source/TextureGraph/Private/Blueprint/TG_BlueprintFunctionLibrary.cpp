// Copyright Epic Games, Inc. All Rights Reserved.

#include "Blueprint/TG_BlueprintFunctionLibrary.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "TG_Graph.h"
#include "Expressions/TG_Expression.h"
#include "Expressions/Input/TG_Expression_Texture.h"
#include "Expressions/Input/TG_Expression_Scalar.h"
#include "Expressions/Input/TG_Expression_Vector.h"
#include "Expressions/Input/TG_Expression_Color.h"
#include "Expressions/Input/TG_Expression_OutputSettings.h"
#include "TG_HelperFunctions.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TG_BlueprintFunctionLibrary)

//////////////////////////////////////////////////////////////////////////
// UTG_BlueprintFunctionLibrary

#define LOCTEXT_NAMESPACE "TG_BlueprintFunctionLibrary"

void UTG_BlueprintFunctionLibrary::SetTextureParameterValue(UObject* WorldContextObject, UTextureGraph* InTextureGraph, FName ParameterName, UTexture* ParameterValue)
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		auto bFoundParameter = false;
		if (InTextureGraph)
		{
			auto PinParam = InTextureGraph->Graph()->FindParamPin(ParameterName);
			if (PinParam)
			{
				auto ExpressionPtr = Cast<UTG_Expression_Texture>(PinParam->GetNodePtr()->GetExpression());
				if (ExpressionPtr)
				{
					bFoundParameter = true;
					ExpressionPtr->SetAsset(ParameterValue);
				}
			}
		}

		if (!bFoundParameter)
		{
			AddParamWarning(ParameterName, InTextureGraph, "SetTextureParameterValue");
		}
	}
}

UTexture* UTG_BlueprintFunctionLibrary::GetTextureParameterValue(UObject* WorldContextObject, UTextureGraph* InTextureGraph, FName ParameterName)
{
	UTexture* ParameterValue = nullptr;
	
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		auto bFoundParameter = false;
		if (InTextureGraph)
		{
			auto PinParam = InTextureGraph->Graph()->FindParamPin(ParameterName);
			if (PinParam)
			{
				auto ExpressionPtr = Cast<UTG_Expression_Texture>(PinParam->GetNodePtr()->GetExpression());
				if (ExpressionPtr)
				{
					bFoundParameter = true;
					ParameterValue = ExpressionPtr->Source;
				}
			}
		}

		if (!bFoundParameter)
		{
			AddParamWarning(ParameterName, InTextureGraph,"GetTextureParameterValue");
		}
	}

	return ParameterValue;
}

void UTG_BlueprintFunctionLibrary::SetScalarParameterValue(UObject* WorldContextObject, UTextureGraph* InTextureGraph, FName ParameterName, float ParameterValue)
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		auto bFoundParameter = false;
		if (InTextureGraph)
		{
			auto PinParam = InTextureGraph->Graph()->FindParamPin(ParameterName);
			if (PinParam)
			{
				auto ExpressionPtr = Cast<UTG_Expression_Scalar>(PinParam->GetNodePtr()->GetExpression());
				if (ExpressionPtr)
				{
					ExpressionPtr->Scalar = ParameterValue;
					PinParam->SetValue(ParameterValue);
					bFoundParameter = true;
				}
			}
		}

		if (!bFoundParameter)
		{
			AddParamWarning(ParameterName, InTextureGraph, "SetScalarParameterValue");
		}
	}
}

float UTG_BlueprintFunctionLibrary::GetScalarParameterValue(UObject* WorldContextObject, UTextureGraph* InTextureGraph, FName ParameterName)
{
	float ParameterValue = 0.0f;

	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		auto bFoundParameter = false;
		if (InTextureGraph)
		{
			auto PinParam = InTextureGraph->Graph()->FindParamPin(ParameterName);
			if (PinParam)
			{
				bFoundParameter = PinParam->GetValue(ParameterValue);
			}
		}

		if (!bFoundParameter)
		{
			AddParamWarning(ParameterName, InTextureGraph, "GetScalarParameterValue");
		}
	}

	return ParameterValue;
}

void UTG_BlueprintFunctionLibrary::SetVectorParameterValue(UObject* WorldContextObject, UTextureGraph* InTextureGraph, FName ParameterName, FLinearColor ParameterValue)
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		auto bFoundParameter = false;
		if (InTextureGraph)
		{
			auto PinParam = InTextureGraph->Graph()->FindParamPin(ParameterName);
			if (PinParam)
			{
				auto ExpressionPtr = Cast<UTG_Expression_Vector>(PinParam->GetNodePtr()->GetExpression());
				if (ExpressionPtr)
				{
					ExpressionPtr->Vector = ParameterValue;
					PinParam->SetValue(ParameterValue);
					bFoundParameter = true;
				}
			}
		}

		if (!bFoundParameter)
		{
			AddParamWarning(ParameterName, InTextureGraph, "SetVectorParameterValue");
		}
	}
}

FLinearColor UTG_BlueprintFunctionLibrary::GetVectorParameterValue(UObject* WorldContextObject, UTextureGraph* InTextureGraph, FName ParameterName)
{
	FLinearColor ParameterValue;

	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		auto bFoundParameter = false;
		if (InTextureGraph)
		{
			auto PinParam = InTextureGraph->Graph()->FindParamPin(ParameterName);
			if (PinParam)
			{
				bFoundParameter = PinParam->GetValue(ParameterValue);
			}
		}

		if (!bFoundParameter)
		{
			AddParamWarning(ParameterName, InTextureGraph, "GetVectorParameterValue");
		}
	}

	return ParameterValue;
}

void UTG_BlueprintFunctionLibrary::SetColorParameterValue(UObject* WorldContextObject, UTextureGraph* InTextureGraph, FName ParameterName, FLinearColor ParameterValue)
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		auto bFoundParameter = false;
		if (InTextureGraph)
		{
			auto PinParam = InTextureGraph->Graph()->FindParamPin(ParameterName);
			if (PinParam)
			{
				auto ExpressionPtr = Cast<UTG_Expression_Color>(PinParam->GetNodePtr()->GetExpression());
				if (ExpressionPtr)
				{
					ExpressionPtr->Color = ParameterValue;
					PinParam->SetValue(ParameterValue);
					bFoundParameter = true;
				}
			}
		}

		if (!bFoundParameter)
		{
			AddParamWarning(ParameterName, InTextureGraph, "SetColorParameterValue");
		}
	}
}

FLinearColor UTG_BlueprintFunctionLibrary::GetColorParameterValue(UObject* WorldContextObject, UTextureGraph* InTextureGraph, FName ParameterName)
{
	FLinearColor ParameterValue;

	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		auto bFoundParameter = false;
		if (InTextureGraph)
		{
			auto PinParam = InTextureGraph->Graph()->FindParamPin(ParameterName);
			if (PinParam)
			{
				bFoundParameter = PinParam->GetValue(ParameterValue);
			}
		}

		if (!bFoundParameter)
		{
			AddParamWarning(ParameterName, InTextureGraph, "GetColorParameterValue");
		}
	}

	return ParameterValue;
}

void UTG_BlueprintFunctionLibrary::SetOutputSettingsParameterValue(UObject* WorldContextObject, UTextureGraph* InTextureGraph, FName ParameterName, int Width, int Height, FString FileName /*= "None"*/, FString FolderPath /*= "None"*/, ETG_TextureFormat Format /*= ETG_TextureFormat::BGRA8*/)
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		auto bFoundParameter = false;
		if (InTextureGraph)
		{
			auto PinParam = InTextureGraph->Graph()->FindParamPin(ParameterName);
			if (PinParam)
			{
				auto ExpressionPtr = Cast<UTG_Expression_OutputSettings>(PinParam->GetNodePtr()->GetExpression());
				if (ExpressionPtr)
				{
					FTG_OutputSettings ParameterValue;
					ParameterValue.Set(Width, Height, FileName, FolderPath, Format);
					ExpressionPtr->Settings = ParameterValue;
					PinParam->EditSelfVar()->EditAs<FTG_OutputSettings>() = ParameterValue;
					bFoundParameter = true;
				}
			}
		}

		if (!bFoundParameter)
		{
			AddParamWarning(ParameterName, InTextureGraph, "SetOutputSettingsParameterValue");
		}
	}
}

FTG_OutputSettings UTG_BlueprintFunctionLibrary::GetOutputSettingsParameterValue(UObject* WorldContextObject, UTextureGraph* InTextureGraph, FName ParameterName)
{
	FTG_OutputSettings ParameterValue;

	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		auto bFoundParameter = false;
		if (InTextureGraph)
		{
			auto PinParam = InTextureGraph->Graph()->FindParamPin(ParameterName);
			if (PinParam)
			{
				ParameterValue = PinParam->GetSelfVar()->GetAs<FTG_OutputSettings>();
				bFoundParameter = true;
			}
		}

		if (!bFoundParameter)
		{
			AddParamWarning(ParameterName, InTextureGraph, "GetOutputSettingsParameterValue");
		}
	}

	return ParameterValue;
}

void UTG_BlueprintFunctionLibrary::AddParamWarning(FName ParamName, UObject* ObjectPtr, FString FunctionName)
{
	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("ParamName"), FText::FromName(ParamName));
	FMessageLog("PIE").Warning()
		->AddToken(FTextToken::Create(FText::Format(LOCTEXT("{FunctionName}", "{FunctionName} called on"), FText::FromString(FunctionName))))
		->AddToken(FUObjectToken::Create(ObjectPtr))
		->AddToken(FTextToken::Create(FText::Format(LOCTEXT("WithInvalidParam", "with invalid ParameterName '{ParamName}'. This is likely due to a Blueprint error."), Arguments)));
}
#undef LOCTEXT_NAMESPACE

