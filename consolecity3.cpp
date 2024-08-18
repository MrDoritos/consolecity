#include "../console/advancedConsole.h"
#include "../imgcat/colorMappingFast.h"
#define STB_IMAGE_IMPLEMENTATION
#include "../imgcat/stb_image.h"
#include <math.h>
#include <chrono>
#include <vector>
#include <set>
#include <iterator>

struct default_tile;
struct grass;
struct plop_instance;
struct plop;
struct tile;
struct tilePartial;
struct tileComplete;

tilePartial *getPartial(int, int);
tileComplete getComplete(int, int);
tile *getTile(tilePartial*);

#define ZONING_NONE 0
#define ZONING_RESIDENTIAL 1
#define ZONING_COMMERCIAL 2
#define ZONING_INDUSTRIAL 3
#define TILE_COUNT 12
#define NORTH 1
#define EAST 2
#define SOUTH 4
#define WEST 8
#define NORTH_F x,y-1
#define EAST_F x+1,y
#define SOUTH_F x,y+1
#define WEST_F x-1,y
#define UNDERGROUND_WATER_PIPE 1
#define UNDERGROUND_SUBWAY 2

struct tiles {
	static tile *tileRegistry[TILE_COUNT];
	static int id;
	
	static void add(tile *tile);
	static tile *get(int id);
	
	static tile *DEFAULT_TILE;
	static tile *GRASS;
	static tile *WATER_PIPE;
	static tile *DIRT;
};

/*
octet 0 : 00 - none
			 00 - underground
		0	 0000 - connection
octet 1 : 
			 00 - none
		5	 0 -
	    4   0 - road
		2	 00 - facing
		1	 0 - water
		0   0 - power
octet 2 : 00
			 00
			 00
			 00 - water pipe connection
octet 3 : boolean reserved
		  7 0 - constructed (zone)
		  6 0 - is plop
octet 4 : buildings
		     0000 - building id
			 000 - construction animation
			 0 - master
octet 5 : population
			 0000 - inhabitants / jobs

*/

/*
Utilities
Power - oil, coal, solar, wind, nuclear, power line
Water - water tower, water pump, large water pump, water treatment plant, water pipe
Garbage - landfill, recycling plant, garbage power plant

Mass transit
Bus stop, subway station, train station, freight station
Subway pipe, rail

Roads
Road, Street, Avenue, Highway, One way street

Zoning
Commercial L/M/H
Industrial A/M/H
Residential L/M/H

Civics
Clinic, hospital
Elementary school, high school, college
Fire station, police station
*/

/*
Struct for data stored within the game engine tile map
*/
struct tilePartial {
	tilePartial() { id = 0; data.b = 0; }
	unsigned char id;
	union {
		unsigned char a[8];
		unsigned long b;
		unsigned int c[2];
	} data;
	tilePartial transferProperties(tilePartial in) {
		in.data.a[0] |= data.a[0] & 0b00110000;
		in.data.a[1] |= data.a[1] & 0b00000011;
		in.data.a[2] |= data.a[2] & 0b00000011;
		return in;
	}
	int getBuildingId() {
		return int(data.a[4] & 0b11110000) >> 4;
	}
	void setBuildingId(int id) {
		data.a[4] &= data.a[4] ^ 0b11110000;
		data.a[4] |= (id & 0b1111) << 4;
	}
	float getAnimationProgress() {
		return float(data.a[4] & 0b00001110 >> 1) / 7.0f;
	}
	void setAnimationProgress(float norm) {
		data.a[4] &= data.a[4] ^ 0b00001110;
		data.a[4] |= (int(norm * 7.0f) & 0b111) << 1;
	}
	void setFacing(int direction) {
		data.a[1] &= data.a[1] ^ 0b00001100;
		data.a[1] |= (direction & 0b11) << 2;
	}
	int getFacing() {
		return (data.a[1] & 0b00001100) >> 2;
	}
	bool setBoolean(bool state, int bit, int index = 3) {
		data.a[index] &= data.a[index] ^ (1 << bit);
		data.a[index] |= state ? (1 << bit) : 0;
		return state;
	}
	bool hasBoolean(int bit, int index = 3) {
		return (data.a[index] & (1 << bit));
	}
	bool isPlop() {
		return hasBoolean(6, 3);
	}
	int getPlopId() {
		return int(data.c[1]);
	}
	void setPlopId(int id) {
		if (id < 1) {
			setBoolean(false, 6, 3);
			return;
		}

		setBoolean(true, 6, 3);
		data.c[1] = id;
	}
	void setConnection(int direction, int index = 0) {
		//data.a[index] &= data.a[index] ^ direction;
		data.a[index] |= direction;
	}
	bool hasConnection(int direction, int index = 0) {
		return (data.a[index] & direction);
	}
	void setUnderground(int type) {
		data.a[0] |= (type << 4);
	}
	bool hasUnderground(int type) {
		return ((data.a[0] >> 4) & type);
	}
	bool hasPower() {
		return (data.a[1] & 1);
	}
	void setPower(bool state) {
		data.a[1] &= data.a[1] ^ 1;
		data.a[1] |= state ? 1 : 0;
	}
	bool hasWater() {
		return (data.a[1] & 2);
	}
	bool hasRoad() {
		return hasBoolean(4,1);
	}
	void setRoad(bool state) {
		setBoolean(state, 4,1);
	}
	void setWater(bool state) {
		data.a[1] &= data.a[1] ^ 2;
		data.a[1] |= state ? 2 : 0;
	}
};

/*
Struct for the complete identity of a tile instance
*/
struct tileComplete {
	tileComplete() {}
	tileComplete(tile *parent, tilePartial *partial, int tileX, int tileY) {
		this->parent = parent;
		this->partial = partial;
		this->tileX = tileX;
		this->tileY = tileY;
	}
	bool coordsEquals(tileComplete b) {
		return (this->tileX == b.tileX && this->tileY == b.tileY);
	}
	bool operator==(tileComplete b) {
		return coordsEquals(b);
	}


	tile *parent;
	tilePartial *partial;
	plop *parent_plop;
	plop_instance *parent_plop_instance;
	int tileX;
	int tileY;
};

tilePartial *tileMap = nullptr;
int tileMapHeight = 100;
int tileMapWidth = 100;
int textureSize = 16;

float frametime = 33.333f;
FILE *logFile = stderr;

float xfact = 0.7071067812f; //0.5f;
float yfact = 0.7071067812f; //0.5f;

float scale = 4;
float viewX;
float viewY;


float getOffsetX(float x, float y) {
	return round(((((y * yfact) + (x * xfact)) * xfact) + viewX) * 1000.0f) / 1000.0f;
}

float getOffsetY(float x, float y) {
	return round(((((y * yfact) - (x * xfact)) * yfact) + viewY) * 1000.0f) / 1000.0f;
}

float getWidth() {
	return 2.0f * scale;
}

float getHeight() {
	return 1.0f * scale;
}

int selectorTileId;
int selectorX;
int selectorY;

float waterSupply;
float waterDemand;
int waterNetworks;

int commercialJobs;
int commercialPopulation;
float commercialDemand;
int industrialJobs;
int industrialPopulation;
float industrialDemand;
int residentialCapacity;
int population;
float residentialDemand;
float environment;
float health;
float safety;
float traffic;
float education;
float landvalue;

int microday;
int day;
int month;

struct ch_co_t {
	wchar_t ch;
	color_t co;
	color_t a;
};

ch_co_t *texturechco;

unsigned char *texture;
int textureHeight;
int textureWidth;
int bpp;

bool placementMode;
bool waterView;
bool infoMode;
bool statsMode;
bool queryMode;

struct pixel {
	pixel() {
		r = 0;
		g = 0;
		b = 0;
		a = 255;
	}
	pixel(color_t r, color_t g, color_t b) {
		this->r = r;
		this->g = g;
		this->b = b;
		this->a = 255;
	}
	color_t r,g,b,a;
};

pixel sampleImage(float x, float y) { 
	int imX = x * textureWidth;
	int imY = y * textureHeight;
	
	pixel pix;
	pix.r = texture[imY * (textureWidth * bpp) + (imX * bpp) + 0];
	pix.g = texture[imY * (textureWidth * bpp) + (imX * bpp) + 1];
	pix.b = texture[imY * (textureWidth * bpp) + (imX * bpp) + 2];
	
	if (bpp == 4)
		pix.a = texture[imY * (textureWidth * bpp) + (imX * bpp) + 3];
	
	return pix;
}

ch_co_t sampleImageCHCO(float x, float y) {
	int imX = x * textureWidth;
	int imY = y * textureHeight;
	ch_co_t chco = texturechco[imY * textureWidth + imX];
	return chco;
}

struct tile {
	int id;
	tilePartial defaultState;
	unsigned char textureAtlas[4];
	
	tile() {
		id = 0;		
		
		textureAtlas[0] = 3;
		textureAtlas[1] = 0;
		textureAtlas[2] = 4;
		textureAtlas[3] = 1;
		
		defaultState = getDefaultState();
	}
	
	tile(int t0, int t1, int t2, int t3, bool skip = false) {
		if (!skip) 
			tiles::add(this);
				
		setAtlas(t0,t1,t2,t3);
		
		defaultState = getDefaultState();
	}
	
	void setAtlas(int a, int b, int c, int d) {
		textureAtlas[0] = a;
		textureAtlas[1] = b;
		textureAtlas[2] = c;
		textureAtlas[3] = d;
	}
	
	virtual tilePartial getDefaultState() {
		tilePartial partial;
		partial.id = id;
		return partial;
	}
	
