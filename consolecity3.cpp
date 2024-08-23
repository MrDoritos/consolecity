#include <math.h>
#include <chrono>
#include <vector>
#include <set>
#include <iterator>
#include "graphics.h"
#include "sprites.h"

#pragma region //Forward declarations, defines, and typedefs

typedef float net_t;

struct default_tile;
struct _game;
struct grass;
struct plop;
struct tile;
struct tileBase;
struct tileEvent;
struct tilePartial;
struct tileComplete;

tilePartial *getPartial(int x, int y);
tilePartial *getPartial(posi p);
tileComplete getComplete(int x, int y);
tileComplete getComplete(posi p);
tileEvent getEvent(sizei p);

#define ZONING_NONE 0
#define ZONING_RESIDENTIAL 1
#define ZONING_COMMERCIAL 2
#define ZONING_INDUSTRIAL 3
#define NORTH 1
#define EAST 2
#define SOUTH 4
#define WEST 8
#define NORTH_F x,y-1
#define EAST_F x+1,y
#define WEST_F x-1,y
#define SOUTH_F x,y+1
#define UNDERGROUND_WATER_PIPE 1
#define UNDERGROUND_SUBWAY 2

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

tilePartial *tileMap = nullptr;
int tileMapHeight = 100;
int tileMapWidth = 100;

FILE *logFile = stderr;

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

bool placementMode;
bool waterView;
bool infoMode;
bool statsMode;
bool queryMode;
bool plopsOnly;

#pragma endregion

#pragma region //Enums

enum EVENT {
	PLACE = 1, DESTROY = 2, UPDATE = 4, RANDOM = 8, NETWORK = 16
};

enum FLAG {
	FORCE = 1,
	TICK = 2,
	WATER = 4,
	POWER = 8,
	POLLUTION = 16,
	CRIME = 32,
	HEALTH = 64,
	EDUCATION = 128,
	TRAFFIC = 256,
	WEALTH = 512,
	SILENT = 1024
};

#pragma endregion

#pragma region //Function parameters or basic structs
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
	tileComplete(tileBase *parent, tilePartial *partial, sizei size)
		:size(size) {
		this->parent = parent;
		this->partial = partial;
		this->plop_instance = nullptr;
	}

	bool coordsEquals(tileComplete b) {
		return this->size == b.size;
	}

	bool operator==(tileComplete b) {
		return coordsEquals(b);
	}
	bool operator<(tileComplete b) {
		return size.x < b.size.x || size.y < b.size.y;
	}

	tileBase *parent;
	tilePartial *partial;
	plop *plop_instance;
	sizei size;
};

struct tileEvent : public tileComplete {
	tileEvent(tileComplete tc):tileComplete(tc) {
		init();
	}
	tileEvent() {
		init();
	}

	void init() {
		events = 0;
		flags = 0;
	}

	tileEvent with(int event, int flags) {
		tileEvent copy = *this;
		copy.events |= event;
		copy.flags |= flags;
		return copy;
	}

	tileEvent with(int event) {
		return with(event, 0);
	}

	tileEvent clone(tileEvent tc) {
		return tc.with(events,flags);
	}

	int events;
	int flags;
};

struct network_summary {
	net_t supply = 0, demand = 0, used = 0;
	int networks = 0;
};

#pragma endregion Parameters

#pragma region //Global objects with mostly forward declared functions

/*
Static, and maybe generic, functions for the game
*/
struct util {
	template<template<typename> typename POS, typename T1, typename FUNCTION>
	static void areaLoop(POS<T1> size, FUNCTION func);
	template<template<typename> typename POS, typename T1, typename T2,typename FUNCTION>
	static void radiusLoop(POS<T1> p, T2 radius, FUNCTION func);
	//void radiusLoop(posi p, float radius, FUNCTION func);
};

/*
Global instance registry
*/
struct instance_registry {
	instance_registry() {
		id = 1;
	}

	int nextId() {
		return id++;
	}

	tileBase *addInstance(tileBase *t);

	tileBase *getInstance(int id);

	tileBase *addPlaceable(tileBase *t);

	tileBase *getPlaceable(int id);

	int getPlaceableCount() {
		return placeable.size();
	}

	int id;

	std::vector<tileBase*> instances;
	std::vector<tileBase*> placeable;
} registry; //registry of game tiles and plops

/*
Functions that require a game instance
*/
struct _game {
	sizei mapSize;
	sizei getMapSize() { return mapSize; }

	void init(sizei size);
	std::vector<tileComplete> getTiles(sizei size);
	void destroy(sizei size);
	void destroy(tileEvent e);
	void place(sizei size, tileBase *base);
	void place(tileEvent e);
	void fireEvent(tileEvent e);
	void fireEvent(tileEvent e, int events);
	void fireEvent(sizei size, int events);
	void fireEvent(sizei size, int events, int flags);

	bool isInRadius(int ox, int oy, int px, int py, float r);
	bool isInRadius(sizei size, float r);
	bool isInBounds(posi p);
	bool isInBounds(sizei p);

	template<typename FUNCTION, typename Tradius>
	void tileRadiusLoop(posi p, Tradius radius, FUNCTION func);
	template<typename FUNCTION, typename Tradius>
	void tileRadiusLoop(int x, int y, Tradius radius, FUNCTION func);
	template<typename FUNCTION>
	void tileAreaLoop(sizei size, FUNCTION func);

} game; //singleton for the game

#pragma endregion

#pragma region //Game objects

struct tileEventHandler {
	virtual void handle(tileEvent e, bool silent = false) {
		if (~e.flags & SILENT && !silent)
			fprintf(logFile, "Handling events: %i %i pos: %i %i\n", e.events, e.flags, e.size.x, e.size.y);
		if (e.events & PLACE)
			onPlaceEvent(e);
		if (e.events & DESTROY)
			onDestroyEvent(e);
		if (e.events & UPDATE)
			onUpdateEvent(e);
		if (e.events & RANDOM)
			onRandomEvent(e);
		if (e.events & NETWORK)
			onNetworkEvent(e);
		onEvent(e);
	}
	virtual void onEvent(tileEvent e) {}
	virtual void onPlaceEvent(tileEvent e) {}
	virtual void onDestroyEvent(tileEvent e) {}
	virtual void onUpdateEvent(tileEvent e) {}
	virtual void onRandomEvent(tileEvent e) {}
	virtual void onNetworkEvent(tileEvent e) {}
};

struct network_value {
	network_value() {
		supply = 0;
		demand = 0;
		stored = 0;
		consumer = false;
	}
	network_value(net_t network_val):network_value() {
		if (network_val > 0) {
			supply = stored = network_val;
			consumer = false;
		} else {
			demand = -network_val;
			stored = 0;
			consumer = true;
		}
	}
	network_value(net_t supply, net_t demand)
	:supply(supply),demand(demand),stored(0),consumer(isDemand()) {}

