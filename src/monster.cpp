#include "monster.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <tuple>
#include <unordered_map>

#include "avatar.h"
#include "bodypart.h"
#include "catalua_hooks.h"
#include "catalua_sol.h"
#include "character.h"
#include "coordinate_conversions.h"
#include "creature_tracker.h"
#include "cursesdef.h"
#include "debug.h"
#include "effect.h"
#include "enums.h"
#include "event_bus.h"
#include "event.h"
#include "explosion.h"
#include "field_type.h"
#include "flag.h"
#include "flat_set.h"
#include "game_constants.h"
#include "game.h"
#include "int_id.h"
#include "item_group.h"
#include "item.h"
#include "itype.h"
#include "line.h"
#include "locations.h"
#include "make_static.h"
#include "mapdata.h"
#include "map.h"
#include "map_iterator.h"
#include "mattack_common.h"
#include "melee.h"
#include "messages.h"
#include "mission.h"
#include "mod_manager.h"
#include "mondeath.h"
#include "mondefense.h"
#include "monfaction.h"
#include "mongroup.h"
#include "morale_types.h"
#include "mtype.h"
#include "mutation.h"
#include "npc.h"
#include "options.h"
#include "output.h"
#include "overmapbuffer.h"
#include "pimpl.h"
#include "player.h"
#include "projectile.h"
#include "rng.h"
#include "sounds.h"
#include "string_formatter.h"
#include "string_id.h"
#include "string_utils.h"
#include "submap.h"
#include "text_snippets.h"
#include "translations.h"
#include "trap.h"
#include "weather.h"
#include "profile.h"

static const ammo_effect_str_id ammo_effect_WHIP( "WHIP" );

static const efftype_id effect_attention( "attention" );
static const efftype_id effect_badpoison( "badpoison" );
static const efftype_id effect_beartrap( "beartrap" );
static const efftype_id effect_bleed( "bleed" );
static const efftype_id effect_blind( "blind" );
static const efftype_id effect_bouldering( "bouldering" );
static const efftype_id effect_command_buff( "command_buff" );
static const efftype_id effect_crushed( "crushed" );
static const efftype_id effect_corroding( "corroding" );
static const efftype_id effect_dazed( "dazed" );
static const efftype_id effect_deaf( "deaf" );
static const efftype_id effect_docile( "docile" );
static const efftype_id effect_downed( "downed" );
static const efftype_id effect_emp( "emp" );
static const efftype_id effect_feral_infighting_punishment( "feral_infighting_punishment" );
static const efftype_id effect_feral_killed_recently( "feral_killed_recently" );
static const efftype_id effect_grabbed( "grabbed" );
static const efftype_id effect_grabbing( "grabbing" );
static const efftype_id effect_heavysnare( "heavysnare" );
static const efftype_id effect_hit_by_player( "hit_by_player" );
static const efftype_id effect_in_pit( "in_pit" );
static const efftype_id effect_lightsnare( "lightsnare" );
static const efftype_id effect_migo_atmosphere( "migo_atmosphere" );
static const efftype_id effect_monster_armor( "monster_armor" );
static const efftype_id effect_no_sight( "no_sight" );
static const efftype_id effect_onfire( "onfire" );
static const efftype_id effect_pacified( "pacified" );
static const efftype_id effect_pet( "pet" );
static const efftype_id effect_tpollen( "tpollen" );
static const efftype_id effect_paralyzepoison( "paralyzepoison" );
static const efftype_id effect_poison( "poison" );
static const efftype_id effect_ridden( "ridden" );
static const efftype_id effect_run( "run" );
static const efftype_id effect_smoke( "smoke" );
static const efftype_id effect_stunned( "stunned" );
static const efftype_id effect_supercharged( "supercharged" );
static const efftype_id effect_teargas( "teargas" );
static const efftype_id effect_tied( "tied" );
static const efftype_id effect_webbed( "webbed" );
static const efftype_id effect_well_fed( "well_fed" );

static const itype_id itype_corpse( "corpse" );
static const itype_id itype_milk( "milk" );
static const itype_id itype_milk_raw( "milk_raw" );

static const species_id FISH( "FISH" );
static const species_id FUNGUS( "FUNGUS" );
static const species_id INSECT( "INSECT" );
static const species_id MAMMAL( "MAMMAL" );
static const species_id MOLLUSK( "MOLLUSK" );
static const species_id PLANT( "PLANT" );
static const species_id ROBOT( "ROBOT" );
static const species_id ZOMBIE( "ZOMBIE" );

static const trait_id trait_ANIMALDISCORD( "ANIMALDISCORD" );
static const trait_id trait_ANIMALDISCORD2( "ANIMALDISCORD2" );
static const trait_id trait_ANIMALEMPATH( "ANIMALEMPATH" );
static const trait_id trait_ANIMALEMPATH2( "ANIMALEMPATH2" );
static const trait_id trait_BEE( "BEE" );
static const trait_id trait_FLOWERS( "FLOWERS" );
static const trait_id trait_INATTENTIVE( "INATTENTIVE" );
static const trait_id trait_KILLER( "KILLER" );
static const trait_id trait_MYCUS_FRIEND( "MYCUS_FRIEND" );
static const trait_id trait_PACIFIST( "PACIFIST" );
static const trait_id trait_PHEROMONE_INSECT( "PHEROMONE_INSECT" );
static const trait_id trait_PHEROMONE_MAMMAL( "PHEROMONE_MAMMAL" );
static const trait_id trait_PROF_FERAL( "PROF_FERAL" );
static const trait_id trait_TERRIFYING( "TERRIFYING" );
static const trait_id trait_THRESH_MYCUS( "THRESH_MYCUS" );

struct pathfinding_settings;

// Limit the number of iterations for next upgrade_time calculations.
// This also sets the percentage of monsters that will never upgrade.
// The rough formula is 2^(-x), e.g. for x = 5 it's 0.03125 (~ 3%).
enum {
    UPGRADE_MAX_ITERS = 5
};

static const std::map<creature_size, translation> size_names {
    { creature_size::tiny, to_translation( "size adj", "tiny" ) },
    { creature_size::small, to_translation( "size adj", "small" ) },
    { creature_size::medium, to_translation( "size adj", "medium" ) },
    { creature_size::large, to_translation( "size adj", "large" ) },
    { creature_size::huge, to_translation( "size adj", "huge" ) },
};

static const std::map<monster_attitude, std::pair<std::string, color_id>> attitude_names {
    {monster_attitude::MATT_FRIEND, {translate_marker( "Friendly." ), def_h_white}},
    {monster_attitude::MATT_FPASSIVE, {translate_marker( "Passive." ), def_h_white}},
    {monster_attitude::MATT_FLEE, {translate_marker( "Fleeing!" ), def_c_green}},
    {monster_attitude::MATT_FOLLOW, {translate_marker( "Tracking." ), def_c_yellow}},
    {monster_attitude::MATT_IGNORE, {translate_marker( "Ignoring." ), def_c_light_gray}},
    {monster_attitude::MATT_ZLAVE, {translate_marker( "Zombie slave." ), def_c_green}},
    {monster_attitude::MATT_ATTACK, {translate_marker( "Hostile!" ), def_c_red}},
    {monster_attitude::MATT_UNKNOWN, { "", def_h_white}}, //Should only be used for UI.
    {monster_attitude::MATT_NULL, {translate_marker( "BUG: Behavior unnamed." ), def_h_red}},
};

// Returns all the players around pos who don't have grabbing monsters adjacent to them
static std::vector<player *> find_targets_to_ungrab( const tripoint &pos )
{
    std::vector<player *> result;
    for( auto &player_pos : g->m.points_in_radius( pos, 1, 0 ) ) {
        player *p = g->critter_at<player>( player_pos );
        if( !p || !p->has_effect( effect_grabbed ) ) {
            continue;
        }
        bool grabbed = false;
        for( auto &mon_pos : g->m.points_in_radius( player_pos, 1, 0 ) ) {
            const monster *const mon = g->critter_at<monster>( mon_pos );
            if( mon && mon->has_effect( effect_grabbing ) ) {
                grabbed = true;
                break;
            }
        }
        if( !grabbed ) {
            result.push_back( p );

        }
    }

    return result;
}

//TODO!: make sure all the uses of tack_item etc are properly locationd

monster::monster() : corpse_components( new monster_component_item_location( this ) ),
    tied_item( new monster_tied_item_location( this ) ),
    tack_item( new monster_tack_item_location( this ) ),
    armor_item( new monster_armor_item_location( this ) ),
    storage_item( new monster_storage_item_location( this ) ),
    battery_item( new monster_battery_item_location( this ) ),
    inv( new monster_item_location( this ) )
{
    position.x = 20;
    position.y = 10;
    position.z = -500; // Some arbitrary number that will cause debugmsgs
    unset_dest();
    wandf = 0;
    hp = 60;
    moves = 0;
    friendly = 0;
    anger = 0;
    morale = 2;
    faction = mfaction_id( 0 );
    mission_id = -1;
    no_extra_death_drops = false;
    dead = false;
    death_drops = true;
    made_footstep = false;
    hallucination = false;
    ignoring = 0;
    upgrades = false;
    upgrade_time = -1;
    last_updated = calendar::start_of_cataclysm;
    udder_timer = calendar::turn;
    horde_attraction = MHA_NULL;
    aggro_character = true;
    set_anatomy( anatomy_id( "default_anatomy" ) );
    set_body();
}

monster::monster( const mtype_id &id ) : monster()
{
    type = &id.obj();
    moves = type->speed;
    Creature::set_speed_base( type->speed );
    hp = type->hp;
    for( auto &sa : type->special_attacks ) {
        auto &entry = special_attacks[sa.first];
        entry.cooldown = rng( 0, sa.second->cooldown );
    }
    anger = type->agro;
    morale = type->morale;
    faction = type->default_faction;
    ammo = type->starting_ammo;
    upgrades = type->upgrades && ( type->half_life || type->age_grow );
    reproduces = type->reproduces && type->baby_timer && !monster::has_flag( MF_NO_BREED );
    aggro_character = type->aggro_character;
    if( monster::has_flag( MF_AQUATIC ) ) {
        fish_population = dice( 1, 20 );
    }
}

monster::monster( const mtype_id &id, const tripoint &p ) : monster( id )
{
    position = p;
    unset_dest();
}

monster::monster( const monster &source ) : Creature( source ),
    corpse_components( new monster_component_item_location(
                           this ) ), tied_item( new monster_tied_item_location( this ) ),
    tack_item( new monster_tack_item_location( this ) ),
    armor_item( new monster_armor_item_location( this ) ),
    storage_item( new monster_storage_item_location( this ) ),
    battery_item( new monster_battery_item_location( this ) ),
    inv( new monster_item_location( this ) )
{
    wander_pos = source.wander_pos;
    wandf = source.wandf;
    mounted_player = source.mounted_player;
    mounted_player_id = source.mounted_player_id;
    dragged_foe_id = source.dragged_foe_id;
    friendly = source.friendly;
    anger = source.anger;
    morale = source.morale;
    faction = source.faction;
    mission_id = source.mission_id;
    type = source.type;
    no_extra_death_drops = source.no_extra_death_drops;
    no_corpse_quiet = source.no_corpse_quiet;
    death_drops = source.death_drops;
    made_footstep = source.made_footstep;
    unique_name = source.unique_name;
    hallucination = source.hallucination;
    fish_population = source.fish_population;
    ignoring = source.ignoring;
    lastseen_turn = source.lastseen_turn;
    staircount = source.staircount;
    ammo = source.ammo;

    for( const item * const &it : source.corpse_components ) {
        corpse_components.push_back( item::spawn( *it ) );
    }

    for( const item * const &it : source.inv ) {
        inv.push_back( item::spawn( *it ) );
    }

    hp = source.hp;
    special_attacks = source.special_attacks;
    goal = source.goal;
    position = source.position;
    dead = source.dead;
    upgrades = source.upgrades;
    upgrade_time = source.upgrade_time;
    reproduces = source.reproduces;
    baby_timer = source.baby_timer;
    udder_timer = source.udder_timer;
    horde_attraction = source.horde_attraction;
    path = source.path;
    effect_cache = source.effect_cache;
    summon_time_limit = source.summon_time_limit;

    set_tied_item( item::spawn( *source.tied_item ) );
    set_tack_item( item::spawn( *source.tack_item ) );
    set_armor_item( item::spawn( *source.armor_item ) );
    set_storage_item( item::spawn( *source.storage_item ) );
    set_battery_item( item::spawn( *source.battery_item ) );
    set_anatomy( anatomy_id( "default_anatomy" ) );
};


monster::~monster() = default;

void monster::setpos( const tripoint &p )
{
    if( p == pos() ) {
        return;
    }

    bool wandering = is_wandering();
    g->update_zombie_pos( *this, p );
    position = p;
    if( has_effect( effect_ridden ) && mounted_player && mounted_player->pos() != pos() ) {
        add_msg( m_debug, "Ridden monster %s moved independently and dumped player", get_name() );
        mounted_player->forced_dismount();
    }
    if( wandering ) {
        unset_dest();
    }
}

const tripoint &monster::pos() const
{
    return position;
}

void monster::poly( const mtype_id &id )
{
    double hp_percentage = static_cast<double>( hp ) / static_cast<double>( type->hp );
    type = &id.obj();
    moves = 0;
    Creature::set_speed_base( type->speed );
    anger = type->agro;
    morale = type->morale;
    hp = static_cast<int>( hp_percentage * type->hp );
    special_attacks.clear();
    for( auto &sa : type->special_attacks ) {
        auto &entry = special_attacks[sa.first];
        entry.cooldown = sa.second->cooldown;
    }
    faction = type->default_faction;
    upgrades = type->upgrades;
    reproduces = type->reproduces;

    // HACK: We should know if the monster is in the bubble instead of checking it like this
    if( g->critter_tracker->temporary_id( *this ) >= 0 ) {
        g->critter_tracker->update_faction( *this );
    }
}

bool monster::can_upgrade() const
{
    return upgrades && get_option<float>( "MONSTER_UPGRADE_FACTOR" ) > 0.0;
}

// For master special attack.
void monster::hasten_upgrade()
{
    if( !can_upgrade() || upgrade_time < 1 ) {
        return;
    }

    const int scaled_half_life = type->half_life * get_option<float>( "MONSTER_UPGRADE_FACTOR" );
    int reduction = rng( 1, scaled_half_life );

    // Well-fed animals mature 30% faster
    if( has_effect( effect_well_fed ) ) {
        reduction = static_cast<int>( reduction * 1.3 );
    }

    upgrade_time -= reduction;
    if( upgrade_time < 0 ) {
        upgrade_time = 0;
    }
}

int monster::get_upgrade_time() const
{
    return upgrade_time;
}

// Sets time to upgrade to 0.
void monster::allow_upgrade()
{
    upgrade_time = 0;
}

// This will disable upgrades in case max iters have been reached.
// Checking for return value of -1 is necessary.
int monster::next_upgrade_time()
{
    if( type->age_grow > 0 ) {
        return type->age_grow;
    }
    const int scaled_half_life = type->half_life * get_option<float>( "MONSTER_UPGRADE_FACTOR" );
    int day = 1; // 1 day of guaranteed evolve time
    for( int i = 0; i < UPGRADE_MAX_ITERS; i++ ) {
        if( one_in( 2 ) ) {
            day += rng( 0, scaled_half_life );
            return day;
        } else {
            day += scaled_half_life;
        }
    }
    // didn't manage to upgrade, shouldn't ever then
    upgrades = false;
    return -1;
}

