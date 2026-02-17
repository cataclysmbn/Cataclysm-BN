#include "character_turn.h"

#include "avatar.h"
#include "bionics.h"
#include "calendar.h"
#include "catalua_hooks.h"
#include "catalua_sol.h"
#include "character_effects.h"
#include "character_functions.h"
#include "character_stat.h"
#include "character_martial_arts.h"
#include "character.h"
#include "creature.h"
#include "flag.h"
#include "flag_trait.h"
#include "game.h"
#include "handle_liquid.h"
#include "itype.h"
#include "iuse.h"
#include "magic_enchantment.h"
#include "mutation.h"
#include "overmapbuffer.h"
#include "make_static.h"
#include "map_iterator.h"
#include "morale.h"
#include "player.h"
#include "player_activity.h"
#include "rng.h"
#include "submap.h"
#include "trap.h"
#include "type_id.h"
#include "units_temperature.h"
#include "veh_type.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vpart_position.h"
#include "weather_gen.h"
#include "weather.h"
#include "profile.h"

static const trait_id trait_ACIDBLOOD( "ACIDBLOOD" );
static const trait_id trait_ARACHNID_ARMS_OK( "ARACHNID_ARMS_OK" );
static const trait_id trait_ARACHNID_ARMS( "ARACHNID_ARMS" );
static const trait_id trait_CHITIN_FUR( "CHITIN_FUR" );
static const trait_id trait_CHITIN_FUR2( "CHITIN_FUR2" );
static const trait_id trait_CHITIN_FUR3( "CHITIN_FUR3" );
static const trait_id trait_CHITIN2( "CHITIN2" );
static const trait_id trait_CHITIN3( "CHITIN3" );
static const trait_id trait_COLDBLOOD4( "COLDBLOOD4" );
static const trait_id trait_COMPOUND_EYES( "COMPOUND_EYES" );
static const trait_id trait_EATHEALTH( "EATHEALTH" );
static const trait_id trait_FAT( "FAT" );
static const trait_id trait_FELINE_FUR( "FELINE_FUR" );
static const trait_id trait_FUR( "FUR" );
static const trait_id trait_INSECT_ARMS_OK( "INSECT_ARMS_OK" );
static const trait_id trait_INSECT_ARMS( "INSECT_ARMS" );
static const trait_id trait_LIGHTFUR( "LIGHTFUR" );
static const trait_id trait_LUPINE_FUR( "LUPINE_FUR" );
static const trait_id trait_M_IMMUNE( "M_IMMUNE" );
static const trait_id trait_NOMAD( "NOMAD" );
static const trait_id trait_NOMAD2( "NOMAD2" );
static const trait_id trait_NOMAD3( "NOMAD3" );
static const trait_id trait_PARAIMMUNE( "PARAIMMUNE" );
static const trait_id trait_SLIMY( "SLIMY" );
static const trait_id trait_STIMBOOST( "STIMBOOST" );
static const trait_id trait_SUNLIGHT_DEPENDENT( "SUNLIGHT_DEPENDENT" );
static const trait_id trait_THICK_SCALES( "THICK_SCALES" );
static const trait_id trait_URSINE_FUR( "URSINE_FUR" );
static const trait_id trait_WEBBED( "WEBBED" );
static const trait_id trait_WHISKERS_RAT( "WHISKERS_RAT" );
static const trait_id trait_WHISKERS( "WHISKERS" );
static const trait_id trait_DEBUG_STORAGE( "DEBUG_STORAGE" );

static const trait_flag_str_id trait_flag_MUTATION_FLIGHT( "MUTATION_FLIGHT" );

static const efftype_id effect_bloodworms( "bloodworms" );
static const efftype_id effect_brainworms( "brainworms" );
static const efftype_id effect_darkness( "darkness" );
static const efftype_id effect_depressants( "depressants" );
static const efftype_id effect_dermatik( "dermatik" );
static const efftype_id effect_downed( "downed" );
static const efftype_id effect_fungus( "fungus" );
static const efftype_id effect_happy( "happy" );
static const efftype_id effect_irradiated( "irradiated" );
static const efftype_id effect_masked_scent( "masked_scent" );
static const efftype_id effect_narcosis( "narcosis" );
static const efftype_id effect_onfire( "onfire" );
static const efftype_id effect_paincysts( "paincysts" );
static const efftype_id effect_pkill( "pkill" );
static const efftype_id effect_sad( "sad" );
static const efftype_id effect_sleep_deprived( "sleep_deprived" );
static const efftype_id effect_sleep( "sleep" );
static const efftype_id effect_stim_overdose( "stim_overdose" );
static const efftype_id effect_stim( "stim" );
static const efftype_id effect_tapeworm( "tapeworm" );
static const efftype_id effect_thirsty( "thirsty" );

static const skill_id skill_swimming( "swimming" );
static const skill_id skill_traps( "traps" );

static const bionic_id bio_ground_sonar( "bio_ground_sonar" );
static const bionic_id bio_hydraulics( "bio_hydraulics" );
static const bionic_id bio_speed( "bio_speed" );

static const itype_id itype_UPS( "UPS" );
static const itype_id itype_battery( "battery" );

