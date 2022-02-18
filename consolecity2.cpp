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

#define TILE_COUNT 12
#define NORTH 1
#define EAST 2
#define SOUTH 4
#define WEST 8
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

struct tilePartial {
	tilePartial() { id = 0; data.b = 0; }
	unsigned char id;
	union {
		unsigned char a[8];
		unsigned long b;
	} data;
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
		data.a[index] |= direction;
	}
	bool hasConnection(int direction, int index = 0) {
		return (data.a[index] & direction);
	}
	void setUnderground(int type) {
		data.a[0] |= type << 4;
	}
	bool hasUnderground(int type) {
		return (data.a[0] & type);
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

tilePartial *tileMap = nullptr;
int tileMapHeight = 10;
int tileMapWidth = 10;
int textureSize = 16;
int scale = 4;
int selectorTileId = 0;

float viewX;
float viewY;

int selectorX;
int selectorY;

int waterSupply;
int waterDemand;

unsigned char *texture;
int textureHeight;
int textureWidth;
int bpp;

bool placementMode;
bool waterView;

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

tilePartial *getPartial(int, int);
tile *getTile(tilePartial*);

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
	
	virtual pixel getTexture(tilePartial *tp, float x, float y) {
		//return int(x * 2) > 1 && int(y * 2) < 1 ? pixel(0,0,0) : pixel(255,0,255);
		//return pixel(rand() % 255, rand() % 255, rand() % 255);
		return pixel(x*y* 255, x* 255, y* 255);
	}
	
	//offsetx,offsety top left
	//sizex,sizey width height
	virtual void draw(tilePartial *tp, float offsetx, float offsety, float sizex, float sizey) {
		//return;
		//Default is drawing texture from the atlas. ignoring transparent and retaining scale
		for (int x = 0; x < sizex; x++) {
			for (int y = 0; y < sizey; y++) {
				if (offsetx + x >= adv::width || offsety + y >= adv::height || offsetx + x < 0 || offsety + y < 0)
					continue;
				//float xf = float(x) / sizex;
				//float yf = float(y) / sizey;
				float xf = ((textureAtlas[0] * textureSize) + ((float(x) / sizex) * textureSize)) / textureWidth;
				float yf = ((textureAtlas[1] * textureSize) + ((float(y) / sizey) * textureSize)) / textureHeight;
				pixel pix = sampleImage(xf, yf);
				if (pix.a < 255)
					continue;
				wchar_t ch;
				color_t co;
				getDitherColored(pix.r,pix.g,pix.b,&ch,&co);
				adv::write(offsetx + x, offsety + y, ch, co);
			}
		}
	}
	
	virtual void onCreate(tilePartial *tp, int x, int y) {}
	
	virtual void onUpdate(tilePartial *tp, int x, int y) {}
	
	void updateNeighbors(tilePartial *tp, int x, int y) {
		tilePartial *neighbors[4];
		neighbors[0] = getPartial(x,y-1);//NORTH
		neighbors[1] = getPartial(x+1,y);//EAST
		neighbors[2] = getPartial(x,y+1);//SOUTH
		neighbors[3] = getPartial(x-1,y);//WEST
		getTile(neighbors[0])->onUpdate(neighbors[0], x,y-1);
		getTile(neighbors[1])->onUpdate(neighbors[1], x+1,y);
		getTile(neighbors[2])->onUpdate(neighbors[2], x,y+1);
		getTile(neighbors[3])->onUpdate(neighbors[3], x-1,y);
	}
	
	virtual void onRandomTick(tilePartial *tp, int x, int y) {}
};

struct tileable : public tile {	
	virtual void connectToNeighbors(tilePartial *tp, int x, int y) {
		unsigned char old = tp->data.a[0];
		tp->data.a[0] = 0;
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
				tp->setConnection(1 << i);
			}
		}
	}
	
	virtual bool connectingCondition(tilePartial *tp, int direction) {
		return tp->hasConnection(direction);
	}
	
	int connectingTextureAtlas[4];
	
	void onCreate(tilePartial *tp, int x, int y) override {
		connectToNeighbors(tp,x,y);
	}
	
	void onUpdate(tilePartial *tp, int x, int y) override {
		connectToNeighbors(tp,x,y);
	}
	
	void draw(tilePartial *tp, float offsetx, float offsety, float sizex, float sizey) override {
		for (int x = 0; x < sizex; x++) {
			for (int y = 0; y < sizey; y++) {
				if (offsetx + x >= adv::width || offsety + y >= adv::height || offsetx + x < 0 || offsety + y < 0)
					continue;
				float xf = ((textureAtlas[0] * textureSize) + ((float(x) / sizex) * textureSize)) / textureWidth;
				float yf = ((textureAtlas[1] * textureSize) + ((float(y) / sizey) * textureSize)) / textureHeight;
				pixel pix = sampleImage(xf, yf);
				for (int i = 0; i < 4; i++) {
					if (connectingCondition(tp, 1 << i)) {
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
						if (pix2.a == 255)
							pix = pix2;						
					}
				}
				
				if (pix.a < 255)
					continue;
				wchar_t ch;
				color_t co;
				getDitherColored(pix.r,pix.g,pix.b,&ch,&co);
				adv::write(offsetx + x, offsety + y, ch, co);
			}
		}
	}
};