void monster::try_upgrade( bool pin_time )
{
    if( !can_upgrade() ) {
        return;
    }

    const int current_day = to_days<int>( calendar::turn - calendar::turn_zero );
    //This should only occur when a monster is created or upgraded to a new form
    if( upgrade_time < 0 ) {
        upgrade_time = next_upgrade_time();
        if( upgrade_time < 0 ) {
            return;
        }
        if( pin_time || type->age_grow > 0 ) {
            // offset by today, always true for growing creatures
            upgrade_time += current_day;
        } else {
            // offset by starting season
            // TODO: revisit this and make it simpler
            upgrade_time += to_days<int>( calendar::start_of_cataclysm - calendar::turn_zero );
        }
    }

    // Here we iterate until we either are before upgrade_time or can't upgrade any more.
    // This is so that late into game new monsters can 'catch up' with all that half-life
    // upgrades they'd get if we were simulating whole world.
    while( true ) {
        int effective_upgrade_time = upgrade_time;

        // Well-fed animals mature 30% faster (check remaining time)
        if( has_effect( effect_well_fed ) ) {
            const int original_grow_time = next_upgrade_time();
            const int accelerated_grow_time = static_cast<int>( original_grow_time * 0.7 );
            effective_upgrade_time = ( upgrade_time - original_grow_time ) + accelerated_grow_time;
        }

        if( effective_upgrade_time > current_day ) {
            // not yet
            return;
        }

        if( type->upgrade_into ) {
            //If we upgrade into a blacklisted monster, treat it as though we are non-upgradeable
            if( MonsterGroupManager::monster_is_blacklisted( type->upgrade_into ) ) {
                return;
            }
            poly( type->upgrade_into );
        } else {
            const mtype_id &new_type = MonsterGroupManager::GetRandomMonsterFromGroup( type->upgrade_group );
            if( new_type ) {
                poly( new_type );
            }
        }

        if( !upgrades ) {
            // upgraded into a non-upgradeable monster
            return;
        }

        const int next_upgrade = next_upgrade_time();
        if( next_upgrade < 0 ) {
            // hit never_upgrade
            return;
        }
        upgrade_time += next_upgrade;
    }
}

void monster::try_reproduce()
{
    // This can happen if the monster type has changed (from reproducing to non-reproducing monster)
    if( !type->baby_timer ) {
        return;
    }

    if( !baby_timer ) {
        // Assume this is a freshly spawned monster (because baby_timer is not set yet), set the point when it reproduce to somewhere in the future.
        baby_timer.emplace( calendar::turn + *type->baby_timer );
    }

    bool season_spawn = false;
    bool season_match = true;

    // only 50% of animals should reproduce
    bool female = one_in( 2 );
    for( auto &elem : type->baby_flags ) {
        if( elem == "SUMMER" || elem == "WINTER" || elem == "SPRING" || elem == "AUTUMN" ) {
            season_spawn = true;
        }
    }

    // add a decreasing chance of additional spawns when "catching up" an existing animal
    int chance = -1;
    while( true ) {
        if( *baby_timer > calendar::turn ) {
            return;
        }

        if( season_spawn ) {
            season_match = false;
            for( auto &elem : type->baby_flags ) {
                if( ( season_of_year( *baby_timer ) == SUMMER && elem == "SUMMER" ) ||
                    ( season_of_year( *baby_timer ) == WINTER && elem == "WINTER" ) ||
                    ( season_of_year( *baby_timer ) == SPRING && elem == "SPRING" ) ||
                    ( season_of_year( *baby_timer ) == AUTUMN && elem == "AUTUMN" ) ) {
                    season_match = true;
                }
            }
        }

        chance += 2;

        if( ( season_match && female && one_in( chance ) ) ) {
            reproduce();
        }
        *baby_timer += *type->baby_timer;
    }
}

void monster::reproduce()
{
    if( !reproduces ) {
        return;
    }

    // Well-fed animals produce more offspring (30% increase)
    // Use probabilistic rounding: fractional part becomes probability of +1
    // e.g., 2.6 babies → 60% chance of 3, 40% chance of 2
    int max_babies = type->baby_count;
    if( has_effect( effect_well_fed ) ) {
        const float exact_babies = max_babies * 1.3f;
        max_babies = static_cast<int>( exact_babies );
        const int fractional_percent = static_cast<int>( ( exact_babies - max_babies ) * 100 );
        if( x_in_y( fractional_percent, 100 ) ) {
            max_babies++;
        }
    }

    const int spawn_cnt = rng( 1, max_babies );
    const auto birth = baby_timer && ( *baby_timer <= calendar::turn ) ? *baby_timer : calendar::turn;

    // wildlife creatures that are pets of the player will spawn pet offspring
    const spawn_disposition disposition = is_pet()
                                          ? spawn_disposition::SpawnDisp_Pet
                                          : spawn_disposition::SpawnDisp_Default;

    if( type->baby_monster ) {
        g->m.add_spawn( type->baby_monster, spawn_cnt, pos(), disposition );
    } else {
        detached_ptr<item> item_to_spawn = item::spawn( type->baby_egg, birth, spawn_cnt );

        if( disposition == spawn_disposition::SpawnDisp_Pet ) {
            item_to_spawn->set_flag( flag_SPAWN_FRIENDLY );
        }

        g->m.add_item_or_charges( pos(), std::move( item_to_spawn ), true );
    }
}

void monster::refill_udders()
{
    if( type->starting_ammo.empty() ) {
        debugmsg( "monster %s has no starting ammo to refill udders", get_name() );
        return;
    }
    if( ammo.empty() ) {
        // legacy animals got empty ammo map, fill them up now if needed.
        ammo[type->starting_ammo.begin()->first] = type->starting_ammo.begin()->second;
    }
    auto current_milk = ammo.find( itype_milk_raw );
    if( current_milk == ammo.end() ) {
        current_milk = ammo.find( itype_milk );
        if( current_milk != ammo.end() ) {
            // take this opportunity to update milk udders to raw_milk
            ammo[itype_milk_raw] = current_milk->second;
            // Erase old key-value from map
            ammo.erase( current_milk );
            current_milk = ammo.find( itype_milk_raw );
        }
    }

    // If no milk found, use the first starting_ammo type (e.g., gasoline for hell goats)
    if( current_milk == ammo.end() ) {
        current_milk = ammo.find( type->starting_ammo.begin()->first );
        if( current_milk == ammo.end() ) {
            // This shouldn't happen since we initialized ammo above, but be safe
            return;
        }
    }

    if( current_milk->second == type->starting_ammo.begin()->second ) {
        // already full up
        return;
    }
    if( calendar::turn - udder_timer > 1_days ) {
        // no point granularizing this really, you milk once a day.
        int milk_amount = type->starting_ammo.begin()->second;

        // Well-fed animals produce 30% more milk per day
        if( has_effect( effect_well_fed ) ) {
            milk_amount = static_cast<int>( std::ceil( milk_amount * 1.3 ) );
        }

        ammo.begin()->second = milk_amount;
        udder_timer = calendar::turn;
    }
}

void monster::spawn( const tripoint &p )
{
    position = p;
    unset_dest();
}

std::string monster::get_name() const
{
    return name( 1 );
}

std::string monster::name( unsigned int quantity ) const
{
    if( !type ) {
        debugmsg( "monster::name empty type!" );
        return std::string();
    }
    if( !unique_name.empty() ) {
        return string_format( "%s: %s", type->nname( quantity ), unique_name );
    }
    return type->nname( quantity );
}

// TODO: MATERIALS put description in materials.json?
std::string monster::name_with_armor() const
{
    std::string ret;
    if( type->in_species( INSECT ) ) {
        ret = _( "carapace" );
    } else if( made_of( material_id( "veggy" ) ) ) {
        ret = _( "thick bark" );
    } else if( made_of( material_id( "bone" ) ) ) {
        ret = _( "exoskeleton" );
    } else if( made_of( material_id( "flesh" ) ) || made_of( material_id( "hflesh" ) ) ||
               made_of( material_id( "iflesh" ) ) ) {
        ret = _( "thick hide" );
    } else if( made_of( material_id( "iron" ) ) || made_of( material_id( "steel" ) ) ) {
        ret = _( "armor plating" );
    } else if( made_of( LIQUID ) ) {
        ret = _( "dense jelly mass" );
    } else {
        ret = _( "armor" );
    }
    if( has_effect( effect_monster_armor ) && !inv.empty() ) {
        for( const item * const &armor : inv ) {
            if( armor->is_pet_armor( true ) ) {
                ret += string_format( _( "wearing %1$s" ), armor->tname( 1 ) );
                break;
            }
        }
    }

    return ret;
}

std::string monster::disp_name( bool possessive, bool capitalize_first ) const
{
    if( !possessive ) {
        return string_format( capitalize_first ? _( "The %s" ) : _( "the %s" ), name() );
    } else {
        return string_format( capitalize_first ? _( "The %s's" ) : _( "the %s's" ), name() );
    }
}

std::string monster::skin_name() const
{
    return name_with_armor();
}

void monster::get_HP_Bar( nc_color &color, std::string &text ) const
{
    std::tie( text, color ) = ::get_hp_bar( hp, type->hp, true );
}

std::pair<std::string, nc_color> monster::get_attitude() const
{
    const avatar &ply = get_avatar();
    const auto att = attitude_names.at( ply.has_trait( trait_INATTENTIVE ) ? MATT_UNKNOWN : attitude(
                                            &g->u ) );
    return {
        _( att.first ),
        all_colors.get( att.second )
    };

}

static std::pair<std::string, nc_color> hp_description( int cur_hp, int max_hp )
{
    const avatar &ply = get_avatar();
    std::string damage_info;
    nc_color col;
    if( ply.has_trait( trait_INATTENTIVE ) ) {
        damage_info = _( "It looks fine." );
        col = c_green;
    } else {
        if( cur_hp >= max_hp ) {
            damage_info = _( "It is uninjured." );
            col = c_green;
        } else if( cur_hp >= max_hp * 0.8 ) {
            damage_info = _( "It is lightly injured." );
            col = c_light_green;
        } else if( cur_hp >= max_hp * 0.6 ) {
            damage_info = _( "It is moderately injured." );
            col = c_yellow;
        } else if( cur_hp >= max_hp * 0.3 ) {
            damage_info = _( "It is heavily injured." );
            col = c_yellow;
        } else if( cur_hp >= max_hp * 0.1 ) {
            damage_info = _( "It is severely injured." );
            col = c_light_red;
        } else {
            damage_info = _( "It is nearly dead!" );
            col = c_red;
        }
    }
    // show exact monster HP if in debug mode
    if( debug_mode ) {
        damage_info += " ";
        damage_info += string_format( _( "%1$d/%2$d HP" ), cur_hp, max_hp );
    }

    return std::make_pair( damage_info, col );
}

static std::pair<std::string, nc_color> speed_description( float mon_speed_rating,
        bool immobile = false )
{
    if( immobile ) {
        return std::make_pair( _( "It is immobile." ), c_green );
    }

    const std::array<std::tuple<float, nc_color, std::string>, 8> cases = {{
            std::make_tuple( 1.40f, c_red, _( "It looks much faster than you." ) ),
            std::make_tuple( 1.15f, c_light_red, _( "It looks faster than you." ) ),
            std::make_tuple( 1.05f, c_yellow, _( "It looks slightly faster than you." ) ),
            std::make_tuple( 0.90f, c_white, _( "It looks about as fast as you." ) ),
            std::make_tuple( 0.80f, c_light_cyan, _( "It looks slightly slower than you." ) ),
            std::make_tuple( 0.60f, c_cyan, _( "It looks slower than you." ) ),
            std::make_tuple( 0.30f, c_light_green, _( "It looks much slower than you." ) ),
            std::make_tuple( 0.00f, c_green, _( "It seems to be barely moving." ) )
        }
    };

    const avatar &ply = get_avatar();
    const bool player_knows = !ply.has_trait( trait_INATTENTIVE );
    if( player_knows ) {
        float player_runcost = ply.run_cost( 100 );
        if( player_runcost == 0 ) {
            player_runcost = 1.0f;
        }

        // determine tiles per turn (tpt)
        const float player_tpt = ply.get_speed() / player_runcost;
        const float ratio = player_tpt == 0 ?
                            2.00f : mon_speed_rating / player_tpt;

        for( const std::tuple<float, nc_color, std::string> &speed_case : cases ) {
            if( ratio >= std::get<0>( speed_case ) ) {
                return std::make_pair( std::get<2>( speed_case ), std::get<1>( speed_case ) );
            }
        }
    } else {
        return std::make_pair( _( "It is a creature." ), c_white );
    }

    debugmsg( "speed_description: no ratio value matched" );
    return std::make_pair( _( "Unknown" ), c_white );
}

int monster::print_info( const catacurses::window &w, int vStart, int vLines, int column ) const
{
    const int vEnd = vStart + vLines;

    const bool player_knows = !g->u.has_trait( trait_INATTENTIVE );

    mvwprintz( w, point( column, vStart ), basic_symbol_color(), name() );
    wprintw( w, " " );
    const auto att = get_attitude();

    if( debug_mode ) {
        wprintz( w, c_light_gray, _( " Difficulty " ) + std::to_string( type->difficulty ) );
    }

    if( display_mod_source ) {
        const std::string mod_src = enumerate_as_string( type->src.begin(),
        type->src.end(), []( const std::pair<mtype_id, mod_id> &source ) {
            return string_format( "'%s'", source.second->name() );
        }, enumeration_conjunction::arrow );
        vStart += fold_and_print( w, point( column, vStart + 1 ), getmaxx( w ) - 2, c_cyan,
                                  string_format( _( "Origin: %s" ), mod_src ) );
    }
    if( display_object_ids ) {
        mvwprintz( w, point( column, ++vStart ), c_light_blue, string_format( "[%s]", type->id.str() ) );
    }

    if( sees( g->u ) && player_knows ) {
        mvwprintz( w, point( column, ++vStart ), c_yellow, _( "Aware of your presence!" ) );
    }

    const auto speed_desc = speed_description( speed_rating(), has_flag( MF_IMMOBILE ) );
    mvwprintz( w, point( column, ++vStart ), speed_desc.second, speed_desc.first );


    std::string effects = get_effect_status();
    if( !effects.empty() ) {
        trim_and_print( w, point( column, ++vStart ), getmaxx( w ) - 2, h_white, effects );
    }

    const auto hp_desc = hp_description( hp, type->hp );
    mvwprintz( w, point( column, ++vStart ), hp_desc.second, hp_desc.first );
    if( has_effect( effect_ridden ) && mounted_player ) {
        mvwprintz( w, point( column, ++vStart ), c_white, _( "Rider: %s" ), mounted_player->disp_name() );
    }

    std::vector<std::string> lines = foldstring( type->get_description(), getmaxx( w ) - 1 - column );
    int numlines = lines.size();
    for( int i = 0; i < numlines && vStart <= vEnd; i++ ) {
        mvwprintz( w, point( column, ++vStart ), c_white, lines[i] );
    }

    return vStart;
}

