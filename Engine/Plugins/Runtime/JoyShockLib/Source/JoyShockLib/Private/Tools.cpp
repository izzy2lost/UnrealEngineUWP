
// EmmettJnr: Updated for UE5

#include "Tools.h"

DEFINE_LOG_CATEGORY(LogJoyShockLib);

int16_t JoyShockLib::unsignedToSigned16(uint16_t n) {
	uint16_t A = n;
	uint16_t B = 0xFFFF - A;
	if (A < B) {
		return (int16_t)A;
	}
	else {
		return (int16_t)(-1 * B);
	}
}

int16_t JoyShockLib::uint16_to_int16(uint16_t a) {
	int16_t b;
	char* aPointer = (char*)&a, * bPointer = (char*)&b;
	memcpy(bPointer, aPointer, sizeof(a));
	return b;
}

uint16_t JoyShockLib::combine_uint8_t(uint8_t a, uint8_t b) {
	uint16_t c = ((uint16_t)a << 8) | b;
	return c;
}

int16_t JoyShockLib::combine_gyro_data(uint8_t a, uint8_t b) {
	uint16_t c = combine_uint8_t(a, b);
	int16_t d = uint16_to_int16(c);
	return d;
}

float JoyShockLib::clamp(float a, float min, float max) {
	if (a < min) {
		return min;
	}
	else if (a > max) {
		return max;
	}
	else {
		return a;
	}
}

uint16_t JoyShockLib::clamp(uint16_t a, uint16_t min, uint16_t max) {
	if (a < min) {
		return min;
	}
	else if (a > max) {
		return max;
	}
	else {
		return a;
	}
}

unsigned JoyShockLib::createMask(unsigned a, unsigned b) {
	unsigned r = 0;
	for (unsigned i = a; i <= b; i++)
		r |= 1 << i;

	return r;
}

void JoyShockLib::hex_dump(unsigned char* buf, int len) {
	for (int i = 0; i < len; i++) {
		printf("%02x ", buf[i]);
	}
	printf("\n");
}

void JoyShockLib::hex_dump2(unsigned char* buf, int len) {
	for (int i = 0; i < len; i++) {
		printf("%02x ", buf[i]);
	}
}

void JoyShockLib::hex_dump_0(unsigned char* buf, int len) {
	for (int i = 0; i < len; i++) {
		if (buf[i] != 0) {
			printf("%02x ", buf[i]);
		}
	}
}

void JoyShockLib::int_dump(unsigned char* buf, int len) {
	for (int i = 0; i < len; i++) {
		printf("%i ", buf[i]);
	}
	printf("\n");
}
