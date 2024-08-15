#include "../console/advancedConsole.h"
#include "../imgcat/colorMappingFast.h"
#define STB_IMAGE_IMPLEMENTATION
#include "../imgcat/stb_image.h"
#include <math.h>
#include <chrono>
#include <vector>
#include <iterator>

struct default_tile;
struct grass;
struct road;
struct water_tower;
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
	static tile *ROAD;
	static tile *WATER_TOWER;
	static tile *WATER_PIPE;
	static tile *DIRT;
	static tile *COMMERCIAL_ZONE;
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
float scale = 4;
int selectorTileId = 0;
float frametime = 33.333f;
FILE *logFile = stderr;

float viewX;
float viewY;

//float xfact = 0.5f;
//float yfact = 0.5f;
float xfact = 0.7071067812f;
float yfact = 0.7071067812f;

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

int selectorX;
int selectorY;

int waterSupply;
int waterDemand;
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


struct building {
	building(float t0, float t1, float t2, float t3, int width, int height, int waterConsumption, int powerConsumption, int population, float polution, float waterPolution) {
		atlas[0] = t0;
		atlas[1] = t1;
		atlas[2] = t2;
		atlas[3] = t3;
		this->width = width;
		this->height = height;
		this->waterConsumption = waterConsumption;
		this->powerConsumption = powerConsumption;
		this->population = population;
		this->polution = polution;
		this->waterPolution = waterPolution;
	}
	float atlas[4];
	int width;
	int height;
	int waterConsumption;
	int powerConsumption;
	int population;
	float polution;
	float waterPolution;
};