void Character::recalc_speed_bonus()
{
    // Minus some for weight...
    // Easy test, if Character DOES have this trait then we don't need to check weight carried/capacity
    if( !has_trait( trait_DEBUG_STORAGE ) ) {
        // these are nontrivial calculations, store in variables so they aren't calculated multiple times.
        auto carried = weight_carried();
        auto capacity = weight_capacity();
        if( carried > capacity ) {
            mod_speed_bonus( -25 * ( carried - capacity ) / capacity );
        }
    }
    mod_speed_bonus( -character_effects::get_pain_penalty( *this ).speed );

    if( get_thirst() > thirst_levels::very_thirsty ) {
        mod_speed_bonus( character_effects::get_thirst_speed_penalty( get_thirst() ) );
    }
    // when underweight, you get slower. cumulative with hunger
    mod_speed_bonus( character_effects::get_kcal_speed_penalty( get_kcal_percent() ) );

    for( const auto &maps : *effects ) {
        for( auto &i : maps.second ) {
            if( i.second.is_removed() ) {
                continue;
            }
            bool reduced = resists_effect( i.second );
            mod_speed_bonus( i.second.get_mod( "SPEED", reduced ) );
        }
    }

    // add martial arts speed bonus
    mod_speed_bonus( mabuff_speed_bonus() );

    // Not sure why Sunlight Dependent is here, but OK
    // Ectothermic/COLDBLOOD4 is intended to buff folks in the Summer
    // Threshold-crossing has its charms ;-)
    if( g != nullptr ) {
        if( has_trait( trait_SUNLIGHT_DEPENDENT ) && !g->is_in_sunlight( pos() ) ) {
            mod_speed_bonus( -( g->light_level( posz() ) >= 12 ? 5 : 10 ) );
        }
        const float temperature_speed_modifier = mutation_value( "temperature_speed_modifier" );
        if( temperature_speed_modifier != 0 ) {
            const auto player_local_temp = units::to_fahrenheit( get_weather().get_temperature( pos() ) );
            if( has_trait( trait_COLDBLOOD4 ) || player_local_temp < 65 ) {
                mod_speed_bonus( ( player_local_temp - 65 ) * temperature_speed_modifier );
            }
        }
    }

    if( has_artifact_with( AEP_SPEED_UP ) ) {
        mod_speed_bonus( 20 );
    }
    if( has_artifact_with( AEP_SPEED_DOWN ) ) {
        mod_speed_bonus( -20 );
    }

    float speed_modifier = Character::mutation_value( "speed_modifier" );
    mod_speed_mult( speed_modifier - 1 );

    if( has_bionic( bio_speed ) ) { // add 10% speed bonus
        mod_speed_mult( 0.1 );
    }

    double ench_bonus = enchantment_cache->calc_bonus( enchant_vals::mod::SPEED, get_speed() );
    mod_speed_bonus( ench_bonus );
}

void Character::process_turn()
{
    // Has to happen before reset_stats
    clear_miss_reasons();

    for( bionic &i : get_bionic_collection() ) {
        if( i.incapacitated_time > 0_turns ) {
            i.incapacitated_time -= 1_turns;
            if( i.incapacitated_time == 0_turns ) {
                add_msg_if_player( m_bad, _( "Your %s bionic comes back online." ), i.info().name );
            }
        }
    }

    Creature::process_turn();

    // If we're actively handling something we can't just drop it on the ground
    // in the middle of handling it
    if( activity->targets.empty() ) {
        drop_invalid_inventory();
    }
    process_items();
    // Didn't just pick something up
    last_item = itype_id( "null" );

    suffer();

    // Handle player and NPC morale ticks

    if( calendar::once_every( 1_minutes ) ) {
        update_morale();
    }

    if( calendar::once_every( 9_turns ) ) {
        check_and_recover_morale();
    }

    // NPCs currently don't make any use of their scent, pointless to calculate it
    // TODO: make use of NPC scent.
    if( !is_npc() ) {
        if( !has_effect( effect_masked_scent ) ) {
            restore_scent();
        }
        const int mask_intensity = get_effect_int( effect_masked_scent );

        // Set our scent towards the norm
        int norm_scent = 500;
        int temp_norm_scent = INT_MIN;
        bool found_intensity = false;
        for( const trait_id &mut : get_mutations() ) {
            const std::optional<int> &scent_intensity = mut->scent_intensity;
            if( scent_intensity ) {
                found_intensity = true;
                temp_norm_scent = std::max( temp_norm_scent, *scent_intensity );
            }
        }
        if( found_intensity ) {
            norm_scent = temp_norm_scent;
        }

        for( const trait_id &mut : get_mutations() ) {
            const std::optional<int> &scent_mask = mut->scent_mask;
            if( scent_mask ) {
                norm_scent += *scent_mask;
            }
        }

        // Mask from scent altering items;
        norm_scent += mask_intensity;

        // Now that all the additions are done, multiply norm_scent by any scent modifiers.
        for( const trait_id &mut : get_mutations() ) {
            norm_scent *= mut.obj().scent_modifier;
        }

        // Scent increases fast at first, and slows down as it approaches normal levels.
        // Estimate it will take about norm_scent * 2 turns to go from 0 - norm_scent / 2
        // Without smelly trait this is about 1.5 hrs. Slows down significantly after that.
        if( scent < rng( 0, norm_scent ) ) {
            scent++;
        }

        // Unusually high scent decreases steadily until it reaches normal levels.
        if( scent > norm_scent ) {
            scent--;
        }
    }

    // We can dodge again! Assuming we can actually move...
    if( in_sleep_state() ) {
        blocks_left = 0;
        dodges_left = 0;
    } else if( moves > 0 ) {
        blocks_left = get_num_blocks();
        dodges_left = get_num_dodges();
    }

    // auto-learning. This is here because skill-increases happens all over the place:
    // SkillLevel::readBook (has no connection to the skill or the player),
    // player::read, player::practice, ...
    // Check for spontaneous discovery of martial art styles
    for( auto &style : autolearn_martialart_types() ) {
        const matype_id &ma( style );

        if( !martial_arts_data->has_martialart( ma ) && can_autolearn_martial_art( *this, ma ) ) {
            martial_arts_data->add_martialart( ma );
            add_msg_if_player( m_info, _( "You have learned a new style: %s!" ), ma.obj().name );
        }
    }

    // Update time spent conscious in this overmap tile for the Nomad traits.
    if( !is_npc() && ( has_trait( trait_NOMAD ) || has_trait( trait_NOMAD2 ) ||
                       has_trait( trait_NOMAD3 ) ) &&
        !has_effect( effect_sleep ) && !has_effect( effect_narcosis ) ) {
        const tripoint_abs_omt ompos = global_omt_location();
        const point_abs_omt pos = ompos.xy();
        if( !overmap_time.contains( pos ) ) {
            overmap_time[pos] = 1_turns;
        } else {
            overmap_time[pos] += 1_turns;
        }
    }
    // Decay time spent in other overmap tiles.
    if( !is_npc() && calendar::once_every( 1_hours ) ) {
        const tripoint_abs_omt ompos = global_omt_location();
        const time_point now = calendar::turn;
        time_duration decay_time = 0_days;
        if( has_trait( trait_NOMAD ) ) {
            decay_time = 7_days;
        } else if( has_trait( trait_NOMAD2 ) ) {
            decay_time = 14_days;
        } else if( has_trait( trait_NOMAD3 ) ) {
            decay_time = 28_days;
        }
        auto it = overmap_time.begin();
        while( it != overmap_time.end() ) {
            if( it->first == ompos.xy() ) {
                it++;
                continue;
            }
            // Find the amount of time passed since the player touched any of the overmap tile's submaps.
            const tripoint_abs_omt tpt( it->first, 0 );
            const time_point last_touched = overmap_buffer.scent_at( tpt ).creation_time;
            const time_duration since_visit = now - last_touched;
            // If the player has spent little time in this overmap tile, let it decay after just an hour instead of the usual extended decay time.
            const time_duration modified_decay_time = it->second > 5_minutes ? decay_time : 1_hours;
            if( since_visit > modified_decay_time ) {
                // Reduce the tracked time spent in this overmap tile.
                const time_duration decay_amount = std::min( since_visit - modified_decay_time, 1_hours );
                const time_duration updated_value = it->second - decay_amount;
                if( updated_value <= 0_turns ) {
                    // We can stop tracking this tile if there's no longer any time recorded there.
                    it = overmap_time.erase( it );
                    continue;
                } else {
                    it->second = updated_value;
                }
            }
            it++;
        }
    }
}