std::string monster::extended_description() const
{
    std::string ss;
    const std::pair<std::string, nc_color> att = get_attitude();
    std::string att_colored = colorize( att.first, att.second );
    std::string difficulty_str;
    if( debug_mode ) {
        difficulty_str = _( "Difficulty " ) + std::to_string( type->difficulty );
    } else {
        if( type->difficulty < 3 ) {
            difficulty_str = _( "<color_light_gray>Minimal threat.</color>" );
        } else if( type->difficulty < 10 ) {
            difficulty_str = _( "<color_light_gray>Mildly dangerous.</color>" );
        } else if( type->difficulty < 20 ) {
            difficulty_str = _( "<color_light_red>Dangerous.</color>" );
        } else if( type->difficulty < 30 ) {
            difficulty_str = _( "<color_red>Very dangerous.</color>" );
        } else if( type->difficulty < 50 ) {
            difficulty_str = _( "<color_red>Extremely dangerous.</color>" );
        } else {
            difficulty_str = _( "<color_red>Fatally dangerous!</color>" );
        }
    }

    if( display_mod_source ) {
        ss += _( "Origin: " );
        ss += enumerate_as_string( type->src.begin(),
        type->src.end(), []( const std::pair<mtype_id, mod_id> &source ) {
            return string_format( "'%s'", source.second->name() );
        }, enumeration_conjunction::arrow );
    }
    if( display_object_ids ) {
        if( display_mod_source ) {
            ss += "\n";
        }
        ss += colorize( string_format( "[%s]", type->id.str() ), c_light_blue );
    }

    ss += "\n--\n";

    ss += string_format( _( "This is a %s.  %s %s" ), name(), att_colored,
                         difficulty_str ) + "\n";
    if( !get_effect_status().empty() ) {
        ss += string_format( _( "<stat>It is %s.</stat>" ), get_effect_status() ) + "\n";
    }

    ss += "--\n";
    const std::pair<std::string, nc_color> hp_bar = hp_description( hp, type->hp );
    ss += colorize( hp_bar.first, hp_bar.second ) + "\n";

    const std::pair<std::string, nc_color> speed_desc = speed_description(
                speed_rating(),
                has_flag( MF_IMMOBILE ) );
    ss += colorize( speed_desc.first, speed_desc.second ) + "\n";

    ss += "--\n";
    ss += string_format( "<dark>%s</dark>", type->get_description() ) + "\n";
    ss += "--\n";

    ss += string_format( _( "It is %s in size." ),
                         size_names.at( get_size() ) ) + "\n";

    std::vector<std::string> types = type->species_descriptions();
    if( type->has_flag( MF_ANIMAL ) ) {
        types.emplace_back( _( "an animal" ) );
    }
    if( !types.empty() ) {
        ss += string_format( _( "It is %s." ),
                             enumerate_as_string( types ) ) + "\n";
    }

    using flag_description = std::pair<m_flag, std::string>;
    const auto describe_flags = [this, &ss](
                                    std::string_view format,
                                    const std::vector<flag_description> &&flags_names,
    std::string_view if_empty = "" ) {
        std::string flag_descriptions = enumerate_as_string( flags_names.begin(),
        flags_names.end(), [this]( const flag_description & fd ) {
            return type->has_flag( fd.first ) ? fd.second : "";
        } );
        if( !flag_descriptions.empty() ) {
            ss += string_format( format, flag_descriptions ) + "\n";
        } else if( !if_empty.empty() ) {
            ss += if_empty;
            ss += "\n";
        }
    };

    using property_description = std::pair<bool, std::string>;
    const auto describe_properties = [&ss](
                                         std::string_view format,
                                         const std::vector<property_description> &property_names,
    std::string_view if_empty = "" ) {
        std::string property_descriptions = enumerate_as_string( property_names.begin(),
        property_names.end(), []( const property_description & pd ) {
            return pd.first ? pd.second : "";
        } );
        if( !property_descriptions.empty() ) {
            ss += string_format( format, property_descriptions ) + "\n";
        } else if( !if_empty.empty() ) {
            ss += if_empty;
            ss += "\n";
        }
    };

    describe_flags( _( "It has the following senses: %s." ), {
        {m_flag::MF_HEARS, pgettext( "Hearing as sense", "hearing" )},
        {m_flag::MF_SEES, pgettext( "Sight as sense", "sight" )},
        {m_flag::MF_SMELLS, pgettext( "Smell as sense", "smell" )},
    }, _( "It doesn't have senses." ) );

    describe_flags( _( "It is immune to %s." ), {
        {m_flag::MF_FIREPROOF, pgettext( "Fire as immunity", "fire" )},
        {m_flag::MF_COLDPROOF, pgettext( "Cold as immunity", "cold" )},
        {m_flag::MF_ACIDPROOF, pgettext( "Acid as immunity", "acid" )},
        {m_flag::MF_STUN_IMMUNE, pgettext( "Stun as immunity", "stun" )},
        {m_flag::MF_SLUDGEPROOF, pgettext( "Sludge as immunity", "sludge" )},
        {m_flag::MF_BIOPROOF, pgettext( "Biological hazards as immunity", "biohazards" )},
        {m_flag::MF_DARKPROOF, pgettext( "Dark attacks as immunity", "dark" )},
        {m_flag::MF_LIGHTPROOF, pgettext( "Light attacks as immunity", "light" )},
        {m_flag::MF_PSIPROOF, pgettext( "Psionic attacks as immunity", "psi" )},
    } );

    describe_properties( _( "It can %s." ), {
        {swims(), pgettext( "Swim as an action", "swim" )},
        {flies(), pgettext( "Fly as an action", "fly" )},
        {can_dig(), pgettext( "Dig as an action", "dig" )},
        {climbs(), pgettext( "Climb as an action", "climb" )}
    } );

    describe_flags( _( "<bad>In fight it can %s.</bad>" ), {
        {m_flag::MF_GRABS, pgettext( "Grab as an action", "grab" )},
        {m_flag::MF_VENOM, pgettext( "Poison as an action", "poison" )},
        {m_flag::MF_PARALYZE, pgettext( "Paralyze as an action", "paralyze" )},
        {m_flag::MF_BLEED, _( "cause bleed" )}
    } );

    if( !type->has_flag( m_flag::MF_NOHEAD ) ) {
        ss += std::string( _( "It has a head." ) ) + "\n";
    }

    ss += "--\n";
    ss += std::string( _( "In melee, you can expect to:" ) ) + "\n";
    ss += string_format( _( "Deal average damage per second: <stat>%.1f</stat>" ),
                         g->u.primary_weapon().effective_dps( g->u, *this ) );
    ss += "\n";

    if( debug_mode ) {
        ss += string_format( _( "Current Speed: %1$d" ), get_speed() ) + "\n";
        ss += string_format( _( "Anger: %1$d" ), anger ) + "\n";
        if( !faction_anger.empty() ) {
            ss += string_format( _( "Anger by faction:" ) ) + "\n";
            for( const auto &[id, anger] : faction_anger ) {
                ss += string_format( _( "  %1$s: %2$d" ), id.id().str(), anger ) + "\n";
            }
        }
        ss += string_format( _( "Friendly: %1$d" ), friendly ) + "\n";
        ss += string_format( _( "Morale: %1$d" ), morale ) + "\n";

        const time_duration current_time = calendar::turn - calendar::turn_zero;
        ss += string_format( _( "Current Time: Turn %1$d | Day: %2$d" ),
                             to_turns<int>( current_time ),
                             to_days<int>( current_time ) ) + "\n";

        ss += string_format( _( "Upgrade Time: %1$d (turns left: %2$d) %3$s" ),
                             upgrade_time,
                             to_turns<int>( time_duration::from_days( upgrade_time ) - current_time ),
                             can_upgrade() ? "" : _( "<color_red>(can't upgrade)</color>" ) ) + "\n";

        if( baby_timer.has_value() ) {
            ss += string_format( _( "Reproduction time: %1$d (turns left: %2$d) %3$s" ),
                                 to_turn<int>( baby_timer.value() ),
                                 to_turn<int>( baby_timer.value() - current_time ),
                                 reproduces ? "" : _( "<color_red>(cannot reproduce)</color>" ) ) + "\n";
        }
    }

    return replace_colors( ss );
}

const std::string &monster::symbol() const
{
    return type->sym;
}

nc_color monster::basic_symbol_color() const
{
    return type->color;
}

nc_color monster::symbol_color() const
{
    return color_with_effects();
}

bool monster::is_symbol_highlighted() const
{
    return friendly != 0;
}

nc_color monster::color_with_effects() const
{
    nc_color ret = type->color;
    if( has_effect( effect_beartrap ) || has_effect( effect_stunned ) || has_effect( effect_downed ) ||
        has_effect( effect_tied ) ||
        has_effect( effect_lightsnare ) || has_effect( effect_heavysnare ) ) {
        ret = hilite( ret );
    }
    if( has_effect( effect_pacified ) ) {
        ret = invert_color( ret );
    }
    if( has_effect( effect_onfire ) ) {
        ret = red_background( ret );
    }
    return ret;
}

bool monster::avoid_trap( const tripoint & /* pos */, const trap &tr ) const
{
    // The trap position is not used, monsters are to stupid to remember traps. Actually, they do
    // not even see them.
    // Traps are on the ground, digging monsters go below, fliers and climbers go above.
    if( digging() || flies() ) {
        return true;
    }
    return dice( 3, type->sk_dodge + 1 ) >= dice( 3, tr.get_avoidance() );
}

bool monster::has_flag( const m_flag f ) const
{
    return type->has_flag( f );
}

bool monster::can_see() const
{
    return has_flag( MF_SEES ) && !effect_cache[VISION_IMPAIRED];
}

bool monster::can_hear() const
{
    return has_flag( MF_HEARS ) && !has_effect( effect_deaf );
}

bool monster::can_submerge() const
{
    return ( has_flag( MF_NO_BREATHE ) || swims() || has_flag( MF_AQUATIC ) ) &&
           !has_flag( MF_ELECTRONIC );
}

bool monster::can_drown() const
{
    return !swims() && !has_flag( MF_AQUATIC ) &&
           !has_flag( MF_NO_BREATHE ) && !flies();
}

bool monster::can_climb() const
{
    return climbs() || flies();
}

bool monster::digging() const
{
    return digs() || ( can_dig() && is_underwater() );
}

bool monster::can_dig() const
{
    return has_flag( MF_CAN_DIG );
}

bool monster::digs() const
{
    return has_flag( MF_DIGS );
}

bool monster::flies() const
{
    return has_flag( MF_FLIES );
}

bool monster::climbs() const
{
    return has_flag( MF_CLIMBS );
}

bool monster::swims() const
{
    return has_flag( MF_SWIMS );
}

bool monster::can_act() const
{
    return moves > 0 &&
           ( effects->empty() ||
             ( !has_effect( effect_stunned ) && !has_effect( effect_downed ) && !has_effect( effect_webbed ) ) );
}

int monster::sight_range( const int light_level ) const
{
    // Non-aquatic monsters can't see much when submerged
    if( !can_see() || effect_cache[VISION_IMPAIRED] ||
        ( is_underwater() && !swims() && !has_flag( MF_AQUATIC ) && !digging() ) ) {
        return 1;
    }
    static const int default_daylight = default_daylight_level();
    if( light_level == 0 ) {
        return type->vision_night;
    } else if( light_level == default_daylight ) {
        return type->vision_day;
    }
    int range = light_level * type->vision_day + ( default_daylight - light_level ) *
                type->vision_night;
    range /= default_daylight;

    return range;
}

bool monster::made_of( const material_id &m ) const
{
    return type->made_of( m );
}

bool monster::made_of_any( const std::set<material_id> &ms ) const
{
    return type->made_of_any( ms );
}

bool monster::made_of( phase_id p ) const
{
    return type->phase == p;
}

void monster::set_goal( const tripoint &p )
{
    const map &here = get_map();
    if( !here.inbounds( p ) ) {
        return;
    }

    if( p != this->goal && p != this->pos() ) {
        this->repath_requested = true;
    }
    goal = p;
}

void monster::shift( point sm_shift )
{
    const map &here = get_map();

    const point ms_shift = sm_to_ms_copy( sm_shift );
    position -= ms_shift;
    if( wandf > 0 ) {
        wander_pos -= ms_shift;
    }

    // Pathfinding shifts, we bypass `set_dest` to prevent repathing
    this->goal -= ms_shift;
    if( !here.inbounds( this->goal ) ) {
        // May accidentally occur during long-teleports
        this->goal = pos();
        this->path.clear();
        return;
    }

    // Shift our found paths too, but also validate them
    if( !this->path.empty() ) {
        for( tripoint &p : this->path ) {
            p -= ms_shift;

            if( !here.inbounds( p ) ) {
                // Path started going through OoB regions, so...
                this->path.clear();
                this->repath_requested = true;
                break;
            }
        }
    }
}

detached_ptr<item> monster::set_tack_item( detached_ptr<item> &&to )
{
    return tack_item.swap( std::move( to ) );
}

detached_ptr<item> monster::remove_tack_item()
{
    return set_tack_item( detached_ptr<item>() );
}

item *monster::get_tack_item() const
{
    if( tack_item ) {
        return &*tack_item;
    }
    return nullptr;
}

detached_ptr<item> monster::set_tied_item( detached_ptr<item> &&to )
{
    return tied_item.swap( std::move( to ) );
}

detached_ptr<item> monster::remove_tied_item()
{
    return set_tied_item( detached_ptr<item>() );
}

item *monster::get_tied_item() const
{
    if( tied_item ) {
        return &*tied_item;
    }
    return nullptr;
}

detached_ptr<item> monster::set_armor_item( detached_ptr<item> &&to )
{
    return armor_item.swap( std::move( to ) );
}

detached_ptr<item> monster::remove_armor_item()
{
    return set_armor_item( detached_ptr<item>() );
}

item *monster::get_armor_item() const
{
    if( armor_item ) {
        return &*armor_item;
    }
    return nullptr;
}

detached_ptr<item> monster::set_storage_item( detached_ptr<item> &&to )
{
    return storage_item.swap( std::move( to ) );
}

detached_ptr<item> monster::remove_storage_item()
{
    return set_storage_item( detached_ptr<item>() );
}

item *monster::get_storage_item() const
{
    if( storage_item ) {
        return &*storage_item;
    }
    return nullptr;
}

detached_ptr<item> monster::set_battery_item( detached_ptr<item> &&to )
{
    return battery_item.swap( std::move( to ) );
}

detached_ptr<item> monster::remove_battery_item()
{
    return set_battery_item( detached_ptr<item>() );
}

item *monster::get_battery_item() const
{
    if( battery_item ) {
        return &*battery_item;
    }
    return nullptr;
}

tripoint monster::move_target()
{
    return goal;
}

Creature *monster::attack_target()
{
    if( is_wandering() ) {
        return nullptr;
    }

    Creature *target = g->critter_at( move_target() );
    if( target == nullptr || target == this ||
        attitude_to( *target ) == Attitude::A_FRIENDLY || !sees( *target ) ) {
        return nullptr;
    }

    return target;
}

bool monster::is_fleeing( Character &who ) const
{
    if( effect_cache[FLEEING] ) {
        return true;
    }
    if( anger >= 100 || morale >= 100 ) {
        return false;
    }
    monster_attitude att = attitude( &who );
    return att == MATT_FLEE || ( att == MATT_FOLLOW && rl_dist( pos(), who.pos() ) <= 4 );
}

Attitude monster::attitude_to( const Creature &other ) const
{
    const monster *m = other.is_monster() ? static_cast< const monster *>( &other ) : nullptr;
    const player *p = other.as_player();
    if( m != nullptr ) {
        if( m == this ) {
            return Attitude::A_FRIENDLY;
        }
        // Ignore inactive mechs
        if( m->has_flag( MF_RIDEABLE_MECH ) && !m->has_effect( effect_ridden ) ) {
            return Attitude::A_NEUTRAL;
        }

        static const string_id<monfaction> faction_zombie( "zombie" );
        auto faction_att = faction.obj().attitude( m->faction );
        if( ( friendly != 0 && m->friendly != 0 ) ||
            ( friendly == 0 && m->friendly == 0 && faction_att == MFA_FRIENDLY ) ) {
            // Friendly (to player) monsters are friendly to each other
            // Unfriendly monsters go by faction attitude
            return Attitude::A_FRIENDLY;
        } else if( g->u.has_trait( trait_PROF_FERAL ) && ( faction == faction_zombie ||
                   type->in_species( ZOMBIE ) ) && ( m->faction == faction_zombie ||
                           m->type->in_species( ZOMBIE ) ) ) {
            // Zombies ignoring a feral survivor aren't quite the same as friendly
            // Ignore actually-friendly zombies/ferals but not other friendlies like reprogramed bots
            return Attitude::A_FRIENDLY;
        } else if( ( friendly == 0 && m->friendly == 0 && faction_att == MFA_HATE ) ) {
            // Stuff that hates a specific faction will always attack that faction
            return Attitude::A_HOSTILE;
        } else {
            int effective_anger = anger;
            if( has_flag( MF_FACTION_MEMORY ) ) {
                effective_anger = get_faction_anger( m->faction );
            }

            if( ( friendly == 0 && m->friendly == 0 && faction_att == MFA_NEUTRAL ) ||
                morale < 0 || effective_anger < 10 ) {
                // Stuff that won't attack is neutral to everything
                return Attitude::A_NEUTRAL;
            } else {
                return Attitude::A_HOSTILE;
            }
        }
    } else if( p != nullptr ) {
        switch( attitude( const_cast<player *>( p ) ) ) {
            case MATT_FRIEND:
            case MATT_ZLAVE:
            case MATT_FPASSIVE:
                return Attitude::A_FRIENDLY;
            case MATT_FLEE:
            case MATT_IGNORE:
            case MATT_FOLLOW:
                return Attitude::A_NEUTRAL;
            case MATT_ATTACK:
                return Attitude::A_HOSTILE;
            case MATT_NULL:
            case MATT_UNKNOWN:
            case NUM_MONSTER_ATTITUDES:
                break;
        }
    }
    // Should not happen!, creature should be either player or monster
    return Attitude::A_NEUTRAL;
}

