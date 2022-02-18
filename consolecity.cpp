#include "advancedConsole.h"

//Active Linux Colors
//FG 0-9
//BG Black Blue Green Yellow Red

//Zones
//0 Unzoned
//1 Residential
//2 Commercial
//3 Industrial
//4 Public park
//5 Road

struct tile_t {
	tile_t() {
		zone = 0;
	}
	int zone;
	int propertyValue;
	int landValue;
	int getPollution() {
		return (1.0f / (propertyValue * landValue)) * 1024;
	}
};

tile_t *gameMap;

int mapSize = 50;
int mapLength;
int viewX;
int viewY;

tile_t *getTile(int x, int y) {
	return &gameMap[y * mapSize + x];
}

void init() {
	mapLength = mapSize * mapSize;
	gameMap = new tile_t[mapLength];
	getTile(mapSize * 0.5f, mapSize * 0.5f)->zone = 1;
	getTile(0,0)->zone=2;
	viewX = -(adv::width * 0.5f);
	viewY = -(adv::height * 0.5f);
}

int main() {
	while (!adv::ready) console::sleep(10);

	init();

	adv::setThreadState(false);
	adv::setDoubleWidth(false);
	curs_set(1);

	int key;

	while (!HASKEY(key = console::readKeyAsync(), VK_ESCAPE)) {
		int centerX = adv::width * 0.5f;
		int centerY = adv::height * 0.5f;
		
		console::setCursorPosition(centerX, centerY);
		adv::clear();
		for (int x = 0; x < mapSize; x++) {
			for (int y = 0; y < mapSize; y++) {
				//if (x < 0 || x > mapSize || y < 0 || y > mapSize /*|| x > adv::width || y > adv::height*/)
				//	continue;
				tile_t *drawTile = getTile(x,y);
				char ch = '?';
				color_t co = FRED | BBLACK;
				switch (drawTile->zone) {
					case 0:
						ch = '.';
						co = FBLUE|BBLACK;
						break;
					case 1:
						ch = 'R';
						co = FBLACK|FGREEN;
						break;
					case 2:
						ch = 'C';
						co = FBLACK|FBLUE;
						break;
					case 3:
						ch = 'I';
						co = FBLACK|FYELLOW;
						break;
					case 4:
						ch = 'P';
						co = BGREEN|FBLACK;
						break;
					case 5:
						ch = '-';
						co = FWHITE | BBLACK;
						break;
				}
				
				if (x + viewX < 0 || x + viewX > adv::width || y + viewY < 0 || y + viewY > adv::height)
						continue;
				
				if (x == centerX +-viewX && y == centerY + -viewY)
					co <<= 4;
					
				adv::write(x + viewX,y + viewY,ch,co);
			}
		}
		
		char buf[101];
		snprintf(&buf[0], 100, "vx: %i vy: %i cx: %i cy: %i", viewX, viewY, centerX, centerY);
		adv::write(0,0,&buf[0]);
		
		adv::draw();
		//console::setCursorPosition(centerX, centerY);
		
		
		console::sleep(20);
		
		
		switch (NOMOD(key)) {
			case VK_LEFT:
				viewX++;
				break;
			case VK_RIGHT:
				viewX--;
				break;
			case VK_UP:
				viewY++;
				break;
			case VK_DOWN:
				viewY--;
				break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
				int x = centerX + -viewX, y = centerY + -viewY;
				if (x < 0 || x > mapSize || y < 0 || y > mapSize)
					break;
				getTile(x,y)->zone = key - '0';
				break;
		}
	}

	return 0;
}
