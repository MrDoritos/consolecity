#include <vector>

typedef float net_t;

struct Game {
    /*
    When does state change?
    When does network propagate?
    Who modifies states?
    */

    struct Network {
        net_t network_demand = 0, network_supply = 0; //running sum of network
        net_t production = 0;
        net_t production_max = 0;
        net_t consumption = 0;
        net_t consumption_min = 0, consumption_max = 0;
        net_t available = 0;
        net_t storage = 0, storage_max = 0;
        bool shares = true;

        enum NET_TYPE {
            WATER = 0, POWER = 1, GAS = 2, POLLUTION
        };

        enum OP_TYPE {
            USE, PUT, GET, SET, NET_INPUT, NET_OUTPUT, TICK
        };

        enum EVENT_TYPE {
            UPDATE_DEMAND, UPDATE_SUPPLY, EVENT_TICK
        };

        enum DISTRIB_TYPE {
            EVEN, FCFS, ROAD, DIST  
        };
        protected:

        public:
        virtual net_t onNetworkEvent(OP_TYPE op, net_t *value) {
            net_t v = *value;

            /*
            states

            production
                the rate of production (not changed here)
            production_max
                the maximum rate of production (not changed here)
            consumption
                the rate of consumption (not changed here)
            consumption_max
                the maximum rate of consumption (not changed here)
            
            network_demand
                producer: the demand put on the plop by the network
                consumer: the demand put on the network by the plop
            
            network_supply
                producer: not used, or a sum of max production + storage
                consumer: greater than demand if there's a surplus of 
                            resources, doesn't have to be, but should be equal

            tick
                change figures for the next cycle
                producer: use network_demand, modify network_supply
                consumer: modify network_demand, use network_supply

            is_producer = shares && production_max > 0
                the plop is a producer
            is_consumer = consumption > 0
                the plop is a consumer, even if it's a producer

            
            consumption_overhead
                monthly available and stowed resources

            production_overhead
                the available production potential

            max_deficit
                the maximum amount of resources that can be consumed by the next cycle

            */

            net_t consumption_overhead = available + storage;
            net_t production_overhead = (production_max - production) + storage;
            
            net_t max_deficit = consumption + (storage_max - storage);

            bool saturated = (network_supply >= network_demand);
            bool can_store = (storage_max > storage) && saturated;
            bool is_producer = shares && production_max > 0;
            bool is_consumer = consumption > 0;

            net_t total_demand = consumption, total_supply = production_overhead + production;

            if (can_store) {
                
            }

            if (is_consumer) {
                if (!is_producer) {

                }
                total_demand = consumption_overhead + consumption;
                total_supply = network_supply;
            }
            if (is_producer) {
                total_supply = production_overhead + production;
            }


            switch (op) {
                case TICK: // Create new resources

                    return 0;
                case NET_OUTPUT:
                    return 0;
                case NET_INPUT:
                    return 0;
                case USE:
                    if (!shares) {
                        return 0;
                    }
                    if (v > production) {
                        
                    }
                    return production;
                case PUT:
                    return production;
                case GET:
                    return consumption;
                case SET:
                    return 0;
            }
        }
    };

    struct Pos {
        int x, y;
    };

    struct Size : public Pos {
        int sizeX, sizeY;
    };

    template<typename T>
    struct EventBus {
        virtual void execute() {}
        std::vector<T> listeners;
    };

    struct NetworkProviderAbstract {
        virtual Network *getNetwork(Network::NET_TYPE net) = 0;
        virtual net_t onNetworkEvent(Network::NET_TYPE net, Network::OP_TYPE op, net_t *value) = 0;
        virtual void onNetwork(Network::NET_TYPE net, Network::OP_TYPE op, net_t *value) = 0;
    };

    struct NetworkProvider : public NetworkProviderAbstract {
        Network networks[3];

        Network *getNetwork(Network::NET_TYPE net) {
            return &networks[net];
        }

        void onNetwork(Network::NET_TYPE net, Network::OP_TYPE op, net_t *value) {
            onNetworkEvent(net, op, value);
        }

        virtual net_t onNetworkEvent(Network::NET_TYPE net, Network::OP_TYPE op, net_t *value) {
            if (getNetwork(net) != nullptr)
                return getNetwork(net)->onNetworkEvent(op, value);

            return 0;
        }
    };

    struct TileData {
        unsigned char id;
    };

    struct PlopAbstract;

    struct TileEvent {
        Size size; // user input of selected area
        TileData data; // tile data of origin tile
    };

    //Support singletons of tile and instances of plop
    struct TileAbstract {
        virtual void onPlaceEvent(TileEvent) = 0;
        virtual void onDestroyEvent(TileEvent) = 0;
        virtual void onUpdateEvent(TileEvent) = 0;
        virtual void onRandomEvent(TileEvent) = 0;
        virtual void render(TileEvent) = 0;
        virtual void setInstanceData(TileEvent) = 0;
        virtual PlopAbstract *getPlop(TileEvent) = 0;
    };

    static std::vector<TileAbstract*> tiles;

    //Singleton
    struct Tile : public TileAbstract {
        Tile() {
            tiles.push_back(this);
        }
        void onPlaceEvent(TileEvent s) override {}
        void onDestroyEvent(TileEvent s) override {}
        void onUpdateEvent(TileEvent s) override {}
        void onRandomEvent(TileEvent s) override {}
        void render(TileEvent s) override {}
        void setInstanceData(TileEvent s) override {}
        PlopAbstract *getPlop(TileEvent s) override {return nullptr;}
    };

    struct Sprite {};

    //Created here
    struct TileSprite : public Tile {
        TileSprite(Sprite spr) {
            sprite = spr;
        }
        void render(TileEvent s) override {

        }
        Sprite sprite;
    };

    TileSprite tileGrass(Sprite());

    struct PlopAbstract : public TileAbstract {
        virtual NetworkProvider *getNetworkProvider() = 0;
        virtual PlopAbstract *clone() = 0; // clone default
        virtual Size getSize() = 0;
        
        //virtual void onPlaceEvent() = 0;
        //virtual void onDestroyEvent() = 0;
        //virtual void render() = 0;
    };

    struct Plop : public PlopAbstract, NetworkProvider {
        Plop() {

        }

        virtual NetworkProvider *getNetworkProvider() {
            return this;
        }

        void onPlace() {
            onPlaceEvent();
        }

        virtual void onPlaceEvent() {}

        void onDestroy() {
            onDestroyEvent();
        }

        virtual void onDestroyEvent() {}

        virtual Plop* clone() {}

        virtual void render() {}

        Network networks[3];
        Size size;
        int id;
    };

    struct WaterProvider : public Plop {
        WaterProvider(net_t monthlyCreation) {
            getNetwork(Network::WATER)->production = monthlyCreation;
        }

        net_t onNetworkEvent(Network::NET_TYPE net, Network::OP_TYPE op, net_t *value) {
            if (net == Network::WATER) {
            }
        }
    };

    std::vector<Plop*> instances;

    void tick() {
        
    }
};

int main() {
    Game game;
    game.tick();
    return 0;
}