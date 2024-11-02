
// EmmettJnr: Updated for UE5

#pragma once

#include <chrono>
#include <thread>
#include <map>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>

#pragma warning(disable: 4996)

DECLARE_LOG_CATEGORY_EXTERN(LogJoyShockLib, Log, All);

namespace JoyShockLib
{
	typedef uint8_t u8;
	typedef uint16_t u16;
	typedef uint32_t u32;
	typedef uint64_t u64;
	typedef int8_t s8;
	typedef int16_t s16;
	typedef int32_t s32;
	typedef int64_t s64;

	int16_t unsignedToSigned16(uint16_t n);

	int16_t uint16_to_int16(uint16_t a);

	uint16_t combine_uint8_t(uint8_t a, uint8_t b);

	int16_t combine_gyro_data(uint8_t a, uint8_t b);

	float clamp(float a, float min, float max);

	uint16_t clamp(uint16_t a, uint16_t min, uint16_t max);

	unsigned createMask(unsigned a, unsigned b);

	void hex_dump(unsigned char* buf, int len);

	void hex_dump2(unsigned char* buf, int len);

	void hex_dump_0(unsigned char* buf, int len);

	void int_dump(unsigned char* buf, int len);
}