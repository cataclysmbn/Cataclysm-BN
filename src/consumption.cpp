#include "avatar_functions.h"
#include "npc.h"
#include "pickup.h"
#include "player.h" // IWYU pragma: associated
#include "consumption.h" // IWYU pragma: associated
#include "character.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <tuple>

#include "activity_handlers.h"
#include "addiction.h"
#include "avatar.h"
#include "bionics.h"
#include "calendar.h"
#include "cata_utility.h"
#include "craft_command.h"
#include "debug.h"
#include "enums.h"
#include "flat_set.h"
#include "flag.h"
#include "game.h"
#include "item_contents.h"
#include "itype.h"
#include "magic_enchantment.h"
#include "map.h"
#include "material.h"
#include "messages.h"
#include "monster.h"
#include "morale_types.h"
#include "mtype.h"
#include "mutation.h"
#include "options.h"
#include "pldata.h"
#include "recipe.h"
#include "recipe_dictionary.h"
#include "requirements.h"
#include "rng.h"
#include "rot.h"
#include "stomach.h"
#include "string_formatter.h"
#include "translations.h"
#include "type_id.h"
#include "units.h"
#include "vitamin.h"
#include "weather.h"

static const std::string comesttype_DRINK( "DRINK" );
static const std::string comesttype_FOOD( "FOOD" );

static const skill_id skill_cooking( "cooking" );
static const skill_id skill_survival( "survival" );

static const mtype_id mon_player_blob( "mon_player_blob" );

static const bionic_id bio_digestion( "bio_digestion" );
static const bionic_id bio_furnace( "bio_furnace" );
static const bionic_id bio_syringe( "bio_syringe" );
static const bionic_id bio_taste_blocker( "bio_taste_blocker" );

static const efftype_id effect_bloodworms( "bloodworms" );
static const efftype_id effect_bloated( "bloated" );
static const efftype_id effect_brainworms( "brainworms" );
static const efftype_id effect_foodpoison( "foodpoison" );
static const efftype_id effect_fungus( "fungus" );
static const efftype_id effect_hallu( "hallu" );
static const efftype_id effect_nausea( "nausea" );
static const efftype_id effect_paincysts( "paincysts" );
static const efftype_id effect_poison( "poison" );
static const efftype_id effect_tapeworm( "tapeworm" );
static const efftype_id effect_visuals( "visuals" );

static const itype_id itype_syringe( "syringe" );

static const mutation_category_id mutation_category_URSINE( "URSINE" );

static const trait_id trait_ACIDBLOOD( "ACIDBLOOD" );
static const trait_id trait_AMORPHOUS( "AMORPHOUS" );
static const trait_id trait_ANTIFRUIT( "ANTIFRUIT" );
static const trait_id trait_ANTIJUNK( "ANTIJUNK" );
static const trait_id trait_ANTIWHEAT( "ANTIWHEAT" );
static const trait_id trait_BEAK_HUM( "BEAK_HUM" );
static const trait_id trait_CANNIBAL( "CANNIBAL" );
static const trait_id trait_CARNIVORE( "CARNIVORE" );
static const trait_id trait_EATDEAD( "EATDEAD" );
static const trait_id trait_EATHEALTH( "EATHEALTH" );
static const trait_id trait_FANGS_SPIDER( "FANGS_SPIDER" );
static const trait_id trait_GIZZARD( "GIZZARD" );
static const trait_id trait_GOURMAND( "GOURMAND" );
static const trait_id trait_HERBIVORE( "HERBIVORE" );
static const trait_id trait_LACTOSE( "LACTOSE" );
static const trait_id trait_M_DEPENDENT( "M_DEPENDENT" );
static const trait_id trait_M_IMMUNE( "M_IMMUNE" );
static const trait_id trait_MANDIBLES( "MANDIBLES" );
static const trait_id trait_MEATARIAN( "MEATARIAN" );
static const trait_id trait_MOUTH_TENTACLES( "MOUTH_TENTACLES" );
static const trait_id trait_PARAIMMUNE( "PARAIMMUNE" );
static const trait_id trait_POISRESIST( "POISRESIST" );
static const trait_id trait_PROBOSCIS( "PROBOSCIS" );
static const trait_id trait_PROJUNK( "PROJUNK" );
static const trait_id trait_PROJUNK2( "PROJUNK2" );
static const trait_id trait_PSYCHOPATH( "PSYCHOPATH" );
static const trait_id trait_RUMINANT( "RUMINANT" );
static const trait_id trait_SABER_TEETH( "SABER_TEETH" );
static const trait_id trait_SAPIOVORE( "SAPIOVORE" );
static const trait_id trait_SAPROPHAGE( "SAPROPHAGE" );
static const trait_id trait_SAPROVORE( "SAPROVORE" );
static const trait_id trait_SHARKTEETH( "SHARKTEETH" );
static const trait_id trait_SLIMESPAWNER( "SLIMESPAWNER" );
static const trait_id trait_SPIRITUAL( "SPIRITUAL" );
static const trait_id trait_STIMBOOST( "STIMBOOST" );
static const trait_id trait_THRESH_BIRD( "THRESH_BIRD" );
static const trait_id trait_THRESH_CATTLE( "THRESH_CATTLE" );
static const trait_id trait_THRESH_FELINE( "THRESH_FELINE" );
static const trait_id trait_THRESH_LUPINE( "THRESH_LUPINE" );
static const trait_id trait_THRESH_PLANT( "THRESH_PLANT" );
static const trait_id trait_THRESH_URSINE( "THRESH_URSINE" );
static const trait_id trait_VEGETARIAN( "VEGETARIAN" );
static const trait_id trait_WATERSLEEP( "WATERSLEEP" );

static const vitamin_id vitamin_egg_allergen( "egg_allergen" );
static const vitamin_id vitamin_fruit_allergen( "fruit_allergen" );
static const vitamin_id vitamin_human_flesh_vitamin( "human_flesh_vitamin" );
static const vitamin_id vitamin_junk_allergen( "junk_allergen" );
static const vitamin_id vitamin_meat_allergen( "meat_allergen" );
static const vitamin_id vitamin_milk_allergen( "milk_allergen" );
static const vitamin_id vitamin_nut_allergen( "nut_allergen" );
static const vitamin_id vitamin_veggy_allergen( "veggy_allergen" );
static const vitamin_id vitamin_wheat_allergen( "wheat_allergen" );

static const trait_flag_str_id trait_flag_CANNIBAL( "CANNIBAL" );

// note: cannot use constants from flag.h (e.g. flag_ALLERGEN_VEGGY) here, as they
// might be uninitialized at the time these const arrays are created
static const std::array<vitamin_id, 4> carnivore_blacklist {{
        vitamin_veggy_allergen, vitamin_fruit_allergen,
        vitamin_wheat_allergen, vitamin_nut_allergen
    }};

static const std::array<vitamin_id, 2> herbivore_blacklist {{
        vitamin_meat_allergen, vitamin_egg_allergen
    }};

// Defines the maximum volume that a internal furnace can consume
const units::volume furnace_max_volume( 3_liter );

// TODO: JSONize.
const std::map<itype_id, int> plut_charges = {
    { itype_id( "plut_cell" ),         PLUTONIUM_CHARGES * 10 },
    { itype_id( "plut_slurry_dense" ), PLUTONIUM_CHARGES },
    { itype_id( "plut_slurry" ),       PLUTONIUM_CHARGES / 2 }
};

// TODO: Better name
enum class item_consumption_t {
    none = 0,
    tool,
    component
};

// TODO: Better name
struct prepared_item_consumption {
    prepared_item_consumption( item_consumption_t type, const item &it )
        : type( type )
        , it( it )
    { }
    item_consumption_t type;
    const item &it;

    bool consume( Character &c ) {
        // Ugly
        player &p = dynamic_cast<player &>( c );
        switch( type ) {
            case item_consumption_t::none:
                return true;
            case item_consumption_t::tool: {
                comp_selection<tool_comp> selection = comp_selection<tool_comp>();
                selection.comp = tool_comp( it.typeId(), it.type->charges_to_use() );
                selection.use_from = usage_from::both;
                p.consume_tools( selection, it.type->charges_to_use() );
                return true;
            }
            case item_consumption_t::component: {
                comp_selection<item_comp> selection = comp_selection<item_comp>();
                selection.comp = item_comp( it.typeId(), 1 );
                selection.use_from = usage_from::both;
                return !p.consume_items( selection, 1, is_crafting_component ).empty();
            }
        }

        return false;
    }
};

