// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/ObjectProxyFactory.h"

#include "Param/ClassProxy.h"
#include "ObjectProxy.h"
#include "Param/AnimNextParamUniversalObjectLocator.h"

namespace UE::AnimNext
{

FObjectProxyFactory::FObjectProxyFactory()
{
#if WITH_EDITOR
	OnObjectsReinstancedHandle = FCoreUObjectDelegates::OnObjectsReinstanced.AddLambda([this](const FCoreUObjectDelegates::FReplacementObjectMap& InMap)
	{
		Refresh();
	});
#endif
}

FObjectProxyFactory::~FObjectProxyFactory()
{
#if WITH_EDITOR
	FCoreUObjectDelegates::OnObjectsReinstanced.Remove(OnObjectsReinstancedHandle);
#endif
}

void FObjectProxyFactory::Refresh()
{
	// Remove any invalid classes, refresh any valid ones
	for(auto It = ClassMap.CreateIterator(); It; ++It)
	{
		const UClass* Class = It.Key().ResolveObjectPtr();
		if(Class == nullptr || Class->HasAllClassFlags(CLASS_NewerVersionExists))
		{
			It.RemoveCurrent();
		}
		else
		{
			It.Value()->Refresh(Class);
		}
	}
}

TUniquePtr<IParameterSource> FObjectProxyFactory::CreateParameterSource(const FParameterSourceContext& InContext, const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InInstanceId, TConstArrayView<FName> InRequiredParameters) const
{
	using namespace UE::UniversalObjectLocator;

	if(const FAnimNextParamUniversalObjectLocator* Locator = InInstanceId.GetPtr<FAnimNextParamUniversalObjectLocator>())
	{
		// Resolve locator
		FResolveParams ResolveParams(const_cast<UObject*>(InContext.Object));
		FResolveResult Result = Locator->Locator.Resolve(ResolveParams);
		FResolveResultData ResultData = Result.SyncGet();

		// We shouldn't be loading as part of this call - if this hits we need to consider loading objects up front somehow
		check(!(ResultData.Flags.bWasLoaded || ResultData.Flags.bWasLoadedIndirectly));	

		if(ResultData.Object)
		{
			TStringBuilder<256> ScopeAsString;
			Locator->Locator.ToString(ScopeAsString);
			TSharedRef<FClassProxy> ClassProxy = FindOrCreateClassProxy(ResultData.Object->GetClass());
			TUniquePtr<FObjectProxy> ObjectProxy = MakeUnique<FObjectProxy>(ResultData.Object, ScopeAsString, ClassProxy);
			ObjectProxy->RequestParameterCache(InRequiredParameters);

			return MoveTemp(ObjectProxy);
		}
	}

	return nullptr;
}

TSharedRef<FClassProxy> FObjectProxyFactory::FindOrCreateClassProxy(const UClass* InClass) const 
{
	UE_MT_SCOPED_WRITE_ACCESS(ObjectAccessorsAccessDetector);
	if(TSharedPtr<FClassProxy>* FoundProxy = ClassMap.Find(InClass))
	{
		return FoundProxy->ToSharedRef();
	}

	return ClassMap.Add(InClass, MakeShared<FClassProxy>(InClass)).ToSharedRef();
}

}