	//Demand for network
	virtual net_t getDemand() {
		return consumer ? demand - stored : demand; 
	}
	//Supply for network
	virtual net_t getSupply() {
		return consumer ? 0 : stored;
	}

	virtual net_t give(net_t amount) {
		net_t need = getDemand();
		if (amount > need) {
			amount = need;
			stored += amount;
		} else {
			stored += amount;
			amount = 0;
		}
		return amount;
	}

	virtual net_t take(net_t amount) {
		net_t have = getSupply();
		if (amount > have) {
			amount = have;
			stored = 0;
		} else {
			stored -= amount;
		}
		return amount;
	}

	virtual bool isSupply() {
		return !consumer;
	}

	virtual bool isDemand() {
		return consumer;
	}

	virtual bool isSaturated() {
		return getSupply() == getDemand();
	}

	virtual void onNetworkEvent(tileEvent e) {
		fprintf(logFile, "onNetworkEvent: %i %p %f %f %f %s\n", e.flags, this, supply, demand, stored, consumer ? "consumer" : "producer");
		if (e.flags & TICK) {
			if (consumer)
				stored = stored - demand < 0 ? 0 : stored - demand;
			else
				stored = supply;
		}
	}

	private:
	net_t supply;
	net_t stored;
	net_t demand;
	bool consumer;
};

struct network_provider {
	float efficiency; //0-1
	float funding;
	float monthlyCost;

	network_value *water = 0;
	network_value *power = 0;
	network_value *pollution = 0;
	network_value *crime = 0;
	network_value *health = 0;
	network_value *education = 0;
	network_value *traffic = 0;
	network_value *wealth = 0;

	virtual std::vector<network_value*> getList() {
		std::vector<network_value*> values;
		network_value *array[8] = {
			water,power,pollution,crime,health,education,traffic,wealth
		};
		for (int i = 0; i < 8; i++) {
			if (array[i] != 0)
				values.push_back(array[i]);
		}
		return values;
	}

	virtual void free() {
		for (auto value : getList())
			if (value != nullptr)
				delete value;
		delete this;
	}

	virtual network_value *getNetwork(tileEvent e) {
		if (e.flags & WATER)
			return water;
		if (e.flags & POWER)
			return power;
		if (e.flags & POLLUTION)
			return pollution;
		if (e.flags & CRIME)
			return crime;
		if (e.flags & HEALTH)
			return health;
		if (e.flags & EDUCATION)
			return education;
		if (e.flags & TRAFFIC)
			return traffic;
		if (e.flags & WEALTH)
			return wealth;
		return nullptr;
	}

	virtual void onNetworkEvent(tileEvent e) {
		//fprintf(logFile, "onNetworkEvent: %i %p ", e.flags, this);
		for (network_value *value : getList()) {
			//fprintf(logFile, "%p ", value);
			value->onNetworkEvent(e);
		}
		//fprintf(logFile, "\n");
	}
};

network_provider no_provider;

struct tileBase : public tileEventHandler {
	int id;
	virtual void render(tileEvent e) = 0;
	/*
	Return screenspace coordinates for the XYWH of size
	Not related to sprite size
	Includes XY transformation
	Gives y from bottom now
	*/
	static sizef getRenderArea(sizei size) {
		posf aspect = posf{getWidth(),getHeight()};
		posf origin = getOffsetXY(posf{(float)size.x,(float)size.y});
		posf length = posf{(float)size.width,(float)size.height};
		float ystart = getOffsetY(size.x - 1, size.y + size.height);
		posf end = getOffsetXY(origin.add(length));

		sizef renderbox = {origin.x * aspect.x, //good
						   origin.y * aspect.y, //top
						   length.x * aspect.x, //good
						   length.y * aspect.y};//just length
		
		renderbox.y = ystart * aspect.y;
		renderbox.width = aspect.x;
		renderbox.height = aspect.y;

		return renderbox;
	}
	virtual sizef getRenderArea(tileEvent e) {
		return getRenderArea(e.size.with(1,1));
	}
	virtual sizei getSize(tileEvent e) {
		return e.size;
	}
	/*
		tiles do not need their size set,
		they do not even know their size
	*/
	virtual void setSize(tileEvent e) {}
	virtual tilePartial getDefaultState() {
		tilePartial partial;
		partial.id = id;
		return partial;
	}
	/*
		Return this or return copy as new memory

		How to determine whether or not to be cloned?

		Plops return new memory with id as 0
		Tiles return this, no change in id

		Performs register if needed
	*/
	virtual tileBase *clone() = 0;
	virtual bool isSingleton() = 0;
	virtual void free() = 0;
	virtual bool isSameType(tileComplete tc) {
		return tc.parent == this;
	}
	bool isSameType(posi p) {
		return isSameType(getComplete(p));
	}
	virtual void getConnections(tileEvent e, bool *con) {
		tileComplete neighbors[4];
		getNeighbors(e, neighbors);
		for (int i = 0; i < 4; i++) {
			con[i] = isSameType(neighbors[i]);
		}
	}
	virtual void getNeighbors(tileEvent e, tileComplete *neighbors) {
		neighbors[0] = getComplete(e.size.north());//NORTH
		neighbors[1] = getComplete(e.size.east());//EAST
		neighbors[2] = getComplete(e.size.south());//SOUTH
		neighbors[3] = getComplete(e.size.west());//WEST
	}
	virtual void copyState(tilePartial *tile) {
		*tile = getDefaultState();
	}
	virtual network_provider *getNetworkProvider(tileEvent e) {
		return nullptr;
	}
	void onNetworkEvent(tileEvent e) override {
		network_provider *net = getNetworkProvider(e);
		if (net != nullptr)
			net->onNetworkEvent(e);
	}
	virtual plop *getPlop(tileEvent e) {
		if (e.partial->isPlop() && e.partial->getPlopId() == id)
			return (plop*)registry.getInstance(e.partial->getPlopId());
		return nullptr;
	}
	virtual plop *getPlop() {
		return nullptr;
	}	
	virtual std::vector<tileComplete> getTiles(tileEvent e) {
		return game.getTiles(e.size);
	}
	virtual bool stale(tileEvent e) {
		//Check availability of this
		if (!this)
			return true;

		//We don't know if we are a plop or a tile

		plop *plop = getPlop(e);

		//Check plop availability first
		//Getting nullptr through getPlop is quick
		if (plop != nullptr) {
			/*
				If we are here, 
					isPlop is true in partial
					getPlopId from partial is our id
					registry has plop
			*/
			if (e.plop_instance != plop)
				return true;
		}

		//Was not unset
		if (e.partial->isPlop())
			return true;

		//If don't even have parent, we are nothing
		if (e.parent != this)
			return true;

		//Check tile availability
		if (e.partial->id != id)
			return true;

		return false;
	}
};

