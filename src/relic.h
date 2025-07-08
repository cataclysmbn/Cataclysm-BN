#pragma once

#include <string>
#include <vector>

#include "json_source_location.h"
#include "magic.h"
#include "magic_enchantment.h"
#include "translations.h"

class Creature;
class JsonIn;
class JsonObject;
class JsonOut;
struct tripoint;

enum class relic_recharge_type {
    /** Recharges slowly with time */
    time,
    /** Recharges in sunlight */
    solar,
    /** Creates pain to recharge */
    pain,
    /** Drains HP to recharge */
    hp,
    /** Creates fatigue to recharge */
    fatigue,
    /** Consumes field to recharge */
    field,
    /** Consumes trap to recharge */
    trap,

    num
};

enum class relic_recharge_req {
    /** No additional requirements */
    none,
    /** Must be worn/wielded */
    equipped,
    /**
     * Must be worn on closest layer on at least 1 body part if armor, or
     * must be wielding with nothing worn on left and/or right hand.
     */
    close_to_skin,
    /** Will recharge only when character is asleep */
    sleep,
    /** Must be irradiated/in irradiated tile */
    rad,
    /** Must be wet or in rain */
    wet,
    /** Must be a Z-level above the surface */
    sky,

    num
};

template<>
struct enum_traits<relic_recharge_type> {
    static constexpr relic_recharge_type last = relic_recharge_type::num;
};

template<>
struct enum_traits<relic_recharge_req> {
    static constexpr relic_recharge_req last = relic_recharge_req::num;
};

class relic_recharge
{
        json_source_location src_loc;

    public:
        /** Relic recharge type */
        relic_recharge_type type = relic_recharge_type::time;
        /** Relic recharge requirements */
        relic_recharge_req req = relic_recharge_req::none;
        /** If relic consumes fields to recharge, this specifies field type */
        field_type_str_id field_type;
        /** If relic consumes traps to recharge, this specifies trap type */
        trap_str_id trap_type;
        /** For time-based recharge, specifies recharge interval */
        time_duration interval = 1_seconds;
        /** For intensity-based recharge types, specifies min intensity */
        int intensity_min = 0;
        /** For intensity-based recharge types, specifies max intensity */
        int intensity_max = 0;
        /** Specifies amount of charges gained per charge operation.  Can be 0 or even negative. */
        int rate = 0;
        /** Recharge activation message. */
        std::optional<std::string> message;

        bool operator==( const relic_recharge &rhs ) const;

        void load( const JsonObject &jo );
        void serialize( JsonOut &json ) const;
        void deserialize( JsonIn &jsin );
        void check() const;
};

class relic
{
    private:
        std::vector<fake_spell> active_effects;
        std::vector<enchantment> passive_effects;
        std::vector<relic_recharge> recharge_scheme;

        // the item's name will be replaced with this if the string is not empty
        translation item_name_override;

        int charges_per_activation;
        // activating an artifact overrides all spell casting costs
        int moves;
    public:
        bool operator==( const relic &rhs ) const;

        std::string name() const;
        // returns number of charges that should be consumed
        int activate( Creature &caster, const tripoint &target ) const;

        void load( const JsonObject &jo );

        void serialize( JsonOut &jsout ) const;
        void deserialize( JsonIn &jsin );

        void add_passive_effect( const enchantment &ench );
        void add_active_effect( const fake_spell &sp );
        void add_recharge_scheme( const relic_recharge &r );

        const std::vector<enchantment> &get_enchantments() const {
            return passive_effects;
        }
        const std::vector<relic_recharge> &get_recharge_scheme() const {
            return recharge_scheme;
        }

        void check() const;
};

namespace relic_funcs
{

bool check_recharge_reqs( const item &itm, const relic_recharge &rech, const Character *carrier );
bool process_recharge_entry( item &itm, const relic_recharge &rech, Character *carrier );

void process_recharge( item &itm, Character *carrier );

} // namespace relic_funcs


