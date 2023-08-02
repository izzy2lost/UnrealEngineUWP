// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IHasContext.h"
#include "InstancedStruct.h"
#include "IObjectChooser.h"
#include "ChooserPropertyAccess.generated.h"

#if WITH_EDITOR
struct FBindingChainElement;
#endif


namespace UE::Chooser
{
	struct FCompiledBindingElement
	{
		FCompiledBindingElement() : bIsFunction(false), Offset(0)
		{
		}
		
		explicit FCompiledBindingElement(int InOffset)
		{
			bIsFunction = false;
			Offset = InOffset;
		}
		
		explicit FCompiledBindingElement(UFunction* InFunction)
		{
			bIsFunction = true;
			Function = InFunction;
		}
		
		bool bIsFunction;
		union 
		{
			int Offset;
			UFunction* Function;
		};
	};

	// property type, for numerical conversions
	enum class EPropertyNumericalType
	{
		NONE,
		INT32,
		FLOAT,
		DOUBLE,
	};

	struct FCompiledBinding
	{
		int ContextIndex = 0;
		EPropertyNumericalType PropertyType = EPropertyNumericalType::NONE;
		// type info for number and enum conversions
		TArray<FCompiledBindingElement> CompiledChain;
		const UStruct* TargetType = nullptr;
#if WITH_EDITORONLY_DATA
		int SerialNumber = 0;
		TArray<const UStruct*> Dependencies;
#endif
	};
}


USTRUCT()
struct CHOOSER_API FChooserPropertyBinding 
{
	GENERATED_BODY()
	
	UPROPERTY()
	TArray<FName> PropertyBindingChain;
	
	UPROPERTY()
	int ContextIndex = -1;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FString DisplayName;

	FText CompileMessage;
#endif

	TSharedPtr<UE::Chooser::FCompiledBinding> CompiledBinding;

	FName GetUniqueId() const;

	void Compile(IHasContextClass* HasContext, bool bForce = false);

	
	template <typename T>
	bool GetValuePtr(FChooserEvaluationContext& Context, T*& Value) const;
	
	template <typename T>
	bool GetValue(FChooserEvaluationContext& Context, T& Value) const;
	
	template <typename T>
	bool SetValue(FChooserEvaluationContext& Context, const T& Value) const;
};

USTRUCT()
struct FChooserEnumPropertyBinding : public FChooserPropertyBinding
{
	GENERATED_BODY()
	
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<const UEnum> Enum = nullptr;
#endif
};

USTRUCT()
struct FChooserObjectPropertyBinding : public FChooserPropertyBinding
{
	GENERATED_BODY()

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UClass> AllowedClass = nullptr;
#endif
};

USTRUCT()
struct FChooserStructPropertyBinding : public FChooserPropertyBinding
{
	GENERATED_BODY()

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UScriptStruct> StructType = nullptr;
#endif
};

UENUM()
enum class EContextObjectDirection
{
	Read,
	Write,
	ReadWrite
};

USTRUCT()
struct FContextObjectTypeBase
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category="Type")
	EContextObjectDirection Direction = EContextObjectDirection::Read;
};


USTRUCT()
struct FContextObjectTypeClass : public FContextObjectTypeBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category="Type")
	TObjectPtr<UClass> Class;
};

USTRUCT()
struct FContextObjectTypeStruct : public FContextObjectTypeBase
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category="Type")
	TObjectPtr<UScriptStruct> Struct;
};

namespace UE::Chooser
{
	CHOOSER_API uint8* ResolveCompiledPropertyChain(FChooserEvaluationContext& Context, const FCompiledBinding& CompiledBinding);
	CHOOSER_API bool ResolvePropertyChain(const void*& Container, const UStruct*& StructType, const TArray<FName>& PropertyBindingChain);
	CHOOSER_API bool ResolvePropertyChain(FChooserEvaluationContext& Context, const FChooserPropertyBinding& Binding, const void*& OutContainer, const UStruct*& OutStructType);

#if WITH_EDITOR
	CHOOSER_API void CopyPropertyChain(const TArray<FBindingChainElement>& InBindingChain, FChooserPropertyBinding& OutPropertyBinding);
#endif
}

template <typename T>
bool FChooserPropertyBinding::GetValuePtr(FChooserEvaluationContext& Context, T*& OutResult) const
{
	using namespace  UE::Chooser;
	if (!CompiledBinding.IsValid())
	{
		return false;
	}

	const FCompiledBinding& Binding = *CompiledBinding.Get();
		
	if (uint8* Result = ResolveCompiledPropertyChain(Context, Binding))
	{
		const FCompiledBindingElement& Element = Binding.CompiledChain.Last();
		if (!Element.bIsFunction)
		{
			OutResult = reinterpret_cast<T*>(Result + Binding.CompiledChain.Last().Offset);
			return true;
		}
	}
	return false;
}