void Character::process_one_effect( effect &it, bool is_new )
{
    bool reduced = resists_effect( it );
    double mod = 1;
    bodypart_str_id bp = it.get_bp();
    int val = 0;

    // Still hardcoded stuff, do this first since some modify their other traits
    hardcoded_effects( it );

    const auto get_effect = [&it, is_new]( const std::string & arg, bool reduced ) {
        if( is_new ) {
            return it.get_amount( arg, reduced );
        }
        return it.get_mod( arg, reduced );
    };

    // Handle miss messages
    auto msgs = it.get_miss_msgs();
    if( !msgs.empty() ) {
        for( const auto &i : msgs ) {
            add_miss_reason( _( i.first ), static_cast<unsigned>( i.second ) );
        }
    }

    // Handle health mod
    val = get_effect( "H_MOD", reduced );
    if( val != 0 ) {
        mod = 1;
        if( is_new || it.activated( calendar::turn, "H_MOD", val, reduced, mod ) ) {
            int bounded = bound_mod_to_vals(
                              get_healthy_mod(), val, it.get_max_val( "H_MOD", reduced ),
                              it.get_min_val( "H_MOD", reduced ) );
            // This already applies bounds, so we pass them through.
            mod_healthy_mod( bounded, get_healthy_mod() + bounded );
        }
    }

    // Handle health
    val = get_effect( "HEALTH", reduced );
    if( val != 0 ) {
        mod = 1;
        if( is_new || it.activated( calendar::turn, "HEALTH", val, reduced, mod ) ) {
            mod_healthy( bound_mod_to_vals( get_healthy(), val,
                                            it.get_max_val( "HEALTH", reduced ), it.get_min_val( "HEALTH", reduced ) ) );
        }
    }

    // Handle stim
    val = get_effect( "STIM", reduced );
    if( val != 0 ) {
        mod = 1;
        if( is_new || it.activated( calendar::turn, "STIM", val, reduced, mod ) ) {
            mod_stim( bound_mod_to_vals( get_stim(), val, it.get_max_val( "STIM", reduced ),
                                         it.get_min_val( "STIM", reduced ) ) );
        }
    }

    // Handle hunger
    val = get_effect( "HUNGER", reduced );
    if( val != 0 ) {
        mod = 1;
        if( is_new || it.activated( calendar::turn, "HUNGER", val, reduced, mod ) ) {
            mod_stored_kcal( -10 * bound_mod_to_vals( ( max_stored_kcal() - get_stored_kcal() ) / 10,
                             val, it.get_max_val( "HUNGER", reduced ), it.get_min_val( "HUNGER", reduced ) ) );
        }
    }

    // Handle thirst
    val = get_effect( "THIRST", reduced );
    if( val != 0 ) {
        mod = 1;
        if( is_new || it.activated( calendar::turn, "THIRST", val, reduced, mod ) ) {
            mod_thirst( bound_mod_to_vals( get_thirst(), val, it.get_max_val( "THIRST", reduced ),
                                           it.get_min_val( "THIRST", reduced ) ) );
        }
    }

    // Handle fatigue
    val = get_effect( "FATIGUE", reduced );
    // Prevent ongoing fatigue effects while asleep.
    // These are meant to change how fast you get tired, not how long you sleep.
    if( val != 0 && !in_sleep_state() ) {
        mod = 1;
        if( is_new || it.activated( calendar::turn, "FATIGUE", val, reduced, mod ) ) {
            mod_fatigue( bound_mod_to_vals( get_fatigue(), val, it.get_max_val( "FATIGUE", reduced ),
                                            it.get_min_val( "FATIGUE", reduced ) ) );
        }
    }

    // Handle sleep debt
    val = get_effect( "SLEEPDEBT", reduced );
    if( val != 0 ) {
        mod = 1;
        if( is_new || it.activated( calendar::turn, "SLEEPDEBT", val, reduced, mod ) ) {
            mod_sleep_deprivation( bound_mod_to_vals( get_sleep_deprivation(), val, it.get_max_val( "SLEEPDEBT",
                                   reduced ),
                                   it.get_min_val( "SLEEPDEBT", reduced ) ) );
        }
    }

    // Handle Radiation
    val = get_effect( "RAD", reduced );
    if( val != 0 ) {
        mod = 1;
        if( is_new || it.activated( calendar::turn, "RAD", val, reduced, mod ) ) {
            mod_rad( bound_mod_to_vals( get_rad(), val, it.get_max_val( "RAD", reduced ), 0 ) );
            // Radiation can't go negative
            if( get_rad() < 0 ) {
                set_rad( 0 );
            }
        }
    }

    // Handle Pain
    val = get_effect( "PAIN", reduced );
    if( val != 0 ) {
        mod = 1;
        if( it.get_sizing( "PAIN" ) ) {
            if( has_trait( trait_FAT ) ) {
                mod *= 1.5;
            }
            if( get_size() == creature_size::large ) {
                mod *= 2;
            }
            if( get_size() == creature_size::huge ) {
                mod *= 3;
            }
        }
        if( is_new || it.activated( calendar::turn, "PAIN", val, reduced, mod ) ) {
            int pain_inc = bound_mod_to_vals( get_pain(), val, it.get_max_val( "PAIN", reduced ), 0 );
            mod_pain( pain_inc );
            if( pain_inc > 0 ) {
                character_funcs::add_pain_msg( *this, val, bp );
            }
        }
    }

    // Handle Damage
    val = get_effect( "HURT", reduced );
    if( val != 0 ) {
        mod = 1;
        if( it.get_sizing( "HURT" ) ) {
            if( has_trait( trait_FAT ) ) {
                mod *= 1.5;
            }
            if( get_size() == creature_size::large ) {
                mod *= 2;
            }
            if( get_size() == creature_size::huge ) {
                mod *= 3;
            }
        }
        if( is_new || it.activated( calendar::turn, "HURT", val, reduced, mod ) ) {
            if( !bp ) {
                if( val > 5 ) {
                    add_msg_if_player( _( "Your %s HURTS!" ), body_part_name_accusative( bp_torso ) );
                } else if( val > 0 ) {
                    add_msg_if_player( _( "Your %s hurts!" ), body_part_name_accusative( bp_torso ) );
                }
                apply_damage( nullptr, bodypart_id( "torso" ), val, true );
            } else {
                if( val > 5 ) {
                    add_msg_if_player( _( "Your %s HURTS!" ), body_part_name_accusative( bp ) );
                } else if( val > 0 ) {
                    add_msg_if_player( _( "Your %s hurts!" ), body_part_name_accusative( bp ) );
                }
                apply_damage( nullptr, bp.id(), val, true );
            }
        }
    }

    // Handle Sleep
    val = get_effect( "SLEEP", reduced );
    if( val != 0 ) {
        mod = 1;
        if( ( is_new || it.activated( calendar::turn, "SLEEP", val, reduced, mod ) ) &&
            !has_effect( efftype_id( "sleep" ) ) ) {
            add_msg_if_player( _( "You pass out!" ) );
            fall_asleep( time_duration::from_turns( val ) );
        }
    }

    // Handle painkillers
    val = get_effect( "PKILL", reduced );
    if( val != 0 ) {
        mod = it.get_addict_mod( "PKILL", addiction_level( add_type::PKILLER ) );
        if( is_new || it.activated( calendar::turn, "PKILL", val, reduced, mod ) ) {
            mod_painkiller( bound_mod_to_vals( get_painkiller(), val, it.get_max_val( "PKILL", reduced ), 0 ) );
        }
    }

    // Handle coughing
    mod = 1;
    val = 0;
    if( it.activated( calendar::turn, "COUGH", val, reduced, mod ) ) {
        cough( it.get_harmful_cough() );
    }

    // Handle vomiting
    mod = character_effects::vomit_mod( *this );
    val = 0;
    if( it.activated( calendar::turn, "VOMIT", val, reduced, mod ) ) {
        vomit();
    }

    // Handle stamina
    val = get_effect( "STAMINA", reduced );
    if( val != 0 ) {
        mod = 1;
        if( is_new || it.activated( calendar::turn, "STAMINA", val, reduced, mod ) ) {
            mod_stamina( bound_mod_to_vals( get_stamina(), val,
                                            it.get_max_val( "STAMINA", reduced ),
                                            it.get_min_val( "STAMINA", reduced ) ), false );
        }
    }

    // Speed and stats are handled in recalc_speed_bonus and reset_stats respectively

    if( is_new && it.has_flag( flag_EFFECT_LUA_ON_ADDED ) ) {
        cata::run_hooks( "on_character_effect_added", [ &, this ]( auto & params ) {
            params["char"] = this;
            params["effect"] = &it;
        } );
    }

    if( it.has_flag( flag_EFFECT_LUA_ON_TICK ) ) {
        cata::run_hooks( "on_character_effect", [ &, this ]( auto & params ) {
            params["char"] = this;
            params["effect"] = &it;
        } );
    }
}

