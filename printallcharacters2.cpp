#include "console.h"

int main() {
	for (int i = 0; i < 256; i++)
		console::write(i, 0, '#', color_t(i));

	console::readKey();

	return 0;
}
