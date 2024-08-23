#pragma once
#include "graphics.h"

struct dimension {
	virtual int getWidth() { return 0; }
	virtual int getHeight() { return 0; }
	virtual posi getDim() { return {getWidth(), getHeight()}; }
};

/*
Simple target for drawing
*/
struct draw_target : dimension {
	virtual void draw(posi p, pixel pix) {}
	virtual void draw(posi p, cpix pix) {}
};

struct adv_target : draw_target {
	int getWidth() override { return adv::width; }
	int getHeight() override { return adv::height; }
	void draw(posi p, pixel pix) override {
		cpix c;
		getDitherColored(pix.r, pix.g, pix.b, &c.ch, &c.co);
		adv::write(p.x, p.y, c.ch, c.co);
	}
	void draw(posi p, cpix pix) override {
		adv::write(p.x, p.y, pix.ch, pix.co);
	}
};

/*
From 0-1 of a sample
*/
struct sample_source {
	virtual pixel sampleImage(float x, float y) { return pixel(0, 0, 0); }
	virtual cpix presampledImage(float x, float y) { return cpix(); }
};

/*
Add pixel dimensions to a sample
*/
struct image : sample_source, dimension {
	posi dimensions;
	int getWidth() override { return dimensions.x; }
	int getHeight() override { return dimensions.y; }
};

/*
Load image with pixel dimensions
*/
struct pixel_image : image {
	ubyte *textureData;
	int bpp;

	~pixel_image() {
		stbi_image_free(textureData);
	}

	pixel sampleImage(float x, float y) override {
		posi dim = getDim();

		int imX = x * dim.x;
		int imY = y * dim.y;

		int offset = imY * (dim.x * bpp) + (imX * bpp);

		pixel pix;
		pix.r = textureData[offset + 0];
		pix.g = textureData[offset + 1];
		pix.b = textureData[offset + 2];

		if (bpp == 4)
			pix.a = textureData[offset + 3];

		return pix;
	}

	void load(const char* path) {
		int x, y, n;
		textureData = stbi_load(path, &x, &y, &n, 0);

		if (!textureData)
			adv::error("Error loading image");

		dimensions.x = x;
		dimensions.y = y;
		bpp = n;
	}
};

struct atlas_fragment;

/*
Factory for atlas_fragments
*/
struct atlas : pixel_image {
	int spriteSize;
	atlas(int spriteSize) {
		this->spriteSize = spriteSize;
	}
	atlas_fragment fragment(sizei sprite_units);
};

/*
Image with pixel dimensions and reference to atlas
*/
struct atlas_fragment : image {
	atlas *sourceAtlas;
	sizei atlasArea;
};

atlas_fragment atlas::fragment(sizei sprite_units) {
	atlas_fragment frag;
	sizei pixel_units;
	frag.sourceAtlas = this;

	pixel_units.x = sprite_units.x * spriteSize;
	pixel_units.y = sprite_units.y * spriteSize;
	pixel_units.width = sprite_units.width * spriteSize;
	pixel_units.height = sprite_units.height * spriteSize;

	frag.dimensions = pixel_units.length();
	frag.atlasArea = pixel_units;

	return frag;
}

atlas mainAtlas(16);

struct sprite {
	sprite() {
		sourceAtlas = &mainAtlas;
	}

	sprite(int x, int y) 
	:sprite(x,y,1,1) {}

	sprite(int x, int y, int w, int h)
	:sprite() {
		setAtlas(sizei(x, y, w, h));
	}

	//int atlas[4];
	sizei atlasArea;
	atlas *sourceAtlas;

	virtual void setAtlas(sizei area) {	atlasArea = area; }

	virtual int getAtlW() {	return atlasArea.width; }

	virtual int getAtlH() { return atlasArea.height; }

	virtual int getPixT() { return textureSize; }

	virtual int getPixTW() { return textureWidth; }

	virtual int getPixTH() { return textureHeight; }

	virtual pixel sampleSprite(float x, float y) { return sampleImage(x, y); }

	virtual cpix sampleSpriteCHCO(float x, float y) { return sampleImageCHCO(x, y); }

