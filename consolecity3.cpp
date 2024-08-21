#include <math.h>
#include <chrono>
#include <vector>
#include <set>
#include <iterator>
#include "graphics.h"
#include "sprites.h"

struct default_tile;
struct grass;
struct plop;
struct tile;
struct tileBase;
struct tileEvent;
struct tilePartial;
struct tileComplete;

tilePartial *getPartial(int, int);
tileComplete getComplete(int, int);

#define ZONING_NONE 0
#define ZONING_RESIDENTIAL 1
#define ZONING_COMMERCIAL 2
#define ZONING_INDUSTRIAL 3
#define TILE_COUNT 12
#define NORTH 1
#define EAST 2
#define WEST 4
#define SOUTH 8
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
	}

	bool coordsEquals(tileComplete b) {
		return this->size == b.size;
	}

	bool operator==(tileComplete b) {
		return coordsEquals(b);
	}

	tileBase *parent;
	tilePartial *partial;
	plop *plop_instance;
	sizei size;
};

tilePartial *tileMap = nullptr;
int tileMapHeight = 100;
int tileMapWidth = 100;

FILE *logFile = stderr;

tilePartial *getPartial(int x, int y);
tilePartial *getPartial(posi p);
tileComplete getComplete(int x, int y);
tileComplete getComplete(posi p);

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

enum EVENT {
	PLACE = 1, DESTROY = 2, UPDATE = 3, RANDOM = 4, NETWORK = 5
};

enum FLAG {
	FORCE = 1
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

struct tileEventHandler {
	virtual void handle(tileEvent e) {
		fprintf(logFile, "Handling events: %i pos: %i %i\n", e.events, e.size.x, e.size.y);
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

struct networkProvider {

};

networkProvider no_provider;

struct tileBase;

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
} registry;

struct _game {
	std::vector<tileComplete> getTiles(sizei size);
	void destroy(sizei size);
	void place(sizei size, tileBase *base);
	void fireEvent(tileEvent e);
	void fireEvent(tileEvent e, int event);
	void fireEvent(sizei size, int event);
} game; //singleton for the game

struct tileBase : public tileEventHandler {
	int id;
	virtual void render(tileEvent e) = 0;
	virtual sizef getRenderArea(tileEvent e) {
		posf pos = getOffsetXY(posf{(float)e.size.x,(float)e.size.y});
		posf siz = posf{getWidth()*e.size.width,getHeight()*e.size.height};
		return {pos.x * siz.x, pos.y * siz.y, siz.x, siz.y};
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
	virtual tileBase *clone() {
		return this;
	}
	virtual void free() {
		return;
	}
	virtual void copyState(tilePartial *tile) {
		*tile = getDefaultState();
	}
	virtual networkProvider *getNetworkProvider(tileEvent e) {
		return nullptr;
	}
	virtual plop *getPlop(tileEvent e) {
		if (e.partial->isPlop() && e.partial->getPlopId() == id)
			return (plop*)registry.getInstance(e.partial->getPlopId());
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
		//tex.draw
		draw(e, getRenderArea(e));
	}

	networkProvider *getNetworkProvider(tileEvent e) override {
		return &no_provider;
	}

	plop *getPlop(tileEvent e) override {
		return nullptr;
	}

	virtual void draw(tileEvent e, sizef area) {
		tex.draw(area);
	}
	
	void getNeighbors(tileEvent e, tileEvent *neighbors) {
		int x = e.size.x, y = e.size.y;
		neighbors[0] = getComplete(NORTH_F);//NORTH
		neighbors[1] = getComplete(EAST_F);//EAST
		neighbors[2] = getComplete(WEST_F);//WEST
		neighbors[3] = getComplete(SOUTH_F);//SOUTH
	}
	
	void updateNeighbors(tileEvent e) {
		int x = e.size.x;
		int y = e.size.y;
		tileEvent neighbors[4];
		getNeighbors(e, neighbors);
		neighbors[0].parent->onUpdateEvent(neighbors[0]);
		neighbors[1].parent->onUpdateEvent(neighbors[1]);
		neighbors[2].parent->onUpdateEvent(neighbors[2]);
		neighbors[3].parent->onUpdateEvent(neighbors[3]);
		//tiles::WATER_PIPE->onUpdateEvent(neighbors[0]);
		//tiles::WATER_PIPE->onUpdateEvent(neighbors[1]);
		//tiles::WATER_PIPE->onUpdateEvent(neighbors[2]);
		//tiles::WATER_PIPE->onUpdateEvent(neighbors[3]);
	}
};

struct tileable : public tile {	
	tileable(simple_connecting_sprite tex, bool add = false) :tile(add),tex_connecting(tex) {}

