#pragma once
#include "graphics.h"

struct dimension;
struct draw_target;
struct adv_target;
struct buffer_target;
struct retained_target;
struct sample_source;
struct image;
struct pixel_image;
struct atlas;
struct atlas_fragment;
struct sprite;
struct sprite_overlay;
struct random_overlay;
struct simple_connecting_sprite;

struct dimension {
	virtual int getWidth() { return 0; }
	virtual int getHeight() { return 0; }
	virtual posi getDim() { return {getWidth(), getHeight()}; }
	virtual sizei getSize() { return {{},getWidth(),getHeight()}; }
};

/*
Simple target for drawing
*/
struct draw_target : dimension {
	virtual void draw(posi p, pixel pix) {
		cpix c;
		getDitherColored(pix.r, pix.g, pix.b, &c.ch, &c.co);
		draw(p, c);
	}
	virtual void draw(posi p, cpix pix) {

	}
	virtual void draw(buffer_target *_in) {
		draw(_in, {});
	}
	virtual void draw(buffer_target *_in, posi XY);
	virtual bool is_retained_mode() { return false; }
};

struct adv_target : draw_target {
	int getWidth() override { return adv::width; }
	int getHeight() override { return adv::height; }
	void draw(posi p, cpix pix) override {
		adv::write(p.x, p.y, pix.ch, pix.co);
	}
};

struct buffer_target : draw_target {
	buffer_target(sizei XYWH) {
		this->stale = true;
		this->size = {0,0,0,0};
		this->buffer = nullptr;
		allocate(XYWH);
	}

	~buffer_target() {
		free();
	}

	void clear() {
		for (int i = 0; i < size.area(); i++) {
			buffer[i] = null_cpix();
		}
	}

	void free() {
		if (buffer)
			delete[] buffer;
	}

	void allocate(sizei XYWH) {
		if (size == XYWH)
			return;

		free();
		size = XYWH;
		buffer = new cpix[size.area()];
		clear();
	}

	cpix *buffer;
	sizei size;

	cpix *get(posi p) {
		return &buffer[size.get(p)];
	}

	sizei getSize() override { return size; }
	int getWidth() override { return size.width; }
	int getHeight() override { return size.height; }

	using draw_target::draw;
	void draw(posi p, cpix pix) override {
		*get(p) = pix;
	}

	bool shouldUpdate() {
		return stale;
	}

	bool stale;

	virtual bool is_retained_mode() { return true; }
};

void draw_target::draw(buffer_target *_in, posi XY) {
	sizei os = this->getSize();
	sizei is = _in->getSize();
	for (int x = 0; x < is.width; x++) {
		for (int y = 0; y < is.height; y++) {
			posi ob = XY.add(x,y);
			posi ib = posi(x,y);
			if (os.contains(ob)) {
				cpix pix = *_in->get(ib);
				if (pix.ch != 0)
					draw(ob, pix);
			}
		}
	}
}

struct retained_target : buffer_target {
	retained_target(sizei XYWH, image *img=nullptr, int state=0) : buffer_target(XYWH) {
		this->reference_image = img;
		this->state = state;
	}

	void clear() {
		for (int i = 0; i < size.area(); i++) {
			buffer[i] = null_cpix();
		}
	}

	image *reference_image; 
	int state;
	bool persistent;
	bool render;

	bool shouldDelete() {
		return !persistent && !render;
	}
};

std::vector<retained_target*> retained_targets;

/*
From 0-1 of a sample
*/
struct sample_source {
	virtual pixel sampleImage(float x, float y) { return pixel(0, 0, 0); }
	virtual cpix sampleComposed(float x, float y) { return empty_cpix(); }
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
	bool stbi = false;
	ubyte *textureData;
	cpix *composedData;
	int bpp;

	pixel_image() {
		textureData = nullptr;
		composedData = nullptr;
		bpp = 0;
		stbi = false;
	}

	~pixel_image() {
		free_image_data();
		free_composed_data();
	}

	void free_image_data() {
		if (textureData != nullptr) {
			if (stbi)
				stbi_image_free(textureData);
			else
				delete[] textureData;
		}
	}

	void free_composed_data() {
		if (composedData != nullptr) {
			delete[] composedData;
			composedData = nullptr;
		}
	}