template<>
std::string io::enum_to_string<monster_attitude>( monster_attitude att )
{
    switch( att ) {
        case MATT_NULL:
            return "MATT_NULL";
        case MATT_FRIEND:
            return "MATT_FRIEND";
        case MATT_FPASSIVE:
            return "MATT_FPASSIVE";
        case MATT_FLEE:
            return "MATT_FLEE";
        case MATT_IGNORE:
            return "MATT_IGNORE";
        case MATT_FOLLOW:
            return "MATT_FOLLOW";
        case MATT_ATTACK:
            return "MATT_ATTACK";
        case MATT_ZLAVE:
            return "MATT_ZLAVE";
        case MATT_UNKNOWN:
            return "MATT_UNKNOWN";
        case NUM_MONSTER_ATTITUDES:
            break;
    }
    debugmsg( "Invalid monster_attitude" );
    abort();
}

auto monster::attitude( const Character *u ) const -> monster_attitude
{
    if( friendly != 0 ) {
        if( has_effect( effect_docile ) ) {
            return MATT_FPASSIVE;
        }
        if( u == &g->u ) {
            return MATT_FRIEND;
        }
        // Zombies don't understand not attacking NPCs, but dogs and bots should.
        const npc *np = dynamic_cast< const npc * >( u );
        if( np != nullptr && !np->guaranteed_hostile() && !type->in_species( ZOMBIE ) ) {
            return MATT_FRIEND;
        }
        if( np != nullptr && np->is_hallucination() ) {
            return MATT_IGNORE;
        }
    }
    if( effect_cache[FLEEING] ) {
        return MATT_FLEE;
    }
    if( has_effect( effect_pacified ) ) {
        return MATT_ZLAVE;
    }

    int effective_anger  = anger;
    int effective_morale = morale;

    if( u != nullptr ) {
        if( has_flag( MF_FACTION_MEMORY ) ) {
            const monster *u_as_monster = u->as_monster();
            if( u_as_monster != nullptr ) {
                effective_anger = get_faction_anger( u_as_monster->faction );
            } else if( u->is_player() || u->is_npc() ) {
                effective_anger = get_faction_anger( mfaction_id( "player" ) );
            }
        }

        // Those are checked quite often, so avoiding string construction is a good idea
        static const string_id<monfaction> faction_bee( "bee" );
        if( faction == faction_bee ) {
            if( u->has_trait( trait_BEE ) ) {
                return MATT_FRIEND;
            } else if( u->has_trait( trait_FLOWERS ) ) {
                effective_anger -= 10;
            }
        }

        static const string_id<monfaction> faction_zombie( "zombie" );
        if( faction == faction_zombie || type->in_species( ZOMBIE ) ) {
            if( u->has_trait( trait_PROF_FERAL ) && !u->has_effect( effect_feral_infighting_punishment ) ) {
                return MATT_FRIEND;
            }
        }

        if( type->has_anger_trigger( mon_trigger::NETHER_ATTENTION ) &&
            u->has_effect( effect_attention ) ) {
            return MATT_ATTACK;
        }

        if( type->in_species( FUNGUS ) && ( u->has_trait( trait_THRESH_MYCUS ) ||
                                            u->has_trait( trait_MYCUS_FRIEND ) ) ) {
            return MATT_FRIEND;
        }

        if( effective_anger >= 10 &&
            ( ( type->in_species( MAMMAL ) && u->has_trait( trait_PHEROMONE_MAMMAL ) ) ||
              ( type->in_species( INSECT ) && u->has_trait( trait_PHEROMONE_INSECT ) ) ) ) {
            effective_anger -= 20;
        }

        if( u->has_trait( trait_TERRIFYING ) ) {
            effective_morale -= 10;
        }

        if( has_flag( MF_ANIMAL ) ) {
            if( u->has_trait( trait_PROF_FERAL ) ) {
                // We want all wildlife to amp their fight-or-flight response up to eleven, so anger adjustments in general won't cut it.
                if( effective_anger >= -10 ) {
                    return MATT_ATTACK;
                } else {
                    return MATT_FLEE;
                }
            } else if( u->has_trait( trait_ANIMALEMPATH ) ) {
                effective_anger -= 10;
                if( effective_anger < 10 ) {
                    effective_morale += 55;
                }
            } else if( u->has_trait( trait_ANIMALEMPATH2 ) ) {
                effective_anger -= 20;
                if( effective_anger < 20 ) {
                    effective_morale += 80;
                }
            } else if( u->has_trait( trait_ANIMALDISCORD ) ) {
                if( effective_anger >= 10 ) {
                    effective_anger += 10;
                }
                if( effective_anger < 10 ) {
                    effective_morale -= 5;
                }
            } else if( u->has_trait( trait_ANIMALDISCORD2 ) ) {
                if( effective_anger >= 20 ) {
                    effective_anger += 20;
                }
                if( effective_anger < 20 ) {
                    effective_morale -= 5;
                }
            }
        }

        for( const trait_id &mut : u->get_mutations() ) {
            for( const std::pair<const species_id, int> &elem : mut.obj().anger_relations ) {
                if( type->in_species( elem.first ) ) {
                    effective_anger += elem.second;
                }
            }
        }

        for( const trait_id &mut : u->get_mutations() ) {
            for( const species_id &spe : mut.obj().ignored_by ) {
                if( type->in_species( spe ) ) {
                    return MATT_IGNORE;
                }
            }
        }
    }


    if( effective_morale < 0 ) {
        if( effective_morale + effective_anger > 0 && get_hp() > get_hp_max() / 3 ) {
            return MATT_FOLLOW;
        }
        return MATT_FLEE;
    }
    if( effective_anger <= 0 ) {
        if( get_hp() <= 0.6 * get_hp_max() ) {
            return MATT_FLEE;
        } else {
            return MATT_IGNORE;
        }
    }

    if( effective_anger < 10 ) {
        return MATT_FOLLOW;
    }

    if( u != nullptr && !aggro_character && !u->is_monster() ) {
        return MATT_IGNORE;
    }

    return MATT_ATTACK;
}

int monster::hp_percentage() const
{
    return get_hp( bodypart_id( "torso" ) ) * 100 / get_hp_max();
}

void monster::process_triggers()
{
    process_trigger( mon_trigger::STALK, [this]() {
        return anger > 0 && one_in( 5 ) ? 1 : 0;
    } );

    process_trigger( mon_trigger::FIRE, [this]() {
        int ret = 0;
        for( const auto &p : g->m.points_in_radius( pos(), 3 ) ) {
            ret += 5 * g->m.get_field_intensity( p, fd_fire );
        }
        return ret;
    } );

    // Meat checking is disabled as for now.
    // It's hard to ever see it in action
    // and even harder to balance it without making it exploitable

    if( morale != type->morale && one_in( 10 ) ) {
        if( morale < type->morale ) {
            morale++;
        } else {
            morale--;
        }
    }

    // Don't restore global anger for FACTION_MEMORY monsters - they only use faction-specific anger
    if( !has_flag( MF_FACTION_MEMORY ) && anger != type->agro && one_in( 10 ) ) {
        if( anger < type->agro ) {
            anger++;
        } else {
            anger--;
        }
    }

    // If we got angry at characters have a chance at calming down
    if( anger == type->agro && aggro_character && !type->aggro_character && !x_in_y( anger, 100 ) ) {
        add_msg( m_debug, "%s's character aggro reset", get_name() );
        aggro_character = false;
    }

    // Cap values at [-100, 100] to prevent perma-angry moose etc.
    morale = std::min( 100, std::max( -100, morale ) );
    anger  = std::min( 100, std::max( -100, anger ) );
}

// This adjusts anger/morale levels given a single trigger.
void monster::process_trigger( mon_trigger trig, int amount )
{
    process_trigger( trig, amount, mfaction_id() );
}

void monster::process_trigger( mon_trigger trig,
                               const std::function < auto() -> int > &amount_func )
{
    process_trigger( trig, amount_func, mfaction_id() );
}

void monster::process_trigger( mon_trigger trig, int amount, mfaction_id target_faction )
{
    if( type->has_anger_trigger( trig ) ) {
        if( has_flag( MF_FACTION_MEMORY ) && target_faction.is_valid() ) {
            add_faction_anger( target_faction, amount );
        } else {
            anger += amount;
        }
    }
    if( type->has_fear_trigger( trig ) ) {
        morale -= amount;
    }
    if( type->has_placate_trigger( trig ) ) {
        if( has_flag( MF_FACTION_MEMORY ) && target_faction.is_valid() ) {
            add_faction_anger( target_faction, -amount );
        } else {
            anger -= amount;
        }
    }
}

void monster::process_trigger( mon_trigger trig, const std::function < auto() -> int > &amount_func,
                               mfaction_id target_faction )
{
    if( type->has_anger_trigger( trig ) ) {
        if( has_flag( MF_FACTION_MEMORY ) && target_faction.is_valid() ) {
            add_faction_anger( target_faction, amount_func() );
        } else {
            anger += amount_func();
        }
    }
    if( type->has_fear_trigger( trig ) ) {
        morale -= amount_func();
    }
    if( type->has_placate_trigger( trig ) ) {
        if( has_flag( MF_FACTION_MEMORY ) && target_faction.is_valid() ) {
            add_faction_anger( target_faction, -amount_func() );
        } else {
            anger -= amount_func();
        }
    }
}


// hopefully a good spot for these functions
// eg. reason = "mating season"
void monster::trigger_character_aggro( const char *reason )
{
    add_msg( m_debug, "%s's character aggro is triggered by %s", get_name(), reason );
    aggro_character = true;
}

void monster::trigger_character_aggro_chance( int chance, const char *reason )
{
    if( x_in_y( chance, 100 ) ) {
        trigger_character_aggro( reason );
    }
}

bool monster::is_underwater() const
{
    return Creature::is_underwater() && can_submerge();
}

bool monster::is_on_ground() const
{
    // TODO: actually make this work
    return false;
}

bool monster::has_weapon() const
{
    return false; // monsters will never have weapons, silly
}

bool monster::is_warm() const
{
    return has_flag( MF_WARM );
}

bool monster::in_species( const species_id &spec ) const
{
    return type->in_species( spec );
}

bool monster::is_elec_immune() const
{
    return is_immune_damage( DT_ELECTRIC );
}

bool monster::is_immune_effect( const efftype_id &effect ) const
{
    if( effect == effect_onfire ) {
        return is_immune_damage( DT_HEAT ) ||
               made_of( LIQUID ) ||
               has_flag( MF_FIREY );
    }

    if( effect == effect_corroding ) {
        return is_immune_damage( DT_ACID );
    }

    if( effect == effect_bleed ) {
        return !has_flag( MF_WARM ) ||
               !made_of( material_id( "flesh" ) );
    }

    if( effect == effect_paralyzepoison ||
        effect == effect_badpoison ||
        effect == effect_poison ) {
        return !has_flag( MF_WARM ) ||
               ( !made_of( material_id( "flesh" ) ) && !made_of( material_id( "iflesh" ) ) );
    }

    if( effect == effect_tpollen ) {
        return type->in_species( PLANT );
    }

    // Used by screecher zombies to prevent dazing monsters that can't hear
    if( effect == effect_deaf ) {
        return !has_flag( MF_HEARS );
    }

    if( effect == effect_stunned || effect == effect_dazed ) {
        return has_flag( MF_STUN_IMMUNE );
    }

    if( effect == effect_smoke ||
        effect == effect_teargas ||
        effect == effect_migo_atmosphere ) {
        return has_flag( MF_NO_BREATHE );
    }

    if( effect == effect_bleed ) {
        return !made_of( material_id( "flesh" ) ) && !made_of( material_id( "iflesh" ) );
    }

    return false;
}

bool monster::is_immune_damage( const damage_type dt ) const
{
    switch( dt ) {
        case DT_NULL:
            return true;
        case DT_TRUE:
            return false;
        case DT_BIOLOGICAL:
            return has_flag( MF_BIOPROOF );
        case DT_BASH:
            return false;
        case DT_CUT:
            return false;
        case DT_ACID:
            return has_flag( MF_ACIDPROOF );
        case DT_STAB:
            return false;
        case DT_HEAT:
            return has_flag( MF_FIREPROOF );
        case DT_COLD:
            return has_flag( MF_COLDPROOF );
        case DT_DARK:
            return has_flag( MF_DARKPROOF );
        case DT_LIGHT:
            return has_flag( MF_LIGHTPROOF );
        case DT_PSI:
            return has_flag( MF_PSIPROOF );
        case DT_ELECTRIC:
            return type->sp_defense == &mdefense::zapback ||
                   has_flag( MF_ELECTRIC ) ||
                   has_flag( MF_ELECTRIC_FIELD );
        case DT_BULLET:
            return false;
        default:
            return true;
    }
}

bool monster::is_dead_state() const
{
    return hp <= 0;
}

bool monster::block_hit( Creature *, bodypart_id &, damage_instance & )
{
    return false;
}

bool monster::block_ranged_hit( Creature *, bodypart_id &, damage_instance & )
{
    return false;
}

void monster::absorb_hit( const bodypart_id &, damage_instance &dam )
{
    resistances res = resists();
    for( auto &elem : dam.damage_units ) {
        add_msg( m_debug, "Dam Type: %s :: Ar Pen: %.1f :: Armor Mult: %.1f",
                 name_by_dt( elem.type ), elem.res_pen, elem.res_mult );
        elem.amount -= std::min( res.get_effective_resist( elem ), elem.amount );
    }
}

resistances monster::resists() const
{
    resistances res;
    // TODO: Get armor resists once instead of recalculating per-type
    res.set_resist( DT_BASH, type->armor_bash + armor_bash_bonus + get_worn_armor_val( DT_BASH ) );
    res.set_resist( DT_CUT, type->armor_cut + armor_cut_bonus + get_worn_armor_val( DT_CUT ) );
    res.set_resist( DT_STAB, type->armor_stab + get_worn_armor_val( DT_STAB ) );
    res.set_resist( DT_BULLET, type->armor_bullet + armor_bullet_bonus + get_worn_armor_val(
                        DT_BULLET ) );
    res.set_resist( DT_ACID, type->armor_acid + get_worn_armor_val( DT_ACID ) );
    res.set_resist( DT_HEAT, type->armor_fire + get_worn_armor_val( DT_HEAT ) );
    res.set_resist( DT_COLD, type->armor_cold + get_worn_armor_val( DT_COLD ) );
    res.set_resist( DT_DARK, type->armor_dark + get_worn_armor_val( DT_DARK ) );
    res.set_resist( DT_LIGHT, type->armor_light + get_worn_armor_val( DT_LIGHT ) );
    res.set_resist( DT_PSI, type->armor_psi + get_worn_armor_val( DT_PSI ) );
    res.set_resist( DT_ELECTRIC, type->armor_electric + get_worn_armor_val( DT_ELECTRIC ) );
    return res;
}

void monster::melee_attack( Creature &target )
{
    melee_attack( target, get_hit() );
}