struct tile : public tileBase {
	//int id;
	sprite tex;
	
	tile(bool add = false) : tile(sprite(0,0,1,1),add) {}
	
	tile(sprite tex, bool add = false) : tex(tex) {
		registry.addInstance(this);
		if (add)
			registry.addPlaceable(this);
	}

	tile(int t0, int t1, int t2, int t3, bool skip = false)
		:tile(sprite(t0,t1,t2-t0,t3-t1), !skip) {}
	
	void setAtlas(int a, int b, int c, int d) {
		tex.setAtlas({a,b,c-a,d-b});
	}
	
	void render(tileEvent e) override {
		int v = (e.size.x << 2) | 134;
		v |= (e.size.y << 16) | (v & 0xc0ffee);
		bool hflip = false;
		bool vflip = false;
		switch (v % 3) {
			case 0:
				hflip = true;
				break;
			case 1:
				vflip = true;
				break;
			case 2:
				break;
		}
		tex.draw(getRenderArea(e), sizei(), hflip, vflip);

	}

	void copyState(tilePartial *tile) override {
		tile->id = id;
	}

	network_provider *getNetworkProvider(tileEvent e) override {
		//return &no_provider;
		return nullptr;
	}

	plop *getPlop(tileEvent e) override {
		return nullptr;
	}

	plop *getPlop() override {
		return nullptr;
	}

	tileBase *clone() override {
		return this;
	}
	
	void free() override {
		return;
	}

	bool isSingleton() override {
		return true;
	}

	void updateNeighbors(tileEvent e) {
		tileComplete neighbors[4];
		getNeighbors(e, neighbors);
		for (int i = 0; i < 4; i++)
			if (neighbors[i].parent != nullptr)
				neighbors[i].parent->onUpdateEvent(neighbors[i]);
	}
};

struct tileable : public tile {	
	tileable(simple_connecting_sprite tex, bool add = false) :tile(add),tex_connecting(tex) {}

	simple_connecting_sprite tex_connecting;
	
	void render(tileEvent e) override {
		bool con[4];
		getConnections(e, &con[0]);
		tex_connecting.draw_connections(getRenderArea(e), &con[0]);
	}

	tileBase *clone() override {
		return this;
	}
};

struct plop : public tileBase {
	plop(sprite* tex, int plop_width = 1, int plop_height = 1, bool placeable=true):
	tex(tex),size(0,0,plop_width,plop_height) {
		registry.addInstance(this);
		if (placeable) {
			registry.addPlaceable(this);
		}

		initial_id = id;

		size = sizei(0, 0, plop_width, plop_height);
	}

	plop() {
		size = sizei(0,0,1,1);
		tex = nullptr;
		net = nullptr;
	}

	~plop() {
		registry.instances.erase(std::remove(registry.instances.begin(), registry.instances.end(), this), registry.instances.end());
		registry.placeable.erase(std::remove(registry.placeable.begin(), registry.placeable.end(), this), registry.placeable.end());
	}

	sizef getRenderArea(tileEvent e) override {
		if (e.plop_instance)
			return tileBase::getRenderArea(e.plop_instance->size);
		return tileBase::getRenderArea(e.size);
	}

	void render(tileEvent e) override {
		if (tex == nullptr || e.plop_instance == nullptr)
			return;

		posi rendererPos = e.size;
		sizei plopPos = e.plop_instance->size;
		posi renderingPos = plopPos;
		renderingPos.y = plopPos.end().y - 1;

		if (!(rendererPos == renderingPos))
			return;

		tex->draw(tileBase::getRenderArea(size));
	}	

	tilePartial getDefaultState() override {
		tilePartial tp = tileBase::getDefaultState();
		tp.setPlopId(id);
		return tp;
	}

	void copyState(tilePartial *tile) override {
		tile->setPlopId(id);
	}

	bool operator==(int i) {
		return id == i;
	}

	plop *getPlop(tileEvent e) override {
		return this;
	}

	plop *getPlop() override {
		return this;
	}

	sizei getSize(tileEvent e) override {
		return size;
	}

	bool isSingleton() override {
		return false;
	}

	tileBase *clone() override {
		return clone<plop>(this);
	}

	template<typename T>
	tileBase *clone(T *ref) {
		T *c = new T(*(ref));
		c->id = 0;
		return registry.addInstance(c);
	}

	bool isSameType(tileComplete tc) override {
		return tc.plop_instance && tc.plop_instance->initial_id == initial_id;
	}

	void free() override {
		delete this;
	}

	void setSize(tileEvent e) override {
		size = e.size;
	}

	virtual bool isPlaceable(tileEvent e) {
		fprintf(logFile, "isPlaceable(tileEvent e) not yet implemented\n");
		for (auto tile : getTiles(e)) {
			if (tile.partial->isPlop())
				return false;
		}
		return true;
	}

	network_provider *getNetworkProvider(tileEvent e) override {
		return net;
	}

	void onNetworkEvent(tileEvent e) override {
		//fprintf(logFile, "plop::onNetworkEvent: %i %p \n", e.flags, this);
		if (net != nullptr)
			net->onNetworkEvent(e);
	}

	void onPlaceEvent(tileEvent e) override {
		fprintf(logFile, "plop::onPlaceEvent %p %p %i %i %i\n", this, net, size.x, size.y, id);

	}

	int initial_id;
	sizei size;
	sprite *tex;
	network_provider *net;
};

struct plop_connecting : public plop {
	plop_connecting(simple_connecting_sprite* tex, bool placeable=true):plop(tex,1,1,placeable) {

	}
	
	tileBase *clone() {
		return plop::clone<plop_connecting>(this);
	}

	void render(tileEvent e) override {
		bool con[4];
		e.plop_instance->getConnections(e, &con[0]);
		((simple_connecting_sprite*)tex)->draw_connections(getRenderArea(e), &con[0]);
	}
};

struct network_provider_plop : public plop {
	network_provider_plop(sprite *tex, net_t water, net_t power, int plop_width = 1, int plop_height = 1, bool placeable=false):plop(tex,plop_width,plop_height,placeable) {
		this->net = new network_provider();
		this->net->water = new network_value(_water = water);
		this->net->power = new network_value(_power = power);		
		//fprintf(logFile, "Network provider plop created: %p %p %p %f %f\n", this->net, this->net->water, this->net->power, water, power);
	}

	void free() override {
		net->free();
		delete this;
	}

	void onPlaceEvent(tileEvent e) override {
		fprintf(logFile, "network_provider_plop::onPlaceEvent %p %p %p %i %i %i\n", this, net, net->water, size.x, size.y, id);

		if (net && net->water && net->water->isSupply())
			e.partial->setUnderground(UNDERGROUND_WATER_PIPE);
	}

	network_provider *getNetworkProvider(tileEvent e) override {
		return net;
	}

