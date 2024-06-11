// Copyright Epic Games, Inc. All Rights Reserved.

#include "Scheduler/ScheduleInitializationContext.h"

#include "AnimNextModuleImpl.h"
#include "Modules/ModuleManager.h"
#include "Param/ObjectProxy.h"
#include "Param/ObjectProxyFactory.h"
#include "Param/PropertyBagProxy.h"
#include "Scheduler/ScheduleContext.h"

namespace UE::AnimNext
{

FScheduleInitializationContext::FScheduleInitializationContext(const FScheduleContext& InContext)
	: Context(InContext)
{
}

void FScheduleInitializationContext::ApplyParametersToScope(FName InScope, EParameterScopeOrdering InOrdering, const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InInstanceId, const FParameterSourceContext& InContext, TConstArrayView<FName> InRequiredParameters) const
{
	FAnimNextModuleImpl& AnimNextModule = FModuleManager::GetModuleChecked<FAnimNextModuleImpl>("AnimNext");
	TUniquePtr<IParameterSource> ParameterSource = AnimNextModule.CreateParameterSource(InContext, InInstanceId, InRequiredParameters);
	Context.GetInstanceData().ApplyParametersToScope(InScope, InOrdering, MoveTemp(ParameterSource));
}

void FScheduleInitializationContext::ApplyParametersToScope(FName InScope, EParameterScopeOrdering InOrdering, FName InInstanceId, TConstArrayView<FPropertyBagPropertyDesc> InPropertyDescs, TConstArrayView<TConstArrayView<uint8>> InValues) const
{
	check(InPropertyDescs.Num() == InValues.Num());
	if(InPropertyDescs.Num() > 0)
	{
		TUniquePtr<FPropertyBagProxy> PropertyBagProxy = MakeUnique<FPropertyBagProxy>(InInstanceId);
		PropertyBagProxy->ReplaceAllParameters(InPropertyDescs, InValues);
		Context.GetInstanceData().ApplyParametersToScope(InScope, InOrdering, MoveTemp(PropertyBagProxy));
	}
}

}