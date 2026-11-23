#include "mongroup.h"

#include <algorithm>
#include <set>
#include <ranges>
#include <utility>

#include "action_time_scale.h"
#include "assign.h"
#include "calendar.h"
#include "debug.h"
#include "generic_factory.h"
#include "generic_readers.h"
#include "json.h"
#include "mtype.h"
#include "options.h"
#include "rng.h"
#include "string_id.h"
#include "type_id_implement.h"
#include "flag.h"

namespace
{
// Default id & alias name, legacy id name is `name`
generic_factory<MonsterGroup> all_monster_groups( "Monster Groups", "id", "alias", "name" );
std::unordered_set<std::string> monster_blacklist;
std::unordered_set<std::string> monster_whitelist;
std::unordered_set<std::string> monster_categories_blacklist;
std::unordered_set<std::string> monster_categories_whitelist;
bool monster_whitelist_is_exclusive = false;
} // namespace

IMPLEMENT_STRING_AND_INT_IDS( MonsterGroup, all_monster_groups );

bool mongroup::is_safe() const
{
    return type.obj().is_safe;
}

bool mongroup::empty() const
{
    return population <= 0 && monsters.empty();
}

void mongroup::clear()
{
    population = 0;
    monsters.clear();
}

float mongroup::avg_speed() const
{
    float avg_speed = 0;
    if( monsters.empty() ) {
        const MonsterGroup &g = type.obj();
        int remaining_frequency = g.freq_total;
        for( auto &elem : g.monsters ) {
            if( !elem.monster_group.is_null() ) {
                avg_speed += elem.frequency * mongroup( elem.monster_group, abs_pos, radius,
                                                        population ).avg_speed();
            } else {
                avg_speed += elem.frequency * elem.name.obj().speed;
            }
            remaining_frequency -= elem.frequency;
        }
        if( remaining_frequency > 0 ) {
            avg_speed += g.defaultMonster.obj().speed * remaining_frequency;
        }
        avg_speed /= g.freq_total;
    } else {
        for( auto &it : monsters ) {
            avg_speed += it.type->speed;
        }
        avg_speed /= monsters.size();
    }
    return static_cast<float>( action_time_scale::scaled_overmap_horde_speed( avg_speed ) );
}

const MonsterGroup &MonsterGroupManager::GetUpgradedMonsterGroup( const mongroup_id &group )
{
    const MonsterGroup *groupptr = &group.obj();
    if( get_option<float>( "MONSTER_UPGRADE_FACTOR" ) > 0 ) {
        const time_duration replace_time = groupptr->monster_group_time *
                                           get_option<float>( "MONSTER_UPGRADE_FACTOR" );
        while( groupptr->replace_monster_group &&
               calendar::turn - time_point( calendar::start_of_cataclysm ) > replace_time ) {
            groupptr = &groupptr->new_monster_group.obj();
        }
    }
    return *groupptr;
}

