#pragma once

#include "point.h"
#include "type_id.h"

class item;
enum class item_location_type : int;
class Character;
class Creature;
class submap;
class vehicle;
class monster;
class npc;
struct tripoint;
template<typename T>
class detached_ptr;

template<class T>
class location
{
    public:
        virtual detached_ptr<T> detach( T *obj ) = 0;
        virtual void attach( detached_ptr<T> &&obj ) = 0;
        virtual bool is_loaded( const T *obj ) const = 0;
        virtual tripoint position( const T *obj ) const = 0;
        virtual std::string describe( const Character *ch, const T *obj ) const = 0;
        virtual ~location() = default;
};

class item_location : public location<item>
{
    public:
        virtual item_location_type where() const = 0;
        virtual int obtain_cost( const Character &ch, int qty, const item *it ) const = 0;
};

class character_item_location : public item_location
{
    protected:
        Character *holder;
    public:
        character_item_location( Character *h ): holder( h ) {};
        detached_ptr<item> detach( item *it ) override;
        void attach( detached_ptr<item> &&obj ) override;
        bool is_loaded( const item *it ) const override;
        tripoint position( const item *it ) const override;
        item_location_type where() const override;
        int obtain_cost( const Character &ch, int qty, const item *it ) const override;
        std::string describe( const Character *ch, const item *it ) const override;
};

class npc_mission_item_location : public character_item_location
{
    public:
        npc_mission_item_location( npc *h );
        detached_ptr<item> detach( item *it ) override;
        void attach( detached_ptr<item> &&obj ) override;
};

class wield_item_location :  public item_location
{
    protected:
        Creature *holder;
    public:
        wield_item_location( Creature *h ): item_location(), holder( h ) {};
        detached_ptr<item> detach( item *it ) override;
        void attach( detached_ptr<item> &&obj ) override;
        bool is_loaded( const item *it ) const override;
        tripoint position( const item *it ) const override;
        item_location_type where() const override;
        int obtain_cost( const Character &ch, int qty, const item *it ) const override;
        std::string describe( const Character *ch, const item *it ) const override;
};


class worn_item_location :  public character_item_location
{
    public:
        worn_item_location( Character *h ): character_item_location( h ) {};
        detached_ptr<item> detach( item *it ) override;
        void attach( detached_ptr<item> &&obj ) override;
        int obtain_cost( const Character &ch, int qty, const item *it ) const override;
        std::string describe( const Character *ch, const item *it ) const override;
};

class tile_item_location : public item_location
{
    protected:
        tripoint pos;//abs coords
    public:
        tile_item_location( tripoint position );
        detached_ptr<item> detach( item *it ) override;
        void attach( detached_ptr<item> &&obj ) override;
        bool is_loaded( const item *it ) const override;
        tripoint position( const item *it ) const override;
        item_location_type where() const override;
        int obtain_cost( const Character &ch, int qty, const item *it ) const override;
        std::string describe( const Character *ch, const item *it ) const override;
        void move_by( tripoint offset );
};

class partial_con_item_location : public tile_item_location
{
    public:
        partial_con_item_location( tripoint position );
        detached_ptr<item> detach( item *it ) override;
        void attach( detached_ptr<item> &&obj ) override;
};

class monster_item_location : public item_location
{
    protected:
        monster *on;
    public:
        monster_item_location( monster *on ) : on( on ) {}
        detached_ptr<item> detach( item *it ) override;
        void attach( detached_ptr<item> &&obj ) override;
        bool is_loaded( const item *it ) const override;
        tripoint position( const item *it ) const override;
        item_location_type where() const override;
        int obtain_cost( const Character &ch, int qty, const item *it ) const override;
        std::string describe( const Character *ch, const item *it ) const override;
};

class monster_component_item_location : public monster_item_location
{
    public:
        monster_component_item_location( monster *on ) : monster_item_location( on ) { }
        detached_ptr<item> detach( item *it ) override;
        void attach( detached_ptr<item> &&obj ) override;
};

