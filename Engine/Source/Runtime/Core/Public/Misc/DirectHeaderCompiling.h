// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// Used to allow control flow when direct header compiling to avoid issues with headers
// that typically expect to be included after another header
#ifndef UE_DIRECT_HEADER_COMPILING
	#define UE_DIRECT_HEADER_COMPILING(id) defined __COMPILING_##id
#endif