	void onNetworkEvent(tileEvent e) override {
		//fprintf(logFile, "network_provider_plop::onNetworkEvent %p %p\n", this, net);
		plop::onNetworkEvent(e);
	}


	tileBase *clone() override {
		network_provider_plop* p = (network_provider_plop*)plop::clone<network_provider_plop>(this);
		p->net = new network_provider();
		p->net->water = new network_value(_water);
		p->net->power = new network_value(_power);
		//fprintf(logFile, "clone network_provider_plop %i %p %p %p %p %p %f\n", p->id, this, registry.getInstance(p->id), p, p->net, p->net->water, p->net->water->getSupply());
		return p;
	}

	net_t _water, _power;
};

network_provider_plop water_tower_plop(&water_tower_sprite, 1200.0f, -100.0f, 1, 1, true);
network_provider_plop water_well_plop(&water_well_sprite, 500.0f, -50.0f, 1, 1, true);
network_provider_plop water_pump_large_plop(&large_water_pump_sprite, 24000.0f, -400.0f, 2, 1, true);

plop_connecting road_plop(&road_con_tex_sprite);
plop_connecting street_plop(&street_con_tex_sprite);
plop_connecting pool_plop(&pool_con_tex_sprite);

network_provider_plop tall_building_plop(&tall_building_sprite, -800, -400, 1, 1, true);
network_provider_plop building1_plop(&building1_sprite, -400, 200, 1, 1, true);
plop building2_plop(&building2_sprite, 2, 1);
plop building3_plop(&building3_sprite, 2, 2);
plop building4_plop(&building4_sprite, 1, 1);

tilePartial partially_garbage;

struct selector : public tile {	
	selector():selected(nullptr, &state, sizei(0,0,1,1)) {
		selectedId = 0;

		set(selectedId);
	}

	unsigned int ticks;
	
	tilePartial state;
	tileComplete selected;
	int selectedId;

	void set(int i) {
		selectedId = i;
		if (selectedId > registry.getPlaceableCount() - 1)
			selectedId = 0;
		if (selectedId < 0)
			selectedId = registry.getPlaceableCount() - 1;

		if (selected.parent != nullptr)
			selected.parent->free();

		selected.parent = registry.getPlaceable(selectedId)->clone();
		selected.plop_instance = selected.parent->getPlop(selected);
		setLength(selected.parent->getSize(selected));

	}

	void setLength(sizei p) {
		selected.size.width = p.width;
		selected.size.height = p.height;
		setArea(selected.size);
	}

	void setSize(tileEvent e) override {
		setArea(e.size);
	}

	void setArea(sizei p) {
		selected.size = p;
		selected.parent->setSize(selected);
		selected.parent->copyState(selected.partial);
	}

	void setPos(posi p) {
		setArea(sizei{p,selected.size.width,selected.size.height});
	}

	void next() {
		set(selectedId + 1);
	}

	void prev() {
		set(selectedId - 1);
	}

	void render() {
		sizef area = getRenderArea(selected);

		if (ticks++ % 20 > 9) {
			selector_sprite.draw(area);
			game.tileAreaLoop(selected.size, [&](posi p) {
				selector_sprite.draw(getRenderArea(getEvent(p.with(1,1))));
			});
		}

		if (waterView) {
			game.tileAreaLoop(selected.size, [&](posi p) {
				water_pipe_sprite.draw(getRenderArea(getEvent(p.with(1,1))));
			});
			//water_pipe_sprite.draw(area);
		} else {
			sizei oldSize = selected.parent->getSize(selected);
			if (selected.plop_instance != nullptr) {
				oldSize = selected.plop_instance->size;
				oldSize = registry.getInstance(selected.plop_instance->initial_id)->getSize(selected);

			}
			game.tileAreaLoop(selected.size, [&](posi p) {
				tileComplete tc = selected;
				sizei tmp = p.with(oldSize.width, oldSize.height);
				tc.size = tmp;
				if (tmp.x % oldSize.width == 0 && tmp.y % oldSize.height == 0)
					selected.parent->setSize(tc);
				selected.parent->render(tc);
			});
			selected.parent->setSize(selected);
			//selected.parent->render(selected);
		}		
	}
	
	void render(tileEvent e) override {
		render();
	}
};

network_value *getNetwork(tileEvent e, int type) {
	network_provider *a;
	network_value *b;

	if (e.parent != nullptr) {
		a = e.parent->getNetworkProvider(e);
		if (a != nullptr) {
			b = a->getNetwork(e.with(0, type));
			if (b != nullptr)
				return b;
		}
	}

	if (e.plop_instance != nullptr) {
		a = e.plop_instance->getNetworkProvider(e);
		if (a != nullptr) {
			b = a->getNetwork(e.with(0, type));
			if (~e.flags & SILENT)
				fprintf(logFile, "getNetwork %i %i %p %p %p\n", e.size.x, e.size.y, e.parent, e.plop_instance, b);
			if (b != nullptr)
				return b;
		}
	}

	return nullptr;
}

struct _dirt_tile : public tile {
	void render(tileEvent e) override {
		sizef area = getRenderArea(e);
		if (e.plop_instance) {
			network_value *water = getNetwork(e.with(0,SILENT), WATER);
			if (water && water->isSupply())
				wet_plop_sprite.draw(area);
			else
				dry_plop_sprite.draw(area);
				
		} else {
			if (e.partial->hasWater())
				wet_dirt_sprite.draw(area);
			else
				dry_dirt_sprite.draw(area);
		}
	}
};

struct _water_pipe_tile : public tileable {
	_water_pipe_tile():tileable(water_pipe_con_tex_sprite) {}
	
	bool isSameType(tileComplete tc) override {
		return tc.partial->hasUnderground(UNDERGROUND_WATER_PIPE);
	}
	
	void onPlaceEvent(tileEvent e) override {
		e.partial->setUnderground(UNDERGROUND_WATER_PIPE);
	}

	void onDestroyEvent(tileEvent e) override {
		e.partial->setUnderground(0);
	}
};

_water_pipe_tile water_pipe_tile;
tile default_tile;
tile grass_tile = tile(grass_sprite, true);
tile sand_tile = tile(sand_sprite, true);
tile water_tile = tile(wet_plop_sprite, true);
_dirt_tile dirt_tile;
selector tileSelector;

#pragma endregion

#pragma region //Functions not forward declared

std::vector<tileComplete> walk_network(tileComplete current, bool(*meetsCriteria)(tileComplete), std::vector<tileComplete> &network) {
	if (std::find(network.begin(), network.end(), current) != network.end())
		return network;

	if (current.partial == nullptr)
		return network;

	network.push_back(current);

	posi p = current.size;

	tileComplete q[4] = {
		getComplete(p.north()),
		getComplete(p.east()),
		getComplete(p.south()),
		getComplete(p.west())
	};

	for (int i = 0; i < 4; i++) {
		if (game.isInBounds(q[i].size) && meetsCriteria(q[i])) {
			walk_network(q[i], meetsCriteria, network);
		}
	}

	return network;
}