void monster::melee_attack( Creature &target, float accuracy )
{
    mod_moves( -type->attack_cost );
    if( type->melee_dice == 0 ) {
        // We don't attack, so just return
        return;
    }

    if( this == &target ) {
        // This happens sometimes
        return;
    }

    if( !can_squeeze_to( target.pos() ) ) {
        return;
    }

    int hitspread = target.deal_melee_attack( this, melee::melee_hit_range( accuracy ) );
    const bool attack_success = hitspread >= 0;

    if( target.is_player() ||
        ( target.is_npc() && g->u.attitude_to( target ) == Attitude::A_FRIENDLY ) ) {
        // Make us a valid target
        add_effect( effect_hit_by_player, 10_minutes );
    }

    if( has_flag( MF_HIT_AND_RUN ) ) {
        add_effect( effect_run, 4_turns );
    }

    const bool u_see_me = g->u.sees( *this );

    damage_instance damage = !is_hallucination() ? type->melee_damage : damage_instance();
    if( !is_hallucination() && type->melee_dice > 0 ) {
        damage.add_damage( DT_BASH, dice( type->melee_dice, type->melee_sides ) );
        damage.add_damage( DT_BASH, bash_bonus );
        damage.add_damage( DT_CUT, cut_bonus );
    }

    dealt_damage_instance dealt_dam;

    if( attack_success ) {
        target.deal_melee_hit( this, hitspread, false, damage, dealt_dam );
    }
    const bodypart_str_id bp_hit = dealt_dam.bp_hit;

    const int total_dealt = dealt_dam.total_damage();
    if( !attack_success ) {
        // Miss
        if( u_see_me && !target.in_sleep_state() ) {
            if( target.is_player() ) {
                add_msg( _( "You dodge %s." ), disp_name() );
            } else if( target.is_npc() ) {
                add_msg( _( "%1$s dodges %2$s attack." ),
                         target.disp_name(), disp_name( true ) );
            } else {
                add_msg( _( "%1$s misses %2$s!" ),
                         disp_name( false, true ), target.disp_name() );
            }
        } else if( target.is_player() ) {
            add_msg( _( "You dodge an attack from an unseen source." ) );
        }
    } else if( is_hallucination() || total_dealt > 0 ) {
        // Hallucinations always produce messages but never actually deal damage
        if( u_see_me ) {
            if( target.is_player() ) {
                sfx::play_variant_sound( "melee_attack", "monster_melee_hit",
                                         sfx::get_heard_volume( target.pos() ) );
                sfx::do_player_death_hurt( dynamic_cast<player &>( target ), false );
                //~ 1$s is attacker name, 2$s is bodypart name in accusative.
                add_msg( m_bad, _( "%1$s hits your %2$s." ), disp_name( false, true ),
                         bp_hit->accusative.translated() );
            } else if( target.is_npc() ) {
                if( has_effect( effect_ridden ) && has_flag( MF_RIDEABLE_MECH ) && pos() == g->u.pos() ) {
                    //~ %1$s: name of your mount, %2$s: target NPC name, %3$d: damage value
                    add_msg( m_good, _( "Your %1$s hits %2$s for %3$d damage!" ), name(), target.disp_name(),
                             total_dealt );
                } else {
                    //~ %1$s: attacker name, %2$s: target NPC name, %3$s: bodypart name in accusative
                    add_msg( _( "%1$s hits %2$s %3$s." ), disp_name( false, true ),
                             target.disp_name( true ),
                             bp_hit->accusative.translated() );
                }
            } else {
                if( has_effect( effect_ridden ) && has_flag( MF_RIDEABLE_MECH ) && pos() == g->u.pos() ) {
                    //~ %1$s: name of your mount, %2$s: target creature name, %3$d: damage value
                    add_msg( m_good, _( "Your %1$s hits %2$s for %3$d damage!" ), get_name(), target.disp_name(),
                             total_dealt );
                } else {
                    //~ %1$s: attacker name, %2$s: target creature name
                    add_msg( _( "%1$s hits %2$s!" ), disp_name( false, true ), target.disp_name() );
                }
            }
        } else if( target.is_player() ) {
            //~ %s is bodypart name in accusative.
            add_msg( m_bad, _( "Something hits your %s." ),
                     bp_hit->accusative.translated() );
        }
    } else {
        // No damage dealt
        if( u_see_me ) {
            if( target.is_player() ) {
                //~ 1$s is attacker name, 2$s is bodypart name in accusative, 3$s is armor name
                add_msg( _( "%1$s hits your %2$s, but your %3$s protects you." ), disp_name( false, true ),
                         bp_hit->accusative.translated(), target.skin_name() );
            } else if( target.is_npc() ) {
                //~ $1s is monster name, %2$s is that monster target name,
                //~ $3s is target bodypart name in accusative, $4s is the monster target name,
                //~ 5$s is target armor name.
                add_msg( _( "%1$s hits %2$s %3$s but is stopped by %4$s %5$s." ), disp_name( false, true ),
                         target.disp_name( true ),
                         bp_hit->accusative.translated(),
                         target.disp_name( true ),
                         target.skin_name() );
            } else {
                //~ $1s is monster name, %2$s is that monster target name,
                //~ $3s is target armor name.
                add_msg( _( "%1$s hits %2$s but is stopped by its %3$s." ),
                         disp_name( false, true ),
                         target.disp_name(),
                         target.skin_name() );
            }
        } else if( target.is_player() ) {
            //~ 1$s is bodypart name in accusative, 2$s is armor name.
            add_msg( _( "Something hits your %1$s, but your %2$s protects you." ),
                     bp_hit->accusative.translated(), target.skin_name() );
        }
    }

    target.check_dead_state();

    cata::run_hooks( "on_creature_melee_attacked", [ &, this]( auto & params ) {
        params["char"] = this;
        params["target"] = &target;
        params["success"] = attack_success;
    } );

    if( is_hallucination() ) {
        if( one_in( 7 ) ) {
            die( nullptr );
        }
        return;
    }

    if( total_dealt <= 0 ) {
        return;
    }

    // Add any on damage effects
    for( const auto &eff : type->atk_effs ) {
        if( x_in_y( eff.chance, 100 ) ) {
            const bodypart_str_id &affected_bp = eff.affect_hit_bp ? bp_hit : convert_bp( eff.bp );
            target.add_effect( eff.id, time_duration::from_turns( eff.duration ), affected_bp );
            if( eff.permanent ) {
                target.get_effect( eff.id, affected_bp ).set_permanent();
            }
        }
    }

    const int stab_cut = dealt_dam.type_damage( DT_CUT ) + dealt_dam.type_damage( DT_STAB );

    if( stab_cut > 0 && has_flag( MF_VENOM ) ) {
        target.add_msg_if_player( m_bad, _( "You're envenomed!" ) );
        target.add_effect( effect_poison, 3_minutes );
    }

    if( stab_cut > 0 && has_flag( MF_BADVENOM ) ) {
        target.add_msg_if_player( m_bad,
                                  _( "You feel venom flood your body, wracking you with pain…" ) );
        target.add_effect( effect_badpoison, 4_minutes );
    }

    if( stab_cut > 0 && has_flag( MF_PARALYZE ) ) {
        target.add_msg_if_player( m_bad, _( "You feel venom enter your body!" ) );
        target.add_effect( effect_paralyzepoison, 10_minutes );
    }

    if( total_dealt > 6 && stab_cut > 0 && has_flag( MF_BLEED ) ) {
        // Maybe should only be if DT_CUT > 6... Balance question
        target.add_effect( effect_bleed, 6_minutes, bp_hit );
    }
}

void monster::deal_projectile_attack( Creature *source, dealt_projectile_attack &attack )
{
    this->deal_projectile_attack( source, nullptr, attack, false );
}

void monster::deal_projectile_attack( Creature *source, item *source_weapon,
                                      dealt_projectile_attack &attack )
{
    this->deal_projectile_attack( source, source_weapon,  attack, false );
}

void monster::deal_projectile_attack( Creature *source, item *source_weapon,
                                      dealt_projectile_attack &attack, bool manual_retaliation )
{
    const auto &proj = attack.proj;
    double &missed_by = attack.missed_by; // We can change this here

    // Whip has a chance to scare wildlife even if it misses
    if( proj.has_effect( ammo_effect_WHIP ) && type->in_category( "WILDLIFE" ) && one_in( 3 ) ) {
        add_effect( effect_run, rng( 3_turns, 5_turns ) );
    }

    if( missed_by > 1.0 ) {
        // Total miss
        return;
    }

    // Handled in creature::deal_projectile_attack now, so that not having a head does not make it somehow less likely to hit the torso.
    //// No head = immune to ranged crits
    //if( missed_by < accuracy_critical && has_flag( MF_NOHEAD ) ) {
    //    missed_by = accuracy_critical;
    //}

    Creature::deal_projectile_attack( source, source_weapon, attack );

    if( !is_hallucination() && attack.hit_critter == this ) {
        // Maybe TODO: Get difficulty from projectile speed/size/missed_by
        on_hit( source, bodypart_id( "torso" ), &attack, manual_retaliation );
    }
}

int monster::heal( const int delta_hp, bool overheal )
{
    const int maxhp = type->hp;
    if( delta_hp <= 0 || ( hp >= maxhp && !overheal ) ) {
        return 0;
    }

    const int old_hp = hp;
    hp += delta_hp;
    if( hp > maxhp && !overheal ) {
        hp = maxhp;
    }
    return hp - old_hp;
}

void monster::set_hp( const int hp )
{
    this->hp = hp;
}

void monster::apply_damage( Creature *source, item *source_weapon, item *source_projectile,
                            bodypart_id /*bp*/,
                            int dam,
                            const bool /*bypass_med*/ )
{
    if( is_dead_state() ) {
        return;
    }
    hp -= dam;
    if( hp < 1 ) {
        set_killer( source );
        if( source_weapon ) {
            source_weapon->add_monster_kill( type->id );
        }
        if( source_projectile ) {
            source_projectile->add_monster_kill( type->id );
        }
    } else if( dam > 0 ) {
        mfaction_id attacker_faction;
        if( source != nullptr ) {
            const monster *source_monster = source->as_monster();
            if( source_monster != nullptr ) {
                attacker_faction = source_monster->faction;
            } else if( ( source->is_player() || source->is_npc() ) && !source->is_fake() ) {
                // Only attribute to player faction if it's a real player/NPC, not a fake NPC from a monster turret
                attacker_faction = mfaction_id( "player" );
            }
        }

        process_trigger( mon_trigger::HURT, 1 + ( dam / 3 ), attacker_faction );

        if( source != nullptr && !aggro_character && !source->is_monster() && !source->is_fake() ) {
            trigger_character_aggro( "hurt" );
        }
    }
}
void monster::apply_damage( Creature *source, item *source_weapon, bodypart_id bp, int dam,
                            const bool bypass_med )
{
    apply_damage( source, source_weapon, nullptr, bp, dam, bypass_med );
}
void monster::apply_damage( Creature *source, bodypart_id bp, int dam,
                            const bool bypass_med )
{
    apply_damage( source, nullptr, nullptr, bp, dam, bypass_med );
}

void monster::die_in_explosion( Creature *source )
{
    hp = -9999; // huge to trigger explosion and prevent corpse item
    die( source );
}

bool monster::movement_impaired()
{
    return effect_cache[MOVEMENT_IMPAIRED];
}

bool monster::move_effects( bool )
{
    // This function is relatively expensive, we want that cached
    // IMPORTANT: If adding any new effects here, make SURE to
    // add them to hardcoded_movement_impairing in effect.cpp
    if( !effect_cache[MOVEMENT_IMPAIRED] ) {
        return true;
    }

    bool u_see_me = g->u.sees( *this );
    if( has_effect( effect_tied ) ) {
        // friendly pet, will stay tied down and obey.
        if( friendly == -1 ) {
            return false;
        }
        // non-friendly monster will struggle to get free occasionally.
        // some monsters can't be tangled up with a net/bolas/lasso etc.
        bool immediate_break = type->in_species( FISH ) || type->in_species( MOLLUSK ) ||
                               type->in_species( ROBOT ) || type->bodytype == "snake" || type->bodytype == "blob";
        if( !immediate_break && rng( 0, 900 ) > type->melee_dice * type->melee_sides * 1.5 ) {
            if( u_see_me ) {
                add_msg( _( "The %s struggles to break free of its bonds." ), name() );
            }
        } else if( immediate_break ) {
            remove_effect( effect_tied );
            if( tied_item ) {
                if( u_see_me ) {
                    add_msg( _( "The %s easily slips out of its bonds." ), name() );
                }
                g->m.add_item_or_charges( pos(), remove_tied_item() );
            }
        } else {
            if( tied_item ) {
                item *it = get_tied_item();

                const bool broken = rng( type->melee_dice * type->melee_sides, std::min( 10000,
                                         type->melee_dice * type->melee_sides * 250 ) ) > 800;
                if( !broken ) {
                    g->m.add_item_or_charges( pos(), it->detach() );
                } else {
                    it->detach();
                }
                if( u_see_me ) {
                    if( broken ) {
                        add_msg( _( "The %s snaps the bindings holding it down." ), name() );
                    } else {
                        add_msg( _( "The %s breaks free of the bindings holding it down." ), name() );
                    }
                }
            }
            remove_effect( effect_tied );
        }
        return false;
    }
    if( has_effect( effect_downed ) ) {
        if( rng( 0, 40 ) > type->melee_dice * type->melee_sides * 1.5 ) {
            if( u_see_me ) {
                add_msg( _( "The %s struggles to stand." ), name() );
            }
        } else {
            if( u_see_me ) {
                add_msg( _( "The %s climbs to its feet!" ), name() );
            }
            remove_effect( effect_downed );
        }
        return false;
    }
    if( has_effect( effect_webbed ) ) {
        if( x_in_y( type->melee_dice * type->melee_sides, 6 * get_effect_int( effect_webbed ) ) ) {
            if( u_see_me ) {
                add_msg( _( "The %s breaks free of the webs!" ), name() );
            }
            remove_effect( effect_webbed );
        }
        return false;
    }
    if( has_effect( effect_lightsnare ) ) {
        if( x_in_y( type->melee_dice * type->melee_sides, 12 ) ) {
            remove_effect( effect_lightsnare );
            g->m.spawn_item( pos(), "string_36" );
            g->m.spawn_item( pos(), "snare_trigger" );
            if( u_see_me ) {
                add_msg( _( "The %s escapes the light snare!" ), name() );
            }
        }
        return false;
    }
    if( has_effect( effect_heavysnare ) ) {
        if( type->melee_dice * type->melee_sides >= 7 ) {
            if( x_in_y( type->melee_dice * type->melee_sides, 32 ) ) {
                remove_effect( effect_heavysnare );
                g->m.spawn_item( pos(), "rope_6" );
                g->m.spawn_item( pos(), "snare_trigger" );
                if( u_see_me ) {
                    add_msg( _( "The %s escapes the heavy snare!" ), name() );
                }
            }
        }
        return false;
    }
    if( has_effect( effect_beartrap ) ) {
        if( type->melee_dice * type->melee_sides >= 18 ) {
            if( x_in_y( type->melee_dice * type->melee_sides, 200 ) ) {
                remove_effect( effect_beartrap );
                g->m.spawn_item( pos(), "beartrap" );
                if( u_see_me ) {
                    add_msg( _( "The %s escapes the bear trap!" ), name() );
                }
            }
        }
        return false;
    }
    if( has_effect( effect_crushed ) ) {
        if( x_in_y( type->melee_dice * type->melee_sides, 100 ) ) {
            remove_effect( effect_crushed );
            if( u_see_me ) {
                add_msg( _( "The %s frees itself from the rubble!" ), name() );
            }
        }
        return false;
    }

    // If we ever get more effects that force movement on success this will need to be reworked to
    // only trigger success effects if /all/ rolls succeed
    if( has_effect( effect_in_pit ) ) {
        if( rng( 0, 40 ) > type->melee_dice * type->melee_sides ) {
            return false;
        } else {
            if( u_see_me ) {
                add_msg( _( "The %s escapes the pit!" ), name() );
            }
            remove_effect( effect_in_pit );
        }
    }
    if( has_effect( effect_grabbed ) ) {
        if( dice( type->melee_dice + type->melee_sides, 3 ) < get_effect_int( effect_grabbed ) ||
            !one_in( 4 ) ) {
            return false;
        } else {
            if( u_see_me ) {
                add_msg( _( "The %s breaks free from the grab!" ), name() );
            }
            remove_effect( effect_grabbed );
        }
    }
    return true;
}

