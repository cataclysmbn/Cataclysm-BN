#include "projectile.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "ammo_effect.h"
#include "explosion.h"
#include "game.h"
#include "json.h"
#include "item.h"
#include "map.h"
#include "map_iterator.h"
#include "rng.h"
#include "string_id.h"

projectile::projectile() : custom_explosion( nullptr )
{ }

projectile::~projectile() = default;

projectile::projectile( projectile && )  noexcept = default;


projectile::projectile( const projectile &other )
{
    *this = other;
}

projectile &projectile::operator=( const projectile &other )
{
    impact = other.impact;
    speed = other.speed;
    range = other.range;
    aimedcritmaxbonus = other.aimedcritmaxbonus;
    aimedcritbonus = other.aimedcritbonus;
    proj_effects = other.proj_effects;
    set_drop( item::spawn( *other.get_drop() ) );
    set_custom_explosion( other.get_custom_explosion() );

    return *this;
}

detached_ptr<item> projectile::unset_drop()
{
    detached_ptr<item> ret;
    ret = std::move( drop );
    return ret;
}

void projectile::set_drop( detached_ptr<item> &&it )
{
    drop = std::move( it );
}

item *projectile::get_drop() const
{
    if( !drop ) {
        return &null_item_reference();
    }
    return &*drop;
}

const explosion_data &projectile::get_custom_explosion() const
{
    if( custom_explosion != nullptr ) {
        return *custom_explosion;
    }

    static const explosion_data null_explosion{};
    return null_explosion;
}

void projectile::set_custom_explosion( const explosion_data &ex )
{
    custom_explosion = std::make_unique<explosion_data>( ex );
}

void projectile::unset_custom_explosion()
{
    custom_explosion.reset();
}

void projectile::deserialize( JsonIn &jsin )
{
    JsonObject jo = jsin.get_object();
    load( jo );
}

void projectile::load( JsonObject &jo )
{
    jo.read( "impact", impact );
    range = jo.get_int( "range" );
    speed = jo.get_int( "speed", 1000 );
    aimedcritmaxbonus = jo.get_float( "aimedcritmaxbonus", 0.0 );
    aimedcritbonus = jo.get_float( "aimedcritbonus", 0.0 );
    jo.read( "proj_effects", proj_effects );
}

void apply_ammo_effects( const tripoint &p, const std::set<ammo_effect_str_id> &effects,
                         Creature *source )
{
    map &here = get_map();
    for( const ammo_effect_str_id &ae_id : effects ) {
        const ammo_effect &ae = *ae_id;
        if( ae.aoe_field_type )
            for( auto &pt : here.points_in_radius( p, ae.aoe_radius, ae.aoe_radius_z ) ) {
                if( x_in_y( ae.aoe_chance, 100 ) ) {
                    const bool check_sees = !ae.aoe_check_sees || here.sees( p, pt, ae.aoe_check_sees_radius );
                    const bool check_passable = !ae.aoe_check_passable || here.passable( pt );
                    if( check_sees && check_passable ) {
                        here.add_field( pt, ae.aoe_field_type, rng( ae.aoe_intensity_min, ae.aoe_intensity_max ) );
                    }
                }
            }
        if( ae.aoe_explosion_data ) {
            explosion_handler::explosion( p, ae.aoe_explosion_data, source );
        }
        if( ae.do_flashbang ) {
            explosion_handler::flashbang( p, false, "explosion" );
        }
        if( ae.do_emp_blast ) {
            explosion_handler::emp_blast( p );
        }
    }
}

void apply_ammo_effects( const tripoint &p, const std::set<std::string> &effects, Creature *source )
{
    std::set<ammo_effect_str_id> effect_ids;
    for( const std::string &s : effects ) {
        ammo_effect_str_id id( s );
        if( id.is_valid() ) {
            effect_ids.emplace( id );
        }
    }
    apply_ammo_effects( p, effect_ids, source );
}

static int aoe_of( const ammo_effect_str_id &ae_id )
{
    return std::max( ae_id->aoe_size, ae_id->aoe_explosion_data.safe_range() - 1 );
}

static int aoe_of( const std::string &s )
{
    ammo_effect_str_id ae_id( s );
    if( ae_id.is_valid() ) {
        return std::max( ae_id->aoe_size, ae_id->aoe_explosion_data.safe_range() - 1 );
    } else {
        return 0;
    }
}

int max_aoe_size( const std::set<ammo_effect_str_id> &tags )
{
    int ret = 0;
    for( const ammo_effect_str_id &ae_id : tags ) {
        ret = std::max( ret, aoe_of( ae_id ) );
    }
    return ret;
}

int max_aoe_size( const std::set<std::string> &tags )
{
    int aoe_size = 0;
    for( const std::string &s : tags ) {
        aoe_size = std::max( aoe_size, aoe_of( s ) );
    }
    return aoe_size;
}