	//offsetx,offsety top left
	//sizex,sizey width height
	virtual void draw(tileComplete *tc, float offsetx, float offsety, float sizex, float sizey) {
		//int sizex = sizexf, sizey = sizeyf;
		tilePartial *tp = tc->partial;
		if (offsetx >= adv::width || offsety >= adv::height || offsetx + sizex < 0 || offsety + sizey < 0)
			return;
		for (int x = 0; x < sizex; x++) {
			for (int y = 0; y < sizey; y++) {
				if (offsetx + x >= adv::width || offsety + y >= adv::height || offsetx + x < 0 || offsety + y < 0)
					continue;
				float xf = ((textureAtlas[0] * textureSize) + ((float(x) / sizex) * textureSize)) / textureWidth;
				float yf = ((textureAtlas[1] * textureSize) + ((float(y) / sizey) * textureSize)) / textureHeight;
				ch_co_t chco = sampleImageCHCO(xf, yf);
				if (chco.a < 255)
					continue;
				adv::write(offsetx + x, offsety + y, chco.ch, chco.co);
			}
		}
	}
	
	virtual void onCreate(tileComplete *tc, int x, int y) {}
	
	virtual void onUpdate(tileComplete *tc, int x, int y) {}
	
	virtual void onDestroy(tileComplete *tc, int x, int y) {}
	
	virtual bool hasRoadConnection(tileComplete *tc) { return tc->partial->hasRoad(); }
	
	virtual void updateRoadConnections(tileComplete *tc) {
		tc->partial->setRoad(false);
		tileComplete neighbors[4];
		getNeighbors(tc, &neighbors[0]);
		for (int i = 0; i < 4; i++) {
			//if (neighbors[i].parent->id == tiles::ROAD->id)
			//	tc->partial->setRoad(true);
		}
	}
	
	virtual int getTileZone(tileComplete *tc) {
		return ZONING_NONE;
	}
	
	virtual int getPopulation(tileComplete *tc) {
		return 0;
	}
	
	virtual int getCapacity(tileComplete *tc) {
		return 0;
	}
	
	void getNeighbors(tileComplete *tc, tileComplete *neighbors) {
		int x = tc->tileX;
		int y = tc->tileY;
		neighbors[0] = getComplete(NORTH_F);//NORTH
		neighbors[1] = getComplete(EAST_F);//EAST
		neighbors[2] = getComplete(SOUTH_F);//SOUTH
		neighbors[3] = getComplete(WEST_F);//WEST
	}
	
	virtual bool needsRoadConnection(tileComplete *tc) { return false; }
	
	virtual int waterConsumption(tileComplete *tc) { return 0; }

	virtual int waterProduction(tileComplete *tc) { return 0; }

	virtual int powerConsumption(tileComplete *tc) { return 0; }

	virtual int powerProduction(tileComplete *tc) { return 0; }


	void updateNeighbors(tileComplete *tc, int x, int y) {
		tilePartial *tp = tc->partial;
		tileComplete neighbors[4];
		neighbors[0] = getComplete(x,y-1);//NORTH
		neighbors[1] = getComplete(x+1,y);//EAST
		neighbors[2] = getComplete(x,y+1);//SOUTH
		neighbors[3] = getComplete(x-1,y);//WEST
		neighbors[0].parent->onUpdate(&neighbors[0], x,y-1);
		neighbors[1].parent->onUpdate(&neighbors[1], x+1,y);
		neighbors[2].parent->onUpdate(&neighbors[2], x,y+1);
		neighbors[3].parent->onUpdate(&neighbors[3], x-1,y);
		tiles::WATER_PIPE->onUpdate(&neighbors[0], x,y-1);
		tiles::WATER_PIPE->onUpdate(&neighbors[1], x+1,y);
		tiles::WATER_PIPE->onUpdate(&neighbors[2], x,y+1);
		tiles::WATER_PIPE->onUpdate(&neighbors[3], x-1,y);
	}
	
	virtual void onRandomTick(tileComplete *tc, int x, int y) {}
};

struct tileable : public tile {	
	tileable() {}
	tileable(int t0, int t1, int t2, int t3, int t4, int t5, int t6, int t7, bool skip = false) {
		if (!skip)
			tiles::add(this);
		setAtlas(t0,t1,t2,t3);
		connectingTextureAtlas[0] = t4;
		connectingTextureAtlas[1] = t5;
		connectingTextureAtlas[2] = t6;
		connectingTextureAtlas[3] = t7;
		defaultState = getDefaultState();
	}

	virtual void connectToNeighbors(tileComplete *tc, int x, int y) {
		tilePartial *tp = tc->partial;
		unsigned char old = tp->data.a[0];
		tp->setConnection(0);
		tp->data.a[0] &= tp->data.a[0] ^ 0x0f;
		tilePartial *neighbors[4];
		neighbors[0] = getPartial(x,y-1);//NORTH
		neighbors[1] = getPartial(x+1,y);//EAST
		neighbors[2] = getPartial(x,y+1);//SOUTH
		neighbors[3] = getPartial(x-1,y);//WEST
		for (int i = 0; i < 4; i++) {
			if (neighbors[i]->id == tp->id) {
				if (!((old & (1 << i))>0));
					//getTile(neighbors[i])->onUpdate(neighbors[i])
				//tp->data.a[0] |= (1 << i);
				//neighbors[i]->setConnection(1 << (i + 2 % 4));
				tp->setConnection(1 << i);
			}
		}
	}
	
	virtual bool connectingCondition(tileComplete *tc, int direction) {
		tilePartial *tp = tc->partial;
		return tp->hasConnection(direction);
	}
	
	int connectingTextureAtlas[4];
	
	void onCreate(tileComplete *tc, int x, int y) override {
		tilePartial *tp = tc->partial;
		connectToNeighbors(tc,x,y);
	}
	
	void onUpdate(tileComplete *tc, int x, int y) override {
		tilePartial *tp = tc->partial;
		connectToNeighbors(tc,x,y);
	}
	
	void draw(tileComplete *tc, float offsetx, float offsety, float sizex, float sizey) override {
		tilePartial *tp = tc->partial;
		for (int x = 0; x < sizex; x++) {
			for (int y = 0; y < sizey; y++) {
				if (offsetx + x >= adv::width || offsety + y >= adv::height || offsetx + x < 0 || offsety + y < 0)
					continue;
				float xf = ((textureAtlas[0] * textureSize) + ((float(x) / sizex) * textureSize)) / textureWidth;
				float yf = ((textureAtlas[1] * textureSize) + ((float(y) / sizey) * textureSize)) / textureHeight;
				pixel pix = sampleImage(xf, yf);
				ch_co_t chco = sampleImageCHCO(xf, yf);
				for (int i = 0; i < 4; i++) {
					if (connectingCondition(tc, 1 << i)) {
						float xfto = (float(x) / sizex) * textureSize;
						float yfto = (float(y) / sizey) * textureSize;
						if (i == 1 || i ==2)
							xfto = ((connectingTextureAtlas[0] + 0.99f) * textureSize) - xfto;
						else
							xfto = (connectingTextureAtlas[0] * textureSize) + xfto;
						if (i == 0 || i == 1)
							yfto = ((connectingTextureAtlas[1] + 0.99f) * textureSize) - yfto;
						else
							yfto = (connectingTextureAtlas[1] * textureSize) + yfto;
						xfto /= textureWidth;
						yfto /= textureHeight;
						pixel pix2 = sampleImage(xfto, yfto);
						ch_co_t chco2 = sampleImageCHCO(xfto, yfto);
						if (pix2.a == 255) {
							pix = pix2;			
							chco = chco2;
						}							
					}
				}
				
				if (pix.a < 255)
					continue;
				//wchar_t ch;
				//color_t co;
				//getDitherColored(pix.r,pix.g,pix.b,&ch,&co);
				adv::write(offsetx + x, offsety + y, chco.ch, chco.co);
			}
		}
	}
};

int tiles::id = 0;

struct sprite {
	sprite(int x, int y) 
	:sprite(x,y,1,1) {
	}
	sprite(int x, int y, int w, int h) {
		atlas[0] = x;
		atlas[1] = y;
		atlas[2] = x + w;
		atlas[3] = y + h;
	}

	int atlas[4];

	virtual int getAtlW() {	return atlas[2] - atlas[0];	}

	virtual int getAtlH() { return atlas[3] - atlas[1];	}

	virtual int getPixT() { return textureSize; }

	virtual int getPixTW() { return textureWidth; }

	virtual int getPixTH() { return textureHeight; }

	virtual pixel sampleSprite(float x, float y) { return sampleImage(x, y); }

	virtual ch_co_t sampleSpriteCHCO(float x, float y) { return sampleImageCHCO(x, y); }

	virtual void drawPixel(int x, int y, pixel pix) {
		wchar_t ch;
		color_t co;
		getDitherColored(pix.r, pix.g, pix.b, &ch, &co);
		adv::write(x, y, ch, co);
	}