void monster::add_effect( const efftype_id &eff_id, const time_duration &dur )
{
    Creature::add_effect( eff_id, dur, bodypart_str_id::NULL_ID() );
}

void monster::add_effect( const efftype_id &eff_id, const time_duration &dur,
                          const bodypart_str_id &,
                          int intensity, bool force, bool deferred )
{
    // Effects are not applied to specific monster body part
    Creature::add_effect( eff_id, dur, bodypart_str_id::NULL_ID(), intensity, force, deferred );
}

std::string monster::get_effect_status() const
{
    std::vector<std::string> effect_status;
    for( auto &elem : *effects ) {
        for( auto &_it : elem.second ) {
            if( !_it.second.is_removed() && elem.first->is_show_in_info() ) {
                effect_status.push_back( _it.second.disp_name() );
            }
        }
    }

    return enumerate_as_string( effect_status );
}

int monster::get_worn_armor_val( damage_type dt ) const
{
    if( !has_effect( effect_monster_armor ) ) {
        return 0;
    }
    if( armor_item ) {
        return armor_item->damage_resist( dt );
    }
    return 0;
}

int monster::get_armor_cut( bodypart_id bp ) const
{
    ( void ) bp;
    // TODO: Add support for worn armor?
    return static_cast<int>( type->armor_cut ) + armor_cut_bonus + get_worn_armor_val( DT_CUT );
}

int monster::get_armor_bash( bodypart_id bp ) const
{
    ( void ) bp;
    return static_cast<int>( type->armor_bash ) + armor_bash_bonus + get_worn_armor_val( DT_BASH );
}

int monster::get_armor_bullet( bodypart_id bp ) const
{
    ( void ) bp;
    return static_cast<int>( type->armor_bullet ) + armor_bullet_bonus + get_worn_armor_val(
               DT_BULLET );
}

int monster::get_armor_type( damage_type dt, bodypart_id bp ) const
{
    int worn_armor = get_worn_armor_val( dt );

    switch( dt ) {
        case DT_TRUE:
        case DT_BIOLOGICAL:
            return 0;
        case DT_BASH:
            return get_armor_bash( bp );
        case DT_CUT:
            return get_armor_cut( bp );
        case DT_BULLET:
            return get_armor_bullet( bp );
        case DT_ACID:
            return worn_armor + static_cast<int>( type->armor_acid );
        case DT_STAB:
            return worn_armor + static_cast<int>( type->armor_stab ) + armor_cut_bonus * 0.8f;
        case DT_HEAT:
            return worn_armor + static_cast<int>( type->armor_fire );
        case DT_COLD:
            return worn_armor + static_cast<int>( type->armor_cold );
        case DT_DARK:
            return worn_armor + static_cast<int>( type->armor_dark );
        case DT_LIGHT:
            return worn_armor + static_cast<int>( type->armor_light );
        case DT_PSI:
            return worn_armor + static_cast<int>( type->armor_psi );
        case DT_ELECTRIC:
            return worn_armor + static_cast<int>( type->armor_electric );
        case DT_NULL:
        case NUM_DT:
            // Let it error below
            break;
    }

    debugmsg( "Invalid damage type: %d", dt );
    return 0;
}

float monster::get_hit_base() const
{
    return type->melee_skill;
}

float monster::get_dodge_base() const
{
    return type->sk_dodge;
}

float monster::hit_roll() const
{
    float hit = get_hit();
    if( has_effect( effect_bouldering ) ) {
        hit /= 4;
    }

    return melee::melee_hit_range( hit );
}

bool monster::has_grab_break_tec() const
{
    return false;
}

float monster::stability_roll() const
{
    int size_bonus = 0;
    switch( type->size ) {
        case creature_size::tiny:
            size_bonus -= 7;
            break;
        case creature_size::small:
            size_bonus -= 3;
            break;
        case creature_size::large:
            size_bonus += 5;
            break;
        case creature_size::huge:
            size_bonus += 10;
            break;
        default:
            break; // keep default
    }

    int stability = dice( type->melee_sides, type->melee_dice ) + size_bonus;
    if( has_effect( effect_stunned ) ) {
        stability -= rng( 1, 5 );
    }
    return stability;
}

float monster::get_dodge() const
{
    if( has_effect( effect_downed ) ) {
        return 0.0f;
    }

    float ret = Creature::get_dodge();
    if( has_effect( effect_lightsnare ) || has_effect( effect_heavysnare ) ||
        has_effect( effect_beartrap ) || has_effect( effect_tied ) ) {
        ret /= 2;
    }

    if( has_effect( effect_bouldering ) ) {
        ret /= 4;
    }

    return ret;
}

float monster::get_melee() const
{
    return type->melee_skill;
}

float monster::dodge_roll()
{
    return get_dodge() * 5;
}

int monster::get_grab_strength() const
{
    return type->grab_strength;
}

float monster::fall_damage_mod() const
{
    if( flies() ) {
        return 0.0f;
    }

    switch( type->size ) {
        case creature_size::tiny:
            return 0.2f;
        case creature_size::small:
            return 0.6f;
        case creature_size::medium:
            return 1.0f;
        case creature_size::large:
            return 1.4f;
        case creature_size::huge:
            return 2.0f;
        default:
            return 1.0f;
    }

    return 0.0f;
}

int monster::impact( const int force, const tripoint &p )
{
    if( force <= 0 ) {
        return force;
    }

    const float mod = fall_damage_mod();
    int total_dealt = 0;
    if( g->m.has_flag( TFLAG_SHARP, p ) ) {
        const int cut_damage = std::max( 0.0f, 10 * mod - get_armor_cut( bodypart_id( "torso" ) ) );
        apply_damage( nullptr, bodypart_id( "torso" ), cut_damage );
        total_dealt += 10 * mod;
    }

    const int bash_damage = std::max( 0.0f, force * mod - get_armor_bash( bodypart_id( "torso" ) ) );
    apply_damage( nullptr, bodypart_id( "torso" ), bash_damage );
    total_dealt += force * mod;

    add_effect( effect_downed, time_duration::from_turns( rng( 0, mod * 3 + 1 ) ) );

    return total_dealt;
}

void monster::reset_bonuses()
{
    effect_cache.reset();

    Creature::reset_bonuses();
}

void monster::reset_stats()
{
    // Nothing here yet
}

void monster::reset_special( const std::string &special_name )
{
    const auto iter = type->special_attacks.find( special_name );
    if( iter != type->special_attacks.end() ) {
        set_special( special_name, iter->second->cooldown );
    }
}

void monster::reset_special_rng( const std::string &special_name )
{
    const auto iter = type->special_attacks.find( special_name );
    if( iter != type->special_attacks.end() ) {
        set_special( special_name, rng( 0, iter->second->cooldown ) );
    }
}

void monster::set_special( const std::string &special_name, int time )
{
    const auto iter = special_attacks.find( special_name );
    if( iter != special_attacks.end() ) {
        iter->second.cooldown = time;
    } else {
        debugmsg( "%s has no special attack %s", disp_name(), special_name );
    }
}

void monster::disable_special( const std::string &special_name )
{
    const auto iter = special_attacks.find( special_name );
    if( iter != special_attacks.end() ) {
        iter->second.enabled = false;
    } else {
        debugmsg( "%s has no special attack %s", disp_name(), special_name );
    }
}

int monster::shortest_special_cooldown() const
{
    int countdown = std::numeric_limits<int>::max();
    for( const std::pair<const std::string, mon_special_attack> &sp_type : special_attacks ) {
        const mon_special_attack &local_attack_data = sp_type.second;
        if( !local_attack_data.enabled ) {
            continue;
        }
        countdown = std::min( countdown, local_attack_data.cooldown );
    }
    return countdown;
}

void monster::normalize_ammo( const int old_ammo )
{
    int total_ammo = 0;
    // Sum up the ammo entries to get a ratio.
    for( const auto &ammo_entry : type->starting_ammo ) {
        total_ammo += ammo_entry.second;
    }
    if( total_ammo == 0 ) {
        // Should never happen, but protect us from a div/0 if it does.
        return;
    }
    // Previous code gave robots 100 rounds of ammo.
    // This reassigns whatever is left from that in the appropriate proportions.
    for( const auto &ammo_entry : type->starting_ammo ) {
        ammo[ammo_entry.first] = old_ammo * ammo_entry.second / ( 100 * total_ammo );
    }
}

void monster::explode()
{
    // Handled in mondeath::normal
    // +1 to avoid overflow when evaluating -hp
    hp = INT_MIN + 1;
}

void monster::set_summon_time( const time_duration &length )
{
    summon_time_limit = length;
}

void monster::decrement_summon_timer()
{
    if( !summon_time_limit ) {
        return;
    }
    if( *summon_time_limit <= 0_turns ) {
        die( nullptr );
    } else {
        *summon_time_limit -= 1_turns;
    }
}

void monster::process_turn()
{
    ZoneScoped;

    decrement_summon_timer();
    if( !is_hallucination() ) {
        for( const std::pair<const emit_id, time_duration> &e : type->emit_fields ) {
            if( !calendar::once_every( e.second ) ) {
                continue;
            }
            const emit_id emid = e.first;
            if( emid == emit_id( "emit_shock_cloud" ) ) {
                if( has_effect( effect_emp ) ) {
                    continue; // don't emit electricity while EMPed
                } else if( has_effect( effect_supercharged ) ) {
                    g->m.emit_field( pos(), emit_id( "emit_shock_cloud_big" ) );
                    continue;
                }
            }
            g->m.emit_field( pos(), emid );
        }
    }

    // Special attack cooldowns are updated here.
    // Loop through the monster's special attacks, same as monster::move.
    for( const auto &sp_type : type->special_attacks ) {
        const std::string &special_name = sp_type.first;
        const auto local_iter = special_attacks.find( special_name );
        if( local_iter == special_attacks.end() ) {
            continue;
        }
        mon_special_attack &local_attack_data = local_iter->second;
        if( !local_attack_data.enabled ) {
            continue;
        }

        if( local_attack_data.cooldown > 0 ) {
            local_attack_data.cooldown--;
        }
    }
    // Persist grabs as long as there's an adjacent target.
    if( has_effect( effect_grabbing ) ) {
        for( auto &dest : g->m.points_in_radius( pos(), 1, 0 ) ) {
            const player *const p = g->critter_at<player>( dest );
            if( p && p->has_effect( effect_grabbed ) ) {
                add_effect( effect_grabbing, 2_turns );
            }
        }
    }
    // We update electrical fields here since they act every turn.
    if( has_flag( MF_ELECTRIC_FIELD ) ) {
        if( has_effect( effect_emp ) ) {
            if( calendar::once_every( 10_turns ) ) {
                sounds::sound( pos(), 5, sounds::sound_t::combat, _( "hummmmm." ), false, "humming", "electric" );
            }
        } else {
            for( const tripoint &zap : g->m.points_in_radius( pos(), 1 ) ) {
                const bool player_sees = g->u.sees( zap );
                const auto items = g->m.i_at( zap );
                for( const auto &item : items ) {
                    if( item->made_of( LIQUID ) && item->flammable() ) { // start a fire!
                        g->m.add_field( zap, fd_fire, 2, 1_minutes );
                        sounds::sound( pos(), 30, sounds::sound_t::combat,  _( "fwoosh!" ), false, "fire", "ignition" );
                        break;
                    }
                }
                if( zap != pos() ) {
                    explosion_handler::emp_blast( zap ); // Fries electronics due to the intensity of the field
                }
                const auto t = g->m.ter( zap );
                if( t == ter_str_id( "t_gas_pump" ) || t == ter_str_id( "t_gas_pump_a" ) ) {
                    if( one_in( 4 ) ) {
                        explosion_handler::explosion( pos(), nullptr, 40, 0.8, true );
                        if( player_sees ) {
                            add_msg( m_warning, _( "The %s explodes in a fiery inferno!" ), g->m.tername( zap ) );
                        }
                    } else {
                        if( player_sees ) {
                            add_msg( m_warning, _( "Lightning from %1$s engulfs the %2$s!" ), name(),
                                     g->m.tername( zap ) );
                        }
                        g->m.add_field( zap, fd_fire, 1, 2_turns );
                    }
                }
            }
            if( get_weather().lightning_active && !has_effect( effect_supercharged ) &&
                g->m.is_outside( pos() ) ) {
                get_weather().lightning_active = false; // only one supercharge per strike
                sounds::sound( pos(), 300, sounds::sound_t::combat, _( "BOOOOOOOM!!!" ), false, "environment",
                               "thunder_near" );
                sounds::sound( pos(), 20, sounds::sound_t::combat, _( "vrrrRRRUUMMMMMMMM!" ), false, "explosion",
                               "default" );
                if( g->u.sees( pos() ) ) {
                    add_msg( m_bad, _( "Lightning strikes the %s!" ), name() );
                    add_msg( m_bad, _( "Your vision goes white!" ) );
                    g->u.add_effect( effect_blind, rng( 1_minutes, 2_minutes ) );
                }
                add_effect( effect_supercharged, 12_hours );
            } else if( has_effect( effect_supercharged ) && calendar::once_every( 5_turns ) ) {
                sounds::sound( pos(), 20, sounds::sound_t::combat, _( "VMMMMMMMMM!" ), false, "humming",
                               "electric" );
            }
        }
    }

    Creature::process_turn();
}

