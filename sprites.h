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

cpix empty_cpix() {
	cpix r;
	r.ch = 'X';
	r.co = FBLACK|BRED;
	return r;
}

/*
From 0-1 of a sample
*/
struct sample_source {
	virtual pixel sampleImage(float x, float y) { return pixel(0, 0, 0); }
	virtual cpix presampledImage(float x, float y) { return empty_cpix(); }
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
	cpix *presampledData;
	int bpp;

	pixel_image() {
		textureData = nullptr;
		presampledData = nullptr;
		bpp = 0;
	}

	~pixel_image() {
		stbi_image_free(textureData);
	}

	cpix presampledImage(float x, float y) override {
		if (!presampledData)
			return empty_cpix();

		posi dim = getDim();

		int imX = x * dim.x;
		int imY = y * dim.y;

		return presampledData[imY * dim.x + imX];
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

	void presample() {
		posi dim = getDim();
		int size = dim.x * dim.y;

		if (presampledData)
			delete[] presampledData;

		presampledData = new cpix[size];

		for (int i = 0; i < size; i++) {
			int offset = i * bpp;
			pixel pix;
			pix.r = textureData[offset + 0];
			pix.g = textureData[offset + 1];
			pix.b = textureData[offset + 2];
			if (bpp == 4)
				pix.a = textureData[offset + 3];
			cpix *target = &presampledData[i];
			getDitherColored(pix.r, pix.g, pix.b, &target->ch, &target->co);
			target->a = pix.a;
		}
	}

	void load(const char* path) {
		int x, y, n;
		textureData = stbi_load(path, &x, &y, &n, 0);

		if (!textureData)
			adv::error("Error loading image");

		dimensions.x = x;
		dimensions.y = y;
		bpp = n;

		presample();
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
	sizei atlasPixelArea;
	sizei atlasSpriteArea;
	
	posf mapToAtlas(posf p) {
		posf r;
		r.x = atlasPixelArea.x + p.x * atlasPixelArea.width;
		r.y = atlasPixelArea.y + p.y * atlasPixelArea.height;
		r.x /= sourceAtlas->getWidth();
		r.y /= sourceAtlas->getHeight();
		return r;
	}

	pixel sampleImage(float x, float y) override {
		posf atl = mapToAtlas(posf(x, y));
		return sourceAtlas->sampleImage(atl.x, atl.y);
	}
	cpix presampledImage(float x, float y) override {
		posf atl = mapToAtlas(posf(x, y));
		return sourceAtlas->presampledImage(atl.x, atl.y);
	}
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
	frag.atlasPixelArea = pixel_units;
	frag.atlasSpriteArea = sprite_units;

	return frag;
}

atlas mainAtlas(16);
adv_target mainTarget;

struct sprite : atlas_fragment {
	sprite(int x, int y) 
	:sprite(x,y,1,1) {}

	sprite(int x, int y, int w, int h)
	:atlas_fragment(mainAtlas.fragment(sizei(x, y, w, h))),
	 target(&mainTarget) {
		setAtlas(sizei(x, y, w, h));
	}

	draw_target *target;

	virtual void setAtlas(sizei area) {	atlasSpriteArea = area; }
	virtual int getPixT() { return sourceAtlas->spriteSize; }
	virtual int getPixTW() { return sourceAtlas->getWidth(); }
	virtual int getPixTH() { return sourceAtlas->getHeight(); }
	virtual int getAtlW() {	return atlasSpriteArea.width; }
	virtual int getAtlH() { return atlasSpriteArea.height; }
	virtual int getAtlX0() { return atlasSpriteArea.x; }
	virtual int getAtlY0() { return atlasSpriteArea.y; }
	virtual int getAtlX1() { return atlasSpriteArea.x + atlasSpriteArea.width; }
	virtual int getAtlY1() { return atlasSpriteArea.y + atlasSpriteArea.height; }

	/*
	Send start screen coordinates, screen width and height per sprite unit
	*/
	virtual void draw(sizei screen, sizei atlasXYWH, bool hflip = false, bool vflip = false) {
		sizei atlas = atlasSpriteArea;
		
		posi scrdim = target->getDim();
		sizei scrLimit = {0, 0, scrdim.x, scrdim.y};

		/*
		New origin for drawing
		*/
		posi origin = screen.start().add(0, screen.height);


		int yOffset = screen.height * (atlas.height - 1);
		sizei area = {origin.x, 
					  origin.y, 
					  screen.width * atlas.width, 
					  screen.height * atlas.height};

		for (int x = 0; x < area.width; x++) {
			for (int y = 0; y < area.height; y++) {
				//posi scr = area.start().add(x,y-yOffset);
				posi scr = area.start().add(x,-y);

				if (!scrLimit.contains(scr))
					continue;

				float xf = float(x) / area.width;
				float yf = float(y) / area.height;

				if (hflip)
					xf = .99999 - xf;
				if (!vflip)
					yf = .99999 - yf;

				cpix chco = presampledImage(xf, yf);
				if (chco.a < 255)
					continue;

				target->draw(scr, chco);
			}
		}

		target->draw(origin, empty_cpix());
	}

	void draw(sizef area, sizei atlas, bool hflip = false, bool vflip = false) {
		draw(area.x, area.y, area.width, area.height, atlas.x, atlas.y, atlas.width, atlas.height, hflip, vflip);
	}

	void draw(sizef area) {
		draw(area, atlasSpriteArea);
	}

	void draw(int scrX0, int scrY0, int scrW, int scrH) {
		draw(sizei{scrX0, scrY0, scrW, scrH}, atlasSpriteArea);	
	}	

	void draw(int scrX0, int scrY0, int scrW, int scrH, int atl0, int atl1, int atl2, int atl3, bool hflip = false, bool vflip = false) {
		draw(sizei{scrX0, scrY0, scrW, scrH}, sizei{atl0, atl1, atl2, atl3}, hflip, vflip);
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

	pixel sampleImage(float x, float y) override {
		//return pixel(12,190,50);
		int imX = x * _pixTW;
		int imY = y * _pixTH;
		return canvas[imY * _pixTW + imX];
	}

	cpix presampledImage(float x, float y) override {
		int imX = x * _pixTW;
		int imY = y * _pixTH;
		cpix chco;
		
		float r = getPixT() / getPixTW();

		pixel pix = sampleImage(x, y);

		getDitherColored(pix.r, pix.g, pix.b, &chco.ch, &chco.co);
		chco.a = pix.a;

		return chco;
	}

	void set(sprite *sp) {
		float spxf0 = (sp->getAtlX0() * sp->getPixT()) / float(sp->sourceAtlas->getWidth());
		float spyf0 = (sp->getAtlY0() * sp->getPixT()) / float(sp->sourceAtlas->getHeight());
		float spxf1 = (sp->getAtlX1() * sp->getPixT()) / float(sp->sourceAtlas->getWidth());
		float spyf1 = (sp->getAtlY1() * sp->getPixT()) / float(sp->sourceAtlas->getHeight());

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

				pixel pix = sp->sampleImage(sxf, syf); 
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