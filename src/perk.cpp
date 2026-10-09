#include "perk.h"

#include "assign.h"
#include "debug.h"
#include "enchantments/enchantment.h"
#include "enums.h"
#include "generic_factory.h"
#include "json.h"
#include "monster.h"
#include "mtype.h"
#include "type_id.h"
#include "type_id_implement.h"

#include <algorithm>
#include <optional>
#include <ranges>
#include <sstream>
#include <vector>

namespace
{
generic_factory<perk> all_perks( "Perks" );
}

IMPLEMENT_STRING_AND_INT_IDS( perk, all_perks );

void perk::load( const JsonObject &jo, const std::string &src )
{
    mandatory( jo, was_loaded, "name", name );
    mandatory( jo, was_loaded, "description", description );
    mandatory( jo, was_loaded, "category", category );
    optional( jo, was_loaded, "hidden", hidden, false );
    if( jo.has_array( "enchantments" ) ) {
        for( JsonObject jobj : jo.get_array( "enchantments" ) ) {
            enchantment ench;
            ench.load( jobj );
            if( !ench.id.is_empty() ) {
                ench = ench.id.obj();
            }
            add_enchantment( ench );
        }
    }
}

void perk::add_enchantment( enchantment &nench )
{
    for( enchantment &ench : enchantments ) {
        if( ench.add( nench ) ) {
            return;
        }
    }
    enchantments.emplace_back( enchantment( nench ) );
}

std::vector<enchantment> perk::get_enchantments() const
{
    return enchantments;
}

bool perk::is_hidden() const
{
    return hidden;
}

std::string perk::get_name() const
{
    return name.translated();
}

std::string perk::get_description() const
{
    std::ostringstream oss;
    oss << description.translated() << "\n";
    oss << "\n" << _( "Effects:" ) << "\n";
    bool added_string = false;
    for( const enchantment &ench : enchantments ) {
        for( const std::string str : ench.get_effect_string( false ) ) {
            oss << "  " << str << "\n";
            added_string = true;
        }
    }
    return added_string ? oss.str() : "";
}

std::string perk::get_category() const
{
    return category.translated();
}

void perk::load_perks( const JsonObject &jo, const std::string &src )
{
    all_perks.load( jo, src );
}

void perk::finalize_all()
{
    all_perks.finalize();
    for( const perk &bd : all_perks.get_all() ) {
        const_cast<perk &>( bd ).finalize();
    }
}

void perk::finalize()
{
    for( enchantment &ench : enchantments ) {
        ench.finalize();
    }
}

void perk::check() const
{
    for( const enchantment &ench : enchantments ) {
        ench.check();
    }
}

void perk::check_consistency()
{
    all_perks.check();
}

void perk::reset()
{
    all_perks.reset();
}

std::vector<perk> perk::get_all()
{
    return all_perks.get_all();
}

