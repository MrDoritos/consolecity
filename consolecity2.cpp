#include "advancedConsole.h"
#include "colorMappingFast.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <math.h>

struct default_tile;
struct grass;
struct road;
struct water_tower;

struct tile;

#define TILE_COUNT 11
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
			 0000 - connection
octet 1 : 00 - none
			 00 - none
			 00 - facing
			 0 - water
			 0 - power
octet 2 : 00
			 00
			 00
			 00 - water pipe connection
octet 3 : boolean reserved
		  7 0 - constructed (zone)
octet 4 : buildings
		     0000 - building id
			 000 - construction animation
			 0 - master
octet 5 : population
			 0000 - inhabitants / jobs


*/

struct tilePartial {
	tilePartial() { id = 0; data.b = 0; }
	unsigned char id;
	union {
		unsigned char a[8];
		unsigned long b;
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
	}
	bool hasBoolean(int bit, int index = 3) {
		return (data.a[index] & (1 << bit));
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
	
	tile *parent;
	tilePartial *partial;
	int tileX;
	int tileY;
};

tilePartial *tileMap = nullptr;
int tileMapHeight = 100;
int tileMapWidth = 100;
int textureSize = 16;
int scale = 4;
int selectorTileId = 0;

float viewX;
float viewY;

int selectorX;
int selectorY;

int waterSupply;
int waterDemand;

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

tilePartial *getPartial(int, int);
tileComplete getComplete(int, int);
tile *getTile(tilePartial*);

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
	
