#include "catalua_creature_filters.h"

#include <functional>
#include <string>
#include <unordered_map>

#include "game.h"
#include "monster.h"
#include "type_id.h"
#include "profile.h"

using LuaValue = sol::basic_object<sol::basic_reference<>>;
using MonsterVec = std::vector<monster *>;

namespace
{
struct FilterContext {
    const monster *mon;
    MonsterVec &output;
    bool past_limit = false;
};
} // namespace

static bool filter_limit( FilterContext &context, size_t limit )
{
    if( context.output.size() > limit ) {
        context.past_limit = true;
        return false;
    }
    return true;
}

static bool filter_type_ids( const FilterContext &context, const std::unordered_set<mtype_id> &ids )
{
    return ids.contains( context.mon->type->id );
}

static bool filter_faction_ids( const FilterContext &context,
                                const std::unordered_set<mfaction_id> &ids )
{
    return ids.contains( context.mon->faction );
}

static bool filter_species_ids( const FilterContext &context,
                                const std::unordered_set<species_id> &ids )
{
    for( const auto &ms : context.mon->type->species ) {
        if( ids.contains( ms ) ) {
            return true;
        }
    }
    return false;
}

static bool filter_sees( const FilterContext &context, const std::vector<monster *> &mons )
{
    const auto mon_pos = context.mon->abs_pos();
    for( const auto &other_mon : mons ) {
        if( mon_pos != other_mon->abs_pos() && other_mon->sees( *context.mon ) ) {
            return true;
        }
    }
    return false;
}

static bool filter_within_range_of( const FilterContext &context, float range,
                                    const std::vector<monster *> other_monsters )
{
    auto mpos = context.mon->abs_pos();
    for( const auto &other_mon : other_monsters ) {
        if( mpos == other_mon->abs_pos() ) { continue; }
        if( abs( rl_dist_exact( mpos, other_mon->abs_pos() ) ) <= range ) {
            return true;
        }
    }
    return false;
}

static bool filter_hostile_to( const FilterContext &context, const std::vector<monster *> &mons )
{
    const auto mpos = context.mon->abs_pos();
    for( const auto &other_mon : mons ) {
        if( mpos == other_mon->abs_pos() ) { continue; }
        if( context.mon->attitude_to( *other_mon ) == A_HOSTILE ) {
            return true;
        }
    }
    return false;
}

static const
std::unordered_map<std::string, std::function<std::function<bool( FilterContext &context )>( LuaValue &val )>>
        handlers
= {
    {
        "limit", []( LuaValue & val ) -> std::function<bool( FilterContext &context )> {
            const auto limit_value = val.as<size_t>();
            return [limit_value]( FilterContext & context ) -> bool { return filter_limit( context, limit_value );};
        }
    },
    {
        "type_ids", []( LuaValue & val ) -> std::function<bool( FilterContext &context )> {
            const auto types = val.as<std::unordered_set<mtype_id>>();
            return [types]( FilterContext & context ) -> bool { return filter_type_ids( context, types );};
        }
    },
    {
        "faction_ids", []( LuaValue & val ) -> std::function<bool( FilterContext &context )> {
            const auto types = val.as<std::unordered_set<mfaction_id>>();
            return [types]( FilterContext & context ) -> bool { return filter_faction_ids( context, types );};
        }
    },
    {
        "species_ids", []( LuaValue & val ) -> std::function<bool( FilterContext &context )> {
            const auto types = val.as<std::unordered_set<species_id>>();
            return [types]( FilterContext & context ) -> bool { return filter_species_ids( context, types ); };
        }
    },
    {
        "sees", []( LuaValue & val ) -> std::function<bool( FilterContext &context )> {
            const auto types = val.as<std::vector<monster *>>();
            return [types]( FilterContext & context ) -> bool { return filter_sees( context, types ); };
        }
    },
    {
        "within_range_of", []( LuaValue & val ) -> std::function<bool( FilterContext &context )> {
            const auto values = val.as<sol::table>();
            const auto range = values["range"].get<float>();
            const auto mons = values["monsters"].get<std::vector<monster *>>();
            return [range, mons]( FilterContext & context ) -> bool { return filter_within_range_of( context, range, mons ); };
        }
    },
    {
        "hostile_to", []( LuaValue & val ) -> std::function<bool( FilterContext &context )> {
            const auto types = val.as<std::vector<monster *>>();
            return [types]( FilterContext & context ) -> bool { return filter_hostile_to( context, types ); };
        }
    },
};

std::vector<monster *> filter_monsters_from_lua( const sol::table &filters )
{
    ZoneScoped;
    std::vector<monster *> monsters;
    FilterContext context = {
        .mon = nullptr,
        .output = monsters
    };
    std::vector<std::function<bool( FilterContext &context )>> all_filters;
    for( auto &&[key, value] : filters ) {
        auto str_key = key.as<std::string>();
        if( auto it = handlers.find( str_key ); it != handlers.end() ) {
            all_filters.push_back( it->second( value ) );
        } else {
            debugmsg( "Unknown filter %s", str_key.c_str() );
        }
    }
    if( const auto rng = g->all_monsters(); rng.items ) {
        for( const auto &wp : *rng.items ) {
            const auto sp = std::static_pointer_cast<monster>( wp.lock() );
            if( !sp ) { continue; }

            const monster *mon = sp.get();
            context.mon = mon;
            bool matching = true;
            for( auto filter : all_filters ) {
                if( !filter( context ) ) {
                    matching = false;
                    break;
                }
            }
            if( matching ) {
                monsters.push_back( sp.get() );
            }
            if( context.past_limit ) {
                break;
            }
        }
    }
    return monsters;
}
