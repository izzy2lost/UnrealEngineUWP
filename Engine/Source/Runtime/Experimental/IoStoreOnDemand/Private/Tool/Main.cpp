// Copyright Epic Games, Inc. All Rights Reserved.

#if defined(UE_WITH_IAS_TOOL)

#include "Command.h"

namespace UE::IO::IAS::Tool {

////////////////////////////////////////////////////////////////////////////////
int32 Main(int32 ArgC, TCHAR* ArgV[])
{
	return FCommand::Main(ArgC, ArgV);
}

} // namespace UE::IO::IAS::Tool

#endif // UE_WITH_IAS_TOOL