void monster::die( Creature *nkiller )
{
    if( dead ) {
        // We are already dead, don't die again, note that monster::dead is
        // *only* set to true in this function!
        return;
    }
    // We were carrying a creature, deposit the rider
    if( has_effect( effect_ridden ) && mounted_player ) {
        mounted_player->forced_dismount();
    }
    g->set_critter_died();
    dead = true;
    set_killer( nkiller );
    if( !death_drops ) {
        return;
    }
    if( !no_extra_death_drops ) {
        drop_items_on_death();
    }
    // TODO: should actually be class Character
    player *ch = dynamic_cast<player *>( get_killer() );
    if( !is_hallucination() && ch != nullptr ) {
        if( ( has_flag( MF_GUILT ) && ch->is_player() ) || ( ch->has_trait( trait_PACIFIST ) &&
                has_flag( MF_HUMAN ) ) ) {
            // has guilt flag or player is pacifist && monster is humanoid
            mdeath::guilt( *this );
        }
        g->events().send<event_type::character_kills_monster>( ch->getID(), type->id );
        if( ch->is_player() && ch->has_trait( trait_KILLER ) ) {
            if( one_in( 4 ) ) {
                const translation snip = SNIPPET.random_from_category( "killer_on_kill" ).value_or( translation() );
                ch->add_msg_if_player( m_good, "%s", snip );
            }
            ch->add_morale( MORALE_KILLER_HAS_KILLED, 5, 10, 6_hours, 4_hours );
            ch->rem_morale( MORALE_KILLER_NEED_TO_KILL );
        }
        static const string_id<monfaction> faction_zombie( "zombie" );
        // Feral survivors are motivated to kill anything human
        if( ch->has_trait( trait_PROF_FERAL ) && has_flag( MF_HUMAN ) ) {
            if( !ch->has_effect( effect_feral_killed_recently ) ) {
                ch->add_msg_if_player( m_good, _( "The voices in your head quiet down a bit." ) );
            }
            if( faction != faction_zombie && !type->in_species( ZOMBIE ) ) {
                ch->add_effect( effect_feral_killed_recently, 3_days );
            } else {
                // Killing fellow ferals works but is less efficient, and comes with risk of punishment.
                ch->add_effect( effect_feral_killed_recently, 6_hours );
                if( one_in( 3 ) ) {
                    ch->add_msg_if_player( m_bad,
                                           _( "The rush of blood seems to drive off the smell of decay for a moment." ) );
                    ch->add_effect( effect_feral_infighting_punishment, 6_hours );
                }
            }
        }
    }
    // Drop items stored in optionals
    add_item( remove_tack_item() );
    add_item( remove_armor_item() );
    add_item( remove_storage_item() );
    add_item( remove_tied_item() );

    if( has_effect( effect_lightsnare ) ) {
        add_item( item::spawn( "string_36", calendar::start_of_cataclysm ) );
        add_item( item::spawn( "snare_trigger", calendar::start_of_cataclysm ) );
    }
    if( has_effect( effect_heavysnare ) ) {
        add_item( item::spawn( "rope_6", calendar::start_of_cataclysm ) );
        add_item( item::spawn( "snare_trigger", calendar::start_of_cataclysm ) );
    }
    if( has_effect( effect_beartrap ) ) {
        add_item( item::spawn( "beartrap", calendar::start_of_cataclysm ) );
    }
    if( has_effect( effect_grabbing ) ) {
        remove_effect( effect_grabbing );
        for( player *p : find_targets_to_ungrab( pos() ) ) {
            p->add_msg_player_or_npc( m_good, _( "The last enemy holding you collapses!" ),
                                      _( "The last enemy holding <npcname> collapses!" ) );
            p->remove_effect( effect_grabbed );
        }
    }
    if( !is_hallucination() ) {
        for( detached_ptr<item> &it : inv.clear() ) {
            g->m.add_item_or_charges( pos(), std::move( it ) );
        }
    }

    // If we're a queen, make nearby groups of our type start to die out
    if( !is_hallucination() && has_flag( MF_QUEEN ) ) {
        // The submap coordinates of this monster, monster groups coordinates are
        // submap coordinates.
        const tripoint abssub = ms_to_sm_copy( g->m.getabs( pos() ) );
        // Do it for overmap above/below too
        for( const tripoint &p : points_in_radius( abssub, HALF_MAPSIZE, 1 ) ) {
            // TODO: fix point types
            for( auto &mgp : overmap_buffer.groups_at( tripoint_abs_sm( p ) ) ) {
                if( MonsterGroupManager::IsMonsterInGroup( mgp->type, type->id ) ) {
                    mgp->dying = true;
                }
            }
        }
    }
    mission::on_creature_death( *this );
    // Also, perform our death function
    if( is_hallucination() || summon_time_limit ) {
        //Hallucinations always just disappear
        mdeath::disappear( *this );
        return;
    }

    //Not a hallucination, go process the death effects.
    for( const auto &deathfunction : type->dies ) {
        deathfunction( *this );
    }
    // Process other on-death triggers (spawn monster(s), etc)
    for( const auto &deathfunction : type->on_death ) {
        deathfunction( *this );
    }

    // Determine killer's faction
    mfaction_id killer_faction;
    if( nkiller != nullptr ) {
        const monster *killer_monster = nkiller->as_monster();
        if( killer_monster != nullptr ) {
            killer_faction = killer_monster->faction;
        } else if( ( nkiller->is_player() || nkiller->is_npc() ) && !nkiller->is_fake() ) {
            // Only attribute to player faction if it's a real player/NPC, not a fake NPC from a monster turret
            killer_faction = mfaction_id( "player" );
        }
    }

    // If our species fears seeing one of our own die, process that
    int anger_adjust = 0;
    int morale_adjust = 0;
    if( type->has_anger_trigger( mon_trigger::FRIEND_DIED ) ) {
        anger_adjust += 15;
        if( nkiller != nullptr && !nkiller->is_monster() && !nkiller->is_fake() ) {
            // A character killed our friend
            trigger_character_aggro( "killing a friendly creature" );
        }
    }
    if( type->has_fear_trigger( mon_trigger::FRIEND_DIED ) ) {
        morale_adjust -= 15;
    }
    if( type->has_placate_trigger( mon_trigger::FRIEND_DIED ) ) {
        anger_adjust -= 15;
    }

    if( anger_adjust != 0 || morale_adjust != 0 ) {
        int light = g->light_level( posz() );
        for( monster &critter : g->all_monsters() ) {
            if( critter.faction != this->faction ) {
                continue;
            }

            if( g->m.sees( critter.pos(), pos(), light ) ) {
                critter.morale += morale_adjust;

                if( critter.has_flag( MF_FACTION_MEMORY ) && killer_faction.is_valid() ) {
                    critter.add_faction_anger( killer_faction, anger_adjust );
                } else {
                    critter.anger += anger_adjust;
                }
            }
        }
    }
    cata::run_hooks( "on_mon_death", [ &, this]( auto & params ) {
        params["mon"] = this;
        params["killer"] = get_killer();
    } );
}

bool monster::use_mech_power( int amt )
{
    if( is_hallucination() || !has_flag( MF_RIDEABLE_MECH ) || !battery_item ) {
        return false;
    }
    amt = -amt;
    battery_item->ammo_consume( amt, pos() );
    return battery_item->ammo_remaining() > 0;
}

int monster::mech_str_addition() const
{
    return type->mech_str_bonus;
}

bool monster::check_mech_powered() const
{
    if( is_hallucination() || !has_flag( MF_RIDEABLE_MECH ) || !battery_item ) {
        return false;
    }
    if( battery_item->ammo_remaining() <= 0 ) {
        return false;
    }
    const itype &type = *battery_item->type;
    if( battery_item->ammo_remaining() <= type.magazine->capacity / 10 && one_in( 10 ) ) {
        add_msg( m_bad, _( "Your %s emits a beeping noise as its batteries start to get low." ),
                 get_name() );
    }
    return true;
}

static void process_item_valptr( item *ptr, monster &mon )
{
    if( ptr && ptr->needs_processing() ) {
        ptr->attempt_detach( [&mon]( detached_ptr<item> &&it ) {
            return item::process( std::move( it ), nullptr, mon.pos(), false );
        } );
    }
}

void monster::process_items()
{
    ZoneScoped;

    inv.remove_with( [this]( detached_ptr<item> &&it ) {
        if( it->needs_processing() ) {
            return item::process( std::move( it ), nullptr, pos(), false );
        }
        return std::move( it );
    } );

    process_item_valptr( &*storage_item, *this );
    process_item_valptr( &*armor_item, *this );
    process_item_valptr( &*tack_item, *this );
    process_item_valptr( &*tied_item, *this );
}

void monster::drop_items_on_death()
{
    if( is_hallucination() ) {
        return;
    }
    if( !type->death_drops ) {
        return;
    }

    std::vector<detached_ptr<item>> items = item_group::items_from( type->death_drops,
                                            calendar::start_of_cataclysm );

    // This block removes some items, according to item spawn scaling factor
    const float spawn_rate = get_option<float>( "ITEM_SPAWNRATE" );
    if( spawn_rate < 1 ) {
        // Temporary vector, to remember which items will be dropped
        std::vector<detached_ptr<item>> remaining;
        for( auto &it : items ) {
            if( it->has_flag( flag_MISSION_ITEM ) || rng_float( 0, 1 ) < spawn_rate ) {
                remaining.push_back( std::move( it ) );
            }
        }
        // If there aren't any items left, there's nothing left to do
        if( remaining.empty() ) {
            return;
        }
        items = std::move( remaining );
    }

    g->m.spawn_items( pos(), std::move( items ) );
}

void monster::process_one_effect( effect &it, bool is_new )
{
    // Monsters don't get trait-based reduction, but they do get effect based reduction
    bool reduced = resists_effect( it );
    const auto get_effect = [&it, is_new]( const std::string & arg, bool reduced ) {
        if( is_new ) {
            return it.get_amount( arg, reduced );
        }
        return it.get_mod( arg, reduced );
    };

    mod_speed_bonus( get_effect( "SPEED", reduced ) );
    mod_dodge_bonus( get_effect( "DODGE", reduced ) );
    mod_hit_bonus( get_effect( "HIT", reduced ) );
    mod_bash_bonus( get_effect( "BASH", reduced ) );
    mod_cut_bonus( get_effect( "CUT", reduced ) );
    mod_size_bonus( get_effect( "SIZE", reduced ) );

    int val = get_effect( "HURT", reduced );
    if( val > 0 ) {
        if( is_new || it.activated( calendar::turn, "HURT", val, reduced, 1 ) ) {
            apply_damage( nullptr, bodypart_id( "torso" ), val );
        }
    }

    const efftype_id &id = it.get_id();
    // TODO: MATERIALS use fire resistance
    if( it.impairs_movement() ) {
        effect_cache[MOVEMENT_IMPAIRED] = true;
    } else if( id == effect_onfire ) {
        int dam = 0;
        if( made_of( material_id( "veggy" ) ) ) {
            dam = rng( 10, 20 );
        } else if( made_of( material_id( "flesh" ) ) || made_of( material_id( "iflesh" ) ) ) {
            dam = rng( 5, 10 );
        }

        dam -= get_armor_type( DT_HEAT, bodypart_id( "torso" ) );
        if( dam > 0 ) {
            apply_damage( nullptr, bodypart_id( "torso" ), dam );
        } else {
            it.set_duration( 0_turns );
        }
    } else if( id == effect_run ) {
        effect_cache[FLEEING] = true;
    } else if( id == effect_no_sight || id == effect_blind ) {
        effect_cache[VISION_IMPAIRED] = true;
    } else if( id == effect_command_buff ) {
        effect_cache[PATHFINDING_OVERRIDE] = true;
    }

    if( is_new && it.has_flag( flag_EFFECT_LUA_ON_ADDED ) ) {
        cata::run_hooks( "on_mon_effect_added", [ &, this ]( auto & params ) {
            params["mon"] = this;
            params["effect"] = &it;
        } );
    }

    if( it.has_flag( flag_EFFECT_LUA_ON_TICK ) ) {
        cata::run_hooks( "on_mon_effect", [ &, this ]( auto & params ) {
            params["mon"] = this;
            params["effect"] = &it;
        } );
    }
}

void monster::process_effects_internal()
{
    // Monster only effects
    for( auto &elem : *effects ) {
        for( auto &_effect_it : elem.second ) {
            if( !_effect_it.second.is_removed() ) {
                process_one_effect( _effect_it.second, false );
            }
        }
    }

    //If this monster has the ability to heal in combat, do it now.
    int regeneration_amount = type->regenerates;
    float regen_multiplier = 0;
    //Apply effect-triggered regeneration modifiers
    for( const auto &regeneration_modifier : type->regeneration_modifiers ) {
        if( has_effect( regeneration_modifier.first ) ) {
            effect &e = get_effect( regeneration_modifier.first );
            regen_multiplier = 1.00 + regeneration_modifier.second.base_modifier +
                               ( e.get_intensity() - 1 ) * regeneration_modifier.second.scale_modifier;
            regeneration_amount = std::round( regeneration_amount * regen_multiplier );
        }
    }
    //Prevent negative regeneration
    if( regeneration_amount < 0 ) {
        regeneration_amount = 0;
    }
    const int healed_amount = heal( round( regeneration_amount ) );
    if( healed_amount > 0 && g->u.sees( *this ) ) {
        add_msg( m_warning, _( "The %1$s regenerates %2$s damage." ), name(), healed_amount );
    }

    if( type->regenerates_in_dark ) {
        const float light = g->m.ambient_light_at( pos() );
        add_msg( m_debug, _( "%1$s local light level: %2$s" ), name(), light );
        // Requires standing in a properly dark tile, scales as it gets darker
        if( light < 11.0f && one_in( 2 ) && hp < type->hp ) {
            // Regen will max out at 50 at 6.0 light (barely able to craft), or top off to max HP
            int dark_regen_amount = std::min( static_cast<int>( 110.0f - ( light * 10.0f ) ), type->hp - hp );
            dark_regen_amount = std::min( dark_regen_amount, 50 );
            heal( round( dark_regen_amount ) );
            if( dark_regen_amount > 0 && g->u.sees( *this ) ) {
                add_msg( m_warning, _( "The %1$s uses the darkness to regenerate %2$s damage." ), name(),
                         dark_regen_amount );
            }
        }
    }

    // Monster will regen morale and aggression if it is on max HP
    // It regens more morale and aggression if is currently fleeing.
    if( type->regen_morale && hp >= type->hp ) {
        if( is_fleeing( g->u ) ) {
            morale = type->morale;
            // Don't restore global anger for FACTION_MEMORY monsters
            if( !has_flag( MF_FACTION_MEMORY ) ) {
                anger = type->agro;
            }
        }
        if( morale <= type->morale ) {
            morale += 1;
        }
        // Don't restore global anger for FACTION_MEMORY monsters
        if( !has_flag( MF_FACTION_MEMORY ) && anger <= type->agro ) {
            anger += 1;
        }
        if( morale < 0 ) {
            morale += 5;
        }
        // Don't restore global anger for FACTION_MEMORY monsters
        if( !has_flag( MF_FACTION_MEMORY ) && anger < 0 ) {
            anger += 5;
        }
    }

    // If this critter dies in sunlight, check & assess damage.
    if( has_flag( MF_SUNDEATH ) && g->is_in_sunlight( pos() ) ) {
        if( g->u.sees( *this ) ) {
            add_msg( m_good, _( "The %s burns horribly in the sunlight!" ), name() );
        }
        apply_damage( nullptr, bodypart_id( "torso" ), 100 );
        if( hp < 0 ) {
            hp = 0;
        }
    }
}

bool monster::make_fungus()
{
    if( is_hallucination() ) {
        return true;
    }
    if( type->in_species( FUNGUS ) ) { // No friendly-fungalizing ;-)
        return true;
    }
    if( !made_of( material_id( "flesh" ) ) && !made_of( material_id( "hflesh" ) ) &&
        !made_of( material_id( "veggy" ) ) && !made_of( material_id( "iflesh" ) ) &&
        !made_of( material_id( "bone" ) ) ) {
        // No fungalizing robots or weird stuff (mi-gos are technically fungi, blobs are goo)
        return true;
    }
    if( type->has_flag( MF_NO_FUNG_DMG ) ) {
        return true; // Returns true when monster immune to fungal damage.
    }
    if( type->fungalize_into.is_empty() ) {
        return false;
    }

    const std::string old_name = name();
    poly( type->fungalize_into );

    if( g->u.sees( pos() ) ) {
        add_msg( m_info, _( "The spores transform %1$s into a %2$s!" ),
                 old_name, name() );
    }

    return true;
}

void monster::make_friendly()
{
    unset_dest();
    friendly = rng( 5, 30 ) + rng( 0, 20 );
}

void monster::make_ally( const monster &z )
{
    friendly = z.friendly;
    faction = z.faction;
}

void monster::make_pet()
{
    friendly = -1;
    g->critter_tracker->update_faction( *this );
    add_effect( effect_pet, 1_turns );
}

bool monster::is_pet() const
{
    return ( friendly == -1 && has_effect( effect_pet ) );
}

bool monster::is_hallucination() const
{
    return hallucination;
}

field_type_id monster::bloodType() const
{
    if( is_hallucination() ) {
        return fd_null;
    }
    return type->bloodType();
}
field_type_id monster::gibType() const
{
    if( is_hallucination() ) {
        return fd_null;
    }
    return type->gibType();
}

creature_size monster::get_size() const
{
    // Don't allow size bonuses from effects to make the creature larger than huge or smaller than tiny
    return std::max( std::min( creature_size( type->size + size_bonus ), creature_size::huge ),
                     creature_size::tiny );
}

units::mass monster::get_weight() const
{
    return units::operator*( type->weight, ( get_size() + 1 ) / ( type->size + 1 ) );
}

units::mass monster::weight_capacity() const
{
    return type->weight * type->mountable_weight_ratio;
}

units::volume monster::get_volume() const
{
    return units::operator*( type->volume, ( get_size() + 1 ) / ( type->size + 1 ) );
}

void monster::add_msg_if_npc( const std::string &msg ) const
{
    if( g->u.sees( *this ) ) {
        add_msg( replace_with_npc_name( msg ) );
    }
}

