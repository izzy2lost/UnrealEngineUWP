// Copyright Epic Games, Inc. All Rights Reserved.

#include "IHttpThreadedRequest.h"
#include "HttpModule.h"
#include "HttpManager.h"

void IHttpThreadedRequest::FinishRequestNotInHttpManager()
{
	if (IsInGameThread())
	{
		if (DelegateThreadPolicy == EHttpRequestDelegateThreadPolicy::CompleteOnGameThread)
		{
			FinishRequest();
		}
		else
		{
			FHttpModule::Get().GetHttpManager().AddHttpThreadTask([StrongThis = StaticCastSharedRef<IHttpThreadedRequest>(AsShared())]()
			{
				StrongThis->FinishRequest();
			});
		}
	}
	else
	{
		if (DelegateThreadPolicy == EHttpRequestDelegateThreadPolicy::CompleteOnHttpThread)
		{
			FinishRequest();
		}
		else
		{
			FHttpModule::Get().GetHttpManager().AddGameThreadTask([StrongThis = StaticCastSharedRef<IHttpThreadedRequest>(AsShared())]()
			{
				StrongThis->FinishRequest();
			});
		}
	}
}