// Should return item_location instead, but it's really hard to get it from inventory
static std::optional<prepared_item_consumption> find_food_heater( Character &c,
        const inventory &inv, bool has_fire )
{
    if( has_fire ) {
        std::vector<item *> fire_heaters = inv.items_with( []( const item & it ) {
            return it.has_flag( flag_HEATS_FOOD_USING_FIRE );
        } );
        if( !fire_heaters.empty() ) {
            return prepared_item_consumption( item_consumption_t::none, *fire_heaters.front() );
        }
    }
    std::vector<item *> charged_heaters = inv.items_with( [&c]( const item & it ) {
        return it.has_flag( flag_HEATS_FOOD_USING_CHARGES ) &&
               it.has_flag( flag_HEATS_FOOD ) &&
               c.has_enough_charges( it, false );
    } );
    if( !charged_heaters.empty() ) {
        return prepared_item_consumption( item_consumption_t::tool, *charged_heaters.front() );
    }
    std::vector<item *> consumed_heaters = inv.items_with( []( const item & it ) {
        return it.has_flag( flag_HEATS_FOOD_IS_CONSUMED ) &&
               it.has_flag( flag_HEATS_FOOD ) &&
               is_crafting_component( it );
    } );
    if( !consumed_heaters.empty() ) {
        return prepared_item_consumption( item_consumption_t::component, *consumed_heaters.front() );

    }
    return std::nullopt;
}

static int compute_default_effective_kcal( const item &comest, const Character &you,
        const cata::flat_set<flag_id> &extra_flags = {} )
{
    if( !comest.get_comestible() ) {
        return 0;
    }

    // As float to avoid rounding too many times
    float kcal = comest.get_comestible()->default_nutrition.kcal;

    // Many raw foods give less calories, as your body has expends more energy digesting them.
    bool cooked = comest.has_flag( flag_COOKED ) || extra_flags.count( flag_COOKED );
    if( comest.has_flag( flag_RAW ) && !cooked ) {
        kcal *= 0.75f;
    }

    if( get_option<bool>( "cooking_kcal_buff" ) && comest.get_kcal_mult() > 1 ) {
        kcal *= comest.get_kcal_mult();
    }
    if( you.has_trait( trait_GIZZARD ) ) {
        kcal *= 0.6f;
    }

    if( you.has_trait( trait_CARNIVORE ) && comest.has_flag( flag_CARNIVORE_OK ) &&
        comest.has_any_vitamin( carnivore_blacklist ) ) {
        // TODO: Comment pizza scrapping
        kcal *= 0.5f;
    }

    const float relative_rot = comest.get_relative_rot();
    // Saprophages get full nutrition from rotting food
    if( relative_rot > 1.0f && !you.has_trait( trait_SAPROPHAGE ) ) {
        // Everyone else only gets a half of the nutrition
        kcal *= 0.5f;
    }

    // Bionic digestion gives extra nutrition
    if( you.has_bionic( bio_digestion ) ) {
        kcal *= 1.5f;
    }

    return static_cast<int>( kcal );
}

// Compute default effective vitamins for an item, taking into account player
// traits, but not components of the item.
static std::map<vitamin_id, int> compute_default_effective_vitamins(
    const item &it, const Character &you )
{
    if( !it.get_comestible() ) {
        return {};
    }

    std::map<vitamin_id, int> res = it.get_comestible()->default_nutrition.vitamins;

    for( const trait_id &trait : you.get_mutations() ) {
        const auto &mut = trait.obj();
        // make sure to iterate over every material defined for vitamin absorption
        // TODO: put this loop into a function and utilize it again for bionics
        for( const auto &mat : mut.vitamin_absorb_multi ) {
            // this is where we are able to check if the food actually is changed by the trait
            if( mat.first == material_id( "all" ) || it.made_of( mat.first ) ) {
                const std::map<vitamin_id, double> &mat_vit_map = mat.second;
                for( auto &vit : res ) {
                    auto vit_factor = mat_vit_map.find( vit.first );
                    if( vit_factor != mat_vit_map.end() ) {
                        vit.second *= vit_factor->second;
                    }
                }
            }
        }
    }

    return res;
}

// Calculate the effective nutrients for a given item, taking
// into account character traits but not item components.
static nutrients compute_default_effective_nutrients( const item &comest,
        const Character &you, const cata::flat_set<flag_id> &extra_flags = {} )
{
    return { compute_default_effective_kcal( comest, you, extra_flags ),
             compute_default_effective_vitamins( comest, you ) };
}

// Calculate the nutrients that the given character would receive from consuming
// the given item, taking into account the item components and the character's
// traits.
// This is used by item display, making actual nutrition available to character.
nutrients Character::compute_effective_nutrients( const item &comest ) const
{
    if( !comest.is_comestible() ) {
        return {};
    }

    // if item has components, will derive calories from that instead.
    const std::vector<item *> &components = comest.get_components().as_vector();
    if( !components.empty() && !comest.has_flag( flag_NUTRIENT_OVERRIDE ) ) {
        nutrients tally{};
        for( const item * const &component : components ) {
            nutrients component_value =
                compute_effective_nutrients( *component ) * component->charges;
            if( component->has_flag( flag_BYPRODUCT ) ) {
                tally -= component_value;
            } else {
                tally += component_value;
            }
        }
        if( comest.get_kcal_mult() > 1 ) {
            tally.kcal *= comest.get_kcal_mult();
        }
        return tally / comest.recipe_charges;
    } else {
        return compute_default_effective_nutrients( comest, *this );
    }
}

// Calculate range of nutrients obtainable for a given item when crafted via
// the given recipe
std::pair<nutrients, nutrients> Character::compute_nutrient_range(
    const item &comest, const recipe_id &recipe_i,
    const cata::flat_set<flag_id> &extra_flags ) const
{
    if( !comest.is_comestible() ) {
        return {};
    }

    const recipe &rec = *recipe_i;
    int charges = comest.count();
    // if item has components, will derive calories from that instead.
    if( comest.has_flag( flag_NUTRIENT_OVERRIDE ) || !g->u.can_make( &rec, charges ) ) {
        nutrients result = compute_default_effective_nutrients( comest, *this );
        return { result, result };
    }

    nutrients tally_min;
    nutrients tally_max;

    cata::flat_set<flag_id> our_extra_flags = extra_flags;

    if( rec.hot_result() || rec.dehydrate_result() ) {
        our_extra_flags.insert( flag_COOKED );
    }

    const requirement_data requirements = rec.simple_requirements();
    const requirement_data::alter_item_comp_vector &component_requirements =
        requirements.get_components();

    for( const std::vector<item_comp> &component_options : component_requirements ) {
        nutrients this_min;
        nutrients this_max;
        bool first = true;
        for( const item_comp &component_option : component_options ) {
            if( component_option.has( g->u.crafting_inventory(), rec.get_component_filter(), charges ) ) {
                std::pair<nutrients, nutrients> component_option_range =
                    compute_nutrient_range( component_option.type, our_extra_flags );
                component_option_range.first *= component_option.count;
                component_option_range.second *= component_option.count;

                if( first ) {
                    std::tie( this_min, this_max ) = component_option_range;
                    first = false;
                } else {
                    this_min.min_in_place( component_option_range.first );
                    this_max.max_in_place( component_option_range.second );
                }
            }
        }
        tally_min += this_min;
        tally_max += this_max;
    }

    for( const std::pair<const itype_id, int> &byproduct : rec.byproducts ) {
        item &byproduct_it = *item::spawn_temporary( byproduct.first, calendar::turn, byproduct.second );
        nutrients byproduct_nutr = compute_default_effective_nutrients( byproduct_it, *this );
        tally_min -= byproduct_nutr;
        tally_max -= byproduct_nutr;
    }
    if( comest.get_kcal_mult() > 1 ) {
        tally_min.kcal *= comest.get_kcal_mult();
        tally_max.kcal *= comest.get_kcal_mult();
    }

    return { tally_min / charges, tally_max / charges };
}