bool isWaterNetwork(tileComplete tc) {
	return tc.partial->hasUnderground(UNDERGROUND_WATER_PIPE);
}

template<typename FUNCTION>
network_summary balanceNetworks(FLAG type, FUNCTION func) {
	std::vector<tileComplete> sparse;
	sizei map = game.getMapSize();
	network_summary sm;

	//Values are changing
	game.fireEvent(map, NETWORK, type|TICK);

	//Take network tiles
	game.tileAreaLoop(map, [&](posi p) {
		tileEvent e = getComplete(p);
		network_value *v = getNetwork(e.with(0,SILENT), type);
		if (v == nullptr)
			return;
		if (v->isSupply()) {
			if (std::find(sparse.begin(), sparse.end(), e) != sparse.end())
				return;
			sparse.push_back(e);
			fprintf(logFile, "Network piece: %i %i %f %f\n", p.x, p.y, v->getSupply(), v->getDemand());
		}
	});

	std::vector<std::vector<tileComplete>> networks;

	//Walk present networks
	for (tileComplete tc : sparse) {
		bool newNetwork = true;
		
		for (auto network : networks) {
			if (std::find(network.begin(), network.end(), tc) != network.end()) {
				newNetwork = false;
				break;
			}
		}

		if (newNetwork) {
			std::vector<tileComplete> network;
			walk_network(tc, func, network);
			networks.push_back(network);
			sm.networks++;
		}
	}

	//Balance networks
	for (auto network : networks) {
		std::vector<tileComplete> consumers, providers;
		net_t input = 0, output = 0;

		for (tileEvent e : network) {
			game.tileRadiusLoop(e.size, 5, [&](posi p) {
				tileEvent e2 = getComplete(p);
				network_value *consum = getNetwork(e2.with(0, SILENT), type);

				if (consum && consum->isDemand())
					if (std::find(consumers.begin(), consumers.end(), e2) == consumers.end()) {
						output += consum->getDemand();
						consumers.push_back(e2);
					}
			});

			network_value *net = getNetwork(e.with(0, SILENT), type);

			if (net && net->isSupply())
				if (std::find(providers.begin(), providers.end(), e) == providers.end()) {
					input += net->getSupply();
					providers.push_back(e);
				}
		}

		if (providers.empty())
			continue;

		fprintf(logFile, "This network: %li %li %f %f\n", providers.size(), consumers.size(), input, output);

		sm.demand += output;
		sm.supply += input;

		net_t ratio = input / output;
		if (ratio == 0 || ratio < 0 || output < 0.1)
			continue;
		float maxRadius = 5.0f;
		float radius = ratio * maxRadius;
		if (radius < 0)
			radius = 0;
		if (radius > maxRadius)
			radius = maxRadius;

		for (tileEvent e : providers) {
			network_value *net = getNetwork(e.with(0, SILENT), type);
			if (net && net->isSupply())
				net->take(ratio > 1 ? net->getSupply() : net->getSupply() / ratio);

		}

		for (tileEvent e : consumers) {
			network_value *net = getNetwork(e.with(0, SILENT), type);
			if (net && net->isDemand()) 
				input -= net->give(input);
		}

		sm.used = sm.supply - input;
	}

	fprintf(logFile, "Balanced\n");

	//Values are changing
	//game.fireEvent(map, NETWORK, type|TICK);

	return sm;
}

#pragma endregion

#pragma region //Function implementations that are forward declared

template<template<typename> typename POS, typename T1, typename FUNCTION>
void util::areaLoop(POS<T1> size, FUNCTION func) {
	for (int x = size.x; x < size.x + size.width; x++) {
		for (int y = size.y; y < size.y + size.height; y++) {
			POS<T1> target(x,y);
			func(target);
		}
	}
}

template<template<typename> typename POS, typename T1, typename T2,typename FUNCTION>
void util::radiusLoop(POS<T1> p, T2 radius, FUNCTION func) {
	for (T1 x = p.x - radius; x < p.x + radius; x++) {
		for (T1 y = p.y - radius; y < p.y + radius; y++) {
			POS<T1> target(x,y);
			if (p.distsq(target) < radius * radius)
				func(target);
		}
	}
}

bool _game::isInRadius(int ox, int oy, int px, int py, float r) {
	return isInRadius({ox,oy,px,py}, r);
}

bool _game::isInRadius(sizei size, float r) {
	return size.distsq() < r * r;
}

bool _game::isInBounds(posi p) {
	return isInBounds(p.with(1,1));
}

bool _game::isInBounds(sizei p) {
	return getMapSize().contains(p);
}

template<typename FUNCTION, typename Tradius>
void _game::tileRadiusLoop(posi p, Tradius radius, FUNCTION func) {
	util::radiusLoop(p, radius, [&](posi target) {
		if (isInBounds(target))
			func(target);
	});
}

template<typename FUNCTION, typename Tradius>
void _game::tileRadiusLoop(int x, int y, Tradius radius, FUNCTION func) {
	tileRadiusLoop(posi(x,y), radius, func);
}

template<typename FUNCTION>
void _game::tileAreaLoop(sizei size, FUNCTION func) {
	util::areaLoop(size, [&](posi p) {
		if (isInBounds(p))
			func(p);
	});
}

std::vector<tileComplete> _game::getTiles(sizei size) {
	std::vector<tileComplete> tiles;

	if (size.width == 1 && size.height == 1) {
		tiles.push_back(getComplete(size));
		return tiles;
	}

	for (int x = size.x; x < size.x + size.width; x++) {
		for (int y = size.y; y < size.y + size.height; y++) {
			tiles.push_back(getComplete(x,y));
		}
	}

	return tiles;
}

void _game::fireEvent(sizei size, int events) {
	fireEvent(getEvent(size), events);
}

void _game::fireEvent(tileEvent e, int events) {
	fireEvent(e.with(events));
}

void _game::fireEvent(sizei size, int events, int flags) {
	fireEvent(getEvent(size).with(events,flags));
}

void _game::fireEvent(tileEvent e) {
	//Send events here
	bool loghere = (~e.flags & SILENT); //Silence is not specified
	if (loghere) //We may call multiple times for a single operation
		fprintf(logFile, "Firing events: [%i %i] pos: [%i %i] -> [%i %i]\n", e.events, e.flags, e.size.x, e.size.y, e.size.x + e.size.width, e.size.y + e.size.height);

	//To plop (single instance over many tiles)
	if (e.plop_instance && !e.plop_instance->stale(e)) {
		e.plop_instance->handle(e);
	}
	
	if (e.parent && !e.parent->stale(e) && e.size.width == 1 && e.size.height == 1) {
		e.parent->handle(e);
		return;
	}

	//To tile (many instances over many tiles)
	for (tileComplete tc : getTiles(e.size)) {
		if (tc.parent && !tc.parent->stale(tc))
			//Duplicate flags and events and fire
			tc.parent->handle(e.clone(tc),true);

		//Will fire multiple times for single instance!!!
		if (tc.plop_instance)
			tc.plop_instance->handle(e.clone(tc), false);
	}
}