class monster_tied_item_location :  public monster_item_location
{
    public:
        monster_tied_item_location( monster *on ) : monster_item_location( on ) { }
        detached_ptr<item> detach( item *it ) override;
        void attach( detached_ptr<item> &&obj ) override;
};
class monster_tack_item_location :  public monster_item_location
{
    public:
        monster_tack_item_location( monster *on ) : monster_item_location( on ) { }
        detached_ptr<item> detach( item *it ) override;
        void attach( detached_ptr<item> &&obj ) override;
};
class monster_armor_item_location :  public monster_item_location
{
    public:
        monster_armor_item_location( monster *on ) : monster_item_location( on ) { }
        detached_ptr<item> detach( item *it ) override;
        void attach( detached_ptr<item> &&obj ) override;
};
class monster_storage_item_location :  public monster_item_location
{
    public:
        monster_storage_item_location( monster *on ) : monster_item_location( on ) { }
        detached_ptr<item> detach( item *it ) override;
        void attach( detached_ptr<item> &&obj ) override;
};

class monster_battery_item_location :  public monster_item_location
{
    public:
        monster_battery_item_location( monster *on ) : monster_item_location( on ) { }
        detached_ptr<item> detach( item *it ) override;
        void attach( detached_ptr<item> &&obj ) override;
};

class vehicle_item_location : public item_location
{
    protected:
        vehicle *veh;
        int hack_id;
    public:
        vehicle_item_location( vehicle *veh, int hack_id ) : veh( veh ), hack_id( hack_id ) {}
        detached_ptr<item> detach( item *it ) override;
        void attach( detached_ptr<item> &&obj ) override;
        bool is_loaded( const item *it ) const override;
        tripoint position( const item *it ) const override;
        item_location_type where() const override;
        int obtain_cost( const Character &ch, int qty, const item *it ) const override;
        std::string describe( const Character *ch, const item *it ) const override;
};

class vehicle_base_item_location : public vehicle_item_location
{
    public:
        vehicle_base_item_location( vehicle *veh, int hack_id ) : vehicle_item_location( veh, hack_id ) {}
        detached_ptr<item> detach( item *it ) override;
        void attach( detached_ptr<item> &&obj ) override;
        int obtain_cost( const Character &ch, int qty, const item *it ) const override;
        std::string describe( const Character *ch, const item *it ) const override;
};

class contents_item_location :  public item_location
{
    protected:
        item *container;
    public:
        contents_item_location( item *cont ) : container( cont ) {}
        detached_ptr<item> detach( item *it ) override;
        void attach( detached_ptr<item> &&obj ) override;
        bool is_loaded( const item *it ) const override;
        tripoint position( const item * ) const override;
        item_location_type where() const override;
        int obtain_cost( const Character &ch, int qty, const item *it ) const override;
        std::string describe( const Character *ch, const item *it ) const override;
        void on_changed( const item *it ) const;

        item *parent() const;
};

class component_item_location : public contents_item_location
{
    public:
        component_item_location( item *cont ) : contents_item_location( cont ) {}
        detached_ptr<item> detach( item *it ) override;
        void attach( detached_ptr<item> &&obj ) override;
};

class fake_item_location : public item_location
{
    public:
        fake_item_location() = default;
        detached_ptr<item> detach( item *it ) override;
        void attach( detached_ptr<item> &&obj ) override;
        bool is_loaded( const item *it ) const override;
        tripoint position( const item *it ) const override;
        item_location_type where() const override;
        int obtain_cost( const Character &ch, int qty, const item *it ) const override;
        std::string describe( const Character *ch, const item *it ) const override;
};

class temp_item_location : public item_location
{
    public:
        temp_item_location() = default;
        detached_ptr<item> detach( item *it ) override;
        void attach( detached_ptr<item> &&it ) override;
        bool is_loaded( const item *it ) const override;
        tripoint position( const item *it ) const override;
        item_location_type where() const override;
        int obtain_cost( const Character &ch, int qty, const item *it ) const override;
        std::string describe( const Character *ch, const item *it ) const override;
};