	void draw(int scrX0 /*screen offset x*/, int scrY0 /*screen offset y*/, int scrW /*draw width on screen*/, int scrH /*draw height on screen*/, int atl0, int atl1, int atl2, int atl3, bool hflip = false, bool vflip = false) {
		//Default is drawing texture from the atlas. ignoring transparent and retaining scale
		int *textureAtlas = &atlas[0];
		int atlW = getAtlW(); // number of images
		int atlH = getAtlH(); // number of images

		int scrX1 = scrX0 + scrW; // max x for draw
		int scrY1 = scrY0 + scrH; // max y for draw
		int maxW = adv::width; // max screen width
		int maxH = adv::height; // max screen height

		int pixT = getPixT();//textureSize; // each texture size in pixels
		int pixTW = getPixTW();//textureWidth; // total atlas texture width
		int pixTH = getPixTH();//textureHeight; // total atlas texture height

		int pixX0 = atl0 * pixT; 
		int pixY0 = atl1 * pixT;
		int pixX1 = atl2 * pixT;
		int pixY1 = atl3 * pixT;		

		int pixAtlW = atlW * pixT; // total atlas texture width
		int pixAtlH = atlH * pixT; // total atlas texture height

		float fAtlX0 = pixX0 / pixTW;
		float fAtlY0 = pixY0 / pixTH;
		float fAtlX1 = pixX1 / pixTW;
		float fAtlY1 = pixY1 / pixTH;

		int maxIterW = scrW * atlW;
		int maxIterH = scrH * atlH;

		for (int x = 0; x < maxIterW; x++) { // 0 - screen pixels per current atlas texture X index
			for (int y = 0; y < maxIterH; y++) { // 0 - screen pixels per current atlas texture Y index

				float xsmpf = float(x) / maxIterW; // fraction of screen
				float ysmpf = float(y) / maxIterH;

				int scrX = scrX0 + x; // current screen pixel X
				int scrY = scrY0 + y - ((atlH - 1) * scrH); // current screen pixel Y with atlas height offset, why do we do this?

				int xsmp = x;
				int ysmp = y;

				if (hflip) {
					xsmp = (scrW * (atlW) - 1) - x;
					xsmpf = .999999 - xsmpf;
				}
				if (vflip) {
					ysmp = scrH * atlH - y;
					ysmpf = .999999 - ysmpf;
				}

				//bounds check for screen
				if (scrX >= maxW || scrY >= maxH)
					continue;
				if (scrX < 0 || scrY < 0)
					continue;

				//using x/ysmpf instead
				//sampler takes fraction of atlas
				float xf = pixX0;
				float yf = pixY0;
				xf += xsmpf * pixAtlW;
				yf += ysmpf * pixAtlH;
				xf /= pixTW;
				yf /= pixTH;

				//float xf = (pixX0 + (float(xsmp) / scrW) * pixT) / pixTW;
				//float yf = (pixY0 + (float(ysmp) / scrH) * pixT) / pixTH;

				pixel pix = sampleSprite(xf, yf);
				if (pix.a < 255)
					continue;

				ch_co_t chco = sampleSpriteCHCO(xf, yf);
				
				adv::write(scrX, scrY, chco.ch, chco.co);
			}
		}
	}

	virtual void draw(int scrX0, int scrY0, int scrW, int scrH) {
		draw(scrX0, scrY0, scrW, scrH, atlas[0], atlas[1], atlas[2], atlas[3]);
	}
};

struct sprite_overlay : sprite {
	sprite_overlay(sprite *base):sprite(*base) {
		_pixTW = base->getAtlW() * base->getPixT();
		_pixTH = base->getAtlH() * base->getPixT();
		fprintf(stderr, "New overlay sprite: %i %i\n", _pixTW, _pixTH);
		canvas = new pixel[_pixTW * _pixTH];
		set(base);

		
	}

	~sprite_overlay() {
		//delete[] canvas;
	}

	pixel *canvas;
	int _pixTW;
	int _pixTH;

	int getPixTW() override { return _pixTW; }
	int getPixTH() override { return _pixTH; }

	pixel sampleSprite(float x, float y) override {
		//return pixel(12,190,50);
		int imX = x * _pixTW;
		int imY = y * _pixTH;
		return canvas[imY * _pixTW + imX];
	}

	ch_co_t sampleSpriteCHCO(float x, float y) override {
		int imX = x * _pixTW;
		int imY = y * _pixTH;
		ch_co_t chco;
		
		float r = getPixT() / getPixTW();

		pixel pix = sampleSprite(x, y);

		getDitherColored(pix.r, pix.g, pix.b, &chco.ch, &chco.co);
		chco.a = pix.a;

		return chco;
	}

	void set(sprite *sp) {
		float spxf0 = (sp->atlas[0] * sp->getPixT()) / float(textureWidth);
		float spyf0 = (sp->atlas[1] * sp->getPixT()) / float(textureHeight);
		float spxf1 = (sp->atlas[2] * sp->getPixT()) / float(textureWidth);
		float spyf1 = (sp->atlas[3] * sp->getPixT()) / float(textureHeight);

		int cw = getPixTW();
		int ch = getPixTH();

		fprintf(stderr, "Setting overlay sprite: %i %i\n", cw, ch);


		return;
		for (int x = 0; x < cw; x++) {
			for (int y = 0; y < ch; y++) {
				float xf = float(x) / cw;
				float yf = float(y) / ch;

				float sxf = (spxf0 + (xf * (spxf1 - spxf0)));
				float syf = (spyf0 + (yf * (spyf1 - spyf0)));

				//canvas[y * getPixTW() + x] = sampleImage(sxf, syf);
				canvas[y * cw + x] = pixel(60,180,60);
			}
		}
	}

	void add(sprite *sp) {
		float spxf0 = (sp->atlas[0] * sp->getPixT() / float(sp->getPixTW()));
		float spyf0 = (sp->atlas[1] * sp->getPixT() / float(sp->getPixTH()));
		float spxf1 = (sp->atlas[2] * sp->getPixT() / float(sp->getPixTW()));
		float spyf1 = (sp->atlas[3] * sp->getPixT() / float(sp->getPixTH()));

		for (int x = 0; x < getPixTW(); x++) {
			for (int y = 0; y < getPixTH(); y++) {
				float xf = float(x) / getPixTW();
				float yf = float(y) / getPixTH();

				float sxf = (spxf0 + (xf * (spxf1 - spxf0)));
				float syf = (spyf0 + (yf * (spyf1 - spyf0)));

				pixel pix = sp->sampleSprite(sxf, syf); 
				//pixel pix = sprite.sampleSprite(sp, sxf, syf);
				
				//sum pixel using alpha
				pixel *can = &canvas[y * getPixTW() + x];

				continue;

				can->r = (can->r * (255 - pix.a) + pix.r * pix.a);
				can->g = (can->g * (255 - pix.a) + pix.g * pix.a);
				can->b = (can->b * (255 - pix.a) + pix.b * pix.a);
				if (can->a + pix.a > 255)
					can->a = 255;
				else
					can->a = can->a + pix.a;
			}
		}
	}
};

struct random_overlay : sprite_overlay {
	random_overlay(sprite *base, sprite *overlay):sprite_overlay(base) {
		//if (rand() % 5 == 0)
			//add(overlay);
			set(base);

			
		for (int i = 0; i < getPixTW() * getPixTH(); i++)
			canvas[i] = pixel(160, 120, 190);
	}
};

struct simple_connecting_sprite : sprite {
	//simple_connecting_sprite(sprite base, sprite over):sprite(base), base(base), over(over) {}

	simple_connecting_sprite(sprite *base, sprite *over):sprite(*base), base(base), over(over) {}

	//default sprite is tr
	void draw_connections(int scrX0, int scrY0, int scrW, int scrH,
						  bool tl, bool tr, bool bl, bool br) {
		base->draw(scrX0, scrY0, scrW, scrH);
		bool con[] = {tl,tr,bl,br};
		bool args[] = {
			false,true,
			true,true,
			false,false,		
			true,false,
		};


		for (int i = 0; i < 4; i++) {
			if (con[i])
				over->draw(scrX0, scrY0, scrW, scrH, over->atlas[0], over->atlas[1], over->atlas[2], over->atlas[3], args[i * 2], args[i * 2 + 1]);
		}
	}

	sprite *base, *over;
};

struct plop_registry {
	plop_registry() {
		id = 1;
	}

	int nextId() {
		return id++;
	}

	plop_instance* registerPlop(plop_instance * p);
	plop_instance* getInstance(int id);
	plop* getPlaceable(int index);
	int getPlaceableCount() {
		return placeable.size();

	}

	int id;
	std::vector<plop_instance*> instances;
	std::vector<plop*> placeable;
};

plop_registry plops;
typedef float net_t;

struct plop_network_value {
	plop_network_value(net_t min, net_t max, net_t value) {
		this->value = value;
		this->min = min;
		this->max = max;
	}

	virtual bool isSaturated() { return abs(max) <= abs(value); }

	virtual bool isDepleted() { return abs(min) >= abs(value); }

	virtual bool isSupply() { return value > 0 || max > 0; }
	
	virtual net_t getValue() { return value; }

	virtual net_t getMin() { return min; }

	virtual net_t getMax() { return max; }

	/*
	Try to add value to the current value, returns the amount that was not added or subtracted



	Ex:
	Consumer
		Max: -800
		Value: -200
		Min: 0
		Tmp: 500
		Amount: 300
		Return: 300

		Final values
		Max: -800
		Value: -500 
		Min: 0
	*/
	virtual net_t addValue(net_t amount) {
		net_t tmp = abs(value) + amount;
		fprintf(logFile, "Adding value: %f %f %f %f\n", value, amount, tmp, value - tmp);

		if (abs(tmp) > abs(max))
			value = max;
		else
		if (abs(tmp) < abs(min))
			value = min;
		else {
			value = tmp;
			if (max < 0) {
				value = -tmp;
			}
		}

		return amount - (tmp + value);
	}

	virtual void setMax(net_t max) { this->max = max; }

	virtual void setMin(net_t min) { this->min = min; }

	virtual void setValue(net_t value) { this->value = value; }
	
	protected:
	net_t value;
	net_t min;
	net_t max;
};

struct plop_network_static : plop_network_value {
	plop_network_static(net_t min, net_t max, net_t value) : plop_network_value(min, max, value) {
	}

	virtual net_t addValue(net_t amount) {
		return 0;
	}
};

struct plop_properties {
	plop_properties() {

	}

	float efficiency; //0-1
	float funding;
	float monthlyCost;
	int width, height;