void Character::process_effects_internal()
{
    //Special Removals
    if( has_effect( effect_darkness ) && g->is_in_sunlight( pos() ) ) {
        remove_effect( effect_darkness );
    }
    if( has_trait( trait_M_IMMUNE ) && has_effect( effect_fungus ) ) {
        vomit();
        remove_effect( effect_fungus );
        add_msg_if_player( m_bad, _( "We have mistakenly colonized a local guide!  Purging now." ) );
    }
    if( has_trait( trait_PARAIMMUNE ) && ( has_effect( effect_dermatik ) ||
                                           has_effect( effect_tapeworm ) ||
                                           has_effect( effect_bloodworms ) ||
                                           has_effect( effect_brainworms ) ||
                                           has_effect( effect_paincysts ) ) ) {
        remove_effect( effect_dermatik );
        remove_effect( effect_tapeworm );
        remove_effect( effect_bloodworms );
        remove_effect( effect_brainworms );
        remove_effect( effect_paincysts );
        add_msg_if_player( m_good, _( "Something writhes and inside of you as it dies." ) );
    }
    if( has_trait( trait_ACIDBLOOD ) && ( has_effect( effect_dermatik ) ||
                                          has_effect( effect_bloodworms ) ||
                                          has_effect( effect_brainworms ) ) ) {
        remove_effect( effect_dermatik );
        remove_effect( effect_bloodworms );
        remove_effect( effect_brainworms );
    }
    if( has_trait( trait_EATHEALTH ) && has_effect( effect_tapeworm ) ) {
        remove_effect( effect_tapeworm );
        add_msg_if_player( m_good, _( "Your bowels gurgle as something inside them dies." ) );
    }

    //Human only effects
    for( auto &elem : *effects ) {
        for( auto &_effect_it : elem.second ) {
            if( !_effect_it.second.is_removed() ) {
                process_one_effect( _effect_it.second, false );
            }
        }
    }
}