void monster::add_msg_player_or_npc( const std::string &/*player_msg*/,
                                     const std::string &npc_msg ) const
{
    if( g->u.sees( *this ) ) {
        add_msg( replace_with_npc_name( npc_msg ) );
    }
}

void monster::add_msg_if_npc( const game_message_params &params, const std::string &msg ) const
{
    if( g->u.sees( *this ) ) {
        add_msg( params, replace_with_npc_name( msg ) );
    }
}

void monster::add_msg_player_or_npc( const game_message_params &params,
                                     const std::string &/*player_msg*/, const std::string &npc_msg ) const
{
    if( g->u.sees( *this ) ) {
        add_msg( params, replace_with_npc_name( npc_msg ) );
    }
}

units::mass monster::get_carried_weight() const
{
    units::mass total_weight = 0_gram;
    if( tack_item ) {
        total_weight += tack_item->weight();
    }
    if( storage_item ) {
        total_weight += storage_item->weight();
    }
    if( armor_item ) {
        total_weight += armor_item->weight();
    }
    for( const item * const &it : inv ) {
        total_weight += it->weight();
    }
    return total_weight;
}

units::volume monster::get_carried_volume() const
{
    units::volume total_volume = 0_ml;
    for( const item * const &it : inv ) {
        total_volume += it->volume();
    }
    return total_volume;
}

bool monster::is_dead() const
{
    return dead || is_dead_state();
}

bool monster::is_nemesis() const
{
    return has_flag( MF_NEMESIS );
}

void monster::init_from_item( const item &itm )
{
    if( itm.typeId() == itype_corpse ) {
        set_speed_base( get_speed_base() * 0.8 );
        const int burnt_penalty = itm.burnt;
        hp = static_cast<int>( hp * 0.7 );
        if( itm.damage_level( 4 ) > 0 ) {
            set_speed_base( speed_base / ( itm.damage_level( 4 ) + 1 ) );
            hp /= itm.damage_level( 4 ) + 1;
        }

        hp -= burnt_penalty;

        // HP can be 0 or less, in this case revive_corpse will just deactivate the corpse
        if( hp > 0 && type->has_flag( MF_REVIVES_HEALTHY ) ) {
            hp = type->hp;
            set_speed_base( type->speed );
        }
        const std::string up_time = itm.get_var( "upgrade_time" );
        if( !up_time.empty() ) {
            upgrade_time = std::stoi( up_time );
        }
    } else {
        // must be a robot
        const int damfac = itm.max_damage() - std::max( 0, itm.damage() ) + 1;
        // One hp at least, everything else would be unfair (happens only to monster with *very* low hp),
        hp = std::max( 1, hp * damfac / ( itm.max_damage() + 1 ) );
    }
}

detached_ptr<item> monster::to_item() const
{
    if( type->revert_to_itype.is_empty() ) {
        return detached_ptr<item>();
    }
    // Birthday is wrong, but the item created here does not use it anyway (I hope).
    detached_ptr<item> result = item::spawn( type->revert_to_itype, calendar::turn );
    const int damfac = std::max( 1, ( result->max_damage() + 1 ) * hp / type->hp );
    result->set_damage( std::max( 0, ( result->max_damage() + 1 ) - damfac ) );
    // If we have a nickname, save it via the item's label
    if( !unique_name.empty() ) {
        result->set_var( "item_label", unique_name );
    }
    // If it's configured as a pet and not merely friendly, make sure next time we skip re-rolling for a friendly deployment.
    if( has_effect( effect_pet ) ) {
        result->set_flag( flag_SPAWN_FRIENDLY );
    }
    return result;
}

float monster::power_rating() const
{
    float ret = get_size() - 1; // Zed gets 1, cat -1, hulk 3
    ret += has_flag( MF_ELECTRONIC ) ? 2 : 0; // Robots tend to have guns
    // Hostile stuff gets a big boost
    // Neutral moose will still get burned if it comes close
    return ret;
}

float monster::speed_rating() const
{
    float ret = get_speed() / 100.0f;
    const auto leap = type->special_attacks.find( "leap" );
    if( leap != type->special_attacks.end() ) {
        // TODO: Make this calculate sane values here
        ret += 0.5f;
    }

    return ret;
}

void monster::on_hit( Creature *source, bodypart_id, dealt_projectile_attack const *const proj )
{
    this->on_hit( source, bodypart_id( "torso" ), proj, false );
}
void monster::on_hit( Creature *source, bodypart_id, dealt_projectile_attack const *const proj,
                      bool manual_retaliation )
{
    if( is_hallucination() ) {
        return;
    }

    if( rng( 0, 100 ) <= static_cast<int>( type->def_chance ) && !manual_retaliation ) {
        type->sp_defense( *this, source, proj );
    }

    // Determine attacker's faction
    mfaction_id attacker_faction;
    if( source != nullptr ) {
        const monster *source_monster = source->as_monster();
        if( source_monster != nullptr ) {
            attacker_faction = source_monster->faction;
        } else if( ( source->is_player() || source->is_npc() ) && !source->is_fake() ) {
            // Only attribute to player faction if it's a real player/NPC, not a fake NPC from a monster turret
            attacker_faction = mfaction_id( "player" );
        }
    }

    // Adjust anger/morale of same-species monsters, if appropriate
    int anger_adjust = 0;
    int morale_adjust = 0;
    if( type->has_anger_trigger( mon_trigger::FRIEND_ATTACKED ) ) {
        anger_adjust += 15;
        if( source != nullptr && !aggro_character && !source->is_monster() && !source->is_fake() ) {
            // A character attacked our friend
            trigger_character_aggro( "killing a friendly creature" );
        }
    }
    if( type->has_fear_trigger( mon_trigger::FRIEND_ATTACKED ) ) {
        morale_adjust -= 15;
    }
    if( type->has_placate_trigger( mon_trigger::FRIEND_ATTACKED ) ) {
        anger_adjust -= 15;
    }

    if( anger_adjust != 0 || morale_adjust != 0 ) {
        int light = g->light_level( posz() );
        for( monster &critter : g->all_monsters() ) {
            if( critter.faction != this->faction ) {
                continue;
            }

            if( g->m.sees( critter.pos(), pos(), light ) ) {
                critter.morale += morale_adjust;

                if( critter.has_flag( MF_FACTION_MEMORY ) && attacker_faction.is_valid() ) {
                    critter.add_faction_anger( attacker_faction, anger_adjust );
                } else {
                    critter.anger += anger_adjust;
                }
            }
        }
    }

    check_dead_state();
    // TODO: Faction relations
}

void monster::on_damage_of_type( int amt, damage_type dt, const bodypart_id &bp )
{
    Creature::on_damage_of_type( amt, dt, bp );
    int full_hp = get_hp_max();
    if( has_effect( effect_grabbing ) && ( dt == DT_BASH || dt == DT_CUT || dt == DT_STAB ) &&
        x_in_y( amt * 10, full_hp ) ) {
        remove_effect( effect_grabbing );
        for( player *p : find_targets_to_ungrab( pos() ) ) {
            p->add_msg_player_or_npc( m_good, _( "%s flinches, letting you go!" ),
                                      _( "%s flinches, letting <npcname> go!" ),
                                      disp_name( false, true ) );
            p->remove_effect( effect_grabbed );
        }
    }
}

int monster::get_hp_max( const bodypart_id & ) const
{
    return type->hp;
}

int monster::get_hp_max() const
{
    return type->hp;
}

int monster::get_hp( const bodypart_id & ) const
{
    return hp;
}

int monster::get_hp() const
{
    return hp;
}

float monster::get_mountable_weight_ratio() const
{
    return type->mountable_weight_ratio;
}

void monster::hear_sound( const tripoint &source, const int vol, const int dist )
{
    if( !can_hear() ) {
        return;
    }

    static const string_id<monfaction> faction_zombie( "zombie" );
    const bool feral_friend = ( faction == faction_zombie || type->in_species( ZOMBIE ) ) &&
                              g->u.has_trait( trait_PROF_FERAL ) && !g->u.has_effect( effect_feral_infighting_punishment );

    // Hackery: If player is currently a feral and you're a zombie, ignore any sounds close to their position.
    if( feral_friend && rl_dist( g->u.pos(), source ) <= 10 ) {
        return;
    }

    const bool goodhearing = has_flag( MF_GOODHEARING );
    const int volume = goodhearing ? 2 * vol - dist : vol - dist;
    // Error is based on volume, louder sound = less error
    if( volume <= 0 ) {
        return;
    }

    int max_error = 0;
    if( volume < 2 ) {
        max_error = 10;
    } else if( volume < 5 ) {
        max_error = 5;
    } else if( volume < 10 ) {
        max_error = 3;
    } else if( volume < 20 ) {
        max_error = 1;
    }

    int target_x = source.x + rng( -max_error, max_error );
    int target_y = source.y + rng( -max_error, max_error );
    // target_z will require some special check due to soil muffling sounds

    int wander_turns = volume * ( goodhearing ? 6 : 1 );

    process_trigger( mon_trigger::SOUND, volume );
    if( morale >= 0 && anger >= 10 ) {
        // TODO: Add a proper check for fleeing attitude
        // but cache it nicely, because this part is called a lot
        wander_to( tripoint( target_x, target_y, source.z ), wander_turns );
    } else if( morale < 0 ) {
        // Monsters afraid of sound should not go towards sound
        wander_to( tripoint( 2 * posx() - target_x, 2 * posy() - target_y, 2 * posz() - source.z ),
                   wander_turns );
    }
}

monster_horde_attraction monster::get_horde_attraction()
{
    if( horde_attraction == MHA_NULL ) {
        horde_attraction = static_cast<monster_horde_attraction>( rng( 1, 5 ) );
    }
    return horde_attraction;
}

void monster::set_horde_attraction( monster_horde_attraction mha )
{
    horde_attraction = mha;
}

bool monster::will_join_horde( int size )
{
    const monster_horde_attraction mha = get_horde_attraction();
    if( mha == MHA_NEVER ) {
        return false;
    } else if( mha == MHA_ALWAYS ) {
        return true;
    } else if( g->m.has_flag( TFLAG_INDOORS, pos() ) && ( mha == MHA_OUTDOORS ||
               mha == MHA_OUTDOORS_AND_LARGE ) ) {
        return false;
    } else if( size < 3 && ( mha == MHA_LARGE || mha == MHA_OUTDOORS_AND_LARGE ) ) {
        return false;
    } else {
        return true;
    }
}

void monster::on_unload()
{
    last_updated = calendar::turn;
}

void monster::on_load()
{
    try_upgrade( false );
    try_reproduce();
    if( has_flag( MF_MILKABLE ) ) {
        refill_udders();
    }

    const time_duration dt = calendar::turn - last_updated;
    last_updated = calendar::turn;
    if( dt <= 0_turns ) {
        return;
    }

    // Don't restore global anger for FACTION_MEMORY monsters
    if( !has_flag( MF_FACTION_MEMORY ) && anger != type->agro ) {
        int dt_left_a = to_turns<int>( dt );

        if( std::abs( anger - type->agro ) > 15 ) {
            const int adjust_by_a = std::min( ( dt_left_a / 4 ),
                                              ( std::abs( anger - type->agro ) - 15 ) );
            dt_left_a -= adjust_by_a * 4;
            if( anger < type->agro ) {
                anger += adjust_by_a;
            } else {
                anger -= adjust_by_a;
            }
        }

        if( anger > type->agro ) {
            anger -= std::min( static_cast<int>( std::ceil( dt_left_a / 8.0 ) ),
                               std::abs( anger - type->agro ) );
        } else {
            anger += std::min( ( dt_left_a / 8 ),
                               std::abs( anger - type->agro ) );
        }
        // If we got angry at characters have a chance at calming down
        if( aggro_character && !type->aggro_character && !x_in_y( anger, 100 ) ) {
            add_msg( m_debug, "%s's character aggro reset", name() );
            aggro_character = false;
        }
    }


    float regen = type->regenerates;
    if( regen <= 0 ) {
        if( has_flag( MF_REVIVES ) ) {
            regen = 1.0f / to_turns<int>( 1_hours );
        } else if( made_of( material_id( "flesh" ) ) || made_of( material_id( "veggy" ) ) ) {
            // Most living stuff here
            regen = 0.25f / to_turns<int>( 1_hours );
        }
    }
    if( has_effect( effect_well_fed ) ) {
        regen *= 2.0f;
    }
    const int heal_amount = roll_remainder( regen * to_turns<int>( dt ) );
    const int healed = heal( heal_amount );
    int healed_speed = 0;
    if( healed < heal_amount && get_speed_base() < type->speed ) {
        int old_speed = get_speed_base();
        set_speed_base( std::min( get_speed_base() + heal_amount - healed, type->speed ) );
        healed_speed = get_speed_base() - old_speed;
    }

    add_msg( m_debug, "on_load() by %s, %d turns, healed %d hp, %d speed",
             name(), to_turns<int>( dt ), healed, healed_speed );
}

const pathfinding_settings &monster::get_legacy_pathfinding_settings() const
{
    return !effect_cache[PATHFINDING_OVERRIDE] ?
           type->legacy_path_settings
           : type->legacy_path_settings_buffed;

}

std::pair<PathfindingSettings, RouteSettings> monster::get_pathfinding_pair()
const
{
    return !effect_cache[PATHFINDING_OVERRIDE] ?
           std::make_pair( type->path_settings, type->route_settings ) :
           std::make_pair( type->path_settings_buffed, type->route_settings_buffed );

}

std::set<tripoint> monster::get_legacy_path_avoid() const
{
    return std::set<tripoint>();
}

const std::vector<item *> &monster::get_items() const
{
    return inv.as_vector();
}

void monster::add_item( detached_ptr<item> &&it )
{
    if( !it ) {
        return;
    }
    if( it->is_null() ) {
        debugmsg( "Tried to add a null item to a monster" );
        return;
    }
    inv.push_back( std::move( it ) );
}

detached_ptr<item> monster::remove_item( item *it )
{
    auto iter = std::ranges::find( inv, it );
    detached_ptr<item> ret;
    if( iter != inv.end() ) {
        inv.erase( iter, &ret );
    }
    return ret;
}

location_vector<item>::iterator monster::remove_item( location_vector<item>::iterator &it,
        detached_ptr<item> *result )
{
    return inv.erase( it, result );
}

std::vector<detached_ptr<item>> monster::clear_items()
{
    return inv.clear();
}

void monster::drop_items( const tripoint &p )
{
    for( detached_ptr<item> &it : inv.clear() ) {
        g->m.add_item_or_charges( p, std::move( it ) );
    }
}

void monster::drop_items()
{
    drop_items( pos() );
}

void monster::add_corpse_component( detached_ptr<item> &&it )
{
    corpse_components.push_back( std::move( it ) );
}

detached_ptr<item> monster::remove_corpse_component( item &it )
{
    for( auto iter = corpse_components.begin(); iter != corpse_components.end(); iter++ ) {
        if( *iter == &it ) {
            detached_ptr<item> ret;
            corpse_components.erase( iter, &ret );
            return ret;
        }
    }
    return detached_ptr<item>();
}

std::vector<detached_ptr<item>> monster::remove_corpse_components()
{
    return corpse_components.clear();
}

void monster::add_faction_anger( mfaction_id target_faction, int amount )
{
    if( !has_flag( MF_FACTION_MEMORY ) ) {
        anger += amount;
        return;
    }

    // Don't get angry at own faction (prevents friendly fire loops)
    if( target_faction == this->faction ) { return; }

    // Don't track anger for invalid factions
    if( !target_faction.is_valid() ) { return; }

    faction_anger[target_faction] += amount;
}

auto monster::get_faction_anger( mfaction_id target_faction ) const -> int
{
    if( !has_flag( MF_FACTION_MEMORY ) ) {
        return anger;
    }

    auto it = faction_anger.find( target_faction );
    return ( it != faction_anger.end() ) ? it->second : 0;
}