//Quantity is adjusted directly as a side effect of this function
MonsterGroupResult MonsterGroupManager::GetResultFromGroup(
    const mongroup_id &group_name, int *quantity, bool use_pack_size )
{
    auto &group = GetUpgradedMonsterGroup( group_name );
    int spawn_chance = rng( 1, group.freq_total ); //Default 1000 unless specified
    //Our spawn details specify, by default, a single instance of the default monster
    MonsterGroupResult spawn_details = MonsterGroupResult( group.defaultMonster, 1 );

    if( group.defaultMonster != mtype_id::NULL_ID() ) {
        int count = 0;
        auto montype = group.defaultMonster;
        while( count < group.evolve_repeat ) {
            if( rng( 0, 100 ) < group.evolve_chance ) {
                if( montype->upgrade_into ) {
                    //If we upgrade into a blacklisted monster, treat it as though we are non-upgradeable
                    if( MonsterGroupManager::monster_is_blacklisted( montype->upgrade_into ) ) {
                        count = group.evolve_repeat;
                    }
                    montype = montype->upgrade_into;
                } else {
                    montype = MonsterGroupManager::GetRandomMonsterFromGroup( montype->upgrade_group );
                }
            }
            count++;
        }
        spawn_details = MonsterGroupResult( montype, 1 );
    }

    bool monster_found = false;
    // Loop invariant values
    const time_point sunset = ::sunset( calendar::turn );
    const time_point sunrise = ::sunrise( calendar::turn );
    const season_type season = season_of_year( calendar::turn );
    // Step through spawn definitions from the monster group until one is found or
    for( auto it = group.monsters.begin(); it != group.monsters.end() && !monster_found; ++it ) {
        // There's a lot of conditions to work through to see if this spawn definition is valid
        bool valid_entry = true;
        //Insure that the time is not before the spawn first appears or after it stops appearing
        valid_entry = valid_entry && ( calendar::start_of_cataclysm + it->starts < calendar::turn );
        valid_entry = valid_entry && ( it->lasts_forever() ||
                                       calendar::start_of_cataclysm + it->ends > calendar::turn );

        std::vector<std::pair<time_point, time_point> > valid_times_of_day;
        bool season_limited = false;
        bool season_matched = false;
        //Collect the various spawn conditions, and then insure they are met appropriately
        for( auto &elem : it->conditions ) {
            //Collect valid time of day ranges
            if( elem == "DAY" || elem == "NIGHT" || elem == "DUSK" || elem == "DAWN" ) {
                if( elem == "DAY" ) {
                    valid_times_of_day.emplace_back( sunrise, sunset );
                } else if( elem == "NIGHT" ) {
                    valid_times_of_day.emplace_back( sunset, sunrise );
                } else if( elem == "DUSK" ) {
                    valid_times_of_day.emplace_back( sunset - 1_hours, sunset + 1_hours );
                } else if( elem == "DAWN" ) {
                    valid_times_of_day.emplace_back( sunrise - 1_hours, sunrise + 1_hours );
                }
            }

            //If we have any seasons listed, we know to limit by season, and if any season matches this season, we are good to spawn
            if( elem == "SUMMER" || elem == "WINTER" || elem == "SPRING" || elem == "AUTUMN" ) {
                season_limited = true;
                if( ( season == SUMMER && elem == "SUMMER" ) ||
                    ( season == WINTER && elem == "WINTER" ) ||
                    ( season == SPRING && elem == "SPRING" ) ||
                    ( season == AUTUMN && elem == "AUTUMN" ) ) {
                    season_matched = true;
                }
            }
        }

        //Make sure the current time of day is within one of the valid time ranges for this spawn
        bool is_valid_time_of_day = false;
        if( valid_times_of_day.empty() ) {
            //Then it can spawn whenever, since no times were defined
            is_valid_time_of_day = true;
        } else {
            //Otherwise, it's valid if it matches any of the times of day
            for( auto &elem : valid_times_of_day ) {
                if( calendar::turn > elem.first && calendar::turn < elem.second ) {
                    is_valid_time_of_day = true;
                }
            }
        }
        if( !is_valid_time_of_day ) {
            valid_entry = false;
        }

        //If we are limited by season, make sure we matched a season
        if( season_limited && !season_matched ) {
            valid_entry = false;
        }

        //If the entry was valid, check to see if we actually spawn it
        if( valid_entry ) {
            //If the monsters frequency is greater than the spawn_chance, select this spawn rule
            if( it->frequency >= spawn_chance ) {
                int pack_size = 1;
                if( use_pack_size || it->pack_maximum > 1 ) {
                    pack_size = rng( it->pack_minimum, it->pack_maximum );
                }
                int count = 0;
                auto montype = it->name;
                while( count < group.evolve_repeat ) {
                    if( rng( 0, 100 ) < group.evolve_chance ) {
                        if( montype->upgrade_into ) {
                            //If we upgrade into a blacklisted monster, treat it as though we are non-upgradeable
                            if( MonsterGroupManager::monster_is_blacklisted( montype->upgrade_into ) ) {
                                count = group.evolve_repeat;
                            }
                            montype = montype->upgrade_into;
                        } else {
                            montype = MonsterGroupManager::GetRandomMonsterFromGroup( montype->upgrade_group );
                        }
                    }
                    count++;
                }
                spawn_details = MonsterGroupResult( montype, pack_size );
                //And if a quantity pointer with remaining value was passed, will modify the external value as a side effect
                //We will reduce it by the spawn rule's cost multiplier
                if( quantity ) {
                    *quantity -= std::max( 1, it->cost_multiplier * spawn_details.pack_size );
                }
                monster_found = true;
                //Otherwise, subtract the frequency from spawn result for the next loop around
            } else {
                spawn_chance -= it->frequency;
            }
        }
    }

    // Force quantity to decrement regardless of whether we found a monster.
    if( quantity && !monster_found ) {
        ( *quantity )--;
    }

    return spawn_details;
}

