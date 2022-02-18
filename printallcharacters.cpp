#include <iostream>
#include <stdio.h>
#include <stdlib.h>

#define FBLACK 0b00000000
#define FWHITE 0b00000111
#define BBLACK 0b00000000
#define BWHITE 0b01110000

void print(int color, char character) {
	int fg = (color & 0b00001111);
	int bg = (color & 0b11110000) >> 4;

	if (fg & 0b1000)
		fg += 82;
	else
		fg += 30;

	if (bg & 0b1000)
		bg += 92;
	else
		bg += 40;

	printf("\x1b[%i;%im%c", fg, bg, character);
}

int main() {
	for (int color = 0; color < 256; color++) {
		if (color == 128)
			printf("\n");
		print(color, '#');
	}

	print(FWHITE|BBLACK, '!');

	return 0;
}