	virtual void connectToNeighbors(tileEvent e) {
		bool con[4];
		getConnections(e, &con[0]);
		for (int i = 0; i < 4; i++)
			e.partial->setConnection(con[i] << i);
	}

	virtual void getConnections(tileEvent e, bool *con) {
		for (int i = 0; i < 4; i++) {
			con[i] = connectingCondition(e, 1 << i);
		}
	}
	
	virtual bool connectingCondition(tileEvent e, int direction) {
		return e.partial->hasConnection(direction);
	}
	
	//int connectingTextureAtlas[4];
	simple_connecting_sprite tex_connecting;
	
	void onPlaceEvent(tileEvent e) override {
		connectToNeighbors(e);
	}
	
	void onUpdateEvent(tileEvent e) override {
		connectToNeighbors(e);
	}
	
	void draw(tileEvent e, sizef area) override {
		bool con[4];
		getConnections(e, &con[0]);
		tex_connecting.draw_connections(area, &con[0]);
	}
};

typedef float net_t;

struct plop_network_value {
	plop_network_value(net_t min, net_t max, net_t value) {
		this->value = value;
		this->min = min;
		this->max = max;
	}

	virtual bool isSaturated() { return abs(max) <= abs(value); }

	virtual bool isDepleted() { return abs(min) >= abs(value); }

	virtual bool isSupply() { return value > 0 || max > 0; }
	
	virtual net_t getValue() { return value; }

	virtual net_t getMin() { return min; }

	virtual net_t getMax() { return max; }

	/*
	Try to add value to the current value, returns the amount that was not added or subtracted



	Ex:
	Consumer
		Max: -800
		Value: -200
		Min: 0
		Tmp: 500
		Amount: 300
		Return: 300

		Final values
		Max: -800
		Value: -500 
		Min: 0
	*/
	virtual net_t addValue(net_t amount) {
		net_t tmp = abs(value) + amount;
		fprintf(logFile, "Adding value: %f %f %f %f\n", value, amount, tmp, value - tmp);

		if (abs(tmp) > abs(max))
			value = max;
		else
		if (abs(tmp) < abs(min))
			value = min;
		else {
			value = tmp;
			if (max < 0) {
				value = -tmp;
			}
		}

		return amount - (tmp + value);
	}

	virtual void setMax(net_t max) { this->max = max; }

	virtual void setMin(net_t min) { this->min = min; }

	virtual void setValue(net_t value) { this->value = value; }
	
	protected:
	net_t value;
	net_t min;
	net_t max;
};

struct plop_network_static : public plop_network_value {
	plop_network_static(net_t min, net_t max, net_t value) : plop_network_value(min, max, value) {
	}

	virtual net_t addValue(net_t amount) {
		return 0;
	}
};

struct plop_properties {
	plop_properties() {

	}

	float efficiency; //0-1
	float funding;
	float monthlyCost;

