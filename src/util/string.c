#include <stdint.h>

int u64toa(uint64_t in, char* buffer) {
	int digits = 0;
	char* p = nullptr;

	do {
		int dig = in % 10;
        in /= 10;
		buffer[digits++] = '0' + dig;
	} while (in);

	buffer[digits] = 0;

	for (p = buffer + digits - 1; p > buffer; buffer++, p--) {
		char dig = *buffer;
		*buffer = *p;
		*p = dig;
	}

    return digits;
}
