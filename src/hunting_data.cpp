#include "hunting_data.h"

#include <algorithm>
#include <cmath>

#include "calendar.h"
#include "character.h"
#include "generic_factory.h"
#include "item.h"
#include "itype.h"
#include "json.h"
#include "map.h"
#include "options.h"
#include "overmap.h"
#include "overmapbuffer.h"
#include "player.h"
#include "rng.h"
#include "skill.h"
#include "translations.h"
#include "weighted_list.h"

static const skill_id skill_survival( "survival" );
static const skill_id skill_traps( "traps" );

namespace
{
generic_factory<hunting::snaring_hunting_data> hunting_data_factory( "hunting_data" );
hunting::omt_population_tracker population_tracker;
} // namespace

namespace hunting
{

// prey_entry implementation
void prey_entry::deserialize( const JsonObject &jo )
{
    jo.read( "prey", prey );
    jo.read( "weight", weight );
}

// habitat_prey_data implementation
void habitat_prey_data::deserialize( const JsonObject &jo )
{
    for( const JsonMember member : jo ) {
        const std::string &prey_type = member.name(); // e.g., "SMALL_GAME", "LARGE_GAME"
        std::vector<prey_entry> entries;

        for( JsonObject prey_obj : member.get_array() ) {
            prey_entry entry;
            entry.deserialize( prey_obj );
            entries.push_back( entry );
        }

        prey_types[prey_type] = entries;
    }
}

// snaring_hunting_data implementation
void snaring_hunting_data::load( const JsonObject &jo, const std::string & )
{
    deserialize( jo );
}

void snaring_hunting_data::deserialize( const JsonObject &jo )
{
    mandatory( jo, false, "id", id );

    if( jo.has_object( "habitats" ) ) {
        JsonObject habitats_obj = jo.get_object( "habitats" );
        for( const JsonMember member : habitats_obj ) {
            habitat_prey_data data;
            JsonObject habitat_jo = member.get_object();
            data.deserialize( habitat_jo );
            habitats[member.name()] = data;
        }
    }
}

void snaring_hunting_data::check() const
{
    // Validate habitats and prey data
    for( const auto &[habitat_name, prey_data] : habitats ) {
        for( const auto &[prey_type, entries] : prey_data.prey_types ) {
            for( const auto &prey : entries ) {
                // Allow "none" as explicit failure marker
                if( prey.prey.str() != "none" && !prey.prey.is_valid() ) {
                    debugmsg( "Invalid prey '%s' in type '%s', habitat '%s' for hunting_data '%s'",
                              prey.prey.c_str(), prey_type, habitat_name, id.c_str() );
                }
            }
        }
    }
}

void snaring_hunting_data::load_hunting_data( const JsonObject &jo, const std::string &src )
{
    hunting_data_factory.load( jo, src );
}

void snaring_hunting_data::reset()
{
    hunting_data_factory.reset();
}

void snaring_hunting_data::check_consistency()
{
    hunting_data_factory.check();
}

const std::vector<snaring_hunting_data> &snaring_hunting_data::get_all()
{
    return hunting_data_factory.get_all();
}

// OMT population tracker
omt_population_tracker &get_population_tracker()
{
    return population_tracker;
}

void omt_population_tracker::record_visit( const tripoint &pos )
{
    tripoint_abs_omt omt( ms_to_omt_copy( get_map().getabs( pos ) ) );
    auto &data = omt_populations[omt];
    data.last_visit = calendar::turn;
}

void omt_population_tracker::apply_catch_penalty( const tripoint &pos )
{
    tripoint_abs_omt omt( ms_to_omt_copy( get_map().getabs( pos ) ) );
    auto &data = omt_populations[omt];
    data.population = std::max( 0, data.population - 10 );
    data.last_recovery = calendar::turn;
}

int omt_population_tracker::get_population( const tripoint &pos ) const
{
    tripoint_abs_omt omt( ms_to_omt_copy( get_map().getabs( pos ) ) );
    auto it = omt_populations.find( omt );
    if( it == omt_populations.end() ) {
        return 100; // Default population
    }
    return it->second.population;
}

void omt_population_tracker::process_recovery()
{
    time_point now = calendar::turn;
    for( auto &pair : omt_populations ) {
        auto &data = pair.second;
        if( data.population < 100 ) {
            time_duration since_recovery = now - data.last_recovery;
            if( since_recovery >= 1_days ) {
                int days = to_days<int>( since_recovery );
                data.population = std::min( 100, data.population + days * 5 );
                data.last_recovery = now;
            }
        }
    }
}

void omt_population_tracker::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "populations" );
    json.start_array();
    for( const auto &pair : omt_populations ) {
        json.start_object();
        json.member( "omt", pair.first );
        json.member( "pop", pair.second.population );
        json.member( "last_visit", pair.second.last_visit );
        json.member( "last_recovery", pair.second.last_recovery );
        json.end_object();
    }
    json.end_array();
    json.end_object();
}