	plop_network_value *water = 0;
	plop_network_value *power = 0;
	plop_network_value *pollution = 0;
	plop_network_value *crime = 0;
	plop_network_value *health = 0;
	plop_network_value *education = 0;
	plop_network_value *traffic = 0;
	plop_network_value *wealth = 0;
};

struct plop_instance {
	plop_instance(plop *p, int ox, int oy, int sx, int sy) {
		base_plop = p;
		sizeX = sx;
		sizeY = sy;
		originX = ox;
		originY = oy;

		water = 0;
		power = 0;
	}

	plop_instance() {

	}

	virtual plop_properties* createProperties() {
		plop_properties *pp = new plop_properties();
		pp->water = new plop_network_value(0, 100, 0);
		return pp;
	}

	virtual float maxWater(); //references plop, returns max water usage for this plop

	virtual float waterUsage() { //returns actual water usage/supply based on state
		return maxWater();
	}

	virtual bool waterSupply() { //returns true if this plop supplies water
		return waterUsage() > 0;
	}

	virtual bool isWellWatered() { //returns true if this plop has enough water
		if (waterSupply())
			return true;
		return water >= abs(waterUsage());
	}

	virtual void render();

	virtual void onPlace() {
		fprintf(logFile, "Placed plop: %i, ", id);
		for (auto tile : getTiles()) {
			tile.partial->setPlopId(id);
			fprintf(logFile, "[%i %i] ", tile.tileX, tile.tileY);
		}
		if (waterSupply()) {
			//getPartial(originX, originY)->setUnderground(UNDERGROUND_WATER_PIPE);
			tileComplete tc = getComplete(originX, originY);
			tiles::WATER_PIPE->onCreate(&tc, originX, originY);
		}
		fprintf(logFile, "\n");
	}

	virtual void onDestroy() {
		fprintf(logFile, "Destroyed plop: %i, ", id);
		for (auto tile : getTiles()) {
			tile.partial->setPlopId(0);
			tile.partial->setBuildingId(1);
			fprintf(logFile, "[%i %i] ", tile.tileX, tile.tileY);
		}
		fprintf(logFile, "\n");
	}

	~plop_instance() {
		plops.instances.erase(std::remove(plops.instances.begin(), plops.instances.end(), this), plops.instances.end());

	}

	bool operator==(int i) {
		return id == i;
	}

	std::vector<tileComplete> getTiles() {
		std::vector<tileComplete> tiles;
		for (int x = originX; x < originX + sizeX; x++) {
			for (int y = originY; y < originY + sizeY; y++) {
				tiles.push_back(getComplete(x,y));
			}
		}
		return tiles;
	}

	plop *base_plop;
	plop_properties *properties;

	int sizeX, sizeY, originX, originY;
	int id;

	//always part of the plop, not the tile
	//some of these are examples for brainstorming
	float education;
	float crime;
	float fire;
	float health;
	float traffic;
	float wealth;

	//bool watered;
	//bool powered;

	float water;	
	float power;
	float pollution;

	int capacity;
	int population;
};

/*
Whatever the plop type is, this struct is responsible for generating a plop_instance
*/
struct plop {
	plop(sprite* tex, int plop_width = 1, int plop_height = 1, bool placeable=true):plop() {
		this->tex = tex;
		if (placeable) {
			plops.placeable.push_back(this);
		}
		this->width = plop_width;
		this->height = plop_height;

		//default_instance = createInstance();
	}

	plop() {
		width = 1;
		height = 1;

		//default_instance = createInstance();

		tex = nullptr;
		water = 0;
	}

	virtual void render(plop_instance * instance) {
		if (tex == nullptr || instance == nullptr)
			return;

		float width = getWidth();
		float height = getHeight();
			
		float x = getOffsetX(instance->originX, instance->originY) * width;
		float y = getOffsetY(instance->originX, instance->originY) * height;

		tex->draw(x,y,width,height);
	}	

	virtual plop_instance* createInstance() {
		return createInstance(0,0,width,height);
	}

	virtual plop_instance* createInstance(int ox, int oy, int sx, int sy) {
		plop_instance *pi = new plop_instance(this, ox, oy, sx, sy);
		pi->properties = pi->createProperties();
		return pi;
	}

	virtual plop_instance* registerNewInstance(int ox, int oy, int sx, int sy) {
		return plops.registerPlop(createInstance(ox, oy, sx, sy));
	}

	virtual plop_instance* getDefaultInstance() {
		return createInstance();
	}

/*
	virtual void getDefaultInstance(plop_instance * to_fill) {
		to_fill->base_plop = this;
		to_fill->sizeX = width;
		to_fill->sizeY = height;
	}

	virtual plop_instance* getDefaultInstance() {
		return default_instance;
	}
*/

	virtual bool isPlaceable(tileComplete *tc) {
		return !tc->partial->isPlop();
	}

	virtual void place(tileComplete *tc) {
		tc->partial->setPlopId(plops.nextId());
	}

	virtual void onDestroy() {}

	virtual void onPlace() {}

	float water;
	int width;
	int height;
	sprite *tex;
	//plop_instance *default_instance;
};

struct plop_connecting : public plop {
	plop_connecting(simple_connecting_sprite* tex, bool placeable=true):plop(tex,placeable) {
		this->scs = tex;
	}

	simple_connecting_sprite *scs;

	bool testPlop(int x, int y) {
		tileComplete tc = getComplete(x,y);
		if (tc.parent_plop_instance == nullptr)
			return false;
		if (tc.parent_plop == this)
			return true;
		return false;
	}

	void render(plop_instance * instance) override {
		if (scs == nullptr || instance == nullptr)
			return;

		
		float width = getWidth();
		float height = getHeight();
			
		float x = getOffsetX(instance->originX, instance->originY) * width;
		float y = getOffsetY(instance->originX, instance->originY) * height;
		
		scs->draw_connections(x,y,width,height,
			testPlop(instance->originX, instance->originY - 1),
			testPlop(instance->originX + 1, instance->originY),
			testPlop(instance->originX - 1, instance->originY),
			testPlop(instance->originX, instance->originY + 1)
		);
		/*
		tex->draw(x,y,width,height);
		*/
	}

};

struct water_provider_plop : public plop {
	water_provider_plop(sprite *tex, net_t water_creation, int plop_width = 1, int plop_height = 1, bool placeable=true):plop(tex,plop_width,plop_height,placeable) {
		fprintf(logFile, "Water provider plop created: %f\n", water_creation);
		this->water_creation = water_creation;
	}

	struct water_provider_plop_instance : public plop_instance {
		water_provider_plop_instance(water_provider_plop *p, int ox, int oy, int sx, int sy):plop_instance(p, ox, oy, sx, sy) {
			fprintf(logFile, "Water provider plop instance created\n");
		}

		plop_properties* createProperties() override {
			fprintf(logFile, "Water provider plop instance properties created\n");
			plop_properties *pp = plop_instance::createProperties();
			pp->water = new plop_network_static(0, ((water_provider_plop*)base_plop)->water, ((water_provider_plop*)base_plop)->water);
			return pp;
		};
	};

	plop_instance* createInstance(int ox, int oy, int sx, int sy) override {
		plop_instance *pi = new water_provider_plop_instance(this, ox, oy, sx, sy);
		pi->properties = pi->createProperties();
		return pi;
	}

	net_t water_creation;

	virtual float maxWater() {
		return water;
	}
};

struct network_provider_plop : public plop {
	network_provider_plop(sprite *tex, net_t water, net_t power, int plop_width = 1, int plop_height = 1, bool placeable=true):plop(tex,plop_width,plop_height,placeable) {
		_water = water;
		_power = power;
	}

	plop_instance* createInstance(int ox, int oy, int sx, int sy) override {
		plop_instance *pi = new plop_instance(this, ox, oy, sx, sy);
		plop_properties* pp = pi->createProperties();
		pp->water = new plop_network_value(0, _water, 0);
		pp->power = new plop_network_value(0, _power, 0);
		pi->properties = pp;
		return pi;
	}

	net_t _water, _power;
};

plop_instance *plop_registry::registerPlop(plop_instance * p) {
	p->id = nextId();
	instances.push_back(p);
	return p;
}

plop_instance *plop_registry::getInstance(int id) {
	for (auto _pi : instances) {
		if (_pi->id == id)
			return _pi;
	}
	
	return nullptr;
}

plop *plop_registry::getPlaceable(int index) {
	return placeable[index];
}

void plop_instance::render() {
	base_plop->render(this);
}

float plop_instance::maxWater() {
	return base_plop->water;
}

sprite road_sprite(0,0,1,1);
sprite road_con_sprite(1,0,1,1);
sprite street_sprite(6,5,1,1);
sprite street_con_sprite(7,5,1,1);

sprite pool_sprite(4,3,1,1);
sprite pool_con_sprite(5,3,1,1);
sprite dry_pool_sprite(6,3,1,1);
sprite dry_pool_con_sprite(7,3,1,1);

sprite grass_sprite(1,1,1,1);
sprite *grass_sprite_random;
sprite dry_dirt_sprite(2,1,1,1);
sprite wet_dirt_sprite(3,0,1,1);
sprite dry_plop_sprite(1,4,1,1);
sprite wet_plop_sprite(1,3,1,1);

sprite water_tower_sprite(0,1,1,2);
sprite water_well_sprite(0,4,1,1);
sprite large_water_pump_sprite(0,7,2,2);
sprite water_pipe_sprite(3,1,1,1);
sprite water_pipe_con_sprite(0,3,1,1);

sprite no_water_sprite(6,0,1,1);
sprite no_road_sprite(6,1,1,1);

sprite tall_building_sprite(5,0,1,3);
sprite building1_sprite(2,3,1,1);
sprite building2_sprite(0,5,2,2);
sprite building3_sprite(3,4,2,3);