void Character::reset_stats()
{
    const int current_stim = get_stim();

    // Trait / mutation buffs
    if( has_trait( trait_THICK_SCALES ) ) {
        add_miss_reason( _( "Your thick scales get in the way." ), 2 );
    }
    if( has_trait( trait_CHITIN2 ) || has_trait( trait_CHITIN3 ) || has_trait( trait_CHITIN_FUR3 ) ) {
        add_miss_reason( _( "Your chitin gets in the way." ), 1 );
    }
    if( has_trait( trait_COMPOUND_EYES ) && !wearing_something_on( bodypart_id( "eyes" ) ) ) {
        mod_per_bonus( 2 );
    }
    if( has_trait( trait_INSECT_ARMS ) ) {
        add_miss_reason( _( "Your insect limbs get in the way." ), 2 );
    }
    if( has_trait( trait_INSECT_ARMS_OK ) ) {
        if( !wearing_something_on( bodypart_id( "torso" ) ) ) {
            mod_dex_bonus( 1 );
        } else {
            mod_dex_bonus( -1 );
            add_miss_reason( _( "Your clothing restricts your insect arms." ), 1 );
        }
    }
    if( has_trait( trait_WEBBED ) ) {
        add_miss_reason( _( "Your webbed hands get in the way." ), 1 );
    }
    if( has_trait( trait_ARACHNID_ARMS ) ) {
        add_miss_reason( _( "Your arachnid limbs get in the way." ), 4 );
    }
    if( has_trait( trait_ARACHNID_ARMS_OK ) ) {
        if( !wearing_something_on( bodypart_id( "torso" ) ) ) {
            mod_dex_bonus( 2 );
        } else if( !exclusive_flag_coverage( STATIC( flag_id( "OVERSIZE" ) ) )
                   .test( STATIC( bodypart_str_id( "torso" ) ) ) ) {
            mod_dex_bonus( -2 );
            add_miss_reason( _( "Your clothing constricts your arachnid limbs." ), 2 );
        }
    }
    const auto set_fake_effect_dur = [this]( const efftype_id & type, const time_duration & dur ) {
        effect &eff = get_effect( type );
        if( eff.get_duration() == dur ) {
            return;
        }

        if( eff.is_null() && dur > 0_turns ) {
            add_effect( type, dur, bodypart_str_id::NULL_ID() );
        } else if( dur > 0_turns ) {
            eff.set_duration( dur );
        } else {
            remove_effect( type );
        }
    };
    // Painkiller
    set_fake_effect_dur( effect_pkill, 1_turns * get_painkiller() );

    // Pain
    if( get_perceived_pain() > 0 ) {
        const stat_mod ppen = character_effects::get_pain_penalty( *this );
        mod_str_bonus( -ppen.strength );
        mod_dex_bonus( -ppen.dexterity );
        mod_int_bonus( -ppen.intelligence );
        mod_per_bonus( -ppen.perception );
        if( ppen.dexterity > 0 ) {
            add_miss_reason( _( "Your pain distracts you!" ), static_cast<unsigned>( ppen.dexterity ) );
        }
    }

    // Radiation
    set_fake_effect_dur( effect_irradiated, 1_turns * get_rad() );
    // Morale
    const int morale = get_morale_level();
    set_fake_effect_dur( effect_happy, 1_turns * morale );
    set_fake_effect_dur( effect_sad, 1_turns * -morale );

    // Stimulants
    set_fake_effect_dur( effect_stim, 1_turns * current_stim );
    set_fake_effect_dur( effect_depressants, 1_turns * -current_stim );
    if( has_trait( trait_STIMBOOST ) ) {
        set_fake_effect_dur( effect_stim_overdose, 1_turns * ( current_stim - 60 ) );
    } else {
        set_fake_effect_dur( effect_stim_overdose, 1_turns * ( current_stim - 30 ) );
    }
    // Starvation
    if( get_kcal_percent() < 0.95f ) {
        // kcal->percentage of base str
        static const std::vector<std::pair<float, float>> starv_thresholds = { {
                std::make_pair( 0.0f, 0.5f ),
                std::make_pair( 0.8f, 0.1f ),
                std::make_pair( 0.95f, 0.0f )
            }
        };

        const int str_penalty = std::floor( multi_lerp( starv_thresholds, get_kcal_percent() ) );
        add_miss_reason( _( "You're weak from hunger." ),
                         static_cast<unsigned>( str_penalty / 2 ) );
        mod_str_bonus( -str_penalty );
        mod_dex_bonus( -( str_penalty / 2 ) );
        mod_int_bonus( -( str_penalty / 2 ) );
    }
    // Thirst
    set_fake_effect_dur( effect_thirsty, 1_turns * ( get_thirst() - thirst_levels::very_thirsty ) );
    if( get_sleep_deprivation() >= sleep_deprivation_levels::harmless ) {
        set_fake_effect_dur( effect_sleep_deprived, 1_turns * get_sleep_deprivation() );
    } else if( has_effect( effect_sleep_deprived ) ) {
        remove_effect( effect_sleep_deprived );
    }

    // Dodge-related effects
    mod_dodge_bonus( mabuff_dodge_bonus()
                     - ( ( encumb( body_part_leg_l ) + encumb( body_part_leg_r ) ) / 20.0f )
                     - ( encumb( body_part_torso ) / 10.0f ) );
    // Whiskers don't work so well if they're covered
    if( has_trait( trait_WHISKERS ) && !wearing_something_on( bodypart_id( "mouth" ) ) ) {
        mod_dodge_bonus( 1.5 );
    }
    if( has_trait( trait_WHISKERS_RAT ) && !wearing_something_on( bodypart_id( "mouth" ) ) ) {
        mod_dodge_bonus( 3 );
    }
    // depending on mounts size, attacks will hit the mount and use their dodge rating.
    // if they hit the player, the player cannot dodge as effectively.
    if( is_mounted() ) {
        mod_dodge_bonus( -4 );
    }
    // Spider hair is basically a full-body set of whiskers, once you get the brain for it
    if( has_trait( trait_CHITIN_FUR3 ) ) {
        static const std::array<bodypart_id, 5> parts{ { bodypart_id( "head" ), bodypart_id( "arm_r" ), bodypart_id( "arm_l" ), bodypart_id( "leg_r" ), bodypart_id( "leg_l" ) } };
        for( const bodypart_id &bp : parts ) {
            if( !wearing_something_on( bp ) ) {
                mod_dodge_bonus( +1 );
            }
        }
        // Torso handled separately, bigger bonus
        if( !wearing_something_on( bodypart_id( "torso" ) ) ) {
            mod_dodge_bonus( 4 );
        }
    }

    // Apply static martial arts buffs
    martial_arts_data->ma_static_effects( *this );

    if( calendar::once_every( 1_minutes ) ) {
        character_funcs::update_mental_focus( *this );
    }

    // Effects
    for( const auto &maps : *effects ) {
        for( auto i : maps.second ) {
            const auto &it = i.second;
            if( it.is_removed() ) {
                continue;
            }
            bool reduced = resists_effect( it );
            mod_str_bonus( it.get_mod( "STR", reduced ) );
            mod_dex_bonus( it.get_mod( "DEX", reduced ) );
            mod_per_bonus( it.get_mod( "PER", reduced ) );
            mod_int_bonus( it.get_mod( "INT", reduced ) );
        }
    }

    // Bionic buffs
    if( has_active_bionic( bio_hydraulics ) ) {
        mod_str_bonus( 20 );
    }

    mod_str_bonus( get_mod_stat_from_bionic( character_stat::STRENGTH ) );
    mod_dex_bonus( get_mod_stat_from_bionic( character_stat::DEXTERITY ) );
    mod_per_bonus( get_mod_stat_from_bionic( character_stat::PERCEPTION ) );
    mod_int_bonus( get_mod_stat_from_bionic( character_stat::INTELLIGENCE ) );

    // Trait / mutation buffs
    mod_str_bonus( std::floor( mutation_value( "str_modifier" ) ) );
    mod_dodge_bonus( std::floor( mutation_value( "dodge_modifier" ) ) );

    mod_str_bonus( enchantment_cache->calc_bonus( enchant_vals::mod::STRENGTH, get_str_base(), true ) );
    mod_dex_bonus( enchantment_cache->calc_bonus( enchant_vals::mod::DEXTERITY, get_dex_base(),
                   true ) );
    mod_per_bonus( enchantment_cache->calc_bonus( enchant_vals::mod::PERCEPTION, get_per_base(),
                   true ) );
    mod_int_bonus( enchantment_cache->calc_bonus( enchant_vals::mod::INTELLIGENCE, get_int_base(),
                   true ) );

    mod_num_dodges_bonus( enchantment_cache->calc_bonus(
                              enchant_vals::mod::BONUS_DODGE,
                              get_num_dodges_base(),
                              true
                          ) );

    apply_skill_boost();

    // Reset our stats to normal levels
    // Any persistent buffs/debuffs will take place in effects,
    // player::suffer(), etc.

    // repopulate the stat fields
    str_cur = str_max + get_str_bonus();
    dex_cur = dex_max + get_dex_bonus();
    per_cur = per_max + get_per_bonus();
    int_cur = int_max + get_int_bonus();

    // Floor for our stats.  No stat changes should occur after this!
    dex_cur = std::max( dex_cur, 0 );
    str_cur = std::max( str_cur, 0 );
    per_cur = std::max( per_cur, 0 );
    int_cur = std::max( int_cur, 0 );

    recalc_sight_limits();
    recalc_speed_bonus();

    cata::run_hooks( "on_character_reset_stats", [this]( auto & params ) { params["character"] = this; } );
}