bool MonsterGroup::IsMonsterInGroup( const mtype_id &mtypeid ) const
{
    if( defaultMonster == mtypeid ) {
        return true;
    }
    for( auto &m : monsters ) {
        if( m.name == mtypeid ) {
            return true;
        }
    }
    return false;
}

bool MonsterGroupManager::IsMonsterInGroup( const mongroup_id &group, const mtype_id &monster )
{
    return group.obj().IsMonsterInGroup( monster );
}

const mongroup_id &MonsterGroupManager::Monster2Group( const mtype_id &monster )
{
    for( auto &g : MonsterGroup::get_all() ) {
        if( g.IsMonsterInGroup( monster ) ) {
            return g.id;
        }
    }
    return mongroup_id::NULL_ID();
}

std::vector<mtype_id> MonsterGroupManager::GetMonstersFromGroup( const mongroup_id &group )
{
    const MonsterGroup &g = group.obj();

    std::vector<mtype_id> monsters;

    monsters.push_back( g.defaultMonster );

    for( auto &elem : g.monsters ) {
        monsters.push_back( elem.name );
    }
    return monsters;
}

const MonsterGroup &MonsterGroupManager::GetMonsterGroup( const mongroup_id &group )
{
    if( group.is_valid() ) {
        return group.obj();
    } else {
        debugmsg( "Unable to get the group '%s'", group.c_str() );
        // Return a valid group instead of crashing, with the null-monster so it
        // Is ignored while spawning.
        return mongroup_id::NULL_ID().obj();
    }
}

auto MonsterGroupManager::get_all_group_ids() -> std::vector<mongroup_id>
{
    return MonsterGroup::get_all()
    | std::views::transform( []( const MonsterGroup & group ) { return group.id; } )
    | std::ranges::to<std::vector<mongroup_id>>();
}

void MonsterGroupManager::LoadMonsterBlacklist( const JsonObject &jo )
{
    add_array_to_set( monster_blacklist, jo, "monsters" );
    add_array_to_set( monster_categories_blacklist, jo, "categories" );
}

void MonsterGroupManager::LoadMonsterWhitelist( const JsonObject &jo )
{
    if( jo.has_string( "mode" ) && jo.get_string( "mode" ) == "EXCLUSIVE" ) {
        monster_whitelist_is_exclusive = true;
    }
    add_array_to_set( monster_whitelist, jo, "monsters" );
    add_array_to_set( monster_categories_whitelist, jo, "categories" );
}

bool MonsterGroupManager::monster_is_blacklisted( const mtype_id &m )
{
    if( monster_whitelist.contains( m.str() ) ) {
        return false;
    }
    const mtype &mt = m.obj();
    for( const auto &elem : monster_categories_whitelist ) {
        if( mt.categories.contains( elem ) ) {
            return false;
        }
    }
    for( const auto &elem : monster_categories_blacklist ) {
        if( mt.categories.contains( elem ) ) {
            return true;
        }
    }
    if( monster_blacklist.contains( m.str() ) ) {
        return true;
    }
    // Return true if the whitelist mode is exclusive and either whitelist is populated.
    return monster_whitelist_is_exclusive &&
           ( !monster_whitelist.empty() || !monster_categories_whitelist.empty() );
}