simple_connecting_sprite road_con_tex_sprite(&road_sprite, &road_con_sprite);
simple_connecting_sprite street_con_tex_sprite(&street_sprite, &street_con_sprite);
simple_connecting_sprite pool_con_tex_sprite(&pool_sprite, &pool_con_sprite);

water_provider_plop water_tower_plop(&water_tower_sprite, 1200.0f);
water_provider_plop water_well_plop(&water_well_sprite, 500.0f);
water_provider_plop water_pump_large_plop(&large_water_pump_sprite, 24000.0f, 2, 1);

plop_connecting road_plop(&road_con_tex_sprite);
plop_connecting street_plop(&street_con_tex_sprite);
//plop_connecting water_pipe_plop(new sprite(3,1,1,1));
plop_connecting pool_plop(&pool_con_tex_sprite);

network_provider_plop tall_building_plop(&tall_building_sprite, -800, -400, 1, 1);
plop building1_plop(&building1_sprite);
plop building2_plop(&building2_sprite, 2, 1);
plop building3_plop(&building3_sprite, 2, 2);

void init_plops() {
	water_tower_plop.water = 1200.0f;
	water_well_plop.water = 500.0f;
	water_pump_large_plop.water = 24000.0f;
	pool_plop.water = -200.0f;
}

tilePartial *getPartial(int x, int y) {
	if (x >= tileMapWidth || x < 0 || y >= tileMapHeight || y < 0)
		return &tiles::DEFAULT_TILE->defaultState;
	return &tileMap[y * tileMapWidth + x];
}

tileComplete getComplete(int x, int y) {
	tileComplete tc;
	tc.partial = getPartial(x,y);
	tc.parent = getTile(tc.partial);
	tc.parent_plop = nullptr;
	tc.parent_plop_instance = nullptr;
	if (tc.partial->isPlop()) {
		tc.parent_plop_instance = plops.getInstance(tc.partial->getPlopId());
		tc.parent_plop = tc.parent_plop_instance->base_plop;
	}
	tc.tileX = x;
	tc.tileY = y;
	return tc;
}

tileComplete *getComplete(tilePartial *tp, tile *parent, int x, int y, tileComplete *ptr) {
	ptr->partial = tp;
	ptr->parent = parent;
	ptr->tileX = x;
	ptr->tileY = y;
	return ptr;
}

struct selector : public tile {
	selector() {
		textureAtlas[0] = 1;
		textureAtlas[1] = 2;
		textureAtlas[2] = 2;
		textureAtlas[3] = 3;
		
		defaultState = getDefaultState();
	}
	
	unsigned int ticks;
	//tile *selectedTile;
	plop *selectedPlop;
	plop_instance *currentPlop;
	
	void draw(tileComplete *tc, float offsetx, float offsety, float sizex, float sizey) override {
		tilePartial *tp = tc->partial;
		if (ticks++ % 20 > 9) {
			tile::draw(tc,offsetx,offsety,sizex,sizey);
			tileComplete cmp;
			if (waterView) {
				tiles::WATER_PIPE->draw(getComplete(&tiles::WATER_PIPE->defaultState, tiles::WATER_PIPE, tc->tileX, tc->tileY, &cmp), offsetx, offsety, sizex, sizey);
			} else {
				//selectedTile->draw(getComplete(&selectedTile->defaultState, selectedTile, tc->tileX, tc->tileY, &cmp), offsetx, offsety, sizex, sizey);
				
				//plop_instance pi;
				//memcpy(selectedPlop->default_instance, &pi, sizeof(plop_instance));
				currentPlop->originX = tc->tileX;
				currentPlop->originY = tc->tileY;
				//selectedPlop->render(&pi);
				currentPlop->render();
			}
		}
	}
} tileSelector;

struct default_tile : public tile {
	default_tile() {
		tiles::add(this);
	}
};

struct plop_tile : public tile {
	plop_tile() {

	}

	tilePartial getPlop(plop *p) {
		return getPlop(p->createInstance());
	}

	tilePartial getPlop(plop_instance *p) {
		tilePartial tp = this->getDefaultState();
		tp.setPlopId(p->id);
		return tp;
	}

} plop_tile;

struct grass : public tile {
	grass() {
		tiles::add(this);
		
		textureAtlas[0] = 1;
		textureAtlas[1] = 1;
		textureAtlas[2] = 2;
		textureAtlas[3] = 2;
		
		defaultState = getDefaultState();
	}

	void draw(tileComplete *tc, float offsetx, float offsety, float sizex, float sizey) override {
		//grass_sprite_random->draw(offsetx, offsety, sizex, sizey);
		grass_sprite.draw(offsetx, offsety, sizex, sizey);
	}
};

struct dirt : public tile {
	dirt() {
		tiles::add(this);
		
		textureAtlas[0] = 2;
		textureAtlas[1] = 1;
		textureAtlas[2] = 3;
		textureAtlas[3] = 2;
		
		defaultState = getDefaultState();
	}
	
	void draw(tileComplete *tc, float offsetx, float offsety, float sizex, float sizey) override {
		tilePartial *tp = tc->partial;

		if (tc->parent_plop_instance) {
			if (tc->parent_plop_instance->isWellWatered())
				wet_plop_sprite.draw(offsetx, offsety, sizex, sizey);
			else
				dry_plop_sprite.draw(offsetx, offsety, sizex, sizey);
		} else {
			if (tp->hasWater())
				wet_dirt_sprite.draw(offsetx, offsety, sizex, sizey);
			else
				dry_dirt_sprite.draw(offsetx, offsety, sizex, sizey);
		}

		/*
		if (!tp->hasWater())
			if (tc->parent->waterConsumption(tc) > 0)
				setAtlas(1,4,2,5);
			else
				setAtlas(2,1,3,2);		
		else
			setAtlas(1,3,2,4);

		tile::draw(tc,offsetx,offsety,sizex,sizey);		
		*/
	}
};

struct water_pipe : public tileable {
	water_pipe() {
		tiles::add(this);
		
		textureAtlas[0] = 3;
		textureAtlas[1] = 1;
		textureAtlas[2] = 4;
		textureAtlas[3] = 2;
		
		connectingTextureAtlas[0] = 0;
		connectingTextureAtlas[1] = 3;
		connectingTextureAtlas[2] = 1;
		connectingTextureAtlas[3] = 4;
		
		defaultState = getDefaultState();
	}
	
	bool connectingCondition(tileComplete *tc, int direction) override {
		tilePartial *tp = tc->partial;
		if (tp->id == tiles::WATER_PIPE->id)
			return tileable::connectingCondition(tc, direction);
		return tp->hasConnection(direction, 2);
		//return true;
	}
	
	void connectToNeighbors(tileComplete *tc, int x, int y) override {
		tilePartial *tp = tc->partial;
		if (tp->id == tiles::WATER_PIPE->id) {
			tileable::connectToNeighbors(tc,x,y);
			return;
		}
		//tp->data.a[2] = 0;
		//tp->setConnection(0, 2);
		tp->data.a[2] &= tp->data.a[2] ^ 0x0f;
		tilePartial *neighbors[4];
		neighbors[0] = getPartial(x,y-1);//NORTH
		neighbors[1] = getPartial(x+1,y);//EAST
		neighbors[2] = getPartial(x,y+1);//SOUTH
		neighbors[3] = getPartial(x-1,y);//WEST
		for (int i = 0; i < 4; i++) {
			if (neighbors[i]->hasUnderground(UNDERGROUND_WATER_PIPE)) {
				//neighbors[i]->setConnection(1 << (i + 2 % 4), 2);
				tp->setConnection(1 << i, 2);
			}
		}
	}
	
	void updateWater(tileComplete *tc, int x, int y) {
		return;
		tilePartial *tp = tc->partial;
		if (tp->hasUnderground(UNDERGROUND_WATER_PIPE)) {
			if (waterSupply > waterDemand) {
				for (int xx = x - 5; xx < x + 5; xx++) {
					for (int yy = y - 5; yy < y + 5; yy++) {
						int xxx = xx - x;
						int yyy = yy - y;
						if ((xxx * xxx) + (yyy * yyy) < 25) {
							tilePartial *tp2 = getPartial(xx, yy);
							tp2->setWater(1);
						}
					}
				}
			}
		}
	}
	
	void onUpdate(tileComplete *tc, int x, int y) override {
		tilePartial *tp = tc->partial;
		updateWater(tc, x, y);
		tileable::onUpdate(tc,x,y);
	}
	
	void onCreate(tileComplete *tc, int x, int y) override {
		tilePartial *tp = tc->partial;
		tp->setUnderground(UNDERGROUND_WATER_PIPE);
			//tp->setWater(1);
		updateWater(tc, x, y);
		tileable::onCreate(tc,x,y);
	}

	void onDestroy(tileComplete *tc, int x, int y) override {
		tc->partial->setUnderground(0);
		updateNeighbors(tc,x,y);
	}
};

tile *getTile(tilePartial *partial) {
	return tiles::get(partial->id);
}

tile *tiles::DEFAULT_TILE = new default_tile;

tile *tiles::GRASS = new grass;
tile *tiles::WATER_PIPE = new water_pipe;
tile *tiles::DIRT = new dirt;

tile *tiles::tileRegistry[TILE_COUNT];

void tiles::add(tile *tile) {
	tile->id = id++;
	if (tile->id < TILE_COUNT)
		tiles::tileRegistry[tile->id] = tile;
}

tile *tiles::get(int id) {
	if (id < TILE_COUNT)
		return tiles::tileRegistry[id];
	else
		return DEFAULT_TILE;
}

bool isInRadius(int ox, int oy, int px, int py, float r) {
	int dx = ox - px;
	int dy = oy - py;
	return (dx * dx) + (dy * dy) < r * r;
}