building commercial_buildings[] = {
	building(2,3,3,4,1,1,1,0,0,0,0),
	building(0,5,2,7,2,1,1,0,0,0,0),
	building(3,4,5,7,2,2,1,0,0,0,0)
};

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
			if (neighbors[i].parent->id == tiles::ROAD->id)
				tc->partial->setRoad(true);
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

	int atlasWidth() {
		return atlas[2] - atlas[0];
	}

	int atlasHeight() {
		return atlas[3] - atlas[1];
	}

	virtual pixel sampleSprite(float x, float y) {
		return sampleImage(x, y);
	}

	virtual ch_co_t sampleSpriteCHCO(float x, float y) {
		return sampleImageCHCO(x, y);
	}

	virtual void drawPixel(int x, int y, pixel pix) {
		wchar_t ch;
		color_t co;
		getDitherColored(pix.r, pix.g, pix.b, &ch, &co);
		adv::write(x, y, ch, co);
	}

	void draw(int scrX0, int scrY0, int scrW, int scrH, int atl0, int atl1, int atl2, int atl3, bool hflip = false, bool vflip = false) {
		//Default is drawing texture from the atlas. ignoring transparent and retaining scale
		int *textureAtlas = &atlas[0];
		int atlW = atlasWidth();
		int atlH = atlasHeight();

		int scrX1 = scrX0 + scrW;
		int scrY1 = scrY0 + scrH;
		int maxW = adv::width;
		int maxH = adv::height;

		int pixT = textureSize;
		int pixTW = textureWidth;
		int pixTH = textureHeight;

		int pixX0 = atl0 * pixT;
		int pixY0 = atl1 * pixT;
		int pixX1 = atl2 * pixT;
		int pixY1 = atl3 * pixT;		

		for (int x = 0; x < scrW * atlW; x++) { // 0 - screen pixels per current atlas texture X index
			for (int y = 0; y < scrH * atlH; y++) { // 0 - screen pixels per current atlas texture Y index

				int scrX = scrX0 + x; // current screen pixel X
				int scrY = scrY0 + y - ((atlH - 1) * scrH); // current screen pixel Y with atlas height offset

				int xsmp = x;
				int ysmp = y;

				if (hflip)
					xsmp = (scrW * (atlW) - 1) - x;
				if (vflip)
					ysmp = scrH * atlH - y;

				//bounds check for screen
				if (scrX >= maxW || scrY >= maxH)
					continue;
				if (scrX < 0 || scrY < 0)
					continue;

				float xf = (pixX0 + (float(xsmp) / scrW) * pixT) / pixTW;
				float yf = (pixY0 + (float(ysmp) / scrH) * pixT) / pixTH;

				/*
				if (hflip)
					xf = (((atl0 + 0.999f) * textureSize) / textureWidth) - xf;
				if (vflip)
					yf = (((atl1 + 0.999f) * textureSize) / textureHeight) - yf;
				*/

				//convert atlas xy index to texture coord with current screen pixel xy

				
		/*
		mirroring sample direction, not atlas

		if (i == 1 || i ==2)
			xfto = ((connectingTextureAtlas[0] + 0.99f) * textureSize) - xfto;
		else
			xfto = (connectingTextureAtlas[0] * textureSize) + xfto;
		if (i == 0 || i == 1)
			yfto = ((connectingTextureAtlas[1] + 0.99f) * textureSize) - yfto;
		else
			yfto = (connectingTextureAtlas[1] * textureSize) + yfto;
		*/
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

struct overlay : sprite {
	overlay(sprite base):sprite(base) {
		canvas = new pixel[atlasHeight() * atlasWidth()];

	}

	pixel *canvas;

	void add(sprite sp) {
		
	}

	void draw(int offsetx, int offsety, int sizex, int sizey) override {

	}
};

struct simple_connecting_sprite : sprite {
	simple_connecting_sprite(sprite base, sprite over):sprite(base), base(base), over(over) {}

	//default sprite is tr
	void draw_connections(int scrX0, int scrY0, int scrW, int scrH,
						  bool tl, bool tr, bool bl, bool br) {
		base.draw(scrX0, scrY0, scrW, scrH);
		bool con[] = {tl,tr,bl,br};
		bool args[] = {
			false,true,
			true,true,
			false,false,		
			true,false,
		};


		for (int i = 0; i < 4; i++) {
			if (con[i])
				over.draw(scrX0, scrY0, scrW, scrH, over.atlas[0], over.atlas[1], over.atlas[2], over.atlas[3], args[i * 2], args[i * 2 + 1]);
		}
	}

	sprite base, over;
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

struct plop_instance {
	plop_instance(plop *p, int ox, int oy, int sx, int sy) {
		base_plop = p;
		sizeX = sx;
		sizeY = sy;
		originX = ox;
		originY = oy;
	}

	plop_instance() {

	}

	virtual float maxWater();

	virtual float waterUsage() {
		return maxWater();
	}

	virtual bool waterSupply() {
		return waterUsage() > 0;
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
	int sizeX, sizeY, originX, originY;
	int id;

	//always part of the plop, not the tile
	float education;
	float crime;
	float fire;
	float health;

	bool watered;
	bool powered;

	float water;	
	float power;
	float pollution;

	int capacity;
	int population;
};

struct plop {
	plop(sprite* tex, bool placeable=true):plop() {
		this->tex = tex;
		if (placeable) {
			plops.placeable.push_back(this);
		}
	}

	plop() {
		default_instance = createInstance();

		tex = nullptr;
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
		return createInstance(0,0,1,1);
	}

	virtual plop_instance* createInstance(int ox, int oy, int sx, int sy) {
		return new plop_instance(this, ox, oy, sx, sy);
	}

	virtual plop_instance* registerNewInstance(int ox, int oy, int sx, int sy) {
		return plops.registerPlop(createInstance(ox, oy, sx, sy));
	}

	virtual void getDefaultInstance(plop_instance * to_fill) {
		to_fill->base_plop = this;
		to_fill->sizeX = 1;
		to_fill->sizeY = 1;
	}

	virtual plop_instance* getDefaultInstance() {
		return default_instance;
	}

	virtual bool isPlaceable(tileComplete *tc) {
		return !tc->partial->isPlop();
	}

	virtual void place(tileComplete *tc) {
		tc->partial->setPlopId(plops.nextId());
	}

	virtual void onDestroy() {}

	virtual void onPlace() {}

	float water;

	sprite *tex;
	plop_instance *default_instance;
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
sprite street_con_sprite(6,5,1,1);
sprite pool_sprite(4,3,1,1);
sprite pool_con_sprite(5,3,1,1);

plop water_tower_plop(new sprite(0,1,1,2));
plop water_well_plop(new sprite(0,4,1,1));
plop_connecting road_plop(new simple_connecting_sprite(road_sprite, road_con_sprite));
plop_connecting street_plop(new simple_connecting_sprite(street_sprite, street_con_sprite));
//plop_connecting water_pipe_plop(new sprite(3,1,1,1));
plop_connecting pool_plop(new simple_connecting_sprite(pool_sprite, pool_con_sprite));

void init_plops() {
	water_tower_plop.water = 1200.0f;
	water_well_plop.water = 500.0f;
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
	
	void draw(tileComplete *tc, float offsetx, float offsety, float sizex, float sizey) override {
		tilePartial *tp = tc->partial;
		if (ticks++ % 20 > 9) {
			tile::draw(tc,offsetx,offsety,sizex,sizey);
			tileComplete cmp;
			if (waterView) {
				tiles::WATER_PIPE->draw(getComplete(&tiles::WATER_PIPE->defaultState, tiles::WATER_PIPE, tc->tileX, tc->tileY, &cmp), offsetx, offsety, sizex, sizey);
			} else {
				//selectedTile->draw(getComplete(&selectedTile->defaultState, selectedTile, tc->tileX, tc->tileY, &cmp), offsetx, offsety, sizex, sizey);
				
				plop_instance pi;
				memcpy(selectedPlop->getDefaultInstance(), &pi, sizeof(plop_instance));
				pi.originX = tc->tileX;
				pi.originY = tc->tileY;
				selectedPlop->render(&pi);
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
		return getPlop(p->getDefaultInstance());
	}

	tilePartial getPlop(plop_instance *p) {
		tilePartial tp = this->getDefaultState();
		tp.setPlopId(p->id);
		return tp;
	}

} plop_tile;

struct road : public tileable {
	road() {
		tiles::add(this);
		
		textureAtlas[0] = 0;
		textureAtlas[1] = 0;
		textureAtlas[2] = 1;
		textureAtlas[3] = 1;
		
		connectingTextureAtlas[0] = 1;
		connectingTextureAtlas[1] = 0;
		connectingTextureAtlas[2] = 2;
		connectingTextureAtlas[3] = 1;
		
		defaultState = getDefaultState();
	}
};

void buildingRenderer(tileComplete *tc, building *bd, float offsetx, float offsety, float sizex, float sizey) {
	float textureSizeW = bd->atlas[2] - bd->atlas[0];
	float textureSizeH = bd->atlas[3] - bd->atlas[1];
	for (int x = 0; x < sizex * textureSizeW; x++) {
		for (int y = 0; y < sizey * textureSizeH; y++) {
			if (offsetx + x >= adv::width || offsety + y - (textureSizeH - 1) * sizey >= adv::height || offsetx + x < 0 || offsety + y - ((textureSizeH - 1) * sizey) < 0)
				;//continue;
			float xf = ((bd->atlas[0] * textureSize) + ((float(x) / sizex) * textureSize)) / textureWidth;
			float yf = ((bd->atlas[1] * textureSize) + ((float(y) / sizey) * textureSize)) / textureHeight;
			ch_co_t chco = sampleImageCHCO(xf, yf);
			if (chco.a < 255)
				continue;
			adv::write(offsetx + x, offsety + y - ((textureSizeH - 1) * sizey), chco.ch, chco.co);
		}
	}
}

struct buildingTest : public tile {
	buildingTest() {
		tiles::add(this);
		defaultState= getDefaultState();
	}
	
	void draw(tileComplete *tc, float offsetx, float offsety, float sizex, float sizey) override {
		buildingRenderer(tc, &commercial_buildings[1], offsetx, offsety, sizex, sizey);
	}
};

struct commercial_zone : public tileable {
	commercial_zone() {
		tiles::add(this);
		
		textureAtlas[0] = 3;
		textureAtlas[1] = 3;
		textureAtlas[2] = 4;
		textureAtlas[3] = 4;
		
		defaultState = getDefaultState();
	}
	
	int getTileZone(tileComplete *tc) override {
		return ZONING_COMMERCIAL;
	}
	
	int getPopulation(tileComplete *tc) override {
		return tc->partial->hasWater() && hasRoadConnection(tc) && tc->partial->hasBoolean(0) ? 10 : 0;
	}
	
	int getCapacity(tileComplete *tc) override {
		return tc->partial->hasWater() && hasRoadConnection(tc) && tc->partial->hasBoolean(0) ? 100 : 0;
	}
	
	void onRandomTick(tileComplete *tc, int x, int y) override {
		tilePartial *tp = tc->partial;
		if (!tp->hasBoolean(0) && tp->hasBoolean(0,4)) {
			if (tp->hasConnection(EAST)) {
				getPartial(EAST_F)->setBoolean(true,0);
				tp->setBuildingId(1);
			}
			else
				tp->setBuildingId(0);			
			
			tp->setBoolean(true, 0);
		}
	}
	
	bool needsRoadConnection(tileComplete *tc) override {
		return tc->partial->hasWater();
	}
	
	int waterConsumption(tileComplete *tc) override {
		if (tc->partial->hasBoolean(0))
			return commercial_buildings[tc->partial->getBuildingId()].waterConsumption * getPopulation(tc) + commercial_buildings[tc->partial->getBuildingId()].waterConsumption;
		return 0;
	}
	
	void connectToNeighbors(tileComplete *tc, int x, int y) override {
		tilePartial *tp = tc->partial;
		tp->setBoolean(true,0,4);
		unsigned char old = tp->data.a[0];
		tp->setConnection(0);
		tp->data.a[0] &= tp->data.a[0] ^ 0x0f;
		tilePartial *neighbors[4];
		neighbors[0] = getPartial(NORTH_F);
		neighbors[1] = getPartial(EAST_F);
		neighbors[2] = getPartial(SOUTH_F);
		neighbors[3] = getPartial(WEST_F);
		for (int i = 0; i < 4; i++) {
			if (neighbors[i]->id == tp->id /*&& neighbors[i]->hasBoolean(0,4)*/) {
				tp->setConnection(1 << i);
				//tp->setBoolean(false,0,4);
			}
		}
	}
	
	void onCreate(tileComplete *tc, int x, int y) override {
		tilePartial *tp = tc->partial;
		connectToNeighbors(tc,x,y);
		
		tilePartial *neighbors[4];
		neighbors[0] = getPartial(x,y-1);//NORTH
		neighbors[1] = getPartial(x+1,y);//EAST
		neighbors[2] = getPartial(x,y+1);//SOUTH
		neighbors[3] = getPartial(x-1,y);//WEST
		
		for (int i = 0; i < 4; i++) {
			if (neighbors[i]->id == tiles::ROAD->id) {
					//getTile(neighbors[i])->onUpdate(neighbors[i])
				//tp->data.a[0] |= (1 << i);
				tp->setFacing(i);
			}
		}
		
		//base
		updateRoadConnections(tc);
	}
	
	void draw(tileComplete *tc, float offsetx, float offsety, float sizex, float sizey) override {		
		tilePartial *tp = tc->partial;
		if (tp->hasBoolean(0)) {
			if (!tp->hasBoolean(0,4))
				return;
			buildingRenderer(tc, &commercial_buildings[tp->getBuildingId()], offsetx, offsety, sizex, sizey);
			return;
		}
		
		for (int x = 0; x < sizex; x++) {
			for (int y = 0; y < sizey; y++) {
				if (offsetx + x >= adv::width || offsety + y >= adv::height || offsetx + x < 0 || offsety + y < 0)
					continue;
				int ta0 = textureAtlas[0];
				int ta1 = textureAtlas[1];
				if (tp->hasBoolean(0)) {
					ta0 = 2;
					ta1 = 3;
				}
				float xf = ((ta0 * textureSize) + ((float(x) / sizex) * textureSize)) / textureWidth;
				float yf = ((ta1 * textureSize) + ((float(y) / sizey) * textureSize)) / textureHeight;
				pixel pix = sampleImage(xf, yf);
				ch_co_t chco = sampleImageCHCO(xf, yf);
				
				if (!tp->hasBoolean(0)) {
					for (int i = 0; i < 4; i++) {
						if (!(tp->data.a[0] & (1 << i))) {
							float xfto = (float(x) / sizex) * textureSize;
							float yfto = (float(y) / sizey) * textureSize;
							if (i == 1 || i ==2)
								xfto = (2.99f * textureSize) - xfto;
							else
								xfto = (2 * textureSize) + xfto;
							if (i == 0 || i == 1)
								yfto = (2.99f * textureSize) - yfto;
							else
								yfto = (2 * textureSize) + yfto;
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
					
					//Arrow
					{
						int facing = tp->getFacing();
						float xfto = (float(x) / sizex) * textureSize;
						float yfto = (float(y) / sizey) * textureSize;
						if (facing == 0 || facing == 1)
							yfto = (2.99f * textureSize) - yfto;
						else
							yfto = (2 * textureSize) + yfto;
						if (facing == 1 || facing == 2)
							xfto = (3.99f * textureSize) - xfto;
						else
							xfto = (3 * textureSize) + xfto;
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

struct grass : public tile {
	grass() {
		tiles::add(this);
		
		textureAtlas[0] = 1;
		textureAtlas[1] = 1;
		textureAtlas[2] = 2;
		textureAtlas[3] = 2;
		
		defaultState = getDefaultState();
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
		if (!tp->hasWater())
			if (tc->parent->waterConsumption(tc) > 0)
				setAtlas(1,4,2,5);
			else
				setAtlas(2,1,3,2);		
		else
			setAtlas(1,3,2,4);
				
		tile::draw(tc,offsetx,offsety,sizex,sizey);		
	}
};

struct multitilesprite : public tile {
	multitilesprite() {}
	multitilesprite(int t0, int t1, int t2, int t3) { tiles::add(this); setAtlas(t0,t1,t2,t3); defaultState = getDefaultState(); }
	
	void draw(tileComplete *tc, float offsetx, float offsety, float sizex, float sizey) override {
		tilePartial *tp = tc->partial;
		//Default is drawing texture from the atlas. ignoring transparent and retaining scale
		int textureSizeW = textureAtlas[2] - textureAtlas[0];
		int textureSizeH = textureAtlas[3] - textureAtlas[1];
		for (int x = 0; x < sizex * textureSizeW; x++) {
			for (int y = 0; y < sizey * textureSizeH; y++) {
				if (offsetx + x >= adv::width || offsety + y - (textureSizeH - 1) * sizey >= adv::height || offsetx + x < 0 || offsety + y - ((textureSizeH - 1) * sizey) < 0)
					continue;
				float xf = ((textureAtlas[0] * textureSize) + ((float(x) / sizex) * textureSize)) / textureWidth;
				float yf = ((textureAtlas[1] * textureSize) + ((float(y) / sizey) * textureSize)) / textureHeight;
				pixel pix = sampleImage(xf, yf);
				if (pix.a < 255)
					continue;
				//wchar_t ch;
				//color_t co;
				//getDitherColored(pix.r,pix.g,pix.b,&ch,&co);
				ch_co_t chco = sampleImageCHCO(xf, yf);
				adv::write(offsetx + x /*+ (textureSizeW * sizex)*/, offsety + y - ((textureSizeH - 1) * sizey), chco.ch, chco.co);
			}
		}
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
tile *tiles::ROAD = new road;
tile *tiles::GRASS = new grass;
//tile *tiles::WATER_TOWER = new water_tower;
tile *tiles::WATER_PIPE = new water_pipe;
tile *tiles::DIRT = new dirt;
tile *tiles::COMMERCIAL_ZONE = new commercial_zone;
//tile *tile0 = new bigbuildingtesttile;
tile *tile1 = new multitilesprite(5,0,6,3);
tile *drypool = new tileable(6,3,7,4,7,3,8,4, true);
tile *nowater = new tile(6,0,7,1, true);
tile *noroad = new tile(6,1,7,2,true);
//tile *waterwell = new water_well;

struct pool : public tileable {
	pool()
		:tileable(4,3,5,4,5,3,6,4) {
		
	}
	
	int waterConsumption(tileComplete *tc) override {
		return 50;
	}
	
	void draw(tileComplete *tc, float offsetx, float offsety, float sizex, float sizey) override {
		tilePartial *tp = tc->partial;
		//if (((int)offsetx) % 2 == 0)//
		if (tp->hasWater())
			tileable::draw(tc,offsetx,offsety,sizex,sizey);
		else {
			drypool->draw(tc,offsetx,offsety,sizex,sizey);
			//nowater->draw(tc,offsetx + (sizex * 0.1f),offsety - (sizey * 0.5f),sizex * 0.8f,sizey * 0.8f);
		}
	}
};

pool *pooltile = new pool();

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
		if (q[i].parent_plop_instance && q[i].parent_plop == current.parent_plop) {
			walk_network(q[i], meetsCriteria, network);
		}


		if (meetsCriteria(q[i])) {
			walk_network(q[i], meetsCriteria, network);
		}
	}

	return network;
}

void place(plop_instance *pi) {
	tileComplete cmp = getComplete(pi->originX, pi->originY);
	if (cmp.parent_plop_instance != nullptr) {
		cmp.parent_plop_instance->onDestroy();
	}
	pi->onPlace();
	cmp.parent->onCreate(&cmp, pi->originX, pi->originY);
	cmp.parent->updateNeighbors(&cmp, pi->originX, pi->originY);
}

void place(int x, int y, plop *p) {
	place(p->registerNewInstance(x, y, 1, 1));
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
				nowater->draw(&tc, offsetx * width + (width * 0.1f), offsety * height - (height * 0.5f), width * 0.8f, height * 0.8f);
			}
			else
				if (tc.parent->needsRoadConnection(&tc) && !tc.parent->hasRoadConnection(&tc)) {
					noroad->draw(&tc, offsetx * width + (width * 0.1f), offsety * height - (height * 0.5f), width * 0.8f, height * 0.8f);
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
	
	while (!adv::ready) console::sleep(10);
	
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
				break;
			case '1':
				selectorTileId--;
				if (selectorTileId < 0)
					//selectorTileId = TILE_COUNT - 1;
					selectorTileId = plops.getPlaceableCount() - 1;

				//tileSelector.selectedTile = tiles::get(selectorTileId);
				tileSelector.selectedPlop = plops.getPlaceable(selectorTileId);

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
			
		if (elapsedTimef < frametime)
			console::sleep(frametime - elapsedTimef);
		//console::sleep(20);
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

				//form water supply networks and distribute available water
				std::vector<std::vector<tileComplete>> networks;
				//for (auto _ip : plops.instances) {
				for (int x = 0; x < tileMapWidth; x++) 
				for (int y = 0; y < tileMapHeight; y++) {
					tileComplete tile = getComplete(x, y);
					plop_instance *_ip = tile.parent_plop_instance;

					if (!tile.partial->hasUnderground(UNDERGROUND_WATER_PIPE))
						continue; // No production, not a water supply, or not a water pipe

					if (_ip && _ip->waterUsage() <= 0)
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
				
				//reset water flags for correct display
				for (int i = 0; i < tileMapWidth * tileMapHeight; i++)
					tileMap[i].setWater(false);

				//eh to test we'll set good networks with water
				for (auto network : networks) {
					for (auto tile : network) {
						tile.partial->setWater(true);
						for (int x = tile.tileX - 5; x < tile.tileX + 5; x++) {
							for (int y = tile.tileY - 5; y < tile.tileY + 5; y++) {
								int xx = x - tile.tileX;
								int yy = y - tile.tileY;
								if ((xx * xx) + (yy * yy) < 25) {
									tilePartial *tp = getPartial(x, y);
									tp->setWater(true);
								}
							}
						}
					}
				}

				break;
				
				//first calculate the demand then resize water spread
				waterDemand = 0;
				
				for (int x = 0; x < tileMapWidth; x++) {
					for (int y = 0; y < tileMapHeight; y++) {
						tileComplete tc = getComplete(x,y);
						tc.partial->setWater(0);
						if (waterDemand + tc.parent->waterConsumption(&tc) <= waterSupply) {
							tc.partial->setWater(1);
						}
						waterDemand += tc.parent->waterConsumption(&tc);
					}
				}
				
				
				break;
			}
		}
	}	
	
	cleanupexit();

	return 0;
}