// Calculate the range of nturients possible for a given item across all
// possible recipes
std::pair<nutrients, nutrients> Character::compute_nutrient_range(
    const itype_id &comest_id, const cata::flat_set<flag_id> &extra_flags ) const
{
    const itype *comest = &*comest_id;
    if( !comest->comestible ) {
        return {};
    }
    //TODO!: wtf is all this shit
    item &comest_it = *item::spawn_temporary( comest, calendar::turn, 1 );
    // The default nutrients are always a possibility
    nutrients min_nutr = compute_default_effective_nutrients( comest_it, *this, extra_flags );

    if( comest->has_flag( flag_NUTRIENT_OVERRIDE ) ||
        recipe_dict.is_item_on_loop( comest->get_id() ) ) {
        return { min_nutr, min_nutr };
    }

    nutrients max_nutr = min_nutr;

    for( const recipe_id &rec : comest->recipes ) {
        nutrients this_min;
        nutrients this_max;

        detached_ptr<item> res = rec->create_result();
        item *result_it = &*res;
        if( result_it->contents.num_item_stacks() == 1 ) {
            item &alt_result = result_it->contents.front();
            if( alt_result.typeId() == comest_it.typeId() ) {
                result_it = &alt_result;
            }
        }
        if( result_it->typeId() != comest_it.typeId() ) {
            debugmsg( "When creating recipe result expected %s, got %s\n",
                      comest_it.typeId().str(), result_it->typeId().str() );
        }

        std::tie( this_min, this_max ) = compute_nutrient_range( *result_it, rec, extra_flags );
        min_nutr.min_in_place( this_min );
        max_nutr.max_in_place( this_max );
    }

    return { min_nutr, max_nutr };
}

int Character::nutrition_for( const item &comest ) const
{
    return compute_effective_nutrients( comest ).kcal / islot_comestible::kcal_per_nutr;
}

std::pair<int, int> Character::fun_for( const item &comest ) const
{
    if( !comest.is_comestible() ) {
        return std::pair<int, int>( 0, 0 );
    }

    // As float to avoid rounding too many times
    float fun = comest.get_comestible_fun();
    float fun_max;

    // Lupines/Felines like dog/cat food. Won't skip the rotting check.
    // This is specifically so that Lupines/Felines don't get the harsher
    //    morale penalties from rotting cat/dog food.
    if( ( comest.has_flag( flag_LUPINE ) && has_trait( trait_THRESH_LUPINE ) ) ||
        ( comest.has_flag( flag_FELINE ) && has_trait( trait_THRESH_FELINE ) ) ) {
        if( fun < 0 ) {
            fun = -fun / 2;
        }
    }

    const float relative_rot = comest.get_relative_rot();

    if( relative_rot > 1.0f && !has_trait( trait_SAPROPHAGE ) && !has_trait( trait_SAPROVORE ) ) {
        // Rotten food should be pretty disgusting.
        // Baseline minumum is the same as eating raw meat as a normal human being.
        fun = std::min( fun - 5, -10.0f );
        fun_max = fun * 6;
    } else {
        // Food is less enjoyable when eaten too often.
        if( fun > 0 || comest.has_flag( flag_NEGATIVE_MONOTONY_OK ) ) {
            for( const consumption_event &event : consumption_history->elems ) {
                if( event.time > calendar::turn - 2_days && event.type_id == comest.typeId() &&
                    event.component_hash == comest.make_component_hash() ) {
                    fun -= comest.get_comestible()->monotony_penalty;
                    // This effect can't drop fun below 0, unless the food has the right flag.
                    // 0 is the lowest we'll go, no need to keep looping.
                    if( fun <= 0 && !comest.has_flag( flag_NEGATIVE_MONOTONY_OK ) ) {
                        fun = 0;
                        break;
                    }
                }
            }
        }

        fun_max = fun < 0 ? fun * 6 : fun * 3;

        // I'd assume since gourmands are just big eaters they still can't stand rotten food.
        if( has_trait( trait_GOURMAND ) ) {
            if( fun < -1 ) {
                fun_max = fun;
                fun /= 2;
            } else if( fun > 0 ) {
                fun_max *= 3;
                fun = fun * 3 / 2;
            }
        }
    }

    return { static_cast< int >( fun ), static_cast< int >( fun_max ) };
}

time_duration Character::vitamin_rate( const vitamin_id &vit ) const
{
    time_duration res = vit.obj().rate();

    for( const auto &m : get_mutations() ) {
        const auto &mut = m.obj();
        auto iter = mut.vitamin_rates.find( vit );
        if( iter != mut.vitamin_rates.end() ) {
            res += iter->second;
        }
    }

    return res;
}

int Character::vitamin_mod( const vitamin_id &vit, int qty, bool capped )
{
    if( !vit.is_valid() ) {
        debugmsg( "Vitamin with id %s does not exist, and cannot be modified", vit.str() );
        return 0;
    }
    // What's going on here? Emplace returns either an iterator to the inserted
    // item or, if it already exists, an iterator to the (unchanged) extant item
    // (Okay, technically it returns a pair<iterator, bool>, the iterator is what we want)
    auto it = vitamin_levels.emplace( vit, 0 ).first;
    const vitamin &v = *it->first;

    if( qty > 0 ) {
        // Accumulations can never occur from food sources
        it->second = std::min( it->second + qty, capped ? 0 : v.max() );
        update_vitamins( vit );

    } else if( qty < 0 ) {
        it->second = std::max( it->second + qty, v.min() );
        update_vitamins( vit );
    }

    return it->second;
}

void Character::vitamins_mod( const std::map<vitamin_id, int> &vitamins, bool capped )
{
    for( auto vit : vitamins ) {
        vitamin_mod( vit.first, vit.second, capped );
    }
}

int Character::vitamin_get( const vitamin_id &vit ) const
{
    if( get_option<bool>( "NO_VITAMINS" ) && vit->type() == vitamin_type::VITAMIN ) {
        return 0;
    }

    const auto &v = vitamin_levels.find( vit );
    return v != vitamin_levels.end() ? v->second : 0;
}

bool Character::vitamin_set( const vitamin_id &vit, int qty )
{
    auto v = vitamin_levels.find( vit );
    if( v == vitamin_levels.end() ) {
        return false;
    }
    vitamin_mod( vit, qty - v->second, false );

    return true;
}

float Character::metabolic_rate_base() const
{
    static const std::string hunger_rate_string( "PLAYER_HUNGER_RATE" );
    static const std::string metabolism_modifier( "metabolism_modifier" );

    const float hunger_rate = get_option< float >( hunger_rate_string );
    const float mut_bonus = 1.0f + mutation_value( metabolism_modifier );
    const float with_mut = hunger_rate * mut_bonus;
    const float ench_bonus = bonus_from_enchantments( with_mut, enchant_vals::mod::METABOLISM );

    return std::max( 0.0f, with_mut + ench_bonus );
}

float Character::metabolic_rate() const
{
    return metabolic_rate_base();
}

morale_type Character::allergy_type( const item &food ) const
{
    using allergy_tuple = std::tuple<trait_id, vitamin_id, morale_type>;
    static const std::array<allergy_tuple, 8> allergy_tuples = {{
            std::make_tuple( trait_VEGETARIAN, vitamin_meat_allergen, MORALE_VEGETARIAN ),
            std::make_tuple( trait_MEATARIAN, vitamin_veggy_allergen, MORALE_MEATARIAN ),
            std::make_tuple( trait_LACTOSE, vitamin_milk_allergen, MORALE_LACTOSE ),
            std::make_tuple( trait_ANTIFRUIT, vitamin_fruit_allergen, MORALE_ANTIFRUIT ),
            std::make_tuple( trait_ANTIJUNK, vitamin_junk_allergen, MORALE_ANTIJUNK ),
            std::make_tuple( trait_ANTIWHEAT, vitamin_wheat_allergen, MORALE_ANTIWHEAT )
        }
    };

    for( const auto &tp : allergy_tuples ) {
        if( has_trait( std::get<0>( tp ) ) &&
            food.has_vitamin( std::get<1>( tp ) ) ) {
            return std::get<2>( tp );
        }
    }

    return MORALE_NULL;
}