	plop_network_value *water = 0;
	plop_network_value *power = 0;
	plop_network_value *pollution = 0;
	plop_network_value *crime = 0;
	plop_network_value *health = 0;
	plop_network_value *education = 0;
	plop_network_value *traffic = 0;
	plop_network_value *wealth = 0;
};
/*
Now the instance
*/
struct plop : public tileBase {
	/*
	plops.instances.erase(std::remove(plops.instances.begin(), plops.instances.end(), this), plops.instances.end());
	*/
	plop(sprite* tex, int plop_width = 1, int plop_height = 1, bool placeable=true):
	plop() {
		this->tex = tex;

		registry.addInstance(this);
		if (placeable) {
			registry.addPlaceable(this);
		}

		size = sizei(plop_width, plop_height);
	}

	plop() {
		size = sizei(1,1);
		tex = nullptr;
	}

	~plop() {
		registry.instances.erase(std::remove(registry.instances.begin(), registry.instances.end(), this), registry.instances.end());
		registry.placeable.erase(std::remove(registry.placeable.begin(), registry.placeable.end(), this), registry.placeable.end());
	}

	void render(tileEvent e) override {
		if (tex == nullptr || e.plop_instance == nullptr)
			return;

		if (!(e.size == e.plop_instance->size)) //if not base tile
			if (!(e.flags & FORCE))
				return;

		tex->draw(getRenderArea(e));
	}	

	tilePartial getDefaultState() override {
		tilePartial tp = tileBase::getDefaultState();
		tp.setPlopId(id);
		return tp;
	}

	bool operator==(int i) {
		return id == i;
	}

	plop *getPlop(tileEvent e) override {
		return this;
	}

	sizei getSize(tileEvent e) override {
		return size;
	}

	tileBase *clone() override {
		plop *c = new plop(*this);
		c->id = 0;
		return registry.addInstance(c);
	}

	void free() override {
		if (!stale(getComplete(size)))
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

	virtual void place(tileEvent e) {
		fprintf(logFile, "place(tileEvent e) not yet implemented\n");
		/*
		fprintf(logFile, "Placed plop: %i, ", id);
		for (auto tile : getTiles()) {
			tile.partial->setPlopId(id);
			fprintf(logFile, "[%i %i] ", tile.tileX, tile.tileY);
		}
		if (waterSupply()) {
			//getPartial(originX, originY)->setUnderground(UNDERGROUND_WATER_PIPE);
			tileComplete tc = getComplete(originX, originY);
			//tiles::WATER_PIPE->onPlaceEvent(tc);
		}
		fprintf(logFile, "\n");
		*/
	}

	virtual void destroy(tileEvent e) {
		fprintf(logFile, "destroy(tileEvent e) not yet implemented\n");
		/*
		fprintf(logFile, "Destroyed plop: %i, ", id);
		for (auto tile : getTiles()) {
			tile.partial->setPlopId(0);
			tile.partial->setBuildingId(1);
			fprintf(logFile, "[%i %i] ", tile.tileX, tile.tileY);
		}
		fprintf(logFile, "\n");
		*/
	}

	/*
	//always part of the plop, not the tile
	//some of these are examples for brainstorming
	float education;
	float crime;
	float fire;
	float health;
	float traffic;
	float wealth;

	//bool watered;
	//bool powered;

	float water;	
	float power;
	float pollution;

	int capacity;
	int population;
	*/

	sizei size;
	sprite *tex;
	plop_properties *prop;
	networkProvider *net;
};

struct plop_connecting : public plop {
	plop_connecting(simple_connecting_sprite* tex, bool placeable=false):plop(tex,placeable) {
		this->scs = tex;
	}

	simple_connecting_sprite *scs;

	bool testPlop(posi p) {
		tileComplete tc = getComplete(p.x,p.y);
		return tc.plop_instance == this;
	}