void _game::destroy(tileEvent e) {
	//size could be a single plop or a group of tiles, or a group of plops
	fireEvent(e.with(DESTROY));

	for (tileComplete tc : getTiles(e.size)) {
		if (tc.parent != nullptr)
			tc.parent->free();
		if (tc.plop_instance != nullptr && registry.getInstance(tc.partial->getPlopId()) != nullptr) {
			for (tileComplete p : tc.plop_instance->getTiles(tc))
				tc.partial->setPlopId(0);
			tc.plop_instance->free();
		}
		if (tc.partial != nullptr)
			grass_tile.copyState(tc.partial);
		else
			*getPartial(tc.size) = grass_tile.getDefaultState();
	}
}

void _game::destroy(sizei size) {
	destroy(getEvent(size));
}

void _game::place(sizei size, tileBase *base) {
	tileEvent e = getEvent(size);
	e.parent = base;
	place(e);
}

void _game::place(tileEvent e) {
	destroy(e);

	e.plop_instance = nullptr;

	for (tileComplete tc : getTiles(e.size)) {
		e.parent->setSize(e);
		e.parent->copyState(tc.partial);
	}

	fireEvent(e.with(PLACE));
}

tileBase *instance_registry::addInstance(tileBase *t) {
	if (getInstance(t->id) != nullptr && t->id != 0) {
		fprintf(logFile, "Instance already exists: %i\n", t->id);
		return t;
	}
	t->id = nextId();
	instances.push_back(t);
	return t;
}

tileBase *instance_registry::getInstance(int id) {
	for (auto _t : instances) {
		if (_t->id == id)
			return _t;
	}
	return nullptr;
}

tileBase *instance_registry::addPlaceable(tileBase *t) {
	placeable.push_back(t);
	return t;
}

tileBase *instance_registry::getPlaceable(int id) {
	return placeable[id];
}

void _game::init(sizei mapsize) {
	srand(time(NULL));
	
	viewX = 1;
	viewY = 2;
	
	new (&tileSelector)selector();
	tileSelector.setPos({3,3});

	scale = 10;
	waterView = false;
	placementMode = false;
	infoMode = false;
	statsMode = false;
	plopsOnly = false;
	
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
	
	//init_plops();

	if (tileMap)
		delete [] tileMap;
	
	mapSize = mapsize;
	tileMapWidth = mapSize.width;
	tileMapHeight = mapSize.height;
	tileMap = new tilePartial[tileMapWidth * tileMapHeight];

	for (int y = 0; y < tileMapHeight; y++) {
		for (int x = 0; x < tileMapWidth; x++) {
			tileEvent e(tileComplete(grass_tile.clone(), &partially_garbage, {x,y,1,1}));
			game.place(e.with(0, SILENT));
			if (rand() % 2 == 0);
			if (rand() % 4 == 0);
		}
	}

	game.place({0,0,tileMapWidth,tileMapHeight}, grass_tile.clone());
	fprintf(logFile, "grass_tile %p\n", grass_tile.clone());
	game.place({2,2,1,1}, water_tower_plop.clone());
	game.place({3,2,1,1}, water_tower_plop.clone());
	game.place({4,2,1,1}, water_tower_plop.clone());
	game.place({5,2,1,1}, water_tower_plop.clone());
	game.place({2,4,2,2}, building3_plop.clone());
	game.place({4,4,2,1}, building2_plop.clone());
}

tilePartial *getPartial(int x, int y) {
	if (x >= tileMapWidth || x < 0 || y >= tileMapHeight || y < 0) {
		fprintf(logFile, "Out of bounds: %i %i\n", x, y);
		return &(partially_garbage = tilePartial());//&tiles::DEFAULT_TILE->defaultState;
	}
	return &tileMap[y * tileMapWidth + x];
}

tilePartial *getPartial(posi p) {
	return getPartial(p.x,p.y);
}

tileComplete getComplete(int x, int y) {
	tileComplete tc(nullptr, nullptr, {x,y,1,1});
	tc.partial = getPartial(x,y);
	tc.parent = registry.getInstance(tc.partial->id);
	tc.plop_instance = nullptr;
	if (tc.partial->isPlop())
		tc.plop_instance = (plop*)registry.getInstance(tc.partial->getPlopId());
	return tc;
}

tileComplete getComplete(posi p) {
	return getComplete(p.x,p.y);
}

tileEvent getEvent(sizei p) {
	tileEvent e = getComplete(p);
	e.size = p;
	return e;
}

#pragma endregion

#pragma region //Display functions

void displayTileMap() {
	float width = getWidth();
	float height = getHeight();

	sizei screen = mainTarget.getSize();
	sizei map = game.getMapSize();

	tileComplete tc;
	tileEvent e;

	for (int x = map.width - 1; x > - 1; x--) {
		for (int y = 0; y < map.height; y++) {			
			sizef renderArea = tileBase::getRenderArea({x,y,1,1});
			renderArea.height = renderArea.height - renderArea.y;

			if (!screen.overlaps(sizei::cast(renderArea)))
				continue;
			
			tc = getComplete(x,y);
			e = tileEvent(tc);
			tilePartial *partial = tc.partial;

			if (waterView) {
				dirt_tile.render(e);
				if (partial->hasUnderground(UNDERGROUND_WATER_PIPE))
					water_pipe_tile.render(e);				
				continue;
			}			

			if (tc.parent && !plopsOnly)
				tc.parent->render(e);
			if (tc.plop_instance) {
				//if (plopsOnly && tc.parent)
				//	tc.parent->render(e);
				tc.plop_instance->render(e);
			}
			/*
			if (tc.parent->waterConsumption(&tc) > 0 && !tc.partial->hasWater()) {
				//nowater->draw(&tc, offsetx * width + (width * 0.1f), offsety * height - (height * 0.5f), width * 0.8f, height * 0.8f);
				no_water_sprite.draw(offsetx * width + (width * 0.1f), offsety * height - (height * 0.5f), width * 0.8f, height * 0.8f);
			}
			else
				if (tc.parent->needsRoadConnection(&tc) && !tc.parent->hasRoadConnection(&tc)) {
					//noroad->draw(&tc, offsetx * width + (width * 0.1f), offsety * height - (height * 0.5f), width * 0.8f, height * 0.8f);
					no_road_sprite.draw(offsetx * width + (width * 0.1f), offsety * height - (height * 0.5f), width * 0.8f, height * 0.8f);
				}
			*/
		}
	}
	/*
	std::vector<plop_instance*> z_sort = plops.instances;

	std::sort(z_sort.begin(), z_sort.end(), [](plop_instance *a, plop_instance *b) {
		return getOffsetY(b->originX, b->originY) > getOffsetY(a->originX, a->originY);
	});

	for (auto _pi : z_sort) {
		if (waterView && !_pi->waterSupply())
			continue;

		_pi->render();
	}
	*/
}
//0,0
void displayXY() {
	posf p = {getScreenOffsetX(0.2,0),getScreenOffsetY(0.5,0)};
	//posf _x = {1,0}, _y = {0,1};
	posf x = getOffsetXY({4,0}).add(-viewX, -viewY);
	posf y = getOffsetXY({0,4}).add(-viewX, -viewY);
	x.x *= getCharacterYoverX();
	y.x *= getCharacterYoverX();

	adv::line(p.x,p.y,p.x + x.x,p.y + x.y,'X',FRED|BBLACK);
	adv::line(p.x,p.y,p.x + y.x,p.y + y.y,'Y',FGREEN|BBLACK);
}

