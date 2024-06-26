// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

namespace PCGComputeConstants
{
	constexpr int MAX_NUM_ATTRS = 128;
	constexpr int NUM_RESERVED_ATTRS = 32; // Reserved for point properties, spline accessors, etc.
	constexpr int MAX_NUM_CUSTOM_ATTRS = MAX_NUM_ATTRS - NUM_RESERVED_ATTRS; // Reserved for custom attributes

	constexpr int ATTRIBUTE_HEADER_SIZE_BYTES = 8;

	constexpr int POINT_DATA_TYPE_ID = 0;
	constexpr int POINT_DATA_HEADER_PREAMBLE_SIZE_BYTES = 16; // 4 bytes for Type, 4 bytes for NumAttrs, 4 bytes for the offset, 4 bytes for TypeInfo
	constexpr int POINT_DATA_HEADER_SIZE_BYTES = POINT_DATA_HEADER_PREAMBLE_SIZE_BYTES + ATTRIBUTE_HEADER_SIZE_BYTES * MAX_NUM_ATTRS;

	constexpr int NUM_POINT_PROPERTIES = 9;
	constexpr int POINT_POSITION_ATTRIBUTE_ID = 0;
	constexpr int POINT_ROTATION_ATTRIBUTE_ID = 1;
	constexpr int POINT_SCALE_ATTRIBUTE_ID = 2;
	constexpr int POINT_BOUNDS_MIN_ATTRIBUTE_ID = 3;
	constexpr int POINT_BOUNDS_MAX_ATTRIBUTE_ID = 4;
	constexpr int POINT_COLOR_ATTRIBUTE_ID = 5;
	constexpr int POINT_DENSITY_ATTRIBUTE_ID = 6;
	constexpr int POINT_SEED_ATTRIBUTE_ID = 7;
	constexpr int POINT_STEEPNESS_ATTRIBUTE_ID = 8;
}