void Character::environmental_revert_effect()
{
    addictions.clear();
    morale->clear();

    set_all_parts_hp_to_max();
    set_stored_kcal( max_stored_kcal() );
    set_thirst( 0 );
    set_fatigue( 0 );
    set_healthy( 0 );
    set_healthy_mod( 0 );
    set_stim( 0 );
    set_pain( 0 );
    set_painkiller( 0 );
    set_rad( 0 );
    set_sleep_deprivation( 0 );

    recalc_sight_limits();
    reset_encumbrance();
}

static bool needs_elec_charges( item *it )
{
    bool not_full = it->ammo_capacity() > it->ammo_remaining();
    if( !not_full ) {
        return false;
    }
    const item *mag = it->magazine_current();
    if( mag ) {
        return mag->has_flag( flag_RECHARGE );
    } else {
        return true;
    }
}

void Character::process_items()
{
    ZoneScoped;

    auto process_item = [this]( detached_ptr<item> &&ptr ) {
        return item::process( std::move( ptr ), as_player(), pos(), false );
    };
    if( primary_weapon().needs_processing() ) {
        primary_weapon().attempt_detach( process_item );
    }

    std::vector<item *> inv_active = inv.active_items();
    for( item *tmp_it : inv_active ) {
        tmp_it->attempt_detach( process_item );
    }

    // worn items
    remove_worn_items_with( [process_item]( detached_ptr<item> &&itm ) {
        if( itm->needs_processing() ) {
            return process_item( std::move( itm ) );
        }
        return std::move( itm );
    } );

    // Active item processing done, now we're recharging.
    std::vector<item *> active_worn_items;
    bool weapon_active = primary_weapon().has_flag( flag_USE_UPS ) &&
                         needs_elec_charges( &primary_weapon() );
    std::vector<size_t> active_held_items;
    int ch_UPS = 0;
    for( size_t index = 0; index < inv.size(); index++ ) {
        item &it = inv.find_item( index );
        if( it.has_flag( flag_IS_UPS ) ) {
            ch_UPS += std::min( it.ammo_remaining() * it.type->tool->ups_eff_mult,
                                it.type->tool->ups_recharge_rate );
        }
        if( it.has_flag( flag_USE_UPS ) && needs_elec_charges( &it ) ) {
            active_held_items.push_back( index );
        }
    }
    bool update_required = get_check_encumbrance();
    for( item *&w : worn ) {
        if( w->has_flag( flag_USE_UPS ) && needs_elec_charges( w ) ) {
            active_worn_items.push_back( w );
        }
        if( w->has_flag( flag_IS_UPS ) ) {
            ch_UPS += std::min( w->ammo_remaining() * w->type->tool->ups_eff_mult,
                                w->type->tool->ups_recharge_rate );
        }
        if( !update_required && w->encumbrance_update_ ) {
            update_required = true;
        }
        w->encumbrance_update_ = false;
    }
    if( update_required ) {
        reset_encumbrance();
        set_check_encumbrance( false );
    }
    if( has_active_bionic( bionic_id( "bio_ups" ) ) ) {
        ch_UPS += std::min( units::to_kilojoule( get_power_level() ), 10 );
    }
    int ch_UPS_used = 0;
    if( weapon_active && ch_UPS_used < ch_UPS ) {
        auto weap = &primary_weapon();
        int used = std::min( ch_UPS, weap->ammo_capacity() - weap->ammo_remaining() );
        ch_UPS -= used;
        ch_UPS_used += used;
        const itype_id ammo = weap->ammo_current();
        if( !ammo.is_null() && ammo != itype_id::NULL_ID() ) {
            weap->ammo_set( ammo, weap->ammo_remaining() + used );
        } else if( weap->ammo_remaining() == 0 ) {
            weap->ammo_set( itype_battery, weap->ammo_remaining() + used );
        } else {
            ch_UPS_used -= used;
            ch_UPS += used;
        }
    }
    // Load all items that use the UPS to their minimal functional charge,
    // The tool is not really useful if its charges are below charges_to_use
    for( size_t index : active_held_items ) {
        if( ch_UPS <= 0 ) {
            break;
        }
        item &it = inv.find_item( index );
        int used = std::min( ch_UPS, it.ammo_capacity() - it.ammo_remaining() );
        ch_UPS -= used;
        ch_UPS_used += used;
        const itype_id ammo = it.ammo_current();
        if( !ammo.is_null() && ammo != itype_id::NULL_ID() ) {
            it.ammo_set( ammo, it.ammo_remaining() + used );
        } else if( it.ammo_remaining() == 0 ) {
            it.ammo_set( itype_battery, it.ammo_remaining() + used );
        } else {
            ch_UPS_used -= used;
            ch_UPS += used;
        }
    }
    for( item *worn_item : active_worn_items ) {
        if( ch_UPS <= 0 ) {
            break;
        }
        int used = std::min( ch_UPS, worn_item->ammo_capacity() - worn_item->ammo_remaining() );
        ch_UPS -= used;
        ch_UPS_used += used;
        const itype_id ammo = worn_item->ammo_current();
        if( !ammo.is_null() && ammo != itype_id::NULL_ID() ) {
            worn_item->ammo_set( ammo, worn_item->ammo_remaining() + used );
        } else if( worn_item->ammo_remaining() == 0 ) {
            worn_item->ammo_set( itype_battery, worn_item->ammo_remaining() + used );
        } else {
            ch_UPS_used -= used;
            ch_UPS += used;
        }
    }
    if( ch_UPS_used > 0 ) {
        use_charges( itype_UPS, ch_UPS_used );
    }
}