	virtual void drawPixel(int x, int y, pixel pix) {
		wchar_t ch;
		color_t co;
		getDitherColored(pix.r, pix.g, pix.b, &ch, &co);
		adv::write(x, y, ch, co);
	}

	void draw(sizei area, sizei atlas, bool hflip = false, bool vflip = false) {
		draw(area.x, area.y, area.width, area.height, atlas.x, atlas.y, atlas.width, atlas.height, hflip, vflip);
	}

	void draw(int scrX0 /*screen offset x*/, int scrY0 /*screen offset y*/, int scrW /*draw width on screen*/, int scrH /*draw height on screen*/, int atl0, int atl1, int atl2, int atl3, bool hflip = false, bool vflip = false) {
		//Default is drawing texture from the atlas. ignoring transparent and retaining scale
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

				cpix chco = sampleSpriteCHCO(xf, yf);
				
				adv::write(scrX, scrY, chco.ch, chco.co);
			}
		}
	}

	void draw(sizef area) {
		draw(area.x, area.y, area.width, area.height);
	}

	virtual int getAtlX0() { return atlasArea.x; }
	virtual int getAtlY0() { return atlasArea.y; }
	virtual int getAtlX1() { return atlasArea.x + atlasArea.width; }
	virtual int getAtlY1() { return atlasArea.y + atlasArea.height; }

	virtual void draw(int scrX0, int scrY0, int scrW, int scrH) {
		draw(scrX0, scrY0, scrW, scrH, getAtlX0(), getAtlY0(), getAtlX1(), getAtlY1());	
	}
};

struct sprite_overlay : sprite {
	sprite_overlay(sprite *base):sprite(*base) {
		_pixTW = base->getAtlW() * base->getPixT();
		_pixTH = base->getAtlH() * base->getPixT();
		//fprintf(stderr, "New overlay sprite: %i %i\n", _pixTW, _pixTH);
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

	cpix sampleSpriteCHCO(float x, float y) override {
		int imX = x * _pixTW;
		int imY = y * _pixTH;
		cpix chco;
		
		float r = getPixT() / getPixTW();

		pixel pix = sampleSprite(x, y);

		getDitherColored(pix.r, pix.g, pix.b, &chco.ch, &chco.co);
		chco.a = pix.a;

		return chco;
	}

	void set(sprite *sp) {
		float spxf0 = (sp->getAtlX0() * sp->getPixT()) / float(textureWidth);
		float spyf0 = (sp->getAtlY0() * sp->getPixT()) / float(textureHeight);
		float spxf1 = (sp->getAtlX1() * sp->getPixT()) / float(textureWidth);
		float spyf1 = (sp->getAtlY1() * sp->getPixT()) / float(textureHeight);

		int cw = getPixTW();
		int ch = getPixTH();

		//fprintf(stderr, "Setting overlay sprite: %i %i\n", cw, ch);


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
		float spxf0 = (sp->getAtlX0() * sp->getPixT() / float(sp->getPixTW()));
		float spyf0 = (sp->getAtlX1() * sp->getPixT() / float(sp->getPixTH()));
		float spxf1 = (sp->getAtlY0() * sp->getPixT() / float(sp->getPixTW()));
		float spyf1 = (sp->getAtlY1() * sp->getPixT() / float(sp->getPixTH()));

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
			true,false,
			false,false,
		};


		for (int i = 0; i < 4; i++) {
			if (con[i])
				over->draw(scrX0, scrY0, scrW, scrH, over->getAtlX0(), over->getAtlY0(), over->getAtlX1(), over->getAtlY1(), args[i * 2], args[i * 2 + 1]);
		}
	}

	void draw_connections(sizef area, bool *con) {
		draw_connections(area.x, area.y, area.width, area.height, con[0], con[1], con[2], con[3]);
	}

	sprite *base, *over;
};

sprite selector_sprite(1,2,1,1);

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
simple_connecting_sprite water_pipe_con_tex_sprite(&water_pipe_sprite, &water_pipe_con_sprite);