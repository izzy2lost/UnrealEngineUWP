// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewModel/MVVMInstancedViewModelGeneratedClass.h"

#include "INotifyFieldValueChanged.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MVVMInstancedViewModelGeneratedClass)

#if WITH_EDITOR
UClass* UMVVMInstancedViewModelGeneratedClass::GetAuthoritativeClass()
{
	return this;
}
#endif

void UMVVMInstancedViewModelGeneratedClass::Link(FArchive& Ar, bool bRelinkExistingProperties)
{
	Super::Link(Ar, bRelinkExistingProperties);

	for (TFieldIterator<FProperty> PropertyIter(this, EFieldIteratorFlags::ExcludeSuper); PropertyIter; ++PropertyIter)
	{
		if (!PropertyIter->RepNotifyFunc.IsNone())
		{
			if (!NativeFunctionLookupTable.ContainsByPredicate([ToFind = PropertyIter->RepNotifyFunc](const FNativeFunctionLookup& Other) { return Other.Name == ToFind; }))
			{
				NativeFunctionLookupTable.Emplace(PropertyIter->RepNotifyFunc, &UMVVMInstancedViewModelGeneratedClass::K2_CallNativeOnRep);
			}
			//NewFunction->RPCId = Params.RPCId;
			//NewFunction->RPCResponseId = Params.RPCResponseId;
		}
	}
}

DEFINE_FUNCTION(UMVVMInstancedViewModelGeneratedClass::K2_CallNativeOnRep)
{
	UObject* CallingObject = P_THIS_OBJECT;
	if (CallingObject == nullptr || !CallingObject->GetClass()->ImplementsInterface(UNotifyFieldValueChanged::StaticClass()))
	{
		return;
	}
	UMVVMInstancedViewModelGeneratedClass* GeneratedClass = Cast<UMVVMInstancedViewModelGeneratedClass>(CallingObject->GetClass());
	ensure(GeneratedClass);
	if (!GeneratedClass)
	{
		return;
	}

	FName PropertyName;
	{
		const UFunction* Func = Stack.CurrentNativeFunction;
		FString FunctionName = Func->GetName();
		if (FunctionName.RemoveFromStart(TEXT("__OnRep_")))
		{
			PropertyName = FName(*FunctionName, EFindName::FNAME_Find);
			if (!PropertyName.IsNone())
			{
				FProperty* FoundProperty = CallingObject->GetClass()->FindPropertyByName(PropertyName);
				if (FoundProperty)
				{
					GeneratedClass->OnPropertyReplicated(CallingObject, FoundProperty);
				}
			}
		}
	}
}

void UMVVMInstancedViewModelGeneratedClass::OnPropertyReplicated(UObject* Object, const FProperty* Property)
{
	TScriptInterface<INotifyFieldValueChanged> CallingInterface = Object;
	UE::FieldNotification::FFieldId FieldId = CallingInterface->GetFieldNotificationDescriptor().GetField(Object->GetClass(), Property->GetFName());
	if (FieldId.IsValid())
	{
		CallingInterface->BroadcastFieldValueChanged(FieldId);
	}
}
