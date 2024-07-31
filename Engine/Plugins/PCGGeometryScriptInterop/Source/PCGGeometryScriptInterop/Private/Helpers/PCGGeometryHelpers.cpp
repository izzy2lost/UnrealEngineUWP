// Copyright Epic Games, Inc. All Rights Reserved.

#include "Helpers/PCGGeometryHelpers.h"

#include "Utils/PCGLogErrors.h"

#include "GeometryScript/GeometryScriptTypes.h"

namespace PCGGeometryHelpers
{
	void GeometryScriptDebugToPCGLog(FPCGContext* Context, const UGeometryScriptDebug* Debug)
	{
		check(Context && Debug);

		for (const FGeometryScriptDebugMessage& Message : Debug->Messages)
		{
			if (Message.MessageType == EGeometryScriptDebugMessageType::ErrorMessage)
			{
				PCGLog::LogErrorOnGraph(Message.Message, Context);
			}
			else
			{
				PCGLog::LogWarningOnGraph(Message.Message, Context);
			}
		}
	}
}