template<typename FUNCTION>
void tileRadiusLoop(int x, int y, float radius, FUNCTION func) {
	for (int xx = x - radius; xx < x + radius; xx++) {
		for (int yy = y - radius; yy < y + radius; yy++) {
			if (isInRadius(xx,yy,x,y,radius))
				func(xx,yy);
			//func(xx,yy);
		}
	}
}

std::vector<tileComplete> walk_network(tileComplete current, bool(*meetsCriteria)(tileComplete), std::vector<tileComplete> &network) {
	if (std::find(network.begin(), network.end(), current) != network.end())
		return network;

	network.push_back(current);

	int startX = current.tileX;
	int startY = current.tileY;

	tileComplete q[4] = {
		getComplete(startX + 1, startY),
		getComplete(startX - 1, startY),
		getComplete(startX, startY + 1),
		getComplete(startX, startY - 1)
	};

	for (int i = 0; i < 4; i++) {
		//if (q[i].parent_plop_instance && q[i].parent_plop == current.parent_plop) {
		//	walk_network(q[i], meetsCriteria, network);
		//}


		if (meetsCriteria(q[i])) {
			walk_network(q[i], meetsCriteria, network);
		}
	}

	return network;
}

void place(plop_instance *pi) {
	for (tileComplete clr : pi->getTiles()) 
		if (clr.parent_plop_instance != nullptr) {
			clr.parent_plop_instance->onDestroy();
			clr.parent_plop_instance->~plop_instance();
		}

	//tileComplete cmp = getComplete(pi->originX, pi->originY);
	//if (cmp.parent_plop_instance != nullptr) {
	//	cmp.parent_plop_instance->onDestroy();
	//}
	pi->onPlace();
	//cmp.parent->onCreate(&cmp, pi->originX, pi->originY);
	//cmp.parent->updateNeighbors(&cmp, pi->originX, pi->originY);
}

void place(int x, int y, plop *p) {
	place(p->registerNewInstance(x, y, p->width, p->height));
}

void destroy(int x, int y) {
	tileComplete tc = getComplete(x, y);
	tilePartial *tp = tc.partial;
	tile *t = tiles::get(tp->id);
	if (tc.parent_plop_instance != nullptr) {
		tc.parent_plop_instance->onDestroy();
		tc.parent_plop_instance->~plop_instance();
	}
	t->onDestroy(&tc, x, y);
	//*tp = t->getDefaultState();
	//tc.parent->updateNeighbors(&tc, x, y);
}

void init() {
	srand(time(NULL));
	
	viewX = 0;
	viewY = 0;
	selectorX = 0;
	selectorY = 0;
	selectorTileId = 0;
	tileSelector = selector();
	//tileSelector.selectedTile = tiles::get(selectorTileId);
	tileSelector.selectedPlop = plops.getPlaceable(selectorTileId);
	tileSelector.currentPlop = tileSelector.selectedPlop->createInstance();

	scale = 4;
	waterView = false;
	placementMode = false;
	infoMode = false;
	statsMode = false;
	
	waterSupply = 0;
	waterDemand = 0;
	waterNetworks = 0;
	
	residentialDemand = 0.5f;
	commercialDemand = 0.5f;
	industrialDemand = 0.5f;
	
	day = 1;
	month = 1;
	
	environment = 0.5f;
	health = 0.5f;
	safety = 0.5f;
	traffic = 0.5f;
	education = 0.5f;
	landvalue = 0.5f;
	
	init_plops();

	if (tileMap)
		delete [] tileMap;
	
	tileMap = new tilePartial[tileMapWidth * tileMapHeight];

	for (auto _ip : plops.instances) {
		_ip->~plop_instance();
	}

	for (int y = 0; y < tileMapHeight; y++) {
		for (int x = 0; x < tileMapWidth; x++) {
			*getPartial(x,y) = tiles::GRASS->getDefaultState();
			if (rand() % 2 == 0)
			;//	*getPartial(x,y) = tiles::ROAD->getDefaultState();
			if (rand() % 4 == 0)
			;//	*getPartial(x,y) = tiles::WATER_TOWER->getDefaultState();
			
			
		}
	}
		/*
			*getPartial(1,3) = tiles::COMMERCIAL_ZONE->getDefaultState();
			*getPartial(2,2) = tiles::COMMERCIAL_ZONE->getDefaultState();
			*getPartial(2,3) = tiles::ROAD->getDefaultState();
			*getPartial(2,4) = tiles::COMMERCIAL_ZONE->getDefaultState();
			*getPartial(3,3) = tiles::COMMERCIAL_ZONE->getDefaultState();
			*/

	//plop_instance* np = water_tower_plop.registerNewInstance(2,2,1,1);
	//*getPartial(2,2) = plop_tile.getPlop(np);
	place(2,2,&water_tower_plop);
	
	for (int y = 0; y < tileMapHeight; y++) {
		for (int x = 0; x < tileMapWidth; x++) {
			tileComplete cmp = getComplete(x,y);
			cmp.parent->onCreate(&cmp, x, y);
			//getTile(getPartial(x,y))->onCreate(getPartial(x,y), x, y);
			//getTile(getPartial(x,y))->onUpdate(getPartial(x,y), x, y);
		}
	}
}

void displayTileMap() {
	float width = getWidth();
	float height = getHeight();
	tileComplete tc;
	
	for (int x = tileMapWidth - 1; x > - 1; x--) {
		for (int y = 0; y < tileMapHeight; y++) {
			//float offsetx = (((y * 0.707106f) + (x * 0.707106f)) * 0.707106f) + viewX;
			//float offsety = (((y * 0.707106f) - (x * 0.707106f)) * 0.707106f) + viewY;
			float offsetx = getOffsetX(x,y);
			float offsety = getOffsetY(x,y);
			
			//Check if in view (TO-DO)
			if (offsetx * width + width < 0 || offsety * height + height < 0 || offsetx * width + width - width > adv::width || offsety * height + height - height > adv::height)
				continue;
			
			tc = getComplete(x,y);
			tilePartial *partial = tc.partial;
			
			//if (partial->isPlop())
			//	continue;

			if (waterView) {
				tiles::DIRT->draw(&tc, offsetx * width, offsety * height, width, height);
				if (!partial->hasUnderground(UNDERGROUND_WATER_PIPE))
					continue;
				//getTile(partial)->draw(partial, offsetx * width, offsety * height, width, height);
				tiles::WATER_PIPE->draw(&tc, offsetx * width, offsety * height, width, height);
				
				continue;
			}			


			tc.parent->draw(&tc, offsetx * width, offsety * height, width, height);
			if (tc.parent->waterConsumption(&tc) > 0 && !tc.partial->hasWater()) {
				//nowater->draw(&tc, offsetx * width + (width * 0.1f), offsety * height - (height * 0.5f), width * 0.8f, height * 0.8f);
				no_water_sprite.draw(offsetx * width + (width * 0.1f), offsety * height - (height * 0.5f), width * 0.8f, height * 0.8f);
			}
			else
				if (tc.parent->needsRoadConnection(&tc) && !tc.parent->hasRoadConnection(&tc)) {
					//noroad->draw(&tc, offsetx * width + (width * 0.1f), offsety * height - (height * 0.5f), width * 0.8f, height * 0.8f);
					no_road_sprite.draw(offsetx * width + (width * 0.1f), offsety * height - (height * 0.5f), width * 0.8f, height * 0.8f);
				}
		}
	}

	std::vector<plop_instance*> z_sort = plops.instances;

	std::sort(z_sort.begin(), z_sort.end(), [](plop_instance *a, plop_instance *b) {
		return getOffsetY(b->originX, b->originY) > getOffsetY(a->originX, a->originY);
	});

	for (auto _pi : z_sort) {
		if (waterView && !_pi->waterSupply())
			continue;

		_pi->render();
	}
}
//0,0
void display() {
	{
		int width = getWidth();
		int height = getHeight();
		for (int x = 0; x < adv::width; x++) {
			for (int y = 0; y < adv::height; y++) {
				//adv::write(x,y,'#', FGREEN | BBLACK);
			}
		}
	}
	
	//Edges of the map
	float corners[] = {
		0.5f,-0.5f,
		tileMapWidth+0.5f,-0.5f,
		tileMapWidth+0.5f,tileMapHeight-0.5f,
		0.5f,tileMapHeight-0.5f,
		0.5f,-0.5f,
	};
	for (int i = 0; i < 8; i+=2) {
		//break;
		//int next = (i + 2) % 8;
		int next = i + 2;
		int width = getWidth();
		int height = getHeight();
		float offsetx1 = getOffsetX(corners[i + 1], corners[i]);// - (xfact / 2.0f);
		float offsety1 = getOffsetY(corners[i + 1], corners[i]);// + (yfact / 2.0f);
		float offsetx2 = getOffsetX(corners[next + 1], corners[next]);// - (xfact / 2.0f);
		float offsety2 = getOffsetY(corners[next + 1], corners[next]);// + (yfact / 2.0f);
		adv::line(offsetx1 * width, offsety1 * height, offsetx2 * width, offsety2 * height, '#', FWHITE|BBLACK);
		//	adv::triangle(offset)
		adv::write(offsetx1 * width, offsety1 * height, 'X', FRED|BBLACK);
	}
	//return;
	
	displayTileMap();
	
	if (placementMode) {
		int width = getWidth();
		int height = getHeight();
		//float offsetx = (((selectorY * 0.707106f) + (selectorX * 0.707106f)) * 0.707106f) + viewX;
		//float offsety = (((selectorY * 0.707106f) - (selectorX * 0.707106f)) * 0.707106f) + viewY;
		float offsetx = getOffsetX(selectorX, selectorY);
		float offsety = getOffsetY(selectorX, selectorY);
		tileComplete cmp;
		tileSelector.draw(getComplete(&tileSelector.defaultState, &tileSelector, selectorX, selectorY, &cmp), offsetx * width, offsety * height, width, height);
	}

	//water_well_plop.render(water_well_plop.createInstance(0,0,0,0));
	
	if (statsMode) {
		const char *text[] = {
			"Environment",
			"Health",
			"Safety",
			"Traffic",
			"Education",
			"Land Value"
		};
		int width = adv::width / 3 - 5;
		int height = 3;
		int widthOffset = adv::width / 3;
		for (int xi = 0; xi < 3; xi++) {
			for (int yi = 0; yi < 2; yi++) {
				//float value = float(rand() % 255) / 255;
				float value = (&environment)[yi * 3 + xi];
				
				pixel pix = pixel((1.0f - value)  * 255, value * 255, 0);
				wchar_t ch;
				color_t co;
				
				getDitherColored(pix.r, pix.g, pix.b, &ch, &co);
				
				int characters = width * value;
				
				adv::write(xi * widthOffset, yi * height, text[yi * 3 + xi]);
				for (int i = 0; i < width; i++) {
					if (i > characters) {
						ch = '#';
						co = FWHITE|BBLACK;
					}
					adv::write(xi * widthOffset + i, yi * height + 1, ch, co);
				}
			}
		}
		
		//RCI
		auto showBarChart = [&](int x, int y, int height, float norm, ch_co_t fillChar) {
			adv::write(x, y, '-');
			adv::write(x, y + height +1, '-');
			adv::line(x,y+1,x,y+height,L'|', FWHITE|BBLACK);
			int charFill = norm * (height-1);
			for (int i = 0; i < charFill; i++) {
				adv::write(x,y+height-i,fillChar.ch,fillChar.co);
			}
		};
		
		showBarChart(adv::width-3, 0, 8, residentialDemand, { L'|', BGREEN|FBLACK });
		showBarChart(adv::width-2, 0, 8, commercialDemand, { L'|', BBLUE|FBLACK });
		showBarChart(adv::width-1, 0, 8, industrialDemand, { L'|', BYELLOW|FBLACK });
	}
	
	//day/month
	{
		char buf[30];
		snprintf(&buf[0], 29, "%i/%i", month,day);
		adv::write(adv::width-4-strlen(&buf[0]),0,&buf[0]);
	}
	
	if (infoMode) {
		int y = 0;
		auto printVar = [&](const char* varname, float value) {
			char buf[100];
			int i = snprintf(&buf[0], 99, "%s: %f", varname, value);
			adv::write(0,y++,&buf[0]);
		};
		printVar("selectorTileId", selectorTileId);
		printVar("viewX", viewX);
		printVar("viewY", viewY);
		printVar("scale", scale);
		printVar("selectorX", selectorX);
		printVar("selectorY", selectorY);
		printVar("waterSupply", waterSupply);
		printVar("waterDemand", waterDemand);
		printVar("waterNetworks", waterNetworks);
		printVar("plopCount", plops.instances.size());
		printVar("placementMode", placementMode ? 1.0f : 0.0f);
		printVar("waterView", waterView ? 1.0f : 0.0f);
		printVar("infoMode", infoMode ? 1.0f : 0.0f);
		printVar("environment", environment);
		printVar("health", environment);
		printVar("safety", safety);
		printVar("traffic", traffic);
		printVar("education", education);
		printVar("landvalue", landvalue);
		printVar("comPop", commercialPopulation);
		printVar("comJob", commercialJobs);
		printVar("comDem", commercialDemand);
		printVar("indPop", industrialPopulation);
		printVar("indJob", industrialJobs);
		printVar("indDem", industrialDemand);
		printVar("resCap", residentialCapacity);
		printVar("resDem", residentialDemand);
		printVar("pop", population);
	}
}