template <typename T>
bool FChooserPropertyBinding::GetValue(FChooserEvaluationContext& Context, T& OutResult) const
{
	using namespace  UE::Chooser;
	if (!CompiledBinding.IsValid())
	{
		return false;
	}

	const FCompiledBinding& Binding = *CompiledBinding.Get();
		
	if (uint8* Result = ResolveCompiledPropertyChain(Context, Binding))
	{
		const FCompiledBindingElement& Element = Binding.CompiledChain.Last();
		if (!Element.bIsFunction)
		{
			switch (Binding.PropertyType)
			{
			case EPropertyNumericalType::FLOAT:
				{
					float FloatResult = *reinterpret_cast<float*>(Result + Binding.CompiledChain.Last().Offset);
					OutResult = static_cast<T>(FloatResult);
					break;
				}
			case EPropertyNumericalType::DOUBLE:
				{
					double DoubleResult = *reinterpret_cast<double*>(Result + Binding.CompiledChain.Last().Offset);
					OutResult = static_cast<T>(DoubleResult);
					break;
				}
			case EPropertyNumericalType::INT32:
				{
					int32 IntResult = *reinterpret_cast<int*>(Result + Binding.CompiledChain.Last().Offset);
					OutResult = static_cast<T>(IntResult);
					break;
				}
			default:
				OutResult = *reinterpret_cast<T*>(Result + Binding.CompiledChain.Last().Offset);
				break;
			}
		}
		else
		{
			UObject* Object = reinterpret_cast<UObject*>(Result);
			if (Element.Function->IsNative())
			{
				FFrame Stack(Object, Element.Function, nullptr, nullptr, Element.Function->ChildProperties);
				switch (Binding.PropertyType)
				{
				case EPropertyNumericalType::FLOAT:
					{
						float FloatResult = 0;
						Element.Function->Invoke(Object, Stack, &FloatResult);
						OutResult = static_cast<T>(FloatResult);
						break;
					}
				case EPropertyNumericalType::DOUBLE:
					{
						double DoubleResult = 0;
						Element.Function->Invoke(Object, Stack, &DoubleResult);
						OutResult = static_cast<T>(DoubleResult);
						break;
					}
				case EPropertyNumericalType::INT32:
					{
						int32 IntResult = 0;
						Element.Function->Invoke(Object, Stack, &IntResult);
						OutResult = static_cast<T>(IntResult);
						break;
					}
				default:
					Element.Function->Invoke(Object, Stack, &OutResult);
					break;
				}
			}
			else
			{
				switch (Binding.PropertyType)
				{
				case EPropertyNumericalType::FLOAT:
					{
						float FloatResult = 0;
						Object->ProcessEvent(Element.Function, &FloatResult);
						OutResult = static_cast<T>(FloatResult);
						break;
					}
				case EPropertyNumericalType::DOUBLE:
					{
						double DoubleResult = 0;
						Object->ProcessEvent(Element.Function, &DoubleResult);
						OutResult = static_cast<T>(DoubleResult);
						break;
					}
				case EPropertyNumericalType::INT32:
					{
						int32 IntResult = 0;
						Object->ProcessEvent(Element.Function, &IntResult);
						OutResult = static_cast<T>(IntResult);
						break;
					}
				default:
					Object->ProcessEvent(Element.Function, &OutResult);
					break;
				}
			}
		}
		return true;
	}
	return false;
}

template <typename T>
bool FChooserPropertyBinding::SetValue(FChooserEvaluationContext& Context, const T& InValue) const
{
	using namespace  UE::Chooser;
	if (!CompiledBinding.IsValid())
	{
		return false;
	}

	const FCompiledBinding& Binding = *CompiledBinding.Get();
		
	if (uint8* Result = ResolveCompiledPropertyChain(Context, Binding))
	{
		const FCompiledBindingElement& Element = Binding.CompiledChain.Last();
		if (!Element.bIsFunction)
		{
			switch (Binding.PropertyType)
			{
			case EPropertyNumericalType::FLOAT:
				{
					*reinterpret_cast<float*>(Result + Binding.CompiledChain.Last().Offset) = static_cast<float>(InValue);
					break;
				}
			case EPropertyNumericalType::DOUBLE:
				{
					*reinterpret_cast<double*>(Result + Binding.CompiledChain.Last().Offset) = static_cast<double>(InValue);
					break;
				}
			case EPropertyNumericalType::INT32:
				{
					*reinterpret_cast<int*>(Result + Binding.CompiledChain.Last().Offset) = static_cast<int>(InValue);
					break;
				}
			default:
				*reinterpret_cast<T*>(Result + Binding.CompiledChain.Last().Offset) = InValue;
				break;
			}
			return true;
		}
	}
	return false;
}