	virtual int waterConsumption(tileComplete *tc) {
		return 0;
	}
	
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

tilePartial *getPartial(int x, int y) {
	if (x >= tileMapWidth || x < 0 || y >= tileMapHeight || y < 0)
		return &tiles::DEFAULT_TILE->defaultState;
	return &tileMap[y * tileMapWidth + x];
}

tileComplete getComplete(int x, int y) {
	tileComplete tc;
	tc.partial = getPartial(x,y);
	tc.parent = getTile(tc.partial);
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

int tiles::id = 0;

struct selector : public tile {
	selector() {
		textureAtlas[0] = 1;
		textureAtlas[1] = 2;
		textureAtlas[2] = 2;
		textureAtlas[3] = 3;
		
		defaultState = getDefaultState();
	}
	
	unsigned int ticks;
	tile *selectedTile;
	
	void draw(tileComplete *tc, float offsetx, float offsety, float sizex, float sizey) override {
		tilePartial *tp = tc->partial;
		if (ticks++ % 20 > 9) {
			tile::draw(tc,offsetx,offsety,sizex,sizey);
			tileComplete cmp;
			if (waterView) {
				tiles::WATER_PIPE->draw(getComplete(&tiles::WATER_PIPE->defaultState, tiles::WATER_PIPE, tc->tileX, tc->tileY, &cmp), offsetx, offsety, sizex, sizey);
			} else {
				selectedTile->draw(getComplete(&selectedTile->defaultState, selectedTile, tc->tileX, tc->tileY, &cmp), offsetx, offsety, sizex, sizey);
			}
		}
	}
} tileSelector;

struct default_tile : public tile {
	default_tile() {
		tiles::add(this);
	}
};

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
} *btt = new buildingTest;

struct commercial_zone : public tileable {
	commercial_zone() {
		tiles::add(this);
		
		textureAtlas[0] = 3;
		textureAtlas[1] = 3;
		textureAtlas[2] = 4;
		textureAtlas[3] = 4;
		
		defaultState = getDefaultState();
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
	
	int waterConsumption(tileComplete *tc) override {
		if (tc->partial->hasBoolean(0))
			return commercial_buildings[tc->partial->getBuildingId()].waterConsumption;
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

struct bigbuildingtesttile : public multitilesprite {
	bigbuildingtesttile() {
		tiles::add(this);
		
		setAtlas(4,0,5,2);
		
		defaultState = getDefaultState();
	}
};


struct water_tower : public multitilesprite {
	water_tower() {
		tiles::add(this);
		
		textureAtlas[0] = 0;
		textureAtlas[1] = 1;
		textureAtlas[2] = 1;
		textureAtlas[3] = 3;
		
		defaultState = getDefaultState();
	}
	
	tilePartial getDefaultState() override {
		tilePartial partial;
		partial.id = id;
		partial.setUnderground(UNDERGROUND_WATER_PIPE);
		return partial;
	}
	
	void onCreate(tileComplete *tc, int x, int y) override {
		waterSupply += 2400;
	}
	
	void onDestroy(tileComplete *tc, int x, int y) override {
		waterSupply -= 2400;
	}
};

struct water_well : public tile {
	water_well() :tile(0,4,1,5){	}
	
	tilePartial getDefaultState() override {
		tilePartial partial;
		partial.id = id;
		partial.setUnderground(UNDERGROUND_WATER_PIPE);
		return partial;
	}
	
	void onCreate(tileComplete *tc, int x, int y) override {
		waterSupply += 1200;
	}
	
	void onDestroy(tileComplete *tc, int x, int y) override {
		waterSupply -= 1200;
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
		if (tp->hasUnderground(UNDERGROUND_WATER_PIPE))
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
};

tile *getTile(tilePartial *partial) {
	return tiles::get(partial->id);
}

tile *tiles::DEFAULT_TILE = new default_tile;
tile *tiles::ROAD = new road;
tile *tiles::GRASS = new grass;
tile *tiles::WATER_TOWER = new water_tower;
tile *tiles::WATER_PIPE = new water_pipe;
tile *tiles::DIRT = new dirt;
tile *tiles::COMMERCIAL_ZONE = new commercial_zone;
tile *tile0 = new bigbuildingtesttile;
tile *tile1 = new multitilesprite(5,0,6,3);
tile *drypool = new tileable(6,3,7,4,7,3,8,4, true);
tile *nowater = new tile(6,0,7,1, true);
tile *waterwell = new water_well;

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

void init() {
	srand(time(NULL));
	
	viewX = 0;
	viewY = 0;
	selectorX = 0;
	selectorY = 0;
	selectorTileId = 0;
	tileSelector = selector();
	tileSelector.selectedTile = tiles::get(selectorTileId);
	scale = 4;
	waterView = false;
	placementMode = false;
	infoMode = false;
	statsMode = false;
	
	waterSupply = 0;
	waterDemand = 0;
	
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
	
			*getPartial(1,3) = tiles::COMMERCIAL_ZONE->getDefaultState();
			*getPartial(2,2) = tiles::COMMERCIAL_ZONE->getDefaultState();
			*getPartial(2,3) = tiles::ROAD->getDefaultState();
			*getPartial(2,4) = tiles::COMMERCIAL_ZONE->getDefaultState();
			*getPartial(3,3) = tiles::COMMERCIAL_ZONE->getDefaultState();
			
	
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
	int width = 2 * scale;
	int height = 1 * scale;
	tileComplete tc;
	
	for (int x = tileMapWidth - 1; x > - 1; x--) {
		for (int y = 0; y < tileMapHeight; y++) {
			float offsetx = (((y * 0.707106f) + (x * 0.707106f)) * 0.707106f) + viewX;
			float offsety = (((y * 0.707106f) - (x * 0.707106f)) * 0.707106f) + viewY;
			
			//Check if in view (TO-DO)
			if (offsetx * width + width < 0 || offsety * height + height < 0 || offsetx * width + width - width > adv::width || offsety * height + height - height > adv::height)
				continue;
			
			tc = getComplete(x,y);
			tilePartial *partial = tc.partial;
			
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
		}
	}
}
//0,0
void display() {
	{
		int width = 2 * scale;
		int height = 1 * scale;
		for (int x = 0; x < adv::width; x++) {
			for (int y = 0; y < adv::height; y++) {
				adv::write(x,y,'#', FGREEN | BBLACK);
			}
		}
	}
	
	//Edges of the map
	int corners[] = {
		0,0,
		tileMapWidth,0,
		tileMapWidth,tileMapHeight,
		0,tileMapHeight,
		0,0,
	};
	for (int i = 0; i < 8; i+=2) {
		break;
		//int next = (i + 2) % 8;
		int next = i + 2;
		int width = 2 * scale;
		int height = 1 * scale;
		float offsetx1 = (((corners[i + 1] * 0.707106f) + (corners[i])) * 0.707106f) + viewX - 0.707106f; 
		float offsety1 = (((corners[i + 1] * 0.707106f) - (corners[i])) * 0.707106f) + viewY + 0.707106f;
		float offsetx2 = (((corners[next + 1] * 0.707106f) + (corners[next])) * 0.707106f) + viewX - 0.707106f;
		float offsety2 = (((corners[next + 1] * 0.707106f) - (corners[next])) * 0.707106f) + viewY + 0.707106f;
		adv::line(offsetx1 * width, offsety1 * height, offsetx2 * width, offsety2 * height, '#', FWHITE|BBLACK);
		adv::write(offsetx1 * width, offsety1 * height, 'X', FRED|BBLACK);
	}
	//return;
	
	displayTileMap();
	
	if (placementMode) {
		int width = 2 * scale;
		int height = 1 * scale;
		float offsetx = (((selectorY * 0.707106f) + (selectorX * 0.707106f)) * 0.707106f) + viewX;
		float offsety = (((selectorY * 0.707106f) - (selectorX * 0.707106f)) * 0.707106f) + viewY;
		tileComplete cmp;
		tileSelector.draw(getComplete(&tileSelector.defaultState, &tileSelector, selectorX, selectorY, &cmp), offsetx * width, offsety * height, width, height);
	}
	
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
				float value = float(rand() % 255) / 255;
				
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
		printVar("placementMode", placementMode ? 1.0f : 0.0f);
		printVar("waterView", waterView ? 1.0f : 0.0f);
		printVar("infoMode", infoMode ? 1.0f : 0.0f);
	}
}

int wmain() {	
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
	
	while (!HASKEY(key = console::readKeyAsync(), VK_ESCAPE)) {
		adv::clear();
		
		if (placementMode) {
			switch (key) {
			case 'w':
				selectorY--;
				viewY+=.707106f * .707106f;
				viewX+=.707106f * .707106f;
				break;
			case 's':
				selectorY++;
				viewY-=.707106f * .707106f;
				viewX-=.707106f * .707106f;
				break;
			case 'd':
				selectorX++;
				viewX-=.707106f * .707106f;
				viewY+=.707106f * .707106f;
				break;
			case 'a':
				selectorX--;
				viewX+=.707106f * .707106f;
				viewY-=.707106f * .707106f;
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
				tilePartial *last = getPartial(selectorX, selectorY);
				*getPartial(selectorX, selectorY) = last->transferProperties(tileSelector.selectedTile->getDefaultState());
				tileComplete cmp = getComplete(selectorX, selectorY);
				cmp.parent->onCreate(&cmp, selectorX, selectorY);
				cmp.parent->updateNeighbors(&cmp, selectorX, selectorY);
			}
				break;
			case 'x':
			{
				if (waterView) {
					
					break;
				}
				tilePartial *last = getPartial(selectorX, selectorY);
				*getPartial(selectorX, selectorY) = last->transferProperties(tiles::GRASS->getDefaultState());
				tileComplete cmp = getComplete(selectorX, selectorY);
				cmp.parent->updateNeighbors(&cmp, selectorX, selectorY);
			}
			//Destroy
				break;
			case '2':
				selectorTileId++;
				if (selectorTileId > TILE_COUNT - 1)
					selectorTileId = 0;
				tileSelector.selectedTile = tiles::get(selectorTileId);
				break;
			case '1':
				selectorTileId--;
				if (selectorTileId < 0)
					selectorTileId = TILE_COUNT - 1;
				tileSelector.selectedTile = tiles::get(selectorTileId);
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
		}
		
		display();
		console::sleep(20);
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
	}	
	
	//_advancedConsoleDestruct();
	adv::construct.~_constructor();
	
	return 1;
}