namespace
{
class mongroup_time_reader : public generic_typed_reader<mongroup_time_reader>
{
    public:
        time_duration get_next( JsonIn &jin ) const {
            if( jin.test_int() ) {
                return 1_hours * jin.get_int();
            } else {
                return read_from_json_string<time_duration>( jin, time_duration::units );
            }
        }
};

class mongroup_entry_reader : public generic_typed_reader<mongroup_entry_reader>
{
    private:
        const bool legacy;
    public:
        mongroup_entry_reader( bool legacy = false ) : legacy( legacy ) {}
        MonsterGroupEntry get_next( JsonIn &jin ) const {
            float mon_upgrade_factor = get_option<float>( "MONSTER_UPGRADE_FACTOR" );

            MonsterGroupEntry entry;
            JsonObject jo = jin.get_object();
            mandatory( jo, false, legacy ? "freq" : "weight", entry.frequency );
            optional( jo, false, "monster", entry.name, mtype_id::NULL_ID() );
            optional( jo, false, "monster_group", entry.monster_group, mongroup_id::NULL_ID() );
            optional( jo, false, "cost_multiplier", entry.cost_multiplier, 1 );
            optional( jo, false, "starts", entry.starts, mongroup_time_reader(), 0_turns );
            optional( jo, false, "ends", entry.ends, mongroup_time_reader(), 0_turns );
            optional( jo, false, "conditions", entry.conditions, string_reader() );
            if( jo.has_array( "pack_size" ) ) {
                JsonArray packarr = jo.get_array( "pack_size" );
                entry.pack_minimum = packarr.next_int();
                entry.pack_maximum = packarr.next_int();
            }
            if( entry.name.is_null() && entry.monster_group.is_null() ) {
                jo.throw_error( "Monster group requires definition of one of `monster` or `monster_group`" );
            } else if( !entry.name.is_null() && !entry.monster_group.is_null() ) {
                // FIXME: I dont know why this path goes when `name` is not null and `monster_group` is
                // jo.throw_error( "Monster group requires definition of only one of `monster` or `monster_group`" );
            }
            entry.starts *= mon_upgrade_factor;
            entry.ends *= mon_upgrade_factor;
            return entry;
        }

        template<typename C>
        void erase_next( JsonIn &jin, C &container ) const {
            JsonObject jo = jin.get_object();
            if( jo.has_string( "monster" ) ) {
                const mtype_id &id = mtype_id( jo.get_string( "monster" ) );
                reader_detail::handler<C>().erase_if( container, [&id]( const MonsterGroupEntry & e ) {
                    return e.name == id;
                } );
            }
            if( jo.has_string( "monster_group" ) ) {
                const mongroup_id &id = mongroup_id( jo.get_string( "monster_group" ) );
                reader_detail::handler<C>().erase_if( container, [&id]( const MonsterGroupEntry & e ) {
                    return e.monster_group == id;
                } );
            }
        }
};
} // namespace

void MonsterGroup::load( const JsonObject &jo, const std::string & /*src*/ )
{
    bool allow_override = jo.get_bool( "override", false );
    if( id.is_valid() && !allow_override ) {
        *this = id.obj();
        was_loaded = true;
    }
    if( use_automatic_sizing ) {
        freq_total = -1;
    }
    optional( jo, was_loaded, "is_animal", is_animal, false );
    optional( jo, was_loaded, "replacement_time", monster_group_time, 1_days );
    optional( jo, was_loaded, "evolve_chance", evolve_chance, 0 );
    optional( jo, was_loaded, "evolve_repeat", evolve_repeat, 0 );
    optional( jo, was_loaded, "is_safe", is_safe, false );
    optional( jo, was_loaded, "freq_total", freq_total, -1 );
    optional( jo, was_loaded, "replace_monster_group", replace_monster_group, false );
    optional( jo, was_loaded, "new_monster_group_id", new_monster_group, mongroup_id::NULL_ID() );

    // This means we use legacy freq based logic
    // Or legacy auto total, but needs to go down the legacy load route
    optional( jo, was_loaded, "default", defaultMonster, mtype_id::NULL_ID() );
    use_automatic_sizing = jo.get_bool( "auto_total", freq_total == -1 );
    optional( jo, was_loaded, "monsters", monsters, mongroup_entry_reader( use_automatic_sizing ) );
    if( use_automatic_sizing ) {
        freq_total = 0;
        for( MonsterGroupEntry &mon : monsters ) {
            freq_total += mon.frequency;
        }
    }
}

