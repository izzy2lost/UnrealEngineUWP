// Copyright Epic Games, Inc. All Rights Reserved.
#include "LookupProxy.h"
#include "ProxyTableFunctionLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ChooserPropertyAccess.h"

FLookupProxy::FLookupProxy()
{
	ProxyTable.InitializeAs(FProxyTableContextProperty::StaticStruct());
}

FObjectChooserBase::EIteratorStatus FLookupProxy::ChooseMulti(FChooserEvaluationContext& Context, FObjectChooserIteratorCallback Callback) const
{
	if (Proxy)
	{
		if (const FChooserParameterProxyTableBase* ProxyTableParameter = ProxyTable.GetPtr<FChooserParameterProxyTableBase>())
		{
			const UProxyTable* Table = nullptr;
			if (ProxyTableParameter->GetValue(Context, Table))
			{
				if (Table)
				{
					return Table->FindProxyObjectMulti(Proxy->Guid, Context, Callback);
				}
			}
		}
		// fallback codepath will look up the table from the property binding on the proxy asset
		return Proxy->FindProxyObjectMulti(Context, Callback);
	}
	return FObjectChooserBase::EIteratorStatus::Continue;
}

UObject* FLookupProxy::ChooseObject(FChooserEvaluationContext& Context) const
{
	if (Proxy)
	{
		if (const FChooserParameterProxyTableBase* ProxyTableParameter = ProxyTable.GetPtr<FChooserParameterProxyTableBase>())
		{
			const UProxyTable* Table = nullptr;
			if (ProxyTableParameter->GetValue(Context, Table))
			{
				if (Table)
				{
					return Table->FindProxyObject(Proxy->Guid, Context);
				}
			}
		}
		// fallback codepath will look up the table from the property binding on the proxy asset
		return Proxy->FindProxyObject(Context);
	}
	return nullptr;
}

UObject* FLookupProxyWithOverrideTable::ChooseObject(FChooserEvaluationContext& Context) const
{
	if (Proxy && OverrideProxyTable)
	{
		return OverrideProxyTable->FindProxyObject(Proxy->Guid, Context);
	}
	return nullptr;
}

FObjectChooserBase::EIteratorStatus FLookupProxyWithOverrideTable::ChooseMulti(FChooserEvaluationContext& Context, FObjectChooserIteratorCallback Callback) const
{
	if (Proxy && OverrideProxyTable)
	{
		return OverrideProxyTable->FindProxyObjectMulti(Proxy->Guid, Context, Callback);
	}
	return FObjectChooserBase::EIteratorStatus::Continue;
}

bool FProxyTableContextProperty::GetValue(FChooserEvaluationContext& Context, const UProxyTable*& OutResult) const
{
	if (Binding.CompiledBinding)
	{
		UProxyTable** ProxyTableReference;
		return Binding.GetValuePtr(Context, ProxyTableReference);
		OutResult = *ProxyTableReference;
	}
	else
	{
		// for temporary backwards compatibility: ProxyTableContextProperties on UProxyAsset are being phased out,
		// but they don't get compiled so we need to keep this code-path temporarily
		const UStruct* StructType = nullptr;
		const void* Container = nullptr;
	
		if (UE::Chooser::ResolvePropertyChain(Context, Binding,Container, StructType))
		{
			if (const FObjectProperty* Property = FindFProperty<FObjectProperty>(StructType, Binding.PropertyBindingChain.Last()))
			{
				OutResult = *Property->ContainerPtrToValuePtr<UProxyTable*>(Container);
				return true;
			}
		}
	}
	

	return false;
}

void FLookupProxy::Compile(IHasContextClass* HasContext, bool bForce)
{
	if (Proxy)
	{
		if (FChooserParameterBase* ProxyTableParam = ProxyTable.GetMutablePtr<FChooserParameterBase>())
		{
			// todo: should also validate here that the ProxyAsset context is compatible with the passed in HasContext
			ProxyTableParam->Compile(Proxy, bForce);
		}
	}
}