ret_val<edible_rating> Character::can_eat( const item &food ) const
{

    const auto &comest = food.get_comestible();
    if( !comest ) {
        return ret_val<edible_rating>::make_failure( _( "That doesn't look edible." ) );
    }

    if( food.has_flag( flag_INEDIBLE ) ) {
        if( ( food.has_flag( flag_CATTLE ) && !has_trait( trait_THRESH_CATTLE ) ) ||
            ( food.has_flag( flag_FELINE ) && !has_trait( trait_THRESH_FELINE ) ) ||
            ( food.has_flag( flag_LUPINE ) && !has_trait( trait_THRESH_LUPINE ) ) ||
            ( food.has_flag( flag_BIRD ) && !has_trait( trait_THRESH_BIRD ) ) ) {
            return ret_val<edible_rating>::make_failure( _( "That doesn't look edible to you." ) );
        }
    }

    if( food.is_craft() ) {
        return ret_val<edible_rating>::make_failure( _( "That doesn't look edible in its current form." ) );
    }

    if( food.has_own_flag( flag_DIRTY ) ) {
        return ret_val<edible_rating>::make_failure(
                   _( "This is full of dirt after being on the ground." ) );
    }

    const bool eat_verb  = food.has_flag( flag_USE_EAT_VERB );
    const bool edible    = eat_verb ||  comest->comesttype == comesttype_FOOD;
    const bool drinkable = !eat_verb && comest->comesttype == comesttype_DRINK;

    // TODO: This condition occurs way too often. Unify it.
    // update Sep. 26 2018: this apparently still occurs way too often. yay!
    if( is_underwater() && !has_trait( trait_WATERSLEEP ) ) {
        return ret_val<edible_rating>::make_failure( _( "You can't do that while underwater." ) );
    }

    if( edible || drinkable ) {
        for( const auto &elem : food.type->materials ) {
            if( !elem->edible() ) {
                return ret_val<edible_rating>::make_failure( _( "That doesn't look edible in its current form." ) );
            }
        }
    }

    if( !comest->tool.is_null() ) {
        const bool has = item::count_by_charges( comest->tool )
                         ? has_charges( comest->tool, 1 )
                         : has_amount( comest->tool, 1 );
        if( !has ) {
            return ret_val<edible_rating>::make_failure( edible_rating::no_tool,
                    string_format( _( "You need a %s to consume that!" ),
                                   item::nname( comest->tool ) ) );
        }
    }

    // For all those folks who loved eating marloss berries.  D:< mwuhahaha
    if( has_trait( trait_M_DEPENDENT ) && !food.has_flag( flag_MYCUS_OK ) &&
        !food.has_flag( flag_NO_INGEST ) ) {
        return ret_val<edible_rating>::make_failure( edible_rating::inedible_mutation,
                _( "We can't eat that.  It's not right for us." ) );
    }
    // Here's why PROBOSCIS is such a negative trait.
    if( has_trait( trait_PROBOSCIS ) && !( drinkable || food.is_medication() ) ) {
        return ret_val<edible_rating>::make_failure( edible_rating::inedible_mutation,
                _( "Ugh, you can't drink that!" ) );
    }
    if( has_trait( trait_CARNIVORE ) && ( compute_effective_nutrients( food ).kcal ) > 0 &&
        food.has_any_vitamin( carnivore_blacklist ) && !food.has_flag( flag_CARNIVORE_OK ) ) {
        return ret_val<edible_rating>::make_failure( edible_rating::inedible_mutation,
                _( "Eww.  Inedible plant stuff!" ) );
    }

    if( ( has_trait( trait_HERBIVORE ) || has_trait( trait_RUMINANT ) ) &&
        food.has_any_vitamin( herbivore_blacklist ) ) {
        // Like non-cannibal, but more strict!
        return ret_val<edible_rating>::make_failure( edible_rating::inedible_mutation,
                _( "The thought of eating that makes you feel sick." ) );
    }

    for( const trait_id &mut : get_mutations() ) {
        if( !food.made_of_any( mut.obj().can_only_eat ) && !mut.obj().can_only_eat.empty() &&
            !food.has_flag( flag_NO_INGEST ) ) {
            return ret_val<edible_rating>::make_failure( edible_rating::inedible_mutation,
                    _( "You can't eat this." ) );
        }
    }

    return ret_val<edible_rating>::make_success();
}

ret_val<edible_rating> Character::will_eat( const item &food, bool interactive ) const
{
    const auto ret = can_eat( food );
    if( !ret.success() ) {
        if( interactive ) {
            add_msg_if_player( m_info, "%s", ret.c_str() );
        }
        return ret;
    }

    std::vector<ret_val<edible_rating>> consequences;
    const auto add_consequence = [&consequences]( const std::string & msg, edible_rating code ) {
        consequences.emplace_back( ret_val<edible_rating>::make_failure( code, msg ) );
    };

    const bool saprophage = has_trait( trait_SAPROPHAGE );
    const auto &comest = food.get_comestible();

    if( food.rotten() ) {
        const bool saprovore = has_trait( trait_SAPROVORE );
        if( !saprophage && !saprovore && !has_bionic( bio_digestion ) ) {
            add_consequence( _( "This is rotten and smells awful!" ), edible_rating::rotten );
        }
    }

    const bool carnivore = has_trait( trait_CARNIVORE );
    if( food.has_vitamin( vitamin_human_flesh_vitamin ) && !has_trait_flag( trait_flag_CANNIBAL ) ) {
        add_consequence( _( "The thought of eating human flesh makes you feel sick." ),
                         edible_rating::cannibalism );
    }

    const bool edible = comest->comesttype == comesttype_FOOD || food.has_flag( flag_USE_EAT_VERB );

    int food_kcal = compute_effective_nutrients( food ).kcal;
    if( food_kcal > 0 && has_effect( effect_nausea ) ) {
        add_consequence( _( "You still feel nauseous and will probably puke it all up again." ),
                         edible_rating::nausea );
    }

    if( ( food_kcal > 0 || comest->quench > 0 ) && has_effect( effect_bloated )
        && !food.has_flag( flag_NO_BLOAT ) ) {
        add_consequence( _( "You're full and will vomit if you try to consume anything." ),
                         edible_rating::bloated );
    }

    if( ( allergy_type( food ) != MORALE_NULL ) || ( carnivore && food.has_flag( flag_ALLERGEN_JUNK ) &&
            !food.has_flag( flag_CARNIVORE_OK ) ) ) {
        add_consequence( _( "Your stomach won't be happy (allergy)." ), edible_rating::allergy );
    }

    if( saprophage && edible && food.rotten() && !food.has_flag( flag_FERTILIZER ) ) {
        // Note: We're allowing all non-solid "food". This includes drugs
        // Hard-coding fertilizer for now - should be a separate flag later
        //~ No, we don't eat "rotten" food. We eat properly aged food, like a normal person.
        //~ Semantic difference, but greatly facilitates people being proud of their character.
        add_consequence( _( "Your stomach won't be happy (not rotten enough)." ),
                         edible_rating::allergy_weak );
    }

    if( !food.has_infinite_charges() &&
        ( ( food_kcal > 0 &&
            get_stored_kcal() + stomach.get_calories() + food_kcal
            > max_stored_kcal() ) ||
          ( comest->quench > 0 && get_thirst() < comest->quench ) ) ) {
        add_consequence( _( "You're full already and the excess food will be wasted." ),
                         edible_rating::too_full );
    }

    if( !consequences.empty() ) {
        if( !interactive ) {
            return consequences.front();
        }
        std::string req;
        for( const auto &elem : consequences ) {
            req += elem.str() + "\n";
        }

        const bool eat_verb  = food.has_flag( flag_USE_EAT_VERB );
        std::string food_tname = food.tname();
        const nc_color food_color = food.color_in_inventory();
        if( eat_verb || comest->comesttype == comesttype_FOOD ) {
            req += string_format( _( "Eat your %s anyway?" ), colorize( food_tname, food_color ) );
        } else if( !eat_verb && comest->comesttype == comesttype_DRINK ) {
            req += string_format( _( "Drink your %s anyway?" ), colorize( food_tname, food_color ) );
        } else {
            req += string_format( _( "Consume your %s anyway?" ), colorize( food_tname, food_color ) );
        }

        if( !query_yn( req ) ) {
            return consequences.front();
        }
    }
    // All checks ended, it's edible (or we're pretending it is)
    return ret_val<edible_rating>::make_success();
}