void centerDisplay() {
	sizei selected = tileSelector.selected.size;
	posi selectedCenter = selected.center(); //tilespace
	int area = selected.area();
	viewX = 0;
	viewY = 0;
	posf map = {(float)-selectedCenter.x, (float)-selectedCenter.y}; //tilespace
	posf selectedOffset = getOffsetXY(map); //screenspace before scaling
	posf off = {(scale / 4 * getCharacterYoverX()), scale / 4}; //screenspace

			//tilespace        tilespace
	off.x = selectedCenter.x - (selected.x / scale * 2);
	off.y = selectedCenter.y - (selected.y / scale);

	off.x = 2; //tilespace
	off.y = 5;

	//viewX and viewY are in screenspace, before scaling

	viewX = selectedOffset.x + getOffsetX(off);
	viewY = selectedOffset.y + getOffsetY(off);

	//posf tileOffset = {40,20};
	posf tileOffset = {adv::width / 4.0f, adv::height / 2.0f};
	
	//I don't know how this works

	viewX = selectedOffset.x + (tileOffset.x / scale); //5
	viewY = selectedOffset.y + (tileOffset.y / scale); //2
}

void displayEdges() {
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
		adv::write(offsetx1 * width, offsety1 * height, 'X', FRED|BBLACK);
	}
}