	cpix sampleComposed(float x, float y) override {
		if (!composedData)
			return empty_cpix();

		posi dim = getDim();

		int imX = x * dim.x;
		int imY = y * dim.y;

		return composedData[imY * dim.x + imX];
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

	void compose() {
		posi dim = getDim();
		int size = dim.x * dim.y;

		free_composed_data();

		composedData = new cpix[size];

		for (int i = 0; i < size; i++) {
			int offset = i * bpp;
			pixel pix;
			pix.r = textureData[offset + 0];
			pix.g = textureData[offset + 1];
			pix.b = textureData[offset + 2];
			if (bpp == 4)
				pix.a = textureData[offset + 3];
			cpix *target = &composedData[i];
			getDitherColored(pix.r, pix.g, pix.b, &target->ch, &target->co);
			target->a = pix.a;
		}
	}

	void load(const char* path) {
		int x, y, n;
		
		free_image_data();

		textureData = stbi_load(path, &x, &y, &n, 0);

		if (!textureData)
			adv::error("Error loading image");

		dimensions.x = x;
		dimensions.y = y;
		bpp = n;

		compose();
	}

	void blank(posi dim) {
		free_image_data();

		textureData = new ubyte[dim.x * dim.y * 4];
		memset(textureData, 0, dim.x * dim.y * 4);

		dimensions = dim;
		bpp = 4;

		compose();
	}

	void overlay(sample_source *src) {
		if (!textureData || !src)
			return;

		posi dim = getDim();

		for (int x = 0; x < dim.x; x++) {
			for (int y = 0; y < dim.y; y++) {
				float xf = float(x) / dim.x;
				float yf = float(y) / dim.y;

				pixel pix = src->sampleImage(xf, yf);
				int offset = (y * dim.x + x) * bpp;

				textureData[offset + 0] = pix.r;
				textureData[offset + 1] = pix.g;
				textureData[offset + 2] = pix.b;
				if (bpp == 4)
					textureData[offset + 3] = pix.a;
			}
		}

		compose();
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
	cpix sampleComposed(float x, float y) override {
		posf atl = mapToAtlas(posf(x, y));
		return sourceAtlas->sampleComposed(atl.x, atl.y);
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
draw_target *mainTarget, *immediateTarget;

struct sprite : atlas_fragment {
	sprite(int x, int y) 
	:sprite(x,y,1,1) {}

	sprite(int x, int y, int w, int h)
	:atlas_fragment(mainAtlas.fragment(sizei(x, y, w, h))) {
		setAtlas(sizei(x, y, w, h));
	}

	virtual void setAtlas(sizei area) {	atlasSpriteArea = area; }

	/*
	Send start screen coordinates, screen width and height per sprite unit
	*/
	virtual void draw(sizei screen, sizei atlasXYWH, draw_target *target, bool hflip = false, bool vflip = false) {
		sizei atlas = atlasSpriteArea;
		
		posi scrdim = target->getDim();
		sizei scrLimit = {0, 0, scrdim.x, scrdim.y};

		posi origin = screen.start();

		sizei area = {
			origin.x, 
			origin.y, 
			screen.width * atlas.width, 
			screen.height * atlas.height
		};

		for (int x = 0; x < area.width; x++) {
			for (int y = 0; y < area.height; y++) {
				posi scr = area.start().add(x,-y);

				if (!scrLimit.contains(scr))
					continue;

				float xf = float(x) / area.width;
				float yf = float(y) / area.height;

				if (hflip)
					xf = .99999 - xf;
				if (!vflip)
					yf = .99999 - yf;

				cpix chco = sampleComposed(xf, yf);
				if (chco.a < 255)
					continue;

				target->draw(scr, chco);
			}
		}
	}

	void draw(sizei area, draw_target *target, bool hflip = false, bool vflip = false) {
		draw(area, atlasSpriteArea, target, hflip, vflip);
	}
};

struct sprite_overlay : sprite {
	sprite_overlay(sprite *base):sprite(*base) {		
		
	}

	pixel_image canvas;

	cpix sampleComposed(float x, float y) override {
		return canvas.sampleComposed(x, y);
	}

	pixel sampleImage(float x, float y) override {
		return canvas.sampleImage(x, y);
	}

	void set(sprite *sp) {
		canvas.blank(getDim());
		add(sp);
	}

	void add(sprite *sp) {
		canvas.overlay(sp);
	}
};

struct random_overlay : sprite_overlay {
	random_overlay(sprite *base, sprite *overlay):sprite_overlay(base) {
		//if (rand() % 5 == 0)
			//add(overlay);
			set(base);

			
	}
};

struct simple_connecting_sprite : sprite {
	//simple_connecting_sprite(sprite base, sprite over):sprite(base), base(base), over(over) {}

	simple_connecting_sprite(sprite *base, sprite *over):sprite(*base), base(base), over(over) {}

	//default sprite is tr
	void draw_connections(sizei area, draw_target *target,
						  bool tl, bool tr, bool bl, bool br) {
		base->draw(area, base->atlasSpriteArea, target);
		bool con[] = {tl,tr,bl,br};
		bool args[] = {
			false,true,
			true,true,
			true,false,
			false,false,
		};


		for (int i = 0; i < 4; i++) {
			if (con[i])
				over->draw(area, over->atlasSpriteArea, target, args[i * 2], args[i * 2 + 1]);
		}
	}

	void draw_connections(sizei area, draw_target *target, bool *con) {
		draw_connections(area, target, con[0], con[1], con[2], con[3]);
	}

	sprite *base, *over;
};

retained_target *make_or_get_retained_target(image *img, sizei XYWH, int state=0) {
	for (retained_target *rt : retained_targets) {
		if (rt->reference_image == img && 
			rt->state == state && 
			rt->size == XYWH)
				return rt;
	}

	retained_target *rt = new retained_target(XYWH, img, state);
	retained_targets.push_back(rt);
	return rt;
}

sprite selector_sprite(1,2,1,1);

sprite road_sprite(0,0,1,1);
sprite road_con_sprite(1,0,1,1);
sprite street_sprite(6,5,1,1);
sprite street_con_sprite(7,5,1,1);

sprite pool_sprite(4,3,1,1);
sprite pool_con_sprite(5,3,1,1);
sprite dry_pool_sprite(6,3,1,1);
sprite dry_pool_con_sprite(7,3,1,1);

sprite sand_sprite(4,2,1,1);
sprite grass_sprite(1,1,1,1);
sprite_overlay grass_sprite_random(&grass_sprite);
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
sprite building4_sprite(4,0,1,2);

simple_connecting_sprite road_con_tex_sprite(&road_sprite, &road_con_sprite);
simple_connecting_sprite street_con_tex_sprite(&street_sprite, &street_con_sprite);
simple_connecting_sprite pool_con_tex_sprite(&pool_sprite, &pool_con_sprite);
simple_connecting_sprite water_pipe_con_tex_sprite(&water_pipe_sprite, &water_pipe_con_sprite);