bool Character::eat( item &food, bool force )
{
    if( !food.is_food() ) {
        return false;
    }
    const auto ret = force ? can_eat( food ) : will_eat( food, is_player() );
    if( !ret.success() ) {
        return false;
    }

    if( has_effect( effect_bloated ) &&
        ( compute_effective_nutrients( food ).kcal > 0 || food.get_comestible()->quench > 0 ) &&
        !food.has_flag( flag_NO_BLOAT ) ) {
        add_msg_if_player( _( "You force yourself to vomit to make space for %s." ), food.tname() );
        vomit();
    }

    int charges_used = 0;
    if( food.type->has_use() ) {
        if( !food.type->can_use( "PETFOOD" ) ) {
            charges_used = food.type->invoke( *this->as_player(), food, pos() );
            if( charges_used <= 0 ) {
                return false;
            }
        }
    }

    // Note: the block below assumes we decided to eat it
    // No coming back from here

    const int nutr = nutrition_for( food );
    const bool spoiled = food.rotten();

    // The item is solid food
    const bool chew = food.get_comestible()->comesttype == comesttype_FOOD ||
                      food.has_flag( flag_USE_EAT_VERB );
    // This item is a drink and not a solid food (and not a thick soup)
    const bool drinkable = !chew && food.get_comestible()->comesttype == comesttype_DRINK;
    // If neither of the above is true then it's a drug and shouldn't get mealtime penalty/bonus

    const bool saprophage = has_trait( trait_SAPROPHAGE );
    if( spoiled && !saprophage ) {
        add_msg_if_player( m_bad, _( "Ick, this %s doesn't taste so good…" ), food.tname() );
        if( !has_trait( trait_SAPROVORE ) && !has_trait( trait_EATDEAD ) &&
            !has_bionic( bio_digestion ) ) {
            add_effect( effect_foodpoison, rng( 6_minutes, ( nutr + 1 ) * 6_minutes ) );
        }
    } else if( spoiled && saprophage ) {
        add_msg_if_player( m_good, _( "Mmm, this %s tastes delicious…" ), food.tname() );
    }

    if( !consume_effects( food ) ) {
        // Already consumed by using `food.type->invoke`?
        if( charges_used > 0 ) {
            food.mod_charges( -charges_used );
        }
        return false;
    }
    food.mod_charges( -1 );

    const bool amorphous = has_trait( trait_AMORPHOUS );
    int mealtime = 250;
    if( drinkable || chew ) {
        // Those bonuses/penalties only apply to food
        // Not to smoking weed or applying bandages!
        if( has_trait( trait_MOUTH_TENTACLES )  || has_trait( trait_MANDIBLES ) ||
            has_trait( trait_FANGS_SPIDER ) ) {
            mealtime /= 2;
        } else if( has_trait( trait_SHARKTEETH ) ) {
            // SHARKBAIT! HOO HA HA!
            mealtime /= 3;
        } else if( has_trait( trait_GOURMAND ) ) {
            // Don't stack those two - that would be 25 moves per item
            mealtime -= 100;
        }

        if( has_trait( trait_BEAK_HUM ) && !drinkable ) {
            // Much better than PROBOSCIS but still optimized for fluids
            mealtime += 200;
        } else if( has_trait( trait_SABER_TEETH ) ) {
            // They get In The Way
            mealtime += 250;
        }

        if( amorphous ) {
            mealtime *= 1.1;
            // Minor speed penalty for having to flow around it
            // rather than just grab & munch
        }
    }

    moves -= mealtime;

    consume_poison( *this, food );

    if( food.has_flag( flag_HIDDEN_HALLU ) ) {
        if( !has_effect( effect_hallu ) ) {
            add_effect( effect_hallu, 6_hours );
        }
    }

    if( amorphous ) {
        add_msg_player_or_npc( _( "You assimilate your %s." ), _( "<npcname> assimilates a %s." ),
                               food.tname() );
    } else if( drinkable ) {
        add_msg_player_or_npc( _( "You drink your %s." ), _( "<npcname> drinks a %s." ),
                               food.tname() );
    } else if( chew ) {
        add_msg_player_or_npc( _( "You eat your %s." ), _( "<npcname> eats a %s." ),
                               food.tname() );
    }

    if( food.get_comestible()->tool->tool ) {
        // Tools like lighters get used
        use_charges( food.get_comestible()->tool, 1 );
    }

    if( food.has_flag( flag_FUNGAL_VECTOR ) && !has_trait( trait_M_IMMUNE ) ) {
        add_effect( effect_fungus, 1_turns, bodypart_str_id::NULL_ID() );
    }

    // Chance to become parasitised
    if( !( has_bionic( bio_digestion ) || has_trait( trait_PARAIMMUNE ) ) ) {
        if( food.get_comestible()->parasites > 0 && !food.has_flag( flag_NO_PARASITES ) &&
            one_in( food.get_comestible()->parasites ) ) {
            switch( rng( 0, 3 ) ) {
                case 0:
                    add_effect( effect_tapeworm, 1_turns, bodypart_str_id::NULL_ID() );
                    break;
                case 1:
                    if( !has_trait( trait_ACIDBLOOD ) ) {
                        add_effect( effect_bloodworms, 1_turns, bodypart_str_id::NULL_ID() );
                    }
                    break;
                case 2:
                    add_effect( effect_brainworms, 1_turns, bodypart_str_id::NULL_ID() );
                    break;
                case 3:
                    add_effect( effect_paincysts, 1_turns, bodypart_str_id::NULL_ID() );
            }
        }
    }

    for( const std::pair<const diseasetype_id, int> &elem : food.get_comestible()->contamination ) {
        if( rng( 1, 100 ) <= elem.second ) {
            expose_to_disease( elem.first );
        }
    }

    consumption_history->elems.emplace_back( food );
    // Clean out consumption_history so it doesn't get bigger than needed.
    while( consumption_history->elems.front().time < calendar::turn - 2_days ) {
        consumption_history->elems.pop_front();
    }

    return true;
}

void Character::modify_health( const islot_comestible &comest )
{
    const int effective_health = comest.healthy;
    // Effectively no cap on health modifiers from food and meds
    const int health_cap = 200;
    mod_healthy_mod( effective_health, effective_health >= 0 ? health_cap : -health_cap );
}

void Character::modify_stimulation( const islot_comestible &comest )
{
    const int current_stim = get_stim();
    if( comest.stim != 0 &&
        ( std::abs( current_stim ) < ( std::abs( comest.stim ) * 3 ) ||
          sgn( current_stim ) != sgn( comest.stim ) ) ) {
        if( comest.stim < 0 ) {
            set_stim( std::max( comest.stim * 3, current_stim + comest.stim ) );
        } else {
            set_stim( std::min( comest.stim * 3, current_stim + comest.stim ) );
        }
    }
    if( has_trait( trait_STIMBOOST ) && ( current_stim > 30 ) &&
        ( ( comest.add == add_type::CAFFEINE ) || ( comest.add == add_type::SPEED ) ||
          ( comest.add == add_type::COKE ) || ( comest.add == add_type::CRACK ) ) ) {
        int hallu_duration = ( current_stim - comest.stim < 30 ) ? current_stim - 30 : comest.stim;
        add_effect( effect_visuals, hallu_duration * 30_minutes );
        std::vector<std::string> stimboost_msg{ _( "The shadows are getting ever closer." ),
                                                _( "You have a bad feeling about this." ),
                                                _( "A powerful sense of dread comes over you." ),
                                                _( "Your skin starts crawling." ),
                                                _( "They're coming to get you." ),
                                                _( "This might've been a bad idea…" ),
                                                _( "You've really done it this time, haven't you?" ),
                                                _( "You have to stay vigilant.  They're always watching…" ),
                                                _( "mistake mistake mistake mistake mistake" ),
                                                _( "Just gotta stay calm, and you'll make it through this." ),
                                                _( "You're starting to feel very jumpy." ),
                                                _( "Something is twitching at the edge of your vision." ),
                                                _( "They know what you've done…" ),
                                                _( "You're feeling even more paranoid than usual." ) };
        add_msg_if_player( m_bad, random_entry_ref( stimboost_msg ) );
    }
}

void Character::modify_fatigue( const islot_comestible &comest )
{
    mod_fatigue( -comest.fatigue_mod );
}

void Character::modify_radiation( const islot_comestible &comest )
{
    irradiate( comest.radiation );
}

void Character::modify_addiction( const islot_comestible &comest )
{
    add_addiction( comest.add, comest.addict );
    if( addiction_craving( comest.add ) != MORALE_NULL ) {
        rem_morale( addiction_craving( comest.add ) );
    }
}
namespace
{

auto clamped_nutr( int nutr ) -> int
{
    return std::max( 5, std::min( 20, nutr / 10 ) );
}

const morale_type MORALE_FOOD_COLD( "morale_food_cold" );
const morale_type MORALE_FOOD_VERY_COLD( "morale_food_very_cold" );

} // namespace