namespace character_funcs
{

void update_body_wetness( Character &who, const w_point &weather )
{
    // Average number of turns to go from completely soaked to fully dry
    // assuming average temperature and humidity
    constexpr time_duration average_drying = 2_hours;

    // A modifier on drying time
    double delay = 1.0;
    // Weather slows down drying
    delay += ( ( weather.humidity - 66 ) - ( units::to_fahrenheit( weather.temperature ) - 65 ) ) / 100;
    delay = std::max( 0.1, delay );
    // Fur/slime retains moisture
    if( who.has_trait( trait_LIGHTFUR ) ||
        who.has_trait( trait_FUR ) ||
        who.has_trait( trait_FELINE_FUR ) ||
        who.has_trait( trait_LUPINE_FUR ) ||
        who.has_trait( trait_CHITIN_FUR ) ||
        who.has_trait( trait_CHITIN_FUR2 ) ||
        who.has_trait( trait_CHITIN_FUR3 ) ) {
        delay = delay * 6 / 5;
    }
    if( who.has_trait( trait_URSINE_FUR ) || who.has_trait( trait_SLIMY ) ) {
        delay *= 1.5;
    }

    if( !x_in_y( 1, to_turns<int>( average_drying * delay / 100.0 ) ) ) {
        // No drying this turn
        return;
    }

    // Now per-body-part stuff
    // To make drying uniform, make just one roll and reuse it
    const int drying_roll = rng( 1, 80 );

    for( std::pair<const bodypart_str_id, bodypart> &pr : who.get_body() ) {
        if( pr.second.get_wetness() == 0 ) {
            continue;
        }
        // This is to normalize drying times
        int drying_chance = pr.second.get_drench_capacity();
        // Body temperature affects duration of wetness
        // Note: Using temp_conv rather than temp_cur, to better approximate environment
        int temp_conv = pr.second.get_temp_conv();
        if( temp_conv >= BODYTEMP_SCORCHING ) {
            drying_chance *= 2;
        } else if( temp_conv >= BODYTEMP_VERY_HOT ) {
            drying_chance = drying_chance * 3 / 2;
        } else if( temp_conv >= BODYTEMP_HOT ) {
            drying_chance = drying_chance * 4 / 3;
        } else if( temp_conv > BODYTEMP_COLD ) {
            // Comfortable, doesn't need any changes
        } else {
            // Evaporation doesn't change that much at lower temp
            drying_chance = drying_chance * 3 / 4;
        }

        if( drying_chance < 1 ) {
            drying_chance = 1;
        }

        // TODO: Make evaporation reduce body heat
        if( drying_chance >= drying_roll ) {
            pr.second.set_wetness( std::max( 0, pr.second.get_wetness() - 1 ) );
        }
    }
    // TODO: Make clothing slow down drying
}

void do_pause( Character &who )
{
    map &here = get_map();

    who.moves = 0;
    who.recoil = MAX_RECOIL;

    // Train swimming if underwater
    if( !who.in_vehicle ) {
        if( ( get_map().ter( who.pos() ).id().str() == "t_open_air" ) ) {
            if( character_funcs::can_fly( who ) ) {
                // add flying flavor text here
                for( const trait_id &tid : who.get_mutations() ) {
                    const mutation_branch &mdata = tid.obj();
                    if( mdata.flags.contains( trait_flag_MUTATION_FLIGHT ) ) {
                        who.mutation_spend_resources( tid );
                    }
                }
            } else {
                g->vertical_move( 0, true );
            }
        }

        if( who.is_underwater() ) {
            who.drench( 100, { {
                    bodypart_str_id( "leg_l" ), bodypart_str_id( "leg_r" ), bodypart_str_id( "torso" ), bodypart_str_id( "arm_l" ),
                    bodypart_str_id( "arm_r" ), bodypart_str_id( "head" ), bodypart_str_id( "eyes" ), bodypart_str_id( "mouth" ),
                    bodypart_str_id( "foot_l" ), bodypart_str_id( "foot_r" ), bodypart_str_id( "hand_l" ), bodypart_str_id( "hand_r" )
                }
            }, true );
        } else if( here.has_flag( TFLAG_DEEP_WATER, who.pos() ) ) {
            // Same as above, except no head/eyes/mouth
            who.drench( 100, { {
                    bodypart_str_id( "leg_l" ), bodypart_str_id( "leg_r" ), bodypart_str_id( "torso" ), bodypart_str_id( "arm_l" ),
                    bodypart_str_id( "arm_r" ), bodypart_str_id( "foot_l" ), bodypart_str_id( "foot_r" ), bodypart_str_id( "hand_l" ),
                    bodypart_str_id( "hand_r" )
                }
            }, true );
        } else if( here.has_flag( "SWIMMABLE", who.pos() ) ) {
            who.drench( 40, { { bodypart_str_id( "foot_l" ), bodypart_str_id( "foot_r" ), bodypart_str_id( "leg_l" ), bodypart_str_id( "leg_r" ) } },
            false );
        }
    }

    // Try to put out clothing/hair fire
    if( who.has_effect( effect_onfire ) ) {
        time_duration total_removed = 0_turns;
        time_duration total_left = 0_turns;
        bool on_ground = who.has_effect( effect_downed );
        for( const body_part bp : all_body_parts ) {
            effect &eff = who.get_effect( effect_onfire, convert_bp( bp ) );
            if( eff.is_null() ) {
                continue;
            }

            // TODO: Tools and skills
            total_left += eff.get_duration();
            // Being on the ground will smother the fire much faster because you can roll
            const time_duration dur_removed = on_ground ? eff.get_duration() / 2 + 2_turns : 5_turns;
            eff.mod_duration( -dur_removed );
            total_removed += dur_removed;
        }

        // Don't drop on the ground when the ground is on fire
        if( total_left > 3_turns && !who.is_dangerous_fields( here.field_at( who.pos() ) ) ) {
            who.add_effect( effect_downed, 2_turns, bodypart_str_id::NULL_ID(), 0, true );
            who.add_msg_player_or_npc( m_warning,
                                       _( "You roll on the ground, trying to smother the fire!" ),
                                       _( "<npcname> rolls on the ground!" ) );
        } else if( total_removed > 0_turns ) {
            who.add_msg_player_or_npc( m_warning,
                                       _( "You attempt to put out the fire on you!" ),
                                       _( "<npcname> attempts to put out the fire on them!" ) );
        }
    }

    // on-pause effects for martial arts
    who.martial_arts_data->ma_onpause_effects( who );

    if( who.is_npc() ) {
        // The stuff below doesn't apply to NPCs
        // search_surroundings should eventually do, though
        return;
    }

    if( who.in_vehicle && one_in( 8 ) ) {
        VehicleList vehs = here.get_vehicles();
        vehicle *veh = nullptr;
        for( auto &v : vehs ) {
            veh = v.v;
            if( veh && veh->is_moving() && veh->player_in_control( who ) ) {
                double exp_temp = 1 + veh->total_mass() / 400.0_kilogram +
                                  std::abs( veh->velocity / 3200.0 );
                int experience = static_cast<int>( exp_temp );
                if( exp_temp - experience > 0 && x_in_y( exp_temp - experience, 1.0 ) ) {
                    experience++;
                }
                who.as_player()->practice( skill_id( "driving" ), experience );
                break;
            }
        }
    }

    search_surroundings( who );
    who.wait_effects();
}

void search_surroundings( Character &who )
{
    if( who.controlling_vehicle ) {
        return;
    }
    const map &here = get_map();
    // Search for traps in a larger area than before because this is the only
    // way we can "find" traps that aren't marked as visible.
    // Detection formula takes care of likelihood of seeing within this range.
    for( const tripoint &tp : here.points_in_radius( who.pos(), 5 ) ) {
        const trap &tr = here.tr_at( tp );
        if( tr.is_null() || tp == who.pos() ) {
            continue;
        }
        if( who.has_active_bionic( bio_ground_sonar ) && !who.knows_trap( tp ) &&
            ( tr.loadid == tr_beartrap_buried ||
              tr.loadid == tr_landmine_buried || tr.loadid == tr_sinkhole ) ) {
            const std::string direction = direction_name( direction_from( who.pos(), tp ) );
            who.add_msg_if_player( m_warning, _( "Your ground sonar detected a %1$s to the %2$s!" ),
                                   tr.name(), direction );
            who.add_known_trap( tp, tr );
        }
        if( !who.sees( tp ) ) {
            continue;
        }
        if( tr.is_always_invisible() || tr.can_see( tp, who ) ) {
            // Already seen, or can never be seen
            continue;
        }
        // Chance to detect traps we haven't yet seen.
        if( tr.detect_trap( tp, who ) ) {
            if( tr.get_visibility() > 0 ) {
                // Only bug player about traps that aren't trivial to spot.
                const std::string direction = direction_name(
                                                  direction_from( who.pos(), tp ) );
                who.add_msg_if_player( _( "You've spotted a %1$s to the %2$s!" ),
                                       tr.name(), direction );
                // Get a bit of experience for spotting traps.
                who.practice( skill_traps, tr.get_visibility() );
            }
            who.add_known_trap( tp, tr );
        }
    }
}

void update_mental_focus( Character &who )
{
    who.focus_pool += character_effects::calc_focus_change( who );
}

} // namespace character_funcs
