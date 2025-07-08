#include "assign.h"

#include <algorithm>

void report_strict_violation( const JsonObject &jo, const std::string &message,
                              const std::string &name )
{
    try {
        // Let the json class do the formatting, it includes the context of the JSON data.
        jo.throw_error( message, name );
    } catch( const JsonError &err ) {
        // And catch the exception so the loading continues like normal.
        debugmsg_of( DL::Warn, "(json-error)\n%s", err.what() );
    }
}


bool assign( const JsonObject &jo, const std::string &name, bool &val, bool )
{
    bool out;

    if( !jo.read( name, out ) ) {
        return false;
    }

    val = out;

    return true;
}

bool assign( const JsonObject &jo, const std::string &name, units::volume &val,
             bool,
             const units::volume lo,
             const units::volume hi )
{
    const auto parse = [&name]( const JsonObject & obj, units::volume & out ) {
        if( obj.has_int( name ) ) {
            out = obj.get_int( name ) * units::legacy_volume_factor;
            return true;
        }

        if( obj.has_string( name ) ) {
            units::volume::value_type tmp;
            std::string suffix;
            std::istringstream str( obj.get_string( name ) );
            str.imbue( std::locale::classic() );
            str >> tmp >> suffix;
            if( str.peek() != std::istringstream::traits_type::eof() ) {
                obj.throw_error( "syntax error when specifying volume", name );
            }
            if( suffix == "ml" ) {
                out = units::from_milliliter( tmp );
            } else if( suffix == "L" ) {
                out = units::from_milliliter( tmp * 1000 );
            } else {
                obj.throw_error( "unrecognized volumetric unit", name );
            }
            return true;
        }

        return false;
    };

    units::volume out;

    // Object via which to report errors which differs for proportional/relative values
    JsonObject err = jo;
    err.allow_omitted_members();
    JsonObject relative = jo.get_object( "relative" );
    relative.allow_omitted_members();
    JsonObject proportional = jo.get_object( "proportional" );
    proportional.allow_omitted_members();

    // Do not require strict parsing for relative and proportional values as rules
    // such as +10% are well-formed independent of whether they affect base value
    if( relative.has_member( name ) ) {
        units::volume tmp;
        err = relative;
        if( !parse( err, tmp ) ) {
            err.throw_error( "invalid relative value specified", name );
        }
        out = val + tmp;

    } else if( proportional.has_member( name ) ) {
        double scalar;
        err = proportional;
        if( !err.read( name, scalar ) || scalar <= 0 || scalar == 1 ) {
            err.throw_error( "multiplier must be a positive number other than 1", name );
        }
        out = val * scalar;

    } else if( !parse( jo, out ) ) {
        return false;
    }

    if( out < lo || out > hi ) {
        err.throw_error( "value outside supported range", name );
    }

    val = out;

    return true;
}

bool assign( const JsonObject &jo,
             const std::string &name,
             units::mass &val,
             bool,
             const units::mass lo,
             const units::mass hi )
{
    const auto parse = [&name]( const JsonObject & obj, units::mass & out ) {
        if( obj.has_int( name ) ) {
            out = units::from_gram<std::int64_t>( obj.get_int( name ) );
            return true;
        }
        if( obj.has_string( name ) ) {
            out = read_from_json_string<units::mass>( *obj.get_raw( name ),
                    units::mass_units );
            return true;
        }
        return false;
    };

    units::mass out;

    // Object via which to report errors which differs for proportional/relative
    // values
    JsonObject err = jo;
    err.allow_omitted_members();
    JsonObject relative = jo.get_object( "relative" );
    relative.allow_omitted_members();
    JsonObject proportional = jo.get_object( "proportional" );
    proportional.allow_omitted_members();

    // Do not require strict parsing for relative and proportional values as
    // rules such as +10% are well-formed independent of whether they affect
    // base value
    if( relative.has_member( name ) ) {
        units::mass tmp;
        err = relative;
        if( !parse( err, tmp ) ) {
            err.throw_error( "invalid relative value specified", name );
        }
        out = val + tmp;

    } else if( proportional.has_member( name ) ) {
        double scalar;
        err = proportional;
        if( !err.read( name, scalar ) || scalar <= 0 || scalar == 1 ) {
            err.throw_error( "multiplier must be a positive number other than 1",
                             name );
        }
        out = val * scalar;

    } else if( !parse( jo, out ) ) {
        return false;
    }

    if( out < lo || out > hi ) {
        err.throw_error( "value outside supported range", name );
    }

    val = out;

    return true;
}

