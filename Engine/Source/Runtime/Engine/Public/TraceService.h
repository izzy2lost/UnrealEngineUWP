// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include <Templates/PimplPtr.h>

class FTraceService
{
public:
	ENGINE_API FTraceService();
	virtual ~FTraceService() {}
	
private:
	TPimplPtr<class FTraceServiceImpl> Impl;
};