void omt_population_tracker::deserialize( const JsonObject &jo )
{
    if( jo.has_array( "populations" ) ) {
        for( JsonObject pop_obj : jo.get_array( "populations" ) ) {
            tripoint_abs_omt omt;
            omt_data data;
            pop_obj.read( "omt", omt );
            pop_obj.read( "pop", data.population );
            pop_obj.read( "last_visit", data.last_visit );
            pop_obj.read( "last_recovery", data.last_recovery );
            omt_populations[omt] = data;
        }
    }
}

// Extract bait types from item's BAIT_XXX flags
// Returns vector of strings with "BAIT_" prefix removed
// e.g., "BAIT_SMALL_GAME" -> "SMALL_GAME"
std::vector<std::string> extract_bait_types( const item *bait_item )
{
    std::vector<std::string> bait_types;
    if( !bait_item ) {
        return bait_types;
    }

    for( const flag_id &flag : bait_item->type->get_flags() ) {
        const std::string &flag_str = flag.str();
        if( flag_str.starts_with( "BAIT_" ) ) {
            bait_types.push_back( flag_str.substr( 5 ) ); // Skip "BAIT_" prefix
        }
    }
    return bait_types;
}

// Find hunting_data by furniture at position
const snaring_hunting_data *find_hunting_data_for_furniture( const tripoint &pos )
{
    map &here = get_map();
    const furn_id &furn_at = here.furn( pos );
    std::string furn_str = furn_at.id().str();

    // Extract base furniture name (remove _empty/_set/_closed suffix)
    std::string base_name;
    if( furn_str.ends_with( "_empty" ) ) {
        base_name = furn_str.substr( 0, furn_str.length() - 6 );
    } else if( furn_str.ends_with( "_set" ) ) {
        base_name = furn_str.substr( 0, furn_str.length() - 4 );
    } else if( furn_str.ends_with( "_closed" ) ) {
        base_name = furn_str.substr( 0, furn_str.length() - 7 );
    } else {
        base_name = furn_str;
    }

    // Find hunting_data by id (which is the furniture base_name)
    snaring_hunting_data_id lookup_id( base_name );

    // Try to find exact match
    for( const auto &hd : snaring_hunting_data::get_all() ) {
        if( hd.id == lookup_id ) {
            return &hd;
        }
    }

    // No match found - use default_hunting as fallback
    for( const auto &hd : snaring_hunting_data::get_all() ) {
        if( hd.id.str() == "default_hunting" ) {
            return &hd;
        }
    }

    return nullptr;
}

// Skill multiplier calculation
float calculate_skill_multiplier( const player &p )
{
    int traps_skill = p.get_skill_level( skill_traps );
    int survival_skill = p.get_skill_level( skill_survival );

    // Base: 60% at 0/0, scales to 85% at 6/6
    float base = 0.60f;
    float skill_bonus = ( traps_skill + survival_skill ) * 0.0208f; // ~2.08% per level
    return std::min( 0.85f, base + skill_bonus );
}

// Presence penalty based on daily visits
int calculate_presence_penalty( const tripoint &pos )
{
    tripoint_abs_omt omt( ms_to_omt_copy( get_map().getabs( pos ) ) );
    auto &tracker = get_population_tracker();
    auto it = tracker.omt_populations.find( omt );

    if( it == tracker.omt_populations.end() ) {
        return 0;
    }

    time_duration since_visit = calendar::turn - it->second.last_visit;
    int hours_since = to_hours<int>( since_visit );

    // Heavy penalty if visited within 24 hours, decays over time
    if( hours_since < 24 ) {
        return 40; // -40% if very recent
    } else if( hours_since < 48 ) {
        return 20; // -20% if within 2 days
    } else if( hours_since < 72 ) {
        return 10; // -10% if within 3 days
    }
    return 0;
}