	void render(tileEvent e) override {
		if (scs == nullptr || e.plop_instance == nullptr)
			return;

		sizef area = getRenderArea(e);
		posi p = e.size;
		
		scs->draw_connections(area.x,area.y,area.width,area.height,
			testPlop(p.add(0,-1)),
			testPlop(p.add(1, 0)),
			testPlop(p.add(-1,0)),
			testPlop(p.add(0,1))
		);
	}

};

struct water_provider_plop : public plop {
	water_provider_plop(sprite *tex, net_t water_creation, int plop_width = 1, int plop_height = 1, bool placeable=false):plop(tex,plop_width,plop_height,placeable) {
		fprintf(logFile, "Water provider plop created: %f\n", water_creation);
		this->water_creation = water_creation;
	}

	net_t water_creation;
};

struct network_provider_plop : public plop {
	network_provider_plop(sprite *tex, net_t water, net_t power, int plop_width = 1, int plop_height = 1, bool placeable=false):plop(tex,plop_width,plop_height,placeable) {
		_water = water;
		_power = power;
	}

	net_t _water, _power;
};

water_provider_plop water_tower_plop(&water_tower_sprite, 1200.0f, 1, 1, true);
water_provider_plop water_well_plop(&water_well_sprite, 500.0f, 1, 1, true);
water_provider_plop water_pump_large_plop(&large_water_pump_sprite, 24000.0f, 2, 1, true);

plop_connecting road_plop(&road_con_tex_sprite);
plop_connecting street_plop(&street_con_tex_sprite);
//plop_connecting water_pipe_plop(new sprite(3,1,1,1));
plop_connecting pool_plop(&pool_con_tex_sprite);

network_provider_plop tall_building_plop(&tall_building_sprite, -800, -400, 1, 1, true);
plop building1_plop(&building1_sprite);
plop building2_plop(&building2_sprite, 2, 1);
plop building3_plop(&building3_sprite, 2, 2);

void init_plops() {
	
}

tilePartial partially_garbage;

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

void _game::fireEvent(sizei size, int event) {
	fireEvent(getComplete(size), event);
}

void _game::fireEvent(tileEvent e, int event) {
	fireEvent(e.with(event));
}

void _game::fireEvent(tileEvent e) {
	//Send events here

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
			tc.parent->handle(e.clone(tc));
	}
}

void _game::destroy(sizei size) {
	//size could be a single plop or a group of tiles, or a group of plops
	fireEvent(size, DESTROY);

	for (tileComplete tc : getTiles(size)) {
		if (tc.parent != nullptr)
			tc.parent->free();
	}
}

void _game::place(sizei size, tileBase *base) {
	/*
		As the game engine we can alter game
		states in ways other functions shouldn't
	*/
	fireEvent(size, DESTROY);

	for (tileComplete tc : getTiles(size)) {
		//free it
		if (tc.parent != nullptr)
			tc.parent->free();

		//set new
		tc.parent = base;
		base->copyState(tc.partial);
	}

	fireEvent(size, PLACE);
}

struct selector : public tile {	
	selector():selected(nullptr, &state, sizei(0,0,1,1)) {
		selectedId = 0;

		set(selectedId);
	}

	unsigned int ticks;
	
	tilePartial state;
	tileComplete selected;
	posi origin;
	int selectedId;

	void set(int i) {
		selectedId = i;
		if (selectedId > registry.getPlaceableCount())
			selectedId = 0;
		if (selectedId < 0)
			selectedId = registry.getPlaceableCount() - 1;

		if (selected.parent != nullptr)
			selected.parent->free();

		selected.parent = registry.getPlaceable(selectedId)->clone();
		setArea(selected.parent->getSize(selected));
	}

	void setSize(tileEvent e) override {
		setArea(e.size);
	}

	void setArea(sizei p) {
		selected.size = p;
		selected.size.x = origin.x;
		selected.size.y = origin.y;
		selected.parent->setSize(selected);
		selected.parent->copyState(selected.partial);
	}

	void setPos(posi p) {
		setArea(sizei{p,selected.size.x,selected.size.y});
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
		}

		if (waterView) {
			water_pipe_sprite.draw(area);
		} else {
			selected.parent->render(tileEvent(selected).with(FORCE));
		}		
	}
	
