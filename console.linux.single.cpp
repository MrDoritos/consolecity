//Linux
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <ncurses.h>
#include <console.h>
#include <sys/ioctl.h>
#include <string>
#include <string.h>

//Define constructor
console::constructor console::cons;
winsize console::w;
color_t console::_activeColor;
bool console::ready;
bool console::useRefresh;

console::constructor::constructor() {	
	useRefresh = true;

	initscr();

	curs_set(0);
	set_escdelay(0);
	setlocale(LC_ALL, "");

	start_color();
	auto map_color = [](int i) {
		switch (i) {
			case COLOR_BLUE: return COLOR_RED;
			case COLOR_RED: return COLOR_BLUE;
			default: return i;
		}
	};
	//for (int i = 0; i < 256; i++) {
	//	init_pair(i + 1, map_color(i % 16), map_color((i / 16) % 16));
	//}

	for (int i = 0; i < 64; i++) {
		init_pair(i + 1, map_color(i % 8), map_color((i / 8) % 8));
	}
	
	//nocbreak(); //Disable ESC sequence checking. Might use timeout(0) exclusively for input
	//nocbreak and raw used simultaniously
	//otherwise use cbreak exclusively
	
	//raw();
	//Do not use raw, it is shit
	
	cbreak();
	
	noecho();
	keypad(stdscr, true);
	console::_activeColor = -1;
	ready = true;
}

console::constructor::~constructor() {
	endwin();
	ready = false;
}

int console::getImage() {
	return IMAGE_LINUX;
}

int console::readKey() {
	timeout(-1);
//	wint_t c;
//	get_wch(&c);
	return getch();
//	return c;
}

int console::readKeyAsync() {
	timeout(0);
	int key = getch();
	if (key == -1)
		key = 0;
	return key;
}

void console::sleep(int millis) {
	usleep(millis * 1000);
}

void console::_refreshSize() {
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
}

int console::getConsoleWidth() {
	console::_refreshSize();
	return int(w.ws_col);
}

int console::getConsoleHeight() {
	console::_refreshSize();
	return int(w.ws_row);
}

void console::setConsoleColor(color_t color) {
	const auto getCursesColor = [](color_t i) {
		short fg = i & 0b00000111, bg = i & 0b01110000;
		int c = 0;
		bg >>= 4;
		c = bg * 8;
		c += fg;
		return c + 1;
	};
	if (console::_activeColor == color)
		return;
	if (console::_activeColor != -1)
		attroff(COLOR_PAIR(getCursesColor(color)));
		//attroff(COLOR_PAIR((color & 0b00001111) + ((color & 0b11110000)) + 1));
	//attroff(COLOR_PAIR((console::_activeColor & 0b00001111) + ((console::_activeColor & 0b11110000)) + 1));
	//attroff(COLOR_PAIR(console::_activeColor + 1));
	//attron(COLOR_PAIR((color & 0b00001111) + ((color & 0b11110000)) + 1));
	//attron(COLOR_PAIR(color + 1));
	attron(COLOR_PAIR(getCursesColor(color)));
	console::_activeColor = color;
}

void console::clear() {
	wclear(getwin(stdout));
	refresh();
}

void console::setCursorLeft(int x) {}

void console::setCursorTop(int y) {}

void console::setCursorPosition(int x, int y) 	{
	move(y,x);
}

void console::write(int x, int y, std::string& str) {
	int w = console::getConsoleWidth();
	int h = console::getConsoleHeight();
	if (y >= h || x >= w)
		return;
	for (int i = 0; i < str.length() && i + x < w; i++) {
		mvaddch(y, x + i, str[i]);
	}
	if (useRefresh)
		refresh();
}

void console::write(int x, int y, const char* str) {
	console::write(x, y, str, _activeColor);
}

void console::write(int x, int y, const char* str, color_t c) {
	int w = console::getConsoleWidth();
	int h = console::getConsoleHeight();
	int l = strlen(str);
	if (y >= h || x >= w)
		return;
	for (int i = 0; i < l && i + x < w; i++) {
		setConsoleColor(c);
		mvaddch(y, x + i, str[i]);
	}

	if (useRefresh)
		refresh();
}

void console::write(int x, int y, std::string& str, color_t c) {
	int w = console::getConsoleWidth();
	int h = console::getConsoleHeight();
	if (y >= h || x >= w)
		return;
	for (int i = 0; i < str.length() && i + x < w; i++) {
		setConsoleColor(c);
		mvaddch(y, x + i, str[i]);
	}
	if (useRefresh)
		refresh();
}

void console::write(char* fb, color_t* cb, int length) {
	int i = 0;
	int w = console::getConsoleWidth();
	int h = console::getConsoleHeight();
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			console::setConsoleColor(cb[i]);
			mvaddch(y, x, fb[i++]);
		}
	}
	if (useRefresh)
		refresh();
}

//void console::write(wchar_t* fb, color_t* cb, int length) {
//	int i = 0;
//	int w = getConsoleWidth();
//	int h = getConsoleHeight();
//	for (int y = 0; y < h; y++) {
//		for (int x = 0; x < w; x++) {
//			setConsoleColor(cb[i]);
//			mvaddnwstr(y, x, &fb[i++], 1);
//		}
//	}
//	if (useRefresh)
//		refresh();
//}

//void console::write(int x, int y, wchar_t character) {
//	mvaddnwstr(y, x, &character, 1);	
//	refresh();
//}

//void console::write(int x, int y, wchar_t character, color_t color) {
//	console::setConsoleColor(color);
//	console::write(x, y, character);
//}

void console::write(int x, int y, char character) {
	mvaddch(y, x, character);	
	refresh();
}

void console::write(int x, int y, char character, color_t color) {
	console::setConsoleColor(color);
	console::write(x, y, character);
}











