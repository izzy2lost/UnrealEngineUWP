// Copyright Epic Games, Inc. All Rights Reserved.

#include "ObjectProxy.h"
#include "Param/ClassProxy.h"
#include "Param/ParamStack.h"

namespace UE::AnimNext
{

FObjectProxy::FObjectProxy(const UObject* InObject, const TSharedRef<FClassProxy>& InClassProxy)
	: Object(InObject)
	, ClassProxy(InClassProxy)
	, RootParameterName(NAME_None)
{
}

FObjectProxy::FObjectProxy(const UObject* InObject, FStringView InObjectLocatorPath, const TSharedRef<FClassProxy>& InClassProxy)
	: Object(InObject)
	, ClassProxy(InClassProxy)
	, RootParameterName(InObjectLocatorPath)
{
	// Always supply the root parameter in index 0
	ParameterCache.AddProperty(RootParameterName, EPropertyBagPropertyType::Object, InObject->GetClass());
	ParameterCache.SetValueObject(RootParameterName, InObject);
}

FName FObjectProxy::GetInstanceId() const
{
	return RootParameterName;
}

void FObjectProxy::Update(float DeltaTime)
{
	const UPropertyBag* PropertyBag = ParameterCache.GetPropertyBagStruct();
	TConstArrayView<FPropertyBagPropertyDesc> PropertyDescs = PropertyBag->GetPropertyDescs();
	uint8* StructData = ParameterCache.GetMutableValue().GetMemory();
	UObject* NonConstObject = const_cast<UObject*>(Object.Get());
	if(RootParameterName != NAME_None)
	{
		PropertyDescs[0].CachedProperty->SetValue_InContainer(StructData, &Object);
	}

	if(Object != nullptr)
	{
		for(int32 ParameterIndex = 0; ParameterIndex < ParametersToUpdate.Num(); ++ParameterIndex)
		{
			const FAnimNextObjectProxyParameter& ParameterToUpdate = ParametersToUpdate[ParameterIndex];
			const FProperty* ResultProperty = PropertyDescs[ParameterToUpdate.ValueParamIndex].CachedProperty;
			void* ResultBuffer = ResultProperty->ContainerPtrToValuePtr<void>(StructData);

			switch(ParameterToUpdate.AccessType)
			{
			case EClassProxyParameterAccessType::Property:
				{
					const FProperty* SourceProperty = ParameterToUpdate.GetProperty();
					checkSlow(SourceProperty);
					checkSlow(SourceProperty->GetClass() == ResultProperty->GetClass());

					const void* SourceBuffer = SourceProperty->ContainerPtrToValuePtr<const void>(Object);
					SourceProperty->CopyCompleteValue(ResultBuffer, SourceBuffer);
					break;
				}
			case EClassProxyParameterAccessType::AccessorFunction:
				{
					UFunction* Function = ParameterToUpdate.GetFunction();
					checkSlow(Function);
					checkSlow(Object->GetClass()->IsChildOf(Function->GetOuterUClass()));

					FFrame Stack(NonConstObject, Function, nullptr, nullptr, Function->ChildProperties);
					Function->Invoke(NonConstObject, Stack, ResultBuffer);
					break;
				}
			case EClassProxyParameterAccessType::HoistedFunction:
				{
					UFunction* Function = ParameterToUpdate.GetFunction();
					checkSlow(Function);

					FFrame Stack(NonConstObject, Function, &NonConstObject, nullptr, Function->ChildProperties);
					Function->Invoke(NonConstObject, Stack, ResultBuffer);
					break;
				}
			default:
				checkNoEntry();
				break;
			}
		}
	}
}

void FObjectProxy::RequestParameterCache(TConstArrayView<FName> InParameterNames)
{
	using namespace UE::AnimNext;

	const int32 NumExistingProperties = ParameterCache.GetNumPropertiesInBag();

	TArray<FPropertyBagPropertyDesc> PropertyDescsToAdd;
	PropertyDescsToAdd.Reserve(InParameterNames.Num());

	for(FName ParameterName : InParameterNames)
	{
		if(!ParameterNameMap.Contains(ParameterName))
		{
			if(const int32* ParameterIndexPtr = ClassProxy->ParameterNameMap.Find(ParameterName))
			{
				const FClassProxyParameter& ClassProxyParameter = ClassProxy->Parameters[*ParameterIndexPtr];

				FAnimNextObjectProxyParameter& NewParameterToUpdate = ParametersToUpdate.AddDefaulted_GetRef();
				NewParameterToUpdate.AccessType = ClassProxyParameter.AccessType;
				NewParameterToUpdate.Function = ClassProxyParameter.Function.Get();
				NewParameterToUpdate.Property = ClassProxyParameter.Property.Get();

				int32 ValueParamIndex = PropertyDescsToAdd.Emplace(ParameterName, ClassProxyParameter.Type.GetContainerType(), ClassProxyParameter.Type.GetValueType(), ClassProxyParameter.Type.GetValueTypeObject());
				NewParameterToUpdate.ValueParamIndex = NumExistingProperties + ValueParamIndex;
				ParameterNameMap.Add(ParameterName, NewParameterToUpdate.ValueParamIndex);
			}
		}
	}

	// Update parameter bag struct
	ParameterCache.AddProperties(PropertyDescsToAdd);

	// Recreate layer handle as layout has changed
	LayerHandle = FParamStack::MakeReferenceLayer(RootParameterName, ParameterCache);
}

void FObjectProxy::RequestParameterCacheAlias(TConstArrayView<TTuple<FName, FName>> InParameterNamePairs)
{
	using namespace UE::AnimNext;

	const int32 NumExistingProperties = ParameterCache.GetNumPropertiesInBag();

	TArray<FPropertyBagPropertyDesc> PropertyDescsToAdd;
	PropertyDescsToAdd.Reserve(InParameterNamePairs.Num());

	for(const TTuple<FName, FName>& ParameterNamePair : InParameterNamePairs)
	{
		if(!ParameterNameMap.Contains(ParameterNamePair.Get<0>()))
		{
			if(const int32* ParameterIndexPtr = ClassProxy->ParameterNameMap.Find(ParameterNamePair.Get<0>()))
			{
				const FClassProxyParameter& ClassProxyParameter = ClassProxy->Parameters[*ParameterIndexPtr];

				FAnimNextObjectProxyParameter& NewParameterToUpdate = ParametersToUpdate.AddDefaulted_GetRef();
				NewParameterToUpdate.AccessType = ClassProxyParameter.AccessType;
				NewParameterToUpdate.Function = ClassProxyParameter.Function.Get();
				NewParameterToUpdate.Property = ClassProxyParameter.Property.Get();

				int32 ValueParamIndex = PropertyDescsToAdd.Emplace(ParameterNamePair.Get<1>(), ClassProxyParameter.Type.GetContainerType(), ClassProxyParameter.Type.GetValueType(), ClassProxyParameter.Type.GetValueTypeObject());
				NewParameterToUpdate.ValueParamIndex = NumExistingProperties + ValueParamIndex;
				ParameterNameMap.Add(ParameterNamePair.Get<0>(), NewParameterToUpdate.ValueParamIndex);
			}
		}
	}

	// Update parameter bag struct
	ParameterCache.AddProperties(PropertyDescsToAdd);

	// Recreate layer handle as layout has changed
	LayerHandle = FParamStack::MakeReferenceLayer(RootParameterName, ParameterCache);
}

void FObjectProxy::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(Object);
	ParameterCache.AddStructReferencedObjects(Collector);
}

}