// Main snare check function
snare_result check_snare( const tripoint &pos, const std::vector<std::string> &bait_flags_str,
                          const player &p, int proximity_penalty )
{
    snare_result result;

    // Get hunting data for this furniture
    const snaring_hunting_data *data = find_hunting_data_for_furniture( pos );

    if( !data ) {
        result.message = _( "No hunting data available." );
        return result;
    }

    // Determine habitat based on OMT terrain (check 3x3 area around trap)
    map &here = get_map();
    tripoint_abs_omt center_omt( ms_to_omt_copy( here.getabs( pos ) ) );

    // Count habitat types in 3x3 OMT area using land_use_code
    std::map<std::string, int> habitat_weights;
    for( int dx = -1; dx <= 1; dx++ ) {
        for( int dy = -1; dy <= 1; dy++ ) {
            tripoint_abs_omt check_pos = center_omt + tripoint( dx, dy, 0 );
            const oter_id &omt_ter = overmap_buffer.ter( check_pos );

            overmap_land_use_code_id land_use = omt_ter->get_land_use_code();
            if( !land_use.is_valid() ) {
                continue; // Skip tiles without land_use_code
            }

            const std::string &habitat = land_use.str();
            habitat_weights[habitat]++;
        }
    }

    // Find dominant habitat
    std::string dominant_habitat = "forest";
    int max_weight = 0;
    for( const auto &[hab, weight] : habitat_weights ) {
        if( weight > max_weight ) {
            max_weight = weight;
            dominant_habitat = hab;
        }
    }

    auto habitat_it = data->habitats.find( dominant_habitat );
    if( habitat_it == data->habitats.end() ) {
        result.message = _( "This area doesn't support wildlife." );
        return result;
    }

    const habitat_prey_data &prey_data = habitat_it->second;

    // Determine prey pool based on bait flags (fully extensible system)
    // Extract prey type from BAIT_XXX flags and match with JSON keys
    std::vector<std::string> bait_types = bait_flags_str;
    std::vector<prey_entry> available_prey;

    for( const std::string &prey_type : bait_types ) {
        auto prey_it = prey_data.prey_types.find( prey_type );
        if( prey_it != prey_data.prey_types.end() ) {
            available_prey.insert( available_prey.end(),
                                   prey_it->second.begin(), prey_it->second.end() );
        }
    }

    if( available_prey.empty() ) {
        result.message = _( "The snare was empty." );
        return result;
    }

    // Calculate success probability
    auto &tracker = get_population_tracker();
    int population = tracker.get_population( pos );
    float pop_factor = population / 50.0; // 0-2 range

    // Habitat bonus based on how many OMTs match
    float habitat_bonus = 1.0 + ( max_weight / 9.0 ) * 0.5; // 1.0-1.5 based on habitat concentration

    // Bait bonus based on variety
    float bait_bonus = bait_types.size() >= 2 ? 1.3 : 1.1;

    float skill_mult = calculate_skill_multiplier( p );
    int presence_pen = calculate_presence_penalty( pos );
    float presence_factor = 1.0 - ( presence_pen / 100.0 );
    float proximity_factor = 1.0 - ( proximity_penalty / 100.0 );

    float success_chance = pop_factor * habitat_bonus * bait_bonus *
                           skill_mult * presence_factor * proximity_factor * get_option<float>( "SNARING_RATE" );

    success_chance = std::clamp( success_chance, 0.0f, 0.95f ); // Max 95% success

    // Roll for success
    if( rng_float( 0, 1 ) <= success_chance ) {
        // Select random prey (including "none" for explicit failure)
        weighted_int_list<mtype_id> prey_list;
        for( const auto &entry : available_prey ) {
            prey_list.add( entry.prey, entry.weight );
        }

        mtype_id selected = *prey_list.pick();

        // Check if "none" was selected (explicit failure)
        if( selected.str() == "none" ) {
            result.message = _( "The snare was empty." );
        } else {
            result.success = true;
            result.prey_id = selected;
            result.message = string_format( _( "You caught a %s!" ), result.prey_id->nname() );

            // Apply penalty only on successful catch
            tracker.apply_catch_penalty( pos );
        }
    } else {
        result.message = _( "The snare was empty." );
    }

    return result;
}

// Process snare catch: run check_snare and generate corpse if successful
snare_catch_result process_snare_catch( const tripoint &pos,
                                        const std::vector<std::string> &bait_flags_str,
                                        const Character &ch, int proximity_penalty,
                                        time_point snared_time )
{
    snare_catch_result result;
    // Convert Character to player for check_snare
    const player &p = dynamic_cast<const player &>( ch );
    result.hunt_result = check_snare( pos, bait_flags_str, p, proximity_penalty );

    // Generate corpse if hunt was successful
    if( result.hunt_result.success && result.hunt_result.prey_id.is_valid() ) {
        result.corpse = item::make_corpse( result.hunt_result.prey_id, snared_time );
    }

    return result;
}

} // namespace hunting
