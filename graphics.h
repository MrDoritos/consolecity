#pragma once
#include "../console/advancedConsole.h"
#include "../imgcat/colorMappingFast.h"
#define STB_IMAGE_IMPLEMENTATION
#include "../imgcat/stb_image.h"

struct ch_co_t;
struct pixel;

template<typename T>
struct _pos {
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
    _size(T width, T height) {
        this->width = width;
        this->height = height;
    }
    _size() {
        width = 0;
        height = 0;
    }
    bool operator==(_size<T> &s) {
        return ((_pos<T>*)this)->operator==(s) && width == s.width && height == s.height;
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

float getWidth() {
	return 2.0f * scale;
}

float getHeight() {
	return 1.0f * scale;
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