void Character::modify_morale( item &food, int nutr )
{
    time_duration morale_time = 12_hours;

    const int nutr_morale = clamped_nutr( nutr );
    const auto food_morale = [&]( const morale_type & type ) -> void {
        add_morale( type, nutr_morale, 20, morale_time, morale_time / 2 );
    };

    if( food.has_flag( flag_EATEN_HOT ) ) {
        auto heater = find_food_heater( *this, crafting_inventory(),
                                        get_map().has_nearby_fire( pos(), PICKUP_RANGE ) );
        if( heater && heater->consume( *this ) ) {
            add_msg_player_or_npc( m_good,
                                   _( "You heat up your %1$s using the %2$s." ),
                                   _( "<npcname> heats up their %1$s using the %2$s." ),
                                   food.tname(), heater->it.tname() );
            morale_time += 2_hours;
            food_morale( MORALE_FOOD_HOT );
        }
    } else if( food.has_flag( flag_EATEN_COLD ) ) {
        const auto temp = rot::temperature_flag_for_location( get_map(), food );

        if( temp == temperature_flag::TEMP_FREEZER ) {
            add_msg_if_player( m_good, _( "This stuff is icy!" ), food.tname() );
            morale_time += 2_hours;
            food_morale( MORALE_FOOD_VERY_COLD );
        } else if( temp == temperature_flag::TEMP_FRIDGE || temp == temperature_flag::TEMP_ROOT_CELLAR ) {
            add_msg_if_player( m_good, _( "This stuff is cold!" ), food.tname() );
            morale_time += 1_hours;
            food_morale( MORALE_FOOD_COLD );
        }
    }



    std::pair<int, int> fun = fun_for( food );
    if( fun.first < 0 ) {
        if( has_active_bionic( bio_taste_blocker ) &&
            get_power_level() > units::from_kilojoule( -fun.first ) ) {
            mod_power_level( units::from_kilojoule( std::min( 0, fun_for( food ).first ) ) );
        } else {
            add_morale( MORALE_FOOD_BAD, fun.first, fun.second, morale_time, morale_time / 2, false,
                        food.type );
        }
    } else if( fun.first > 0 ) {
        add_morale( MORALE_FOOD_GOOD, fun.first, fun.second, morale_time, morale_time / 2, false,
                    food.type );
    }

    if( food.has_flag( flag_HIDDEN_HALLU ) ) {
        if( has_trait( trait_SPIRITUAL ) ) {
            add_morale( MORALE_FOOD_GOOD, 36, 72, 2_hours, 1_hours, false );
        } else {
            add_morale( MORALE_FOOD_GOOD, 18, 36, 1_hours, 30_minutes, false );
        }
    }

    if( food.has_vitamin( vitamin_human_flesh_vitamin ) ) {
        const bool cannibal = has_trait( trait_CANNIBAL );
        const bool psycho = has_trait( trait_PSYCHOPATH );
        const bool sapiovore = has_trait( trait_SAPIOVORE );
        if( cannibal ) {
            add_msg_if_player( m_good, _( "You indulge your shameful hunger." ) );
            add_morale( MORALE_CANNIBAL, 20, 200 );
        } else if( psycho || sapiovore ) {
            // Nothing - doesn't care enough to print a message
        } else {
            add_msg_if_player( m_bad, _( "You feel horrible for eating a person." ) );
            add_morale( MORALE_CANNIBAL, -60, -400, 60_minutes, 30_minutes );
        }
    }

    // Allergy check for food that is ingested (not gum)
    if( !food.has_flag( flag_NO_INGEST ) ) {
        const auto allergy = allergy_type( food );
        if( allergy != MORALE_NULL ) {
            add_msg_if_player( m_bad, _( "Yuck!  How can anybody eat this stuff?" ) );
            add_morale( allergy, -75, -400, 30_minutes, 24_minutes );
        }
        if( food.has_flag( flag_ALLERGEN_JUNK ) ) {
            if( has_trait( trait_PROJUNK ) ) {
                add_msg_if_player( m_good, _( "Mmm, junk food." ) );
                add_morale( MORALE_SWEETTOOTH, 5, 30, 30_minutes, 24_minutes );
            }
            if( has_trait( trait_PROJUNK2 ) ) {
                if( !one_in( 100 ) ) {
                    add_msg_if_player( m_good, _( "When life's got you down, there's always sugar." ) );
                } else {
                    add_msg_if_player( m_good, _( "They may do what they must… you've already won." ) );
                }
                add_morale( MORALE_SWEETTOOTH, 10, 50, 1_hours, 50_minutes );
            }
            // Carnivores CAN eat junk food, but they won't like it much.
            // Pizza-scraping happens in consume_effects.
            if( has_trait( trait_CARNIVORE ) && !food.has_flag( flag_CARNIVORE_OK ) ) {
                add_msg_if_player( m_bad, _( "Your stomach begins gurgling and you feel bloated and ill." ) );
                add_morale( MORALE_NO_DIGEST, -25, -125, 30_minutes, 24_minutes );
            }
        }
    }
    const bool chew = food.get_comestible()->comesttype == comesttype_FOOD ||
                      food.has_flag( flag_USE_EAT_VERB );
    if( !food.rotten() && chew && has_trait( trait_SAPROPHAGE ) ) {
        // It's OK to *drink* things that haven't rotted.  Alternative is to ban water.  D:
        add_msg_if_player( m_bad, _( "Your stomach begins gurgling and you feel bloated and ill." ) );
        add_morale( MORALE_NO_DIGEST, -75, -400, 30_minutes, 24_minutes );
    }
    if( food.has_flag( flag_URSINE_HONEY ) && ( !crossed_threshold() ||
            has_trait( trait_THRESH_URSINE ) ) &&
        mutation_category_level[mutation_category_URSINE] > 40 ) {
        // Need at least 5 bear mutations for effect to show, to filter out mutations in common with other categories
        int honey_fun = has_trait( trait_THRESH_URSINE ) ?
                        std::min( mutation_category_level[mutation_category_URSINE] / 8, 20 ) :
                        mutation_category_level[mutation_category_URSINE] / 12;
        if( honey_fun < 10 ) {
            add_msg_if_player( m_good, _( "You find the sweet taste of honey surprisingly palatable." ) );
        } else {
            add_msg_if_player( m_good, _( "You feast upon the sweet honey." ) );
        }
        add_morale( MORALE_HONEY, honey_fun, 100 );
    }
}

bool Character::consume_effects( item &food )
{
    if( !food.is_comestible() ) {
        debugmsg( "called Character::consume_effects with non-comestible" );
        return false;
    }

    if( has_trait( trait_THRESH_PLANT ) && food.type->can_use( "PLANTBLECH" ) ) {
        // Was used to cap nutrition and thirst, but no longer does this
        return false;
    }
    if( ( has_trait( trait_HERBIVORE ) || has_trait( trait_RUMINANT ) ) &&
        food.has_any_vitamin( herbivore_blacklist ) ) {
        // No good can come of this.
        return false;
    }

    const auto &comest = *food.get_comestible();

    // Rotten food causes health loss
    const float relative_rot = food.get_relative_rot();
    if( relative_rot > 1.0f && !has_trait( trait_SAPROPHAGE ) &&
        !has_trait( trait_SAPROVORE ) && !has_bionic( bio_digestion ) ) {
        const float rottedness = clamp( 2 * relative_rot - 2.0f, 0.1f, 1.0f );
        // ~-1 health per 1 nutrition at halfway-rotten-away, ~0 at "just got rotten"
        // But always round down
        int h_loss = -rottedness * comest.get_default_nutr();
        mod_healthy_mod( h_loss, -200 );
        add_msg( m_debug, "%d health from %0.2f%% rotten food", h_loss, rottedness );
    }

    // Used in hibernation messages.
    const auto nutr = nutrition_for( food );
    const bool skip_health = has_trait( trait_PROJUNK2 ) && comest.healthy < 0;
    // We can handle junk just fine
    if( !skip_health ) {
        modify_health( comest );
    }
    modify_stimulation( comest );
    modify_fatigue( comest );
    modify_radiation( comest );
    modify_addiction( comest );
    modify_morale( food, nutr );

    // Moved here and changed a bit - it was too complex
    // Incredibly minor stuff like this shouldn't require complexity
    if( !is_npc() && has_trait( trait_SLIMESPAWNER ) &&
        max_stored_kcal() < get_stored_kcal() + 4000 && get_thirst() < thirst_levels::slaked ) {
        add_msg_if_player( m_mixed,
                           _( "You feel as though you're going to split open!  In a good way?" ) );
        mod_pain( 5 );
        int numslime = 1;
        for( int i = 0; i < numslime; i++ ) {
            if( monster *const slime = g->place_critter_around( mon_player_blob, pos(), 1 ) ) {
                slime->friendly = -1;
            }
        }
        mod_stored_kcal( -400 );
        mod_thirst( 40 );
        //~ slimespawns have *small voices* which may be the Nice equivalent
        //~ of the Rat King's ALL CAPS invective.  Probably shared-brain telepathy.
        add_msg_if_player( m_good, _( "hey, you look like me!  let's work together!" ) );
    }

    // Set up food for ingestion
    const item &contained_food = food.is_container() ? food.get_contained() : food;
    food_summary ingested {
        compute_effective_nutrients( contained_food )
    };
    // Maybe move tapeworm to digestion
    if( has_effect( effect_tapeworm ) ) {
        ingested.nutr /= 2;
    }
    int excess_kcal = get_stored_kcal() + stomach.get_calories() + ingested.nutr.kcal -
                      max_stored_kcal();

    // Moved hypermetabolism check here to prevent it being gimped by various bloating/vomit problems.
    if( excess_kcal > 0 && has_trait( trait_EATHEALTH ) ) {
        healall( roll_remainder( excess_kcal / 50.0f ) );
        mod_stored_kcal( -excess_kcal );
        excess_kcal = 0;
    }

    int excess_quench = -( get_thirst() - comest.quench );
    stomach.ingest( ingested );
    mod_thirst( -contained_food.type->comestible->quench );


    if( ( excess_kcal > 0 || excess_quench > 0 ) && !food.has_flag( flag_NO_BLOAT ) &&
        !has_trait( trait_GOURMAND ) ) {
        add_effect( effect_bloated, 5_minutes );
    }

    return true;
}