tilePartial *getPartial(int x, int y) {
	if (x >= tileMapWidth || x < 0 || y >= tileMapHeight || y < 0)
		return &tiles::DEFAULT_TILE->defaultState;
	return &tileMap[y * tileMapWidth + x];
}

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
	
	void draw(tilePartial *tp, float offsetx, float offsety, float sizex, float sizey) override {
		if (ticks++ % 20 > 9) {
			tile::draw(tp,offsetx,offsety,sizex,sizey);
			if (waterView) {
				tiles::WATER_PIPE->draw(&tiles::WATER_PIPE->defaultState, offsetx, offsety, sizex, sizey);
			} else {
				selectedTile->draw(&selectedTile->defaultState, offsetx, offsety, sizex, sizey);
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
		
		defaultState = getDefaultState();
	}
		
	void draw(tilePartial *tp, float offsetx, float offsety, float sizex, float sizey) override {
		for (int x = 0; x < sizex; x++) {
			for (int y = 0; y < sizey; y++) {
				if (offsetx + x >= adv::width || offsety + y >= adv::height || offsetx + x < 0 || offsety + y < 0)
					continue;
				float xf = ((textureAtlas[0] * textureSize) + ((float(x) / sizex) * textureSize)) / textureWidth;
				float yf = ((textureAtlas[1] * textureSize) + ((float(y) / sizey) * textureSize)) / textureHeight;
				pixel pix = sampleImage(xf, yf);
				for (int i = 0; i < 4; i++) {
					if (tp->data.a[0] & (1 << i)) {
						float xfto = (float(x) / sizex) * textureSize;
						float yfto = (float(y) / sizey) * textureSize;
						if (i == 1 || i ==2)
							xfto = (1.99f * textureSize) - xfto;
						else
							xfto = (1 * textureSize) + xfto;
						if (i == 0 || i == 1)
							yfto = (0.99f * textureSize) - yfto;
						else
							yfto = (0 * textureSize) + yfto;
						xfto /= textureWidth;
						yfto /= textureHeight;
						pixel pix2 = sampleImage(xfto, yfto);
						if (pix2.a == 255)
							pix = pix2;						
					}
				}
				
				if (pix.a < 255)
					continue;
				wchar_t ch;
				color_t co;
				getDitherColored(pix.r,pix.g,pix.b,&ch,&co);
				adv::write(offsetx + x, offsety + y, ch, co);
			}
		}
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
	
	void onRandomTick(tilePartial *tp, int x, int y) override {
		tp->setBoolean(true, 0);
		//Start to pay them taxes!
		//setAtlas(2,3,3,4);
	}
	
	void onCreate(tilePartial *tp, int x, int y) override {
		connectToNeighbors(tp,x,y);
		
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
	
	void draw(tilePartial *tp, float offsetx, float offsety, float sizex, float sizey) override {
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
							if (pix2.a == 255)
								pix = pix2;						
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
						if (pix2.a == 255)
							pix = pix2;
					}
				}
				
				if (pix.a < 255)
					continue;
				wchar_t ch;
				color_t co;
				getDitherColored(pix.r,pix.g,pix.b,&ch,&co);
				adv::write(offsetx + x, offsety + y, ch, co);
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
	
	void draw(tilePartial *tp, float offsetx, float offsety, float sizex, float sizey) override {
		if (!tp->hasWater()) {		
			textureAtlas[0] = 2;
			textureAtlas[1] = 1;
			textureAtlas[2] = 3;
			textureAtlas[3] = 2;		
		} else {		
			textureAtlas[0] = 1;
			textureAtlas[1] = 3;
			textureAtlas[2] = 2;
			textureAtlas[3] = 4;
		}
				
		tile::draw(tp,offsetx,offsety,sizex,sizey);		
	}
};

struct water_tower : public tile {
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
	
	void draw(tilePartial *tp, float offsetx, float offsety, float sizex, float sizey) override {
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
				wchar_t ch;
				color_t co;
				getDitherColored(pix.r,pix.g,pix.b,&ch,&co);
				adv::write(offsetx + x /*+ (textureSizeW * sizex)*/, offsety + y - ((textureSizeH - 1) * sizey), ch, co);
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
	
	bool connectingCondition(tilePartial *tp, int direction) override {
		if (tp->id == tiles::WATER_PIPE->id)
			return tileable::connectingCondition(tp, direction);
		return tp->hasConnection(direction, 2);
		//return true;
	}
	
	void connectToNeighbors(tilePartial *tp, int x, int y) override {
		if (tp->id == tiles::WATER_PIPE->id)
			tileable::connectToNeighbors(tp,x,y);
		tp->data.a[2] = 0;
		tilePartial *neighbors[4];
		neighbors[0] = getPartial(x,y-1);//NORTH
		neighbors[1] = getPartial(x+1,y);//EAST
		neighbors[2] = getPartial(x,y+1);//SOUTH
		neighbors[3] = getPartial(x-1,y);//WEST
		for (int i = 0; i < 4; i++) {
			if (neighbors[i]->hasUnderground(UNDERGROUND_WATER_PIPE)) {
				tp->setConnection(1 << i, 2);
			}
		}
	}
	
	void onCreate(tilePartial *tp, int x, int y) override {
		tp->setUnderground(UNDERGROUND_WATER_PIPE);
		tileable::onCreate(tp,x,y);
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
			getTile(getPartial(x,y))->onCreate(getPartial(x,y), x, y);
			//getTile(getPartial(x,y))->onUpdate(getPartial(x,y), x, y);
		}
	}
}

void displayTileMap() {
	int width = 2 * scale;
	int height = 1 * scale;
	
	for (int i = 0; i < 3; i++) {
		break;
		color_t a[] = { 255,0,0,0 };
		color_t b[] = { 0,255,0,0 };
		color_t c[] = { 0,0,255,0 };
		pixel pix = pixel(a[i],b[i],c[i]);
		wchar_t ch;
		color_t co;
		getDitherColored(pix.r,pix.g,pix.b,&ch,&co);
		adv::write(i,0,ch,co);
	}
	
	for (int x = tileMapWidth - 1; x > - 1; x--) {
		//for (int x = 0; x < tileMapWidth; x++) {
		for (int y = 0; y < tileMapHeight; y++) {
			float offsetx = (((y * 0.707106f) + (x * 0.707106f)) * 0.707106f) + viewX;
			float offsety = (((y * 0.707106f) - (x * 0.707106f)) * 0.707106f) + viewY;
			tilePartial *partial = getPartial(x,y);
			if (waterView) {
				tiles::DIRT->draw(&tiles::DIRT->defaultState, offsetx * width, offsety * height, width, height);
				if (!partial->hasUnderground(UNDERGROUND_WATER_PIPE))
					continue;
				//getTile(partial)->draw(partial, offsetx * width, offsety * height, width, height);
				tiles::WATER_PIPE->draw(partial, offsetx * width, offsety * height, width, height);
				
				continue;
			}	
			
			//Check if in view (TO-DO)
			if (offsetx * width + width < 0 || offsety * height + height < 0 || offsetx * width + width - width > adv::width || offsety * height + height - height > adv::height)
				continue;
				
			getTile(partial)->draw(partial, offsetx * width, offsety * height, width, height);
			continue;
									
			for (int xp = 0; xp < width; xp++) {
				for (int yp = 0; yp < height; yp++) {
					float xfp = float(xp) / width;
					float yfp = float(yp) / height;
					//pixel pix = getTile(getPartial(x,y))->getTexture(getPartial(x,y), xfp, yfp);
					pixel pix = tiles::DEFAULT_TILE->getTexture(&tiles::DEFAULT_TILE->defaultState, xfp, yfp);
					//pixel pix = pixel(128,128,128);
					wchar_t ch;
					color_t co;
					getDitherColored(pix.r,pix.g,pix.b,&ch,&co);
					adv::write(x * width + (xfp * width), y * height + (yfp * height), ch, co);
				}
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
		tileSelector.draw(&tileSelector.defaultState, offsetx * width, offsety * height, width, height);
	}
}

int wmain() {	
	texture = stbi_load("textures.png", (int*)&textureWidth, (int*)&textureHeight, &bpp, 0);
	
	while (!adv::ready) console::sleep(10);
	
	#ifdef __linux__
	curs_set(0);
	//adv::setDrawingMode(DRAWINGMODE_COMPARE);
	#endif
	adv::setThreadState(false);
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
				//Place
				if (waterView) {
					tilePartial *partial = getPartial(selectorX, selectorY);
					partial->setUnderground(UNDERGROUND_WATER_PIPE);
					tiles::WATER_PIPE->onCreate(partial, selectorX, selectorY);
					tiles::WATER_PIPE->updateNeighbors(partial, selectorX, selectorY);
					break;
				}
				*getPartial(selectorX, selectorY) = tileSelector.selectedTile->getDefaultState();
				getTile(getPartial(selectorX, selectorY))->onCreate(getPartial(selectorX, selectorY), selectorX, selectorY);
				getTile(getPartial(selectorX, selectorY))->updateNeighbors(getPartial(selectorX, selectorY), selectorX, selectorY);
				break;
			case 'x':
				if (waterView) {
					
					break;
				}
				*getPartial(selectorX, selectorY) = tiles::GRASS->getDefaultState();
				getTile(getPartial(selectorX, selectorY))->updateNeighbors(getPartial(selectorX, selectorY), selectorX, selectorY);
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
			case ',':
			//case 'z':
				scale++;
				break;
			case '.':
			//case 'x':
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
				tilePartial *partial = getPartial(x, y);
				getTile(partial)->onRandomTick(partial, x, y);
			}
		}
	}	
	
	//_advancedConsoleDestruct();
	adv::construct.~_constructor();
	
	return 1;
}