bool assign( const JsonObject &jo,
             const std::string &name,
             units::money &val,
             bool,
             const units::money lo,
             const units::money hi )
{
    const auto parse = [&name]( const JsonObject & obj, units::money & out ) {
        if( obj.has_int( name ) ) {
            out = units::from_cent( obj.get_int( name ) );
            return true;
        }
        if( obj.has_string( name ) ) {
            out = read_from_json_string<units::money>( *obj.get_raw( name ),
                    units::money_units );
            return true;
        }
        return false;
    };

    units::money out;

    // Object via which to report errors which differs for proportional/relative
    // values
    JsonObject err = jo;
    err.allow_omitted_members();
    JsonObject relative = jo.get_object( "relative" );
    relative.allow_omitted_members();
    JsonObject proportional = jo.get_object( "proportional" );
    proportional.allow_omitted_members();

    // Do not require strict parsing for relative and proportional values as
    // rules such as +10% are well-formed independent of whether they affect
    // base value
    if( relative.has_member( name ) ) {
        units::money tmp;
        err = relative;
        if( !parse( err, tmp ) ) {
            err.throw_error( "invalid relative value specified", name );
        }
        out = val + tmp;

    } else if( proportional.has_member( name ) ) {
        double scalar;
        err = proportional;
        if( !err.read( name, scalar ) || scalar <= 0 || scalar == 1 ) {
            err.throw_error( "multiplier must be a positive number other than 1",
                             name );
        }
        out = val * scalar;

    } else if( !parse( jo, out ) ) {
        return false;
    }

    if( out < lo || out > hi ) {
        err.throw_error( "value outside supported range", name );
    }

    val = out;

    return true;
}

bool assign( const JsonObject &jo,
             const std::string &name,
             units::energy &val,
             bool,
             const units::energy lo,
             const units::energy hi )
{
    const auto parse = [&name]( const JsonObject & obj, units::energy & out ) {
        if( obj.has_int( name ) ) {
            const std::int64_t tmp = obj.get_int( name );
            if( tmp > units::to_kilojoule( units::energy_max ) ) {
                out = units::energy_max;
            } else {
                out = units::from_kilojoule( tmp );
            }
            return true;
        }
        if( obj.has_string( name ) ) {
            out = read_from_json_string<units::energy>( *obj.get_raw( name ),
                    units::energy_units );
            return true;
        }
        return false;
    };

    units::energy out;

    // Object via which to report errors which differs for proportional/relative
    // values
    JsonObject err = jo;
    err.allow_omitted_members();
    JsonObject relative = jo.get_object( "relative" );
    relative.allow_omitted_members();
    JsonObject proportional = jo.get_object( "proportional" );
    proportional.allow_omitted_members();

    // Do not require strict parsing for relative and proportional values as
    // rules such as +10% are well-formed independent of whether they affect
    // base value
    if( relative.has_member( name ) ) {
        units::energy tmp;
        err = relative;
        if( !parse( err, tmp ) ) {
            err.throw_error( "invalid relative value specified", name );
        }
        out = val + tmp;

    } else if( proportional.has_member( name ) ) {
        double scalar;
        err = proportional;
        if( !err.read( name, scalar ) || scalar <= 0 || scalar == 1 ) {
            err.throw_error( "multiplier must be a positive number other than 1",
                             name );
        }
        out = val * scalar;

    } else if( !parse( jo, out ) ) {
        return false;
    }

    if( out < lo || out > hi ) {
        err.throw_error( "value outside supported range", name );
    }

    val = out;

    return true;
}