bool Character::can_feed_furnace_with( const item &it ) const
{
    if( !it.flammable() || it.has_flag( flag_RADIOACTIVE ) || can_eat( it ).success() ) {
        return false;
    }

    if( !has_active_bionic( bio_furnace ) ) {
        return false;
    }

    // Not even one charge fits
    if( it.charges_per_volume( furnace_max_volume ) < 1 ) {
        return false;
    }

    return !it.has_flag( flag_CORPSE );
}

bool Character::feed_furnace_with( item &it )
{
    if( !can_feed_furnace_with( it ) ) {
        return false;
    }
    if( it.is_favorite &&
        !g->u.query_yn( _( "Are you sure you want to eat your favorited %s?" ), it.tname() ) ) {
        return false;
    }

    const int consumed_charges =  std::min( it.charges, it.charges_per_volume( furnace_max_volume ) );
    const int energy =  get_acquirable_energy( it, rechargeable_cbm::furnace );

    if( energy == 0 ) {
        add_msg_player_or_npc( m_info,
                               _( "You digest your %s, but fail to acquire energy from it." ),
                               _( "<npcname> digests their %s for energy, but fails to acquire energy from it." ),
                               it.tname() );
    } else if( is_max_power() ) {
        add_msg_player_or_npc(
            _( "You digest your %s, but you're fully powered already, so the energy is wasted." ),
            _( "<npcname> digests a %s for energy, they're fully powered already, so the energy is wasted." ),
            it.tname() );
    } else {
        const int profitable_energy = std::min( energy,
                                                units::to_kilojoule( get_max_power_level() - get_power_level() ) );
        if( it.count_by_charges() ) {
            add_msg_player_or_npc( m_info,
                                   vgettext( "You digest %d %s and recharge %d point of energy.",
                                             "You digest %d %s and recharge %d points of energy.",
                                             profitable_energy
                                           ),
                                   vgettext( "<npcname> digests %d %s and recharges %d point of energy.",
                                             "<npcname> digests %d %s and recharges %d points of energy.",
                                             profitable_energy
                                           ), consumed_charges, it.tname(), profitable_energy
                                 );
        } else {
            add_msg_player_or_npc( m_info,
                                   vgettext( "You digest your %s and recharge %d point of energy.",
                                             "You digest your %s and recharge %d points of energy.",
                                             profitable_energy
                                           ),
                                   vgettext( "<npcname> digests a %s and recharges %d point of energy.",
                                             "<npcname> digests a %s and recharges %d points of energy.",
                                             profitable_energy
                                           ), it.tname(), profitable_energy
                                 );
        }
        mod_power_level( units::from_kilojoule( profitable_energy ) );
    }

    it.charges -= consumed_charges;
    mod_moves( -250 );

    return true;
}

bool Character::fuel_bionic_with( item &it )
{
    if( !can_fuel_bionic_with( it ) ) {
        return false;
    }

    const bionic_id bio = get_most_efficient_bionic( get_bionic_fueled_with( it ) );
    const std::string str_loaded  = get_value( it.typeId().str() );

    const int fuel_multiplier = get_bionic_state( bio ).info().fuel_multiplier;

    int loadable = std::min( it.charges * fuel_multiplier, get_fuel_capacity( it.typeId() ) );
    int loaded = 0;

    if( !str_loaded.empty() ) {
        loaded = std::stoi( str_loaded );
    }

    const std::string new_charge = std::to_string( loadable + loaded );

    loadable = std::ceil( loadable / fuel_multiplier );
    it.charges -= loadable;
    // Type and amount of fuel
    set_value( it.typeId().str(), new_charge );
    update_fuel_storage( it.typeId() );
    add_msg_player_or_npc( m_info,
                           //~ %1$i: charge number, %2$s: item name, %3$s: bionics name
                           vgettext( "You load %1$i charge of %2$s in your %3$s.",
                                     "You load %1$i charges of %2$s in your %3$s.", loadable ),
                           //~ %1$i: charge number, %2$s: item name, %3$s: bionics name
                           vgettext( "<npcname> load %1$i charge of %2$s in their %3$s.",
                                     "<npcname> load %1$i charges of %2$s in their %3$s.", loadable ), loadable, it.tname(), bio->name );
    mod_moves( -250 );
    return true;
}

rechargeable_cbm Character::get_cbm_rechargeable_with( const item &it ) const
{
    if( can_feed_furnace_with( it ) ) {
        return rechargeable_cbm::furnace;
    }

    if( can_fuel_bionic_with( it ) ) {
        return rechargeable_cbm::other;
    }

    return rechargeable_cbm::none;
}

int Character::get_acquirable_energy( const item &it, rechargeable_cbm cbm ) const
{
    switch( cbm ) {
        case rechargeable_cbm::none:
            break;

        case rechargeable_cbm::reactor:
            if( it.charges > 0 ) {
                const auto iter = plut_charges.find( it.typeId() );
                return iter != plut_charges.end() ? it.charges * iter->second : 0;
            }

            break;

        case rechargeable_cbm::furnace: {
            units::volume consumed_vol = it.volume();
            units::mass consumed_mass = it.weight();
            if( it.count_by_charges() && it.charges > it.charges_per_volume( furnace_max_volume ) ) {
                const double n_stacks = static_cast<double>( it.charges_per_volume( furnace_max_volume ) ) /
                                        it.type->stack_size;
                consumed_vol = it.type->volume * n_stacks;
                // it.type->weight is in 10g units?
                consumed_mass = it.type->weight * 10 * n_stacks;
            }
            int amount = ( consumed_vol / 250_ml + consumed_mass / 1_gram ) / 9;

            // TODO: JSONize.
            if( it.made_of( material_id( "leather" ) ) ) {
                amount /= 4;
            }
            if( it.made_of( material_id( "wood" ) ) ) {
                amount /= 2;
            }

            return amount;
        }
        case rechargeable_cbm::other:
            const bionic_id &bid = get_most_efficient_bionic( get_bionic_fueled_with( it ) );
            const int to_consume = std::min( it.charges, bid->fuel_capacity );
            const int to_charge = static_cast<int>( it.fuel_energy() * to_consume * bid->fuel_efficiency );
            return to_charge;
    }

    return 0;
}

int Character::get_acquirable_energy( const item &it ) const
{
    return get_acquirable_energy( it, get_cbm_rechargeable_with( it ) );
}

bool Character::can_estimate_rot() const
{
    return get_skill_level( skill_cooking ) >= 3 || get_skill_level( skill_survival ) >= 4;
}

bool Character::can_consume_as_is( const item &it ) const
{
    return it.is_comestible() || can_consume_for_bionic( it );
}

bool Character::can_consume_for_bionic( const item &it ) const
{
    return get_cbm_rechargeable_with( it ) != rechargeable_cbm::none;
}

