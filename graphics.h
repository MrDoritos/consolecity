#pragma once
#include "../console/advancedConsole.h"
#include "../imgcat/colorMappingFast.h"
#define STB_IMAGE_IMPLEMENTATION
#include "../imgcat/stb_image.h"

struct ch_co_t;
struct pixel;

template<typename T>
struct _pos {
    _pos(const _pos<T> &p) {
        x = p.x;
        y = p.y;
    }
    _pos(T x, T y) {
        this->x = x;
        this->y = y;
    }
    _pos() {
        x = 0;
        y = 0;
    }
    bool operator==(_pos<T> &p) {
        return x == p.x && y == p.y;
    }
    _pos<T> operator+(_pos<T> &p) {
        return {x + p.x, y + p.y};
    }
    _pos<T> add(T x, T y) {
        return {this->x + x, this->y + y};
    }
    _pos<T> north(){return this->add(0, -1);}
    _pos<T> south(){return this->add(0, 1);}
    _pos<T> east(){return this->add(1, 0);}
    _pos<T> west(){return this->add(-1, 0);}
    T dist(_pos<T> p) {
        return sqrt(pow(p.x - x, 2) + pow(p.y - y, 2));
    }
    T x, y;
};

template<typename T>
struct _size : public _pos<T> {
    _size(T x, T y, T width, T height) {
        this->x = x;
        this->y = y;
        this->width = width;
        this->height = height;
    }

    _size(const _pos<T> &p, T width, T height): _pos<T>(p) {
        this->width = width;
        this->height = height;
    }

    _size(const _pos<T> &p):_pos<T>(p) {
        width = 1;
        height = 1;
    }

    _size() {
        width = 1;
        height = 1;
    }
    _pos<T> center() {
        return {this->x + width / 2, this->y + height / 2};
    }
    bool operator==(_size<T> &s) {
        return ((_pos<T>*)this)->operator==(s) && width == s.width && height == s.height;
    }
    _size<T> add(_size<T> s) {
        return {this->x + s.x, this->y + s.y, width + s.width, height + s.height};
    }
    _size<T> operator+(_size<T> s) {
        return add(s);
    }
    T area() {
        return width * height;
    }
    T width, height;
};

typedef _pos<int> posi;
typedef _size<int> sizei;
typedef _pos<float> posf;
typedef _size<float> sizef;

ch_co_t *texturechco;

unsigned char *texture;
int textureHeight;
int textureWidth;
int bpp;

int textureSize = 16;
float frametime = 33.333f;
float xfact = 0.7071067812f; //0.5f;
float yfact = 0.7071067812f; //0.5f;

float scale = 4;
float viewX;
float viewY;

struct ch_co_t {
	wchar_t ch;
	color_t co;
	color_t a;
};

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

float getOffsetX(float x, float y) {
	return round(((((y * yfact) + (x * xfact)) * xfact) + viewX) * 1000.0f) / 1000.0f;
}

float getOffsetX(posf p) {
    return getOffsetX(p.x, p.y);
}

float getOffsetY(float x, float y) {
	return round(((((y * yfact) - (x * xfact)) * yfact) + viewY) * 1000.0f) / 1000.0f;
}

float getOffsetY(posf p) {
    return getOffsetY(p.x, p.y);
}

posf getOffsetXY(posf p) {
    return posf(getOffsetX(p), getOffsetY(p));
}

float getCharacterYoverX() {
    return 2.0f;
}

float getWidth() {
	return getCharacterYoverX() * scale;
}

float getHeight() {
	return 1.0f * scale;
}

float getScreenOffset(float ratio, float length, int offset_length) {
    int center = offset_length * ratio;
    return center - (length / 2);   
}

float getScreenOffsetX(float ratio, float width) {
    return getScreenOffset(ratio, width, adv::width);
}

float getScreenOffsetY(float ratio, float height) {
    return getScreenOffset(ratio, height, adv::height);
}

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