void cleanupexit() {
	fprintf(logFile, "Closing console\n");
	adv::_advancedConsoleDestruct();
	fprintf(logFile, "Exit\n");
	exit(0);
}

bool isWaterNetwork(tileComplete tc) {
	return tc.partial->hasUnderground(UNDERGROUND_WATER_PIPE);
}

int wmain() {	
	logFile = fopen("log.txt", "a");
	if (!logFile) {
		fprintf(stderr, "Failed to open log file\n");
		return 1;
	}
	fprintf(logFile, "Opened log %li\n", time(0));

	colormapper_init_table();

	texture = stbi_load("textures.png", (int*)&textureWidth, (int*)&textureHeight, &bpp, 0);
	
	//convert texture to wchar_t and color_t
	texturechco = new ch_co_t[textureWidth * textureHeight];
	
	for (int x = 0; x < textureWidth; x++) {
		for (int y = 0; y < textureHeight; y++) {
			pixel pix = sampleImage(float(x) / textureWidth, float(y) / textureHeight);
			ch_co_t chco;
			chco.a = pix.a;
			getDitherColored(pix.r, pix.g, pix.b, &chco.ch, &chco.co);
			texturechco[int(y * textureWidth) + int(x)] = chco;
		}
	}

	grass_sprite_random = new random_overlay(new sprite_overlay(&grass_sprite), &street_sprite);
	
	//while (!adv::ready) console::sleep(10);
	
	#ifdef __linux__
	//curs_set(0);
	//adv::setDrawingMode(DRAWINGMODE_COMPARE);
	#endif
	adv::setThreadState(false);
	adv::setThreadSafety(false);
	
	init();
	
	int key = 0;
	
	auto tp1 = std::chrono::system_clock::now();
	auto tp2 = std::chrono::system_clock::now();
	
	//fprintf(stderr, "Entering game loop\n");

	while (true) {
		key = console::readKeyAsync();
		
		if (HASKEY(key, VK_ESCAPE) || HASKEY(key, 'q')) {
			cleanupexit();
		}

		adv::clear();
		
		if (placementMode) {
			switch (key) {
			case 'w':
				selectorY--;
				viewY+= yfact * yfact;
				viewX+= xfact * xfact;
				break;
			case 's':
				selectorY++;
				viewY-= yfact * yfact;
				viewX-= xfact * xfact;
				break;
			case 'd':
				selectorX++;
				viewX-= xfact * xfact;
				viewY+= yfact * yfact;
				break;
			case 'a':
				selectorX--;
				viewX+= xfact * xfact;
				viewY-= yfact * yfact;
				break;			
			case 'z':
			{
				//Place
				if (waterView) {
					//tilePartial *partial = getPartial(selectorX, selectorY);
					tileComplete cmp = getComplete(selectorX, selectorY);
					cmp.partial->setUnderground(UNDERGROUND_WATER_PIPE);
					tiles::WATER_PIPE->onCreate(&cmp, selectorX, selectorY);
					tiles::WATER_PIPE->updateNeighbors(&cmp, selectorX, selectorY);
					break;
				}

				place(selectorX, selectorY, tileSelector.selectedPlop);
				//tileComplete tc = getComplete(selectorX, selectorY);
				//tilePartial *last = tc.partial;
				//tc.parent->onDestroy(&tc, selectorX, selectorY);
				//*getPartial(selectorX, selectorY) = last->transferProperties(tileSelector.selectedTile->getDefaultState());
				//*getPartial(selectorX, selectorY) = plop_tile.getPlop(tileSelector.selectedPlop->registerNewInstance(selectorX, selectorY, 1, 1));
				//tileComplete cmp = getComplete(selectorX, selectorY);
				//cmp.parent->onCreate(&cmp, selectorX, selectorY);
				//cmp.parent->updateNeighbors(&cmp, selectorX, selectorY);
				
			}
				break;
			case 'x':
			{
				if (waterView) {
					tileComplete cmp = getComplete(selectorX, selectorY);
					tiles::WATER_PIPE->onDestroy(&cmp, selectorX, selectorY);
					//cmp.partial->setUnderground(0);
					//tiles::WATER_PIPE->updateNeighbors(&cmp, selectorX, selectorY);
					break;
				}

				destroy(selectorX, selectorY);
				//tilePartial *last = getPartial(selectorX, selectorY);
				//*getPartial(selectorX, selectorY) = last->transferProperties(tiles::GRASS->getDefaultState());
				//tileComplete cmp = getComplete(selectorX, selectorY);
				//cmp.parent->updateNeighbors(&cmp, selectorX, selectorY);
			}
			//Destroy
				break;
			case '2':
				selectorTileId++;
				//if (selectorTileId > TILE_COUNT - 1)
				if (selectorTileId > plops.getPlaceableCount() - 1)
					selectorTileId = 0;
				//tileSelector.selectedTile = tiles::get(selectorTileId);
				tileSelector.selectedPlop = plops.getPlaceable(selectorTileId);
				tileSelector.currentPlop = tileSelector.selectedPlop->createInstance();
				break;
			case '1':
				selectorTileId--;
				if (selectorTileId < 0)
					//selectorTileId = TILE_COUNT - 1;
					selectorTileId = plops.getPlaceableCount() - 1;

				//tileSelector.selectedTile = tiles::get(selectorTileId);
				tileSelector.selectedPlop = plops.getPlaceable(selectorTileId);
				tileSelector.currentPlop = tileSelector.selectedPlop->createInstance();
				break;
			}			
		}
		
		switch (key) {
			case VK_RIGHT:
				viewX--;
				break;				
			case VK_UP:
				viewY++;
				break;
			case VK_LEFT:
				viewX++;
				break;
			case VK_DOWN:
				viewY--;
				break;
			case 'w':
				if (!placementMode)
					viewY++;
				break;
			case 'a':
				if (!placementMode)
					viewX++;
				break;
			case 's':
				if (!placementMode)
					viewY--;
				break;
			case 'd':
				if (!placementMode)
					viewX--;	
				break;
			case ',':
			//case 'z':
				scale++;
				break;
			case '.':
			//case 'x':
				if (scale > 1)
					scale--;
				break;
			case '/':
				scale = 4;
				break;
			case '0':
				init();
				break;			
			case 'u':
				waterView = !waterView;
				break;
			case 'i':
				placementMode = !placementMode;
				if (placementMode) {
					//selectorX = viewX;
					//selectorY = viewY;
				}
				break;
			case 'o':
				infoMode = !infoMode;
				break;
			case 'j':
				statsMode = !statsMode;
				break;
			case 'k':
				month++;
				day = 1;
				break;
		}
		
		display();
		
		tp2 = std::chrono::system_clock::now();
		std::chrono::duration<float> elapsedTime = tp2 - tp1;
		float elapsedTimef = std::chrono::duration_cast< std::chrono::microseconds>(elapsedTime).count() / 1000.0f;
		//std::chrono::duration<float, std::milli> elapsedTimef = t2 - t1;
		tp1 = tp2;
			
		//if (elapsedTimef < frametime)
		//	console::sleep(frametime - elapsedTimef);
		//console::sleep(20);
		adv::waitForReadyFrame();
		{
			char buf[50];
			snprintf(&buf[0], 49, "%.1f fps - %.1f ms ft", (1.0f/elapsedTimef)*1000.0f, elapsedTimef);
			adv::write(adv::width/2.0f-(strlen(&buf[0])/2.0f), 0, &buf[0]);
		}
		
		adv::draw();
		
		//Issue random ticks
		//1/10 chance for each
		//100 tiles, issue 10 ticks
		//200 tiles, issue 20 ticks
		{
			int ticksToIssue = tileMapWidth * tileMapHeight * 0.01f;
			//ignore repeat for now
			for (int i = 0; i < ticksToIssue; i++) {
				int x = rand() % tileMapWidth;
				int y = rand() % tileMapHeight;
				tileComplete tc = getComplete(x, y);
				tc.parent->onRandomTick(&tc, x, y);
				
				if (tc.partial->hasUnderground(UNDERGROUND_WATER_PIPE))
					((water_pipe*)(tiles::WATER_PIPE))->updateWater(&tc, x, y);
			}
		}
		
		//game logic
		if (microday++ > 30) {//once a second
			microday = 0;
			day++;
			if (day > 31) {
				month++;
				day = 1;
			}
		}
		
		if (microday == 0)
		switch (day) {
			case 1: { //check for road connection
				for (int x = 0; x < tileMapWidth; x++) {
					for (int y = 0; y < tileMapHeight; y++) {
						tileComplete tc = getComplete(x,y);
						if (tc.parent->needsRoadConnection(&tc))
							tc.parent->updateRoadConnections(&tc);
					}
				}
				break;
			}
			
			case 2: { //concat capacity
				commercialJobs = 0;
				industrialJobs = 0;
				residentialCapacity = 0;
				commercialPopulation = 0;
				industrialPopulation = 0;
				population = 0;
				
				
				for (int x = 0; x < tileMapWidth; x++) {
					for (int y = 0; y < tileMapHeight; y++) {
						tileComplete tc = getComplete(x,y);
						switch (tc.parent->getTileZone(&tc)) {
							case ZONING_RESIDENTIAL: {
								population += tc.parent->getPopulation(&tc);
								residentialCapacity += tc.parent->getCapacity(&tc);
								break;
							}
							case ZONING_COMMERCIAL: {
								commercialPopulation += tc.parent->getPopulation(&tc);
								commercialJobs += tc.parent->getCapacity(&tc);
								break;
							}
							case ZONING_INDUSTRIAL: {
								industrialPopulation += tc.parent->getPopulation(&tc);
								industrialJobs += tc.parent->getPopulation(&tc);
								break;
							}
						}
					}
				}
				
				commercialDemand = (commercialPopulation + 1) / (commercialJobs + 1);
				industrialDemand = (industrialPopulation + 1) / (industrialJobs + 1);
				residentialDemand = (population + 1) / (residentialCapacity + 1);
				break;
			}
			
			case 3: { //water calculations
				waterNetworks = 0;
				waterDemand = 0;
				waterSupply = 0;

				//Network summary
				for (auto _ip : plops.instances) {
					if (!_ip->properties || !_ip->properties->water)
						continue;

					if (_ip->properties->water->isSupply())
						waterSupply += _ip->properties->water->getValue(); 
					else
						waterDemand += abs(_ip->properties->water->getMax());
					/*
					if (_ip->waterSupply())
						waterSupply += _ip->waterUsage();
					else
						waterDemand += abs(_ip->waterUsage());
					*/
				}

				//form water supply networks and distribute available water
				std::vector<std::vector<tileComplete>> networks;
				//for (auto _ip : plops.instances) {
				for (int x = 0; x < tileMapWidth; x++) {
					for (int y = 0; y < tileMapHeight; y++) {
						tileComplete tile = getComplete(x, y);
						plop_instance *_ip = tile.parent_plop_instance;

						if (!tile.partial->hasUnderground(UNDERGROUND_WATER_PIPE))
							continue; // No production, not a water supply, or not a water pipe

						//if (_ip && _ip->waterUsage() <= 0)
						if (_ip && _ip->properties && _ip->properties->water && !_ip->properties->water->isSupply())
							continue;

						bool newNetwork = true;

						for (auto network : networks) {
							if (std::find(network.begin(), network.end(), tile) != network.end()) {
								newNetwork = false;
								break;
							}
						}

						if (newNetwork) {
							std::vector<tileComplete> network;
							networks.push_back(walk_network(tile, isWaterNetwork, network));
							waterNetworks++;
						}
					}
				}

				//reset water flags for correct display
				for (int i = 0; i < tileMapWidth * tileMapHeight; i++) {
					tileMap[i].setWater(false);
				}

				//reset consumer values
				for (auto ip : plops.instances) {
					//if (ip->waterSupply())
					//	continue;
					//ip->water = 0;
					if (!ip || !ip->properties || !ip->properties->water)
						continue;

					if (ip->properties->water->isSupply())
						continue;

					ip->properties->water->setValue(0);
				}

				fprintf(logFile, "Water calculuation, current network count: %li, iter count: %i\n", networks.size(), waterNetworks);

				//eh to test we'll set good networks with water
				for (auto network : networks) {
					std::set<plop_instance*> waterUsers;
					std::set<plop_instance*> waterProviders;

					//abs(tile.parent_plop_instance->waterUsage()) > 0.0f

					float input = 0, output = 0;

					for (auto tile : network) { //find water users
						//if (tile.parent_plop_instance && tile.parent_plop_instance->waterSupply()) {
						if (tile.parent_plop_instance && tile.parent_plop_instance->properties && tile.parent_plop_instance->properties->water && tile.parent_plop_instance->properties->water->isSupply()) {
							if (waterProviders.find(tile.parent_plop_instance) != waterProviders.end())
								continue; //if already accounted for, skip it
							waterProviders.insert(tile.parent_plop_instance);
							//input += tile.parent_plop_instance->waterUsage();
							input += tile.parent_plop_instance->properties->water->getValue();
						}
					}
					
					if (waterProviders.empty())
						continue; //no water providers, no state change for network

					// best would be expanding radius from tiles in network based upon the available water remaining

					//search radius for water users
					for (auto tile : network) {
						tileRadiusLoop(tile.tileX, tile.tileY, 5, [&](int x, int y) {
							tileComplete tc = getComplete(x, y);
							//if (tc.parent_plop_instance && tc.parent_plop_instance->waterUsage() < 0) {
							if (tc.parent_plop_instance && tc.parent_plop_instance->properties && tc.parent_plop_instance->properties->water && !tc.parent_plop_instance->properties->water->isSupply()) {
								if (waterUsers.find(tc.parent_plop_instance) != waterUsers.end())
									return; //if already accounted for, skip it
								waterUsers.insert(tc.parent_plop_instance);
								//output += abs(tc.parent_plop_instance->waterUsage());
								output += abs(tc.parent_plop_instance->properties->water->getMax());
							}
						});
					}

					float ratio = input / output; //base the radius upon this

					float maxRadius = 5.0f;
					float radius = ratio * maxRadius;
					if (radius < 0.0f)
						radius = 0;
					if (radius > maxRadius)
						radius = maxRadius;

					fprintf(logFile, "Network size %li\n", network.size());
					fprintf(logFile, "input %f output %f ratio %f radius %f\n", input, output, ratio, radius);

					//blindly set water based on radius
					for (auto tile : network) {
						tile.partial->setWater(true);
						//int dr = 5;
						tileRadiusLoop(tile.tileX, tile.tileY, radius, [&](int x, int y) {
							tileComplete tc = getComplete(x, y);
							tc.partial->setWater(true);
						});
					}

					for (auto user : waterUsers) {
						//user->water = 0;
						user->properties->water->setValue(0);
						tileRadiusLoop(user->originX, user->originY, radius, [&](int x, int y) {
							//if (user->isWellWatered() || user->waterSupply())
							if (user->properties->water->isSaturated() || user->properties->water->isSupply()) {
								fprintf(logFile, "Saturated or supply\n");
								return;
							}

							tileComplete tc = getComplete(x, y);
							if (!isWaterNetwork(tc))
								return;
							
							//float desiredWater = abs(user->waterUsage());
							//if (input >= desiredWater) {
							//	input -= desiredWater;
							//	user->water += desiredWater;
							//}
							fprintf(logFile, "Calc water\n");
							input -= user->properties->water->addValue(-input); 
						});

						fprintf(logFile, "Water user %f/%f (%f)\n", user->properties->water->getValue(), user->properties->water->getMax(), input);
					}

				}

				break;
			}
		}
	}	
	
	cleanupexit();

	return 0;
}