bool assign( const JsonObject &jo,
             const std::string &name,
             units::probability &val,
             bool strict,
             const units::probability lo,
             const units::probability hi )
{
    const auto parse = [&name]( const JsonObject & obj, units::probability & out ) {
        if( obj.has_string( name ) ) {
            long double tmp;
            std::string suffix;
            std::istringstream str( obj.get_string( name ) );
            str.imbue( std::locale::classic() );
            str >> tmp >> suffix;
            if( str.peek() != std::istringstream::traits_type::eof() ) {
                obj.throw_error( "syntax error when specifying volume", name );
            }
            if( suffix == "pm" ) {
                out = units::from_one_in_million(
                          static_cast<units::probability::value_type>( tmp ) );
            } else if( suffix == "%" ) {
                out = units::from_percent( tmp );
            } else {
                obj.throw_error( "unrecognized volumetric unit", name );
            }
            return true;
        }

        return false;
    };

    return assign_unit_common( jo, name, val, parse, strict, lo, hi );
}

bool assign( const JsonObject &jo,
             const std::string &name,
             units::temperature &val,
             bool strict,
             const units::temperature lo,
             const units::temperature hi )
{
    const auto parse = [&name]( const JsonObject & obj, units::temperature & out ) {
        if( obj.has_string( name ) ) {
            long double value;
            std::string suffix;
            std::istringstream str( obj.get_string( name ) );
            str.imbue( std::locale::classic() );
            str >> value >> suffix;
            if( str.peek() != std::istringstream::traits_type::eof() ) {
                obj.throw_error( "syntax error when specifying temperature", name );
            }
            const auto &unit_suffixes = units::temperature_units;
            auto iter = std::ranges::find_if(
                            unit_suffixes,
                            [&suffix]( const std::pair<std::string, units::temperature> &
            suffix_value ) {
                return suffix_value.first == suffix;
            } );
            if( iter != unit_suffixes.end() ) {
                out = mult_unit( obj, name, iter->second, value );
            } else {
                obj.throw_error( "unrecognized temperature unit", name );
            }

            return true;
        }

        return false;
    };

    return assign_unit_common( jo, name, val, parse, strict, lo, hi );
}

bool assign( const JsonObject &jo,
             const std::string &name,
             nc_color &val,
             const bool )
{
    if( !jo.has_member( name ) ) {
        return false;
    }
    const nc_color out = color_from_string( jo.get_string( name ) );
    if( out == c_unset ) {
        jo.throw_error( "invalid color name", name );
    }

    val = out;
    return true;
}

