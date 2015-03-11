#include "util.h"

void print_byte_decimal(uint8_t byte, char *byte_str)
{
	if (byte > 99)
	{
		byte_str[0] = (byte / 100) + '0';
		byte -= (byte / 100) * 100;
	}
	else
	{
		byte_str[0] = '0';
	}

	if (byte > 9)
	{
		byte_str[1] = (byte / 10) + '0';
		byte -= (byte / 10) * 10;
	}
	else
	{
		byte_str[1] = '0';
	}

	byte_str[2] = byte + '0';
	byte_str[3] = '\0';
}