	void render(tileEvent e) override {
		render();
	}
};

struct _dirt_tile : public tile {
	void draw(tileEvent e, sizef area) override {
		if (e.plop_instance) {
			/*
			if (e.parent_plop_instance->isWellWatered())
				wet_plop_sprite.draw(area);
			else
			*/
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
	
	bool connectingCondition(tileEvent e, int direction) override {
		if (e.partial->id == this->id)
			return tileable::connectingCondition(e, direction);
		return e.partial->hasConnection(direction, 2);
	}
	
	void connectToNeighbors(tileEvent e) override {
		tilePartial *tp = e.partial;
		if (tp->id == this->id) {
			tileable::connectToNeighbors(e);
			return;
		}
		//tp->data.a[2] = 0;
		//tp->setConnection(0, 2);
		tp->data.a[2] &= tp->data.a[2] ^ 0x0f;
		tileEvent neighbors[4];
		getNeighbors(e, neighbors);
		for (int i = 0; i < 4; i++) {
			if (neighbors[i].partial->hasUnderground(UNDERGROUND_WATER_PIPE)) {
				//neighbors[i]->setConnection(1 << (i + 2 % 4), 2);
				tp->setConnection(1 << i, 2);
			}
		}
	}
	
	void updateWater(tileEvent e) {
		tileComplete *tc = &e;
		int x = e.size.x, y = e.size.y;
		return;
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
	
	void onUpdateEvent(tileEvent e) override {
		tilePartial *tp = e.partial;
		updateWater(e);
		tileable::onUpdateEvent(e);
	}
	
	void onPlaceEvent(tileEvent e) override {
		tilePartial *tp = e.partial;
		tp->setUnderground(UNDERGROUND_WATER_PIPE);
			//tp->setWater(1);
		updateWater(e);
		tileable::onPlaceEvent(e);
	}

	void onDestroyEvent(tileEvent e) override {
		e.partial->setUnderground(0);
		updateNeighbors(e);
	}
};

_water_pipe_tile water_pipe_tile;
tile default_tile;
tile grass_tile = tile(grass_sprite, true);
_dirt_tile dirt_tile;
selector tileSelector;

bool isInRadius(int ox, int oy, int px, int py, float r) {
	int dx = ox - px;
	int dy = oy - py;
	return (dx * dx) + (dy * dy) < r * r;
}

bool isInRadius(sizei size, float r) {
	return isInRadius(size.x, size.y, size.width, size.height, r);	
}

template<typename FUNCTION>
void tileRadiusLoop(int x, int y, float radius, FUNCTION func) {
	for (int xx = x - radius; xx < x + radius; xx++) {
		for (int yy = y - radius; yy < y + radius; yy++) {
			if (isInRadius(xx,yy,x,y,radius))
				func(xx,yy);
		}
	}
}

template<typename FUNCTION>
void tileRadiusLoop(posi p, float radius, FUNCTION func) {
	tileRadiusLoop(p.x, p.y, radius, func);
}

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
		if (meetsCriteria(q[i])) {
			walk_network(q[i], meetsCriteria, network);
		}
	}

	return network;
}

void init() {
	srand(time(NULL));
	
	viewX = 0;
	viewY = 0;
	
	tileSelector = selector();

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
			game.place({x,y}, grass_tile.clone());
			if (rand() % 2 == 0);
			if (rand() % 4 == 0);
		}
	}

	game.place({0,0,tileMapWidth,tileMapHeight}, grass_tile.clone());