void displayStats() {
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
		auto showBarChart = [&](int x, int y, int height, float norm, cpix fillChar) {
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

void displayDate() {
	char buf[30];
	snprintf(&buf[0], 29, "%i/%i", month,day);
	adv::write(adv::width-4-strlen(&buf[0]),0,&buf[0]);
}

void displayInfo() {
	displayXY();
	int y = 0;
	auto printVar = [&](const char* varname, float value) {
		char buf[100];
		int i = snprintf(&buf[0], 99, "%s: %f", varname, value);
		adv::write(0,y++,&buf[0]);
	};
	auto printSize = [&](const char* varname, sizei value) {
		char buf[100];
		int i = snprintf(&buf[0], 99, "%s: %i %i %i %i", varname, value.x, value.y, value.width, value.height);
		adv::write(0,y++,&buf[0]);
	};
	printVar("viewX", viewX);
	printVar("viewY", viewY);
	printVar("scale", scale);
	printVar("selectorTileId", tileSelector.selectedId);
	printSize("selectorXYWH", tileSelector.selected.size);
	printVar("waterSupply", waterSupply);
	printVar("waterDemand", waterDemand);
	printVar("waterNetworks", waterNetworks);
	printVar("instanceCount", registry.instances.size());
	printVar("placeableCount", registry.placeable.size());
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

void display() {
	if (placementMode)
		centerDisplay();

	displayEdges();

	displayTileMap();
	
	if (placementMode)
		tileSelector.render();
	
	if (statsMode)
		displayStats();
	
	displayDate();
	
	if (infoMode) 
		displayInfo();
}

#pragma endregion

void cleanupexit() {
	fprintf(logFile, "Closing console\n");
	adv::_advancedConsoleDestruct();
	fprintf(logFile, "Exit\n");
	exit(0);
}

int wmain() {
	logFile = fopen("log.txt", "a");
	if (!logFile) {
		fprintf(stderr, "Failed to open log file\n");
		return 1;
	}

	fprintf(logFile, "[%li] Opened log\n", time(0));

	colormapper_init_table();

	fprintf(logFile, "[%li] Initialized color table\n", time(0));

	mainAtlas.load("textures.png");
	
	fprintf(logFile, "[%li] Loaded texture\n", time(0));

	grass_sprite_random = new random_overlay(new sprite_overlay(&grass_sprite), &street_sprite);

	fprintf(logFile, "[%li] Set advanced console up\n", time(0));

	adv::setThreadState(false);
	adv::setThreadSafety(false);

	fprintf(logFile, "[%li] Game init\n", time(0));

	game.init({0,0,tileMapWidth,tileMapHeight});
	
	int key = 0;
	
	auto tp1 = std::chrono::system_clock::now();
	auto tp2 = std::chrono::system_clock::now();

	fprintf(logFile, "[%li] Game loop\n", time(0));

	while (true) {
		key = console::readKeyAsync();
		
		if (HASKEY(key, VK_ESCAPE) || HASKEY(key, 'q')) {
			cleanupexit();
		}

		adv::clear();
		
		if (placementMode) {
			sizei area = tileSelector.selected.size;
			switch (key) {
			case 'w':
				tileSelector.setPos(tileSelector.selected.size.north());
				viewY+= yfact * yfact;
				viewX+= xfact * xfact;
				break;
			case 's':
				tileSelector.setPos(tileSelector.selected.size.south());
				viewY-= yfact * yfact;
				viewX-= xfact * xfact;
				break;
			case 'd':
				tileSelector.setPos(tileSelector.selected.size.east());
				viewX-= xfact * xfact;
				viewY+= yfact * yfact;
				break;
			case 'a':
				tileSelector.setPos(tileSelector.selected.size.west());
				viewX+= xfact * xfact;
				viewY-= yfact * yfact;
				break;			
			case 'W':				
				if (tileSelector.selected.size.height > 1)
					tileSelector.setArea(area.add(sizei(0,0,0,-1)));
				else 
					tileSelector.setArea(area.add(sizei(0,1,0,-1)));
				break;
			case 'S':
				tileSelector.setArea(area.add(sizei(0,0,0,1)));
				break;
			case 'D':
				tileSelector.setArea(area.add(sizei(0,0,1,0)));
				break;
			case 'A':
				if (tileSelector.selected.size.width > 1)
					tileSelector.setArea(area.add(sizei(0,0,-1,0)));
				break;
			case 'z':
			{
				//Place
				if (waterView) {
					water_pipe_tile.onPlaceEvent(getComplete(tileSelector.selected.size));
					break;
				}

				//Selected tile in its current state and size
				game.place(tileSelector.selected.size, tileSelector.selected.parent->clone());
			}
				break;
			case 'x':
			{
				//Destroy
				if (waterView) {
					water_pipe_tile.onDestroyEvent(getComplete(tileSelector.selected.size));
					break;
				}

				//Destroy tile selector selected (which isn't meant for this)
				game.destroy(tileSelector.selected.size);
			}
				break;
			case '2':
				tileSelector.next();
				break;
			case '1':
				tileSelector.prev();
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
				game.init({0,0,tileMapWidth,tileMapHeight});
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
			case 'l':
				plopsOnly = !plopsOnly;
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
		tp1 = tp2;
			
		adv::waitForReadyFrame();

		{
			char buf[50];
			int len = snprintf(&buf[0], 49, "%.1f fps - %.1f ms ft", (1.0f/elapsedTimef)*1000.0f, elapsedTimef);
			adv::write(getScreenOffsetX(0.5, len), 0, &buf[0]);
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
				tc.parent->onRandomEvent(tc);
				
				if (tc.partial->hasUnderground(UNDERGROUND_WATER_PIPE))
					//((water_pipe*)(tiles::WATER_PIPE))->updateWater(tc);
					water_pipe_tile.onUpdateEvent(tc);
			}
		}
		
		//game logic
		if (microday++ > 30) {//once a second
			microday = 0;
			day++;
			if (day > 31) {
				month++;
				day = 1;
				fprintf(logFile, "Month %i\n", month);
			}
		}
		
		if (microday == 0)
		switch (day) {
			case 1: { //check for road connection
				for (int x = 0; x < tileMapWidth; x++) {
					for (int y = 0; y < tileMapHeight; y++) {
						tileComplete tc = getComplete(x,y);
						//if (tc.parent->needsRoadConnection(&tc))
						//	tc.parent->updateRoadConnections(&tc);
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
						/*
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
						*/
					}
				}
				
				commercialDemand = (commercialPopulation + 1) / (commercialJobs + 1);
				industrialDemand = (industrialPopulation + 1) / (industrialJobs + 1);
				residentialDemand = (population + 1) / (residentialCapacity + 1);
				break;
			}
			
			case 3: {
				network_summary sm =
				balanceNetworks(WATER, isWaterNetwork);

				waterSupply = sm.supply;
				waterDemand = sm.demand;
				waterNetworks = sm.networks;

				break;
			}

			case -1: { //water calculations
				waterNetworks = 0;
				waterDemand = 0;
				waterSupply = 0;

				std::vector<tileBase*> frozen_instances = registry.instances;
				//Network summary
				//Not tile based yet
				for (tileEventHandler *_teh : frozen_instances) {
					if (_teh == nullptr)
						continue;
					tileBase *_ib = (tileBase*)_teh;
					if (_ib == nullptr)
						continue;
					//fprintf(logFile, "Instance %i %p %p\n", _ib->id, _ib, registry.getInstance(_ib->id));
					//fflush(logFile);
					if (registry.getInstance(_ib->id)->getPlop() == nullptr)
						continue;
					plop *p = (plop*)_ib;

					tileComplete tc = getComplete(p->size);
					network_value* water = getNetwork(tc, WATER);
					if (water == nullptr) {
						//fprintf(logFile, "No water provider %i [%i %i] [%i %i]\n", p->id, tc.size.x, tc.size.y, p->size.x, p->size.y);
						continue;
					}

					fprintf(logFile, "Water provider %i [%i %i] [%i %i] %f %f\n", p->id, tc.size.x, tc.size.y, p->size.x, p->size.y, water->getSupply(), water->getDemand());

					waterSupply += water->getSupply();
					waterDemand += water->getDemand();
				}

				//form water supply networks and distribute available water
				std::vector<std::vector<tileComplete>> networks;
				//for (auto _ip : plops.instances) {
				for (int x = 0; x < tileMapWidth; x++) {
					for (int y = 0; y < tileMapHeight; y++) {
						tileEvent tile = getComplete(x, y);

						if (!isWaterNetwork(tile))
							continue; // Sources don't count without network connection

						network_value *water = getNetwork(tile.with(0, SILENT), WATER);
						if (water == nullptr || water->isDemand())
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

				//send network signal
				game.fireEvent({0,0,tileMapWidth,tileMapHeight}, NETWORK, WATER|TICK);

				fprintf(logFile, "Water calculuation, current network count: %li, iter count: %i\n", networks.size(), waterNetworks);

				//eh to test we'll set good networks with water
				for (auto network : networks) {
					std::vector<tileComplete> waterUsers;
					std::vector<tileComplete> waterProviders;

					//abs(tile.parent_plop_instance->waterUsage()) > 0.0f

					float input = 0, output = 0;

					for (tileEvent tile : network) { //find water providers
						network_value* water = getNetwork(tile.with(0, SILENT), WATER);
						if (water == nullptr)
							continue;
						if (std::find(waterProviders.begin(), waterProviders.end(), tile) != waterProviders.end())
							continue; //if already accounted for, skip it
						if (water->isSupply()) {
							waterProviders.push_back(tile);
							//input += tile.parent_plop_instance->waterUsage();
							input += water->getSupply();
						}
					}
					
					if (waterProviders.empty())
						continue; //no water providers, no state change for network

					// best would be expanding radius from tiles in network based upon the available water remaining

					//search radius for water users
					for (auto tile : network) {
						game.tileRadiusLoop(tile.size, 5, [&](posi p) {
							tileEvent tc = getComplete(p);
							network_value *water = getNetwork(tc.with(0, SILENT), WATER);
							if (water == nullptr)
								return;
							if (std::find(waterUsers.begin(), waterUsers.end(), tile) != waterUsers.end())
								return; //if already accounted for, skip it
								
							if (water->isDemand()) {
								waterUsers.push_back(tile);
								output += water->getDemand();
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
						game.tileRadiusLoop(tile.size, radius, [&](posi p) {
							tileComplete tc = getComplete(p);
							if (tc.partial == nullptr)
								return;

							tc.partial->setWater(true);
						});
					}

					for (auto user : waterUsers) {
						network_value *water = getNetwork(user, WATER);
						if (water == nullptr)
							continue;
						game.tileRadiusLoop(user.size, radius, [&](posi p) {
							if (water->isSaturated() || water->isSupply()) {
								fprintf(logFile, "Saturated or supply\n");
								return;
							}

							tileComplete tc = getComplete(p);
							if (!isWaterNetwork(tc))
								return;
							
							fprintf(logFile, "Calc water\n");
							input = water->give(input);
						});

						fprintf(logFile, "Water user %f/%f (%f)\n", water->getDemand(), water->getSupply(), input);
					}

				}


				break;
			}
		}
	}	
	
	cleanupexit();

	return 0;
}