bool Character::can_consume( const item &it ) const
{
    if( can_consume_as_is( it ) ) {
        return true;
    }
    // Checking NO_RELOAD to prevent consumption of `battery` when contained in `battery_car` (#20012)
    return !it.is_container_empty() && !it.has_flag( flag_NO_RELOAD ) &&
           can_consume_as_is( it.contents.front() );
}

item &Character::get_consumable_from( item &it ) const
{
    if( !it.is_container_empty() && can_consume_as_is( it.contents.front() ) ) {
        return it.contents.front();
    } else if( can_consume_as_is( it ) ) {
        return it;
    }

    return null_item_reference();
}

bool query_consume_ownership( item &target, avatar &you )
{
    //Add in a reset here to make sure user keeps getting asked if not set to keep thief setting
    if( you.get_value( "THIEF_MODE_KEEP" ) != "YES" ) {
        you.set_value( "THIEF_MODE", "THIEF_ASK" );
    }
    // We don't care about the opinions of NPCs that will already shoot us on sight, steal away!
    if( !target.is_owned_by( you, true ) && target.get_owner()->likes_u >= -10 ) {
        bool choice = true;
        if( you.get_value( "THIEF_MODE" ) == "THIEF_ASK" ) {
            choice = pickup::query_thief();
        }
        if( you.get_value( "THIEF_MODE" ) == "THIEF_HONEST" || !choice ) {
            return false;
        }
        avatar_funcs::handle_theft_witnesses( you, target.get_owner() );
    }
    return true;
}

detached_ptr<item> Character::consume_item( detached_ptr<item> &&target )
{

    if( !target ) {
        return detached_ptr<item>();
    }
    if( is_underwater() && !has_trait( trait_WATERSLEEP ) ) {
        add_msg_if_player( m_info, _( "You can't do that while underwater." ) );
        return std::move( target );
    }

    item &comest = get_consumable_from( *target );
    if( comest.is_null() || target->is_craft() ) {
        add_msg_if_player( m_info, _( "You can't eat your %s." ), target->tname() );
        if( is_npc() ) {
            debugmsg( "%s tried to eat a %s", name, target->tname() );
        }
        return std::move( target );
    }

    if( is_avatar() && !query_consume_ownership( *target, *as_avatar() ) ) {
        return std::move( target );
    }

    if( consume_med( comest ) ||
        eat( comest ) ||
        feed_furnace_with( comest ) ||
        fuel_bionic_with( comest ) ) {

        if( &comest == &*target ) {
            return target->charges > 0 ? std::move( target ) : detached_ptr<item>();
        } else {
            if( comest.charges <= 0 ) {
                comest.detach();
            }
            target->on_contents_changed();
            return std::move( target );
        }
    }
    return std::move( target );
}

void Character::consume( item &target )
{
    if( target.is_null() ) {
        add_msg_if_player( m_info, _( "You do not have that item." ) );
        return;
    }
    if( is_underwater() && !has_trait( trait_WATERSLEEP ) ) {
        add_msg_if_player( m_info, _( "You can't do that while underwater." ) );
        return;
    }

    item &comest = get_consumable_from( target );
    const auto old_invlet = target.invlet;

    if( comest.is_null() || target.is_craft() ) {
        add_msg_if_player( m_info, _( "You can't eat your %s." ), target.tname() );
        if( is_npc() ) {
            debugmsg( "%s tried to eat a %s", name, target.tname() );
        }
        return;
    }

    const bool wielding = is_wielding( target );
    const bool worn = is_worn( target );
    const bool inv_item = !( wielding || worn );

    bool was_in_container = ( &comest != &target );
    Character &who = *this;
    bool consumed = comest.attempt_detach( [&who]( detached_ptr<item> &&it ) {
        return who.consume_item( std::move( it ) );
    } );

    // Restack and sort so that we don't lie about target's invlet
    if( inv_item ) {
        inv.restack( *this->as_player() );

        // in the case that the consumable was in a container, but the container is now empty (no more charges)
        // the invlet is lost
        // so we find try to find a new container with a consumable of the same type, and re-assign to it
        if( was_in_container && !target.is_favorite && comest.count_by_charges() && comest.charges == 0 &&
            !is_wearing( target ) ) {
            auto &cont_type = target.typeId();
            auto &item_type = comest.typeId();

            auto stacks = inv.const_slice();
            // find a non-empty container of the same type, with the same content type
            for( auto &stack : stacks ) {
                auto &c = stack->front();
                if( c->typeId() != cont_type ) {
                    continue;
                }
                if( !c->is_container() || c->contents.empty() ) {
                    continue;
                }
                if( c->contents.front().typeId() != item_type ) {
                    continue;
                }

                // remove the assignment from the now-empty container regardless,
                // and assign it to the next container if found
                inv_reassign_item( *c, old_invlet, true );

                break;
            }
        }
    }

    if( consumed ) {
        if( was_in_container && wielding ) {
            add_msg_if_player( _( "You are now wielding an empty %s." ), primary_weapon().tname() );
        } else if( was_in_container && worn ) {
            add_msg_if_player( _( "You are now wearing an empty %s." ), target.tname() );
        } else if( was_in_container && !is_npc() ) {
            bool drop_it = false;
            if( get_option<std::string>( "DROP_EMPTY" ) == "no" ) {
                drop_it = false;
            } else if( get_option<std::string>( "DROP_EMPTY" ) == "watertight" ) {
                drop_it = !target.is_watertight_container();
            } else if( get_option<std::string>( "DROP_EMPTY" ) == "all" ) {
                drop_it = true;
            }
            if( drop_it ) {
                add_msg( _( "You drop the empty %s." ), target.tname() );
                put_into_vehicle_or_drop( *this, item_drop_reason::deliberate, target.detach() );
            } else {
                int quantity = inv.const_stack( inv.position_by_item( &target ) ).size();
                char letter = target.invlet ? target.invlet : ' ';
                add_msg( m_info, _( "%c - %d empty %s" ), letter, quantity, target.tname( quantity ) );
            }
        }
    } else if( inv_item ) {
        target.attempt_detach( [&who]( detached_ptr<item> &&it ) {
            return pickup::handle_spillable_contents( who, std::move( it ), get_map() );
        } );

        inv.restack( *this->as_player() );
        inv.unsort();
    }
}

// TODO: Properly split medications and food instead of hacking around
bool Character::consume_med( item &target )
{
    if( !target.is_medication() ) {
        return false;
    }

    const itype_id tool_type = target.get_comestible()->tool;
    const itype *req_tool = &*tool_type;
    bool check_tool = true;
    if( tool_type == itype_syringe && has_bionic( bio_syringe ) ) {
        check_tool = false;
    }
    if( req_tool->tool ) {
        if( check_tool && !(
                has_amount( tool_type, 1 ) &&
                has_charges( tool_type, req_tool->tool->charges_per_use )
            ) ) {
            add_msg_if_player( m_info, _( "You need a %s to consume that!" ), req_tool->nname( 1 ) );
            return false;
        }
        use_charges( tool_type, req_tool->tool->charges_per_use );
    }

    int amount_used = 1;
    if( target.type->has_use() ) {
        amount_used = target.type->invoke( *this->as_player(), target, pos() );
        if( amount_used <= 0 ) {
            return false;
        }
    }

    // TODO: Get the target it was used on
    // Otherwise injecting someone will give us addictions etc.
    if( target.has_flag( flag_NO_INGEST ) ) {
        const auto &comest = *target.get_comestible();
        // Assume that parenteral meds don't spoil, so don't apply rot
        modify_health( comest );
        modify_stimulation( comest );
        modify_fatigue( comest );
        modify_radiation( comest );
        modify_addiction( comest );
        modify_morale( target );
    } else {
        // Take by mouth
        consume_effects( target );
    }

    mod_moves( -250 );
    target.charges -= amount_used;
    return target.charges <= 0;
}

consumption_event::consumption_event( const item &food ) : time( calendar::turn )
{
    type_id = food.typeId();
    component_hash = food.make_component_hash();
}

void consume_poison( Character &consumer, item &food )
{
    // If it's poisonous... poison us.
    // TODO: Move this to a flag
    if( food.poison > 0 && !consumer.has_trait( trait_POISRESIST ) &&
        !consumer.has_trait( trait_EATDEAD ) && !consumer.has_bionic( bio_digestion ) ) {
        if( food.poison >= rng( 2, 4 ) ) {
            consumer.add_effect( effect_poison, food.poison * 1_minutes );
        }

        consumer.add_effect( effect_foodpoison, food.poison * 30_minutes );
    }

}