	game.place({2,2}, water_tower_plop.clone());
}

void displayTileMap() {
	float width = getWidth();
	float height = getHeight();
	tileComplete tc;
	tileEvent e;

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
			e = tc;
			tilePartial *partial = tc.partial;
			
			//if (partial->isPlop())
			//	continue;

			if (waterView) {
				dirt_tile.render(e);
				if (!partial->hasUnderground(UNDERGROUND_WATER_PIPE))
					continue;
				//getTile(partial)->draw(partial, offsetx * width, offsety * height, width, height);
				water_pipe_tile.render(e);
				
				continue;
			}			


			tc.parent->render(e);
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
		adv::write(offsetx1 * width, offsety1 * height, 'X', FRED|BBLACK);
	}

	displayTileMap();
	
	if (placementMode) {
		tileSelector.render();
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
		printVar("selectorTileId", tileSelector.selectedId);
		printVar("viewX", viewX);
		printVar("viewY", viewY);
		printVar("scale", scale);
		printVar("selectorX", tileSelector.origin.x);
		printVar("selectorY", tileSelector.origin.y);
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

	fprintf(logFile, "[%li] Opened log\n", time(0));

	colormapper_init_table();

	fprintf(logFile, "[%li] Initialized color table\n", time(0));

	texture = stbi_load("textures.png", (int*)&textureWidth, (int*)&textureHeight, &bpp, 0);
	
	fprintf(logFile, "[%li] Loaded texture\n", time(0));

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

	fprintf(logFile, "[%li] Converted texture\n", time(0));

	grass_sprite_random = new random_overlay(new sprite_overlay(&grass_sprite), &street_sprite);

	fprintf(logFile, "[%li] Set advanced console up\n", time(0));

	adv::setThreadState(false);
	adv::setThreadSafety(false);

	fprintf(logFile, "[%li] Game init\n", time(0));

	init();
	
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
			switch (key) {
			case 'w':
				tileSelector.setPos(tileSelector.origin.north());
				viewY+= yfact * yfact;
				viewX+= xfact * xfact;
				break;
			case 's':
				tileSelector.setPos(tileSelector.origin.south());
				viewY-= yfact * yfact;
				viewX-= xfact * xfact;
				break;
			case 'd':
				tileSelector.setPos(tileSelector.origin.east());
				viewX-= xfact * xfact;
				viewY+= yfact * yfact;
				break;
			case 'a':
				tileSelector.setPos(tileSelector.origin.west());
				viewX+= xfact * xfact;
				viewY-= yfact * yfact;
				break;			
			case 'z':
			{
				//Place
				if (waterView) {
					tileComplete cmp = getComplete(tileSelector.origin);
					cmp.partial->setUnderground(UNDERGROUND_WATER_PIPE);
					water_pipe_tile.onPlaceEvent(cmp);
					water_pipe_tile.updateNeighbors(cmp);
					break;
				}

				//Selected tile in its current state and size
				game.place(tileSelector.selected.size, tileSelector.selected.parent);				
			}
				break;
			case 'x':
			{
				//Destroy
				if (waterView) {
					tileComplete cmp = getComplete(tileSelector.origin);
					water_pipe_tile.onDestroyEvent(cmp);
					//cmp.partial->setUnderground(0);
					//tiles::WATER_PIPE->updateNeighbors(&cmp, selectorX, selectorY);
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
				tileSelector.next();
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
			
			case 3: { //water calculations
				waterNetworks = 0;
				waterDemand = 0;
				waterSupply = 0;

				/*
				//Network summary
				for (auto _ip : plops.instances) {
					if (!_ip->properties || !_ip->properties->water)
						continue;

					if (_ip->properties->water->isSupply())
						waterSupply += _ip->properties->water->getValue(); 
					else
						waterDemand += abs(_ip->properties->water->getMax());
				}

				//form water supply networks and distribute available water
				std::vector<std::vector<tileComplete>> networks;
				//for (auto _ip : plops.instances) {
				for (int x = 0; x < tileMapWidth; x++) {
					for (int y = 0; y < tileMapHeight; y++) {
						tileComplete tile = getComplete(x, y);
						plop_instance *_ip = tile.parent_plop_instance;

						if (!tile.partial->hasUnderground(UNDERGROUND_WATER_PIPE))
							continue; // No production, not a water supply, or not a water pipe

						//if (_ip && _ip->waterUsage() <= 0)
						if (_ip && _ip->properties && _ip->properties->water && !_ip->properties->water->isSupply())
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

				//reset consumer values
				for (auto ip : plops.instances) {
					//if (ip->waterSupply())
					//	continue;
					//ip->water = 0;
					if (!ip || !ip->properties || !ip->properties->water)
						continue;

					if (ip->properties->water->isSupply())
						continue;

					ip->properties->water->setValue(0);
				}

				fprintf(logFile, "Water calculuation, current network count: %li, iter count: %i\n", networks.size(), waterNetworks);

				//eh to test we'll set good networks with water
				for (auto network : networks) {
					std::set<plop_instance*> waterUsers;
					std::set<plop_instance*> waterProviders;

					//abs(tile.parent_plop_instance->waterUsage()) > 0.0f

					float input = 0, output = 0;

					for (auto tile : network) { //find water users
						//if (tile.parent_plop_instance && tile.parent_plop_instance->waterSupply()) {
						if (tile.parent_plop_instance && tile.parent_plop_instance->properties && tile.parent_plop_instance->properties->water && tile.parent_plop_instance->properties->water->isSupply()) {
							if (waterProviders.find(tile.parent_plop_instance) != waterProviders.end())
								continue; //if already accounted for, skip it
							waterProviders.insert(tile.parent_plop_instance);
							//input += tile.parent_plop_instance->waterUsage();
							input += tile.parent_plop_instance->properties->water->getValue();
						}
					}
					
					if (waterProviders.empty())
						continue; //no water providers, no state change for network

					// best would be expanding radius from tiles in network based upon the available water remaining

					//search radius for water users
					for (auto tile : network) {
						tileRadiusLoop(tile.tileX, tile.tileY, 5, [&](int x, int y) {
							tileComplete tc = getComplete(x, y);
							if (tc.partial == nullptr)
								return;
							//if (tc.parent_plop_instance && tc.parent_plop_instance->waterUsage() < 0) {
							if (tc.parent_plop_instance && tc.parent_plop_instance->properties && tc.parent_plop_instance->properties->water && !tc.parent_plop_instance->properties->water->isSupply()) {
								if (waterUsers.find(tc.parent_plop_instance) != waterUsers.end())
									return; //if already accounted for, skip it
								waterUsers.insert(tc.parent_plop_instance);
								//output += abs(tc.parent_plop_instance->waterUsage());
								output += abs(tc.parent_plop_instance->properties->water->getMax());
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
						//int dr = 5;
						tileRadiusLoop(tile.tileX, tile.tileY, radius, [&](int x, int y) {
							tileComplete tc = getComplete(x, y);
							if (tc.partial == nullptr)
								return;

							tc.partial->setWater(true);
						});
					}

					for (auto user : waterUsers) {
						//user->water = 0;
						user->properties->water->setValue(0);
						tileRadiusLoop(user->originX, user->originY, radius, [&](int x, int y) {
							//if (user->isWellWatered() || user->waterSupply())
							if (user->properties->water->isSaturated() || user->properties->water->isSupply()) {
								fprintf(logFile, "Saturated or supply\n");
								return;
							}

							tileComplete tc = getComplete(x, y);
							if (!isWaterNetwork(tc))
								return;
							
							//float desiredWater = abs(user->waterUsage());
							//if (input >= desiredWater) {
							//	input -= desiredWater;
							//	user->water += desiredWater;
							//}
							fprintf(logFile, "Calc water\n");
							input -= user->properties->water->addValue(-input); 
						});

						fprintf(logFile, "Water user %f/%f (%f)\n", user->properties->water->getValue(), user->properties->water->getMax(), input);
					}

				}
				*/

				break;
			}
		}
	}	
	
	cleanupexit();

	return 0;
}