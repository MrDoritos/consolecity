#pragma once
#include "../console/advancedConsole.h"
#include "../imgcat/colorMappingFast.h"
#define STB_IMAGE_IMPLEMENTATION
#include "../imgcat/stb_image.h"

template<typename T>
struct _size;
template<typename T>
struct _pos;
template<typename character, typename color, typename alpha>
struct _cc;
template<typename _bit>
struct _pixel;

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
        return vecop(p, add);
    }
    _pos<T> operator-(_pos<T> &p) {
        return vecop(p, sub);
    }
    _pos<T> add(T x, T y) {
        return {this->x + x, this->y + y};
    }
    _pos<T> sub(T x, T y) {
        return {this->x - x, this->y - y};
    }
    _pos<T> add(_pos<T> p) {
        return add(p.x, p.y);
    }
    _pos<T> sub(_pos<T> p) {
        return sub(p.x, p.y);
    }
    template<typename VECOP>
    _pos<T> vecop(_pos<T> b, VECOP d) {
        return {d(x, b.x), d(y, b.y)};
    }
    _pos<T> start() {
        return {x,y};
    }
    _pos<T> north(){return this->add(0, -1);}
    _pos<T> south(){return this->add(0, 1);}
    _pos<T> east(){return this->add(1, 0);}
    _pos<T> west(){return this->add(-1, 0);}
    T dist(_pos<T> p) {
        return sqrt(distsq(p));
    }
    T distsq(_pos<T> p) {
        //return pow(p.x - x, 2) + pow(p.y - y, 2);
        return ((p.x - x) * (p.x - x)) + ((p.y - y) * (p.y - y));
    }
    _size<T> with(T width, T height) {
        return {_pos<T>(x, y), width, height};
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

    _size(T x, T y):_pos<T>(x, y) {
        width = 1;
        height = 1;
    }

    _size() {
        width = 1;
        height = 1;
    }
    bool operator==(_size<T> &s) {
        return ((_pos<T>*)this)->operator==(s) && width == s.width && height == s.height;
    }
    _size<T> operator+(_size<T> s) {
        return this->add(s);
    }
    _size<T> operator-(_size<T> s) {
        return this->sub(s);
    }
    _size<T> add(_size<T> s) {
        return {this->x + s.x, this->y + s.y, width + s.width, height + s.height};
    }
    _size<T> sub(_size<T> s) {
        return {this->x - s.x, this->y - s.y, width - s.width, height - s.height};
    }
    template<template<typename> typename POS2, typename T2>
    POS2<T2> cast() {
        return {this->x, this->y, width, height};
    }
    bool contains(_pos<T> p) {
        return p.x >= this->x && p.x < this->x + width && p.y >= this->y && p.y < this->y + height;
    }
    bool contains(_size<T> s) {
        return 
            contains(s.start()) &&
            contains(s.end());
    }
    bool contains(T radius) {
        return distsq() < radius * radius;
    }
    _pos<T> length() {
        return {width, height};
    }
    _pos<T> center() {
        return {this->x + width / 2, this->y + height / 2};
    }
    _pos<T> end() {
        return this->start().add(length());
    }
    T distsq() {
        return this->start().distsq(length());
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
typedef _cc<wchar_t, color_t, color_t> cpix;
typedef unsigned char ubyte;
typedef _pixel<ubyte> pixel;

cpix *texturechco;

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

template<typename character, typename color, typename alpha>
struct _cc {
	character ch;
	color co;
	alpha a;
};

template<typename _bit>
struct _pixel {
	_pixel()
    :r(0), g(0), b(0), a(255) {}
	_pixel(_bit r, _bit g, _bit b)
    :_pixel(r, g, b, 255) {}
    _pixel(_bit r, _bit g, _bit b, _bit a)
    :r(r), g(g), b(b), a(a) {}

	_bit r,g,b,a;
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

cpix sampleImageCHCO(float x, float y) {
	int imX = x * textureWidth;
	int imY = y * textureHeight;
	cpix chco = texturechco[imY * textureWidth + imX];
	return chco;
}