bool assign( const JsonObject &jo,
             const std::string &name,
             damage_instance &val,
             bool strict,
             const damage_instance &lo,
             const damage_instance &hi )
{
    // What we'll eventually be returning for the damage instance
    damage_instance out;

    if( jo.has_array( name ) ) {
        out = load_damage_instance_inherit( jo.get_array( name ), val );
    } else if( jo.has_object( name ) ) {
        out = load_damage_instance_inherit( jo.get_object( name ), val );
    } else {
        // Legacy: remove after 0.F
        float amount = 0.0f;
        float arpen = 0.0f;
        float dmg_mult = 1.0f;
        bool with_legacy = false;

        // There will always be either a prop_damage or damage (name)
        if( jo.has_member( name ) ) {
            with_legacy = true;
            amount = jo.get_float( name );
        } else if( jo.has_member( "prop_damage" ) ) {
            dmg_mult = jo.get_float( "prop_damage" );
            with_legacy = true;
        }
        // And there may or may not be armor penetration
        if( jo.has_member( "pierce" ) ) {
            with_legacy = true;
            arpen = jo.get_float( "pierce" );
        }

        if( with_legacy ) {
            out.add_damage( DT_STAB, amount, arpen, 1.0f, dmg_mult );
        }
    }

    // Object via which to report errors which differs for proportional/relative
    // values
    const JsonObject &err = jo;
    JsonObject relative = jo.get_object( "relative" );
    relative.allow_omitted_members();
    JsonObject proportional = jo.get_object( "proportional" );
    proportional.allow_omitted_members();

    // Currently, we load only either relative or proportional when loading
    // damage There's no good reason for this, but it's simple for now
    if( relative.has_object( name ) ) {
        assign_dmg_relative(
            out, val, load_damage_instance( relative.get_object( name ) ), strict );
    } else if( relative.has_array( name ) ) {
        assign_dmg_relative(
            out, val, load_damage_instance( relative.get_array( name ) ), strict );
    } else if( proportional.has_object( name ) ) {
        assign_dmg_proportional(
            proportional, name, out, val,
            load_damage_instance( proportional.get_object( name ) ), strict );
    } else if( proportional.has_array( name ) ) {
        assign_dmg_proportional(
            proportional, name, out, val,
            load_damage_instance( proportional.get_array( name ) ), strict );
    } else if( relative.has_member( name ) || relative.has_member( "pierce" )
               || relative.has_member( "prop_damage" ) ) {
        // Legacy: Remove after 0.F
        // It is valid for relative to adjust any of pierce, prop_damage, or
        // damage So check for what it's modifying, and modify that
        float amt = 0.0f;
        float arpen = 0.0f;
        float dmg_mult = 1.0f;

        if( relative.has_member( name ) ) {
            amt = relative.get_float( name );
        }
        if( relative.has_member( "pierce" ) ) {
            arpen = relative.get_float( "pierce" );
        }
        if( relative.has_member( "prop_damage" ) ) {
            dmg_mult = relative.get_float( "prop_damage" );
        }

        assign_dmg_relative(
            out, val, damage_instance( DT_STAB, amt, arpen, 1.0f, dmg_mult ),
            strict );
    } else if( proportional.has_member( name )
               || proportional.has_member( "pierce" )
               || proportional.has_member( "prop_damage" ) ) {
        // Legacy: Remove after 0.F
        // It is valid for proportional to adjust any of pierce, prop_damage, or
        // damage So check if it's modifying any of the things before going on
        // to modify it
        float amt = 0.0f;
        float arpen = 0.0f;
        float dmg_mult = 1.0f;

        if( proportional.has_member( name ) ) {
            amt = proportional.get_float( name );
        }
        if( proportional.has_member( "pierce" ) ) {
            arpen = proportional.get_float( "pierce" );
        }
        if( proportional.has_member( "prop_damage" ) ) {
            dmg_mult = proportional.get_float( "prop_damage" );
        }

        assign_dmg_proportional(
            proportional, name, out, val,
            damage_instance( DT_STAB, amt, arpen, 1.0f, dmg_mult ), strict );
    } else if( !jo.has_member( name ) && !jo.has_member( "prop_damage" ) ) {
        // Straight copy-from, not modified by proportional or relative
        out = val;
        strict = false;
    }

    check_assigned_dmg( err, name, out, lo, hi );

    if( strict && out == val ) {
        report_strict_violation( err,
                                 "cannot assign explicit damage value the same "
                                 "as default or inherited value",
                                 name );
    }

    if( out.damage_units.empty() ) {
        out = damage_instance( DT_BULLET, 0.0f );
    }

    // Now that we've verified everything in out is all good, set val to it
    val = out;

    return true;
}

void check_assigned_dmg( const JsonObject &err,
                         const std::string &name,
                         const damage_instance &out,
                         const damage_instance &lo_inst,
                         const damage_instance &hi_inst )
{
    for( const damage_unit &out_dmg : out.damage_units ) {
        auto lo_iter = std::ranges::find_if(
                           lo_inst.damage_units,
        [&out_dmg]( const damage_unit & du ) {
            return du.type == out_dmg.type || du.type == DT_NULL;
        } );

        auto hi_iter = std::ranges::find_if(
                           hi_inst.damage_units,
        [&out_dmg]( const damage_unit & du ) {
            return du.type == out_dmg.type || du.type == DT_NULL;
        } );

        if( lo_iter == lo_inst.damage_units.end() ) {
            err.throw_error(
                "Min damage type used in assign does not match damage type "
                "assigned",
                name );
        }
        if( hi_iter == hi_inst.damage_units.end() ) {
            err.throw_error(
                "Max damage type used in assign does not match damage type "
                "assigned",
                name );
        }

        const damage_unit &hi_dmg = *hi_iter;
        const damage_unit &lo_dmg = *lo_iter;

        if( out_dmg.amount < lo_dmg.amount || out_dmg.amount > hi_dmg.amount ) {
            err.throw_error( "value for damage outside supported range", name );
        }
        if( out_dmg.res_pen < lo_dmg.res_pen
            || out_dmg.res_pen > hi_dmg.res_pen ) {
            err.throw_error(
                "value for armor penetration outside supported range", name );
        }
        if( out_dmg.res_mult < lo_dmg.res_mult
            || out_dmg.res_mult > hi_dmg.res_mult ) {
            err.throw_error(
                "value for armor penetration multiplier outside supported "
                "range",
                name );
        }
        if( out_dmg.damage_multiplier < lo_dmg.damage_multiplier
            || out_dmg.damage_multiplier > hi_dmg.damage_multiplier ) {
            err.throw_error(
                "value for damage multiplier outside supported range", name );
        }
    }
}