void MonsterGroup::load_monster_groups( const JsonObject &jo, const std::string &src )
{
    all_monster_groups.load( jo, src );
}

void MonsterGroup::reset()
{
    all_monster_groups.reset();
    monster_blacklist.clear();
    monster_whitelist.clear();
    monster_whitelist_is_exclusive = false;
    monster_categories_blacklist.clear();
    monster_categories_whitelist.clear();
}

void MonsterGroup::finalize()
{
    for( FreqDef::iterator c = monsters.begin(); c != monsters.end(); ) {
        if( MonsterGroupManager::monster_is_blacklisted( c->name ) ) {
            c = monsters.erase( c );
        } else {
            ++c;
        }
    }
    if( MonsterGroupManager::monster_is_blacklisted( defaultMonster ) ) {
        defaultMonster = mtype_id::NULL_ID();
    }
}

void MonsterGroup::finalize_all()
{
    MonsterGroup null;
    null.id = mongroup_id::NULL_ID();
    null.defaultMonster = mtype_id::NULL_ID();
    all_monster_groups.insert( null );
    all_monster_groups.finalize();
}

std::vector<MonsterGroup> MonsterGroup::get_all()
{
    return all_monster_groups.get_all();
}

void MonsterGroup::check_consistency()
{
    for( auto &mtid : monster_whitelist ) {
        if( !mtype_id( mtid ).is_valid() ) {
            debugmsg( "monster on whitelist %s does not exist", mtid.c_str() );
        }
    }
    for( auto &mtid : monster_blacklist ) {
        if( !mtype_id( mtid ).is_valid() ) {
            debugmsg( "monster on blacklist %s does not exist", mtid.c_str() );
        }
    }
    all_monster_groups.check();
}

void MonsterGroup::check() const
{
    if( !defaultMonster.is_valid() && !defaultMonster.is_null() ) {
        debugmsg( "monster group %s has unknown non-null default monster %s", id.str(),
                  defaultMonster.str() );
    }
    for( const auto &mge : monsters ) {
        if( !mge.name.is_valid() && !mge.name.is_null() ) {
            // mon_null should not be valid here
            debugmsg( "monster group %s contains unknown monster %s", id.str(), mge.name.str() );
        }
        if( !mge.monster_group.is_valid() && !mge.monster_group.is_null() ) {
            // mon_null should not be valid here
            debugmsg( "monster group %s contains unknown monster group %s", id.str(), mge.monster_group.str() );
        }
        if( mge.monster_group.is_null() && mge.name.is_null() ) {
            debugmsg( "monster group %s contains no valid monster or monster group; How did you manage that?",
                      id.str() );
        }
    }

}
bool MonsterGroupManager::is_animal( const mongroup_id &group_name )
{
    const MonsterGroup *groupptr = &group_name.obj();
    return groupptr->is_animal;
}

const mtype_id &MonsterGroupManager::GetRandomMonsterFromGroup( const mongroup_id &group_name )
{
    const auto &group = group_name.obj();
    int spawn_chance = rng( 1, group.freq_total ); //Default 1000 unless specified
    for( const auto &monster_type : group.monsters ) {
        if( monster_type.frequency >= spawn_chance ) {
            if( !monster_type.monster_group.is_null() ) {
                return GetRandomMonsterFromGroup( monster_type.monster_group );
            } else {
                return monster_type.name;
            }
        } else {
            spawn_chance -= monster_type.frequency;
        }
    }

    return group.defaultMonster;
}