void assign_dmg_proportional( const JsonObject &jo,
                              const std::string &name,
                              damage_instance &out,
                              const damage_instance &val,
                              damage_instance proportional,
                              bool &strict )
{
    for( const damage_unit &val_dmg : val.damage_units ) {
        for( damage_unit &scalar : proportional.damage_units ) {
            if( scalar.type != val_dmg.type ) {
                continue;
            }

            // Do not require strict parsing for relative and proportional
            // values as rules such as +10% are well-formed independent of
            // whether they affect base value
            strict = false;

            // Can't have negative percent, and 100% is pointless
            // If it's 0, it wasn't loaded
            if( scalar.amount == 1 || scalar.amount < 0 ) {
                jo.throw_error(
                    "Proportional damage multiplier must be a positive number "
                    "other than 1",
                    name );
            }

            // If it's 0, it wasn't loaded
            if( scalar.res_pen < 0 || scalar.res_pen == 1 ) {
                jo.throw_error(
                    "Proportional armor penetration multiplier must be a "
                    "positive number other than 1",
                    name );
            }

            // It wasn't loaded, so set it 100%
            if( scalar.res_pen == 0 ) {
                scalar.res_pen = 1.0f;
            }

            // Ditto
            if( scalar.amount == 0 ) {
                scalar.amount = 1.0f;
            }

            // If it's 1, it wasn't loaded (or was loaded as 1)
            if( scalar.res_mult <= 0 ) {
                jo.throw_error(
                    "Proportional armor penetration multiplier must be a "
                    "positive number",
                    name );
            }

            // If it's 1, it wasn't loaded (or was loaded as 1)
            if( scalar.damage_multiplier <= 0 ) {
                jo.throw_error(
                    "Proportional damage multiplier must be a positive number",
                    name );
            }

            damage_unit out_dmg( scalar.type, 0.0f );

            out_dmg.amount = val_dmg.amount * scalar.amount;
            out_dmg.res_pen = val_dmg.res_pen * scalar.res_pen;

            out_dmg.res_mult = val_dmg.res_mult * scalar.res_mult;
            out_dmg.damage_multiplier =
                val_dmg.damage_multiplier * scalar.damage_multiplier;

            out.add( out_dmg );
        }
    }
}

void assign_dmg_relative( damage_instance &out,
                          const damage_instance &val,
                          damage_instance relative,
                          bool &strict )
{
    for( const damage_unit &val_dmg : val.damage_units ) {
        for( damage_unit &tmp : relative.damage_units ) {
            if( tmp.type != val_dmg.type ) {
                continue;
            }

            // Do not require strict parsing for relative and proportional
            // values as rules such as +10% are well-formed independent of
            // whether they affect base value
            strict = false;

            // res_mult is set to 1 if it's not specified. Set it to zero so we
            // don't accidentally add to it
            if( tmp.res_mult == 1.0f ) {
                tmp.res_mult = 0;
            }
            // Same for damage_multiplier
            if( tmp.damage_multiplier == 1.0f ) {
                tmp.damage_multiplier = 0;
            }

            damage_unit out_dmg( tmp.type, 0.0f );

            out_dmg.amount = tmp.amount + val_dmg.amount;
            out_dmg.res_pen = tmp.res_pen + val_dmg.res_pen;

            out_dmg.res_mult = tmp.res_mult + val_dmg.res_mult;
            out_dmg.damage_multiplier =
                tmp.damage_multiplier + val_dmg.damage_multiplier;

            out.add( out_dmg );
        }
    }
}
