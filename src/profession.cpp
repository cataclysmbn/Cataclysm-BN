#include "profession.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <map>

#include "addiction.h"
#include "calendar.h"
#include "debug.h"
#include "flag.h"
#include "flat_set.h"
#include "generic_factory.h"
#include "generic_readers.h"
#include "item.h"
#include "item_contents.h"
#include "item_group.h"
#include "itype.h"
#include "json.h"
#include "mission.h"
#include "options.h"
#include "pldata.h"
#include "translations.h"
#include "type_id.h"

namespace
{
generic_factory<profession> all_profs( "profession" );
const profession_id generic_profession_id( "unemployed" );
} // namespace

static class json_item_substitution
{
    public:
        void reset();
        void load( const JsonObject &jo );
        void check_consistency();

    private:
        struct trait_requirements {
            trait_requirements( const JsonObject &obj );
            trait_requirements() = default;
            std::vector<trait_id> present;
            std::vector<trait_id> absent;
            bool meets_condition( const std::vector<trait_id> &traits ) const;
        };
        struct substitution {
            trait_requirements trait_reqs;
            struct info {
                info( const JsonValue &value );
                info() = default;
                itype_id new_item;
                double ratio = 1.0; // new charges / old charges
            };
            std::vector<info> infos;
        };
        std::map<itype_id, std::vector<substitution>> substitutions;
        std::vector<std::pair<itype_id, trait_requirements>> bonuses;
    public:
        std::vector<itype_id> get_bonus_items( const std::vector<trait_id> &traits ) const;
        std::vector<detached_ptr<item>> get_substitution( const item &it,
                                     const std::vector<trait_id> &traits ) const;
} item_substitutions;

/** @relates string_id */
template<>
const profession &string_id<profession>::obj() const
{
    return all_profs.obj( *this );
}

/** @relates string_id */
template<>
bool string_id<profession>::is_valid() const
{
    return all_profs.is_valid( *this );
}

profession::profession()
    : _name_male( no_translation( "null" ) ),
      _name_female( no_translation( "null" ) ),
      _description_male( no_translation( "null" ) ),
      _description_female( no_translation( "null" ) ),
      _point_cost( 0 )
{
}

void profession::load_profession( const JsonObject &jo, const std::string &src )
{
    all_profs.load( jo, src );
}

class skilllevel_reader : public generic_typed_reader<skilllevel_reader>
{
    public:
        std::pair<skill_id, int> get_next( JsonIn &jin ) const {
            JsonObject jo = jin.get_object();
            return std::pair<skill_id, int>( skill_id( jo.get_string( "name" ) ), jo.get_int( "level" ) );
        }
        template<typename C>
        void erase_next( JsonIn &jin, C &container ) const {
            const skill_id id = skill_id( jin.get_string() );
            reader_detail::handler<C>().erase_if( container, [&id]( const std::pair<skill_id, int> &e ) {
                return e.first == id;
            } );
        }
};

class addiction_reader : public generic_typed_reader<addiction_reader>
{
    public:
        addiction get_next( JsonIn &jin ) const {
            JsonObject jo = jin.get_object();
            return addiction( addiction_type( jo.get_string( "type" ) ), jo.get_int( "intensity" ) );
        }
        template<typename C>
        void erase_next( JsonIn &jin, C &container ) const {
            const add_type type = addiction_type( jin.get_string() );
            reader_detail::handler<C>().erase_if( container, [&type]( const addiction & e ) {
                return e.type == type;
            } );
        }
};

class item_reader : public generic_typed_reader<item_reader>
{
    public:
        profession::itypedec get_next( JsonIn &jin ) const {
            // either a plain item type id string, or an array with item type id
            // and as second entry the item description.
            if( jin.test_string() ) {
                return profession::itypedec( jin.get_string() );
            }
            JsonArray jarr = jin.get_array();
            const auto id = jarr.get_string( 0 );
            const snippet_id snippet( jarr.get_string( 1 ) );
            return profession::itypedec( id, snippet );
        }
        template<typename C>
        void erase_next( JsonIn &jin, C &container ) const {
            const std::string id = jin.get_string();
            reader_detail::handler<C>().erase_if( container, [&id]( const profession::itypedec & e ) {
                return e.type_id.str() == id;
            } );
        }
};

void profession::load( const JsonObject &jo, const std::string & )
{
    // If the "name" is an object then we may have to deal with gender-specific titles
    bool add_name_context = false;
    if( jo.has_object( "name" ) ) {
        JsonObject name_obj = jo.get_object( "name" );
        if( name_obj.has_member( "male" ) && name_obj.has_member( "female" ) ) {
            // Gender-specific titles
            _name_male.deserialize( *name_obj.get_raw( "male" ) );
            _name_female.deserialize( *name_obj.get_raw( "female" ) );
        } else {
            // Same profession names for male and female in English
            name_obj.allow_omitted_members();
            _name_male.deserialize( *jo.get_raw( "name" ) );
            _name_female = _name_male;
        }
        add_name_context = true;
    } else if( jo.has_member( "name" ) ) {
        // Same profession names for male and female in English
        _name_male.deserialize( *jo.get_raw( "name" ) );
        _name_female = _name_male;
        add_name_context = true;
    } else if( !was_loaded ) {
        // Throw 'missing mandatory member' error
        jo.get_member( "name" );
        return;
    }

    if( add_name_context ) {
        // Other languages may have different gender-specific titles, so add gender context
        _name_male.add_context( "profession_male" );
        _name_female.add_context( "profession_female" );
    }

    if( jo.has_member( "description" ) ) {
        _description_male.deserialize( *jo.get_raw( "description" ) );
        _description_female = _description_male;
        // These also may differ in other languages!
        _description_male.add_context( "prof_desc_male" );
        _description_female.add_context( "prof_desc_female" );
    } else if( !was_loaded ) {
        // Throw 'missing mandatory member' error
        jo.get_member( "description" );
        return;
    }

    if( jo.has_string( "vehicle" ) ) {
        _starting_vehicle = vproto_id( jo.get_string( "vehicle" ) );
    }
    if( jo.has_array( "pets" ) ) {
        for( JsonObject subobj : jo.get_array( "pets" ) ) {
            int count = subobj.get_int( "amount" );
            mtype_id mon = mtype_id( subobj.get_string( "name" ) );
            for( int start = 0; start < count; ++start ) {
                _starting_pets.push_back( mon );
            }
        }
    }

    if( jo.has_array( "spells" ) ) {
        for( JsonObject subobj : jo.get_array( "spells" ) ) {
            int level = subobj.get_int( "level" );
            spell_id sp = spell_id( subobj.get_string( "id" ) );
            _starting_spells.emplace( sp, level );
        }
    }

    mandatory( jo, was_loaded, "points", _point_cost );

    if( !was_loaded || jo.has_member( "items" ) ) {
        JsonObject items_obj = jo.get_object( "items" );

        if( items_obj.has_array( "both" ) ) {
            optional( items_obj, was_loaded, "both", legacy_starting_items, item_reader {} );
        }
        if( items_obj.has_object( "both" ) ) {
            _starting_items = item_group::load_item_group( items_obj.get_member( "both" ), "collection" );
        }
        if( items_obj.has_array( "male" ) ) {
            optional( items_obj, was_loaded, "male", legacy_starting_items_male, item_reader {} );
        }
        if( items_obj.has_object( "male" ) ) {
            _starting_items_male = item_group::load_item_group( items_obj.get_member( "male" ), "collection" );
        }
        if( items_obj.has_array( "female" ) ) {
            optional( items_obj, was_loaded, "female",  legacy_starting_items_female, item_reader {} );
        }
        if( items_obj.has_object( "female" ) ) {
            _starting_items_female = item_group::load_item_group( items_obj.get_member( "female" ),
                                     "collection" );
        }
    }
    optional( jo, was_loaded, "no_bonus", no_bonus );

    optional( jo, was_loaded, "starting_cash", _starting_cash );

    optional( jo, was_loaded, "skills", _starting_skills, skilllevel_reader {} );
    optional( jo, was_loaded, "addictions", _starting_addictions, addiction_reader {} );
    // TODO: use string_id<bionic_type> or so
    optional( jo, was_loaded, "CBMs", _starting_CBMs, auto_flags_reader<bionic_id> {} );
    // TODO: use string_id<mutation_branch> or so
    optional( jo, was_loaded, "traits", _starting_traits, auto_flags_reader<trait_id> {} );
    optional( jo, was_loaded, "forbidden_traits", _forbidden_traits, auto_flags_reader<trait_id> {} );
    optional( jo, was_loaded, "forbidden_bionics", _forbidden_bionics, auto_flags_reader<bionic_id> {} );
    optional( jo, was_loaded, "allowed_traits", _allowed_traits, auto_flags_reader<trait_id> {} );
    optional( jo, was_loaded, "allowed_bionics", _allowed_bionics, auto_flags_reader<bionic_id> {} );
    optional( jo, was_loaded, "forbids_bionics", _forbids_bionics );
    optional( jo, was_loaded, "flags", flags, auto_flags_reader<> {} );

    optional( jo, was_loaded, "missions", _missions, auto_flags_reader<mission_type_id> {} );
    optional( jo, was_loaded, "npcs", _starting_npcs, auto_flags_reader<npc_class_id> {} );
}

const profession_id &profession::generic()
{
    return generic_profession_id;
}

const std::vector<profession> &profession::get_all()
{
    return all_profs.get_all();
}

void profession::reset()
{
    all_profs.reset();
    item_substitutions.reset();
}

void profession::check_definitions()
{
    item_substitutions.check_consistency();
    for( const auto &prof : all_profs.get_all() ) {
        prof.check_definition();
    }
}

void profession::check_item_definitions( const itypedecvec &items ) const
{
    for( auto &itd : items ) {
        if( !itd.type_id.is_valid() ) {
            debugmsg( "profession %s: item %s does not exist", id.str(), itd.type_id.str() );
        } else if( !itd.snip_id.is_null() ) {
            const itype *type = &*itd.type_id;
            if( type->snippet_category.empty() ) {
                debugmsg( "profession %s: item %s has no snippet category - no description can "
                          "be set", id.str(), itd.type_id.str() );
            } else {
                if( !itd.snip_id.is_valid() ) {
                    debugmsg( "profession %s: there's no snippet with id %s",
                              id.str(), itd.snip_id.str() );
                }
            }
        }
    }
}

void profession::check_definition() const
{
    check_item_definitions( legacy_starting_items );
    check_item_definitions( legacy_starting_items_female );
    check_item_definitions( legacy_starting_items_male );
    if( !no_bonus.is_empty() && !no_bonus.is_valid() ) {
        debugmsg( "no_bonus item '%s' is not an itype_id", no_bonus.c_str() );
    }

    if( !item_group::group_is_defined( _starting_items ) ) {
        debugmsg( "_starting_items group is undefined" );
    }
    if( !item_group::group_is_defined( _starting_items_male ) ) {
        debugmsg( "_starting_items_male group is undefined" );
    }
    if( !item_group::group_is_defined( _starting_items_female ) ) {
        debugmsg( "_starting_items_female group is undefined" );
    }
    if( _starting_vehicle && !_starting_vehicle.is_valid() ) {
        debugmsg( "vehicle prototype %s for profession %s does not exist", _starting_vehicle.c_str(),
                  id.c_str() );
    }
    for( const auto &a : _starting_CBMs ) {
        if( !a.is_valid() ) {
            debugmsg( "bionic %s for profession %s does not exist", a.c_str(), id.c_str() );
        }
    }

    for( auto &t : _starting_traits ) {
        if( !t.is_valid() ) {
            debugmsg( "trait %s for profession %s does not exist", t.c_str(), id.c_str() );
        }
    }

    for( auto &t : _forbidden_bionics ) {
        if( !t.is_valid() ) {
            debugmsg( "bionic %s for profession %s does not exist", t.c_str(), id.c_str() );
        }
    }

    for( auto &t : _allowed_bionics ) {
        if( !t.is_valid() ) {
            debugmsg( "bionic %s for profession %s does not exist", t.c_str(), id.c_str() );
        }
    }
    for( const auto &elem : _starting_pets ) {
        if( !elem.is_valid() ) {
            debugmsg( "startng pet %s for profession %s does not exist", elem.c_str(), id.c_str() );
        }
    }
    for( const auto &elem : _starting_skills ) {
        if( !elem.first.is_valid() ) {
            debugmsg( "skill %s for profession %s does not exist", elem.first.c_str(), id.c_str() );
        }
    }

    for( const auto &m : _missions ) {
        if( !m.is_valid() ) {
            debugmsg( "starting mission %s for profession %s does not exist", m.c_str(), id.c_str() );
        }

        if( std::find( m->origins.begin(), m->origins.end(), ORIGIN_GAME_START ) == m->origins.end() ) {
            debugmsg( "starting mission %s for profession %s must include an origin of ORIGIN_GAME_START",
                      m.c_str(), id.c_str() );
        }
    }
    for( const auto &elem : _starting_npcs ) {
        if( !elem.is_valid() ) {
            debugmsg( "npc class %s for profession %s does not exist", elem.c_str(), id.c_str() );
        }
    }
}

bool profession::has_initialized()
{
    return generic_profession_id.is_valid();
}

const profession_id &profession::ident() const
{
    return id;
}

std::string profession::gender_appropriate_name( bool male ) const
{
    if( male ) {
        return _name_male.translated();
    } else {
        return _name_female.translated();
    }
}

std::string profession::description( bool male ) const
{
    if( male ) {
        return _description_male.translated();
    } else {
        return _description_female.translated();
    }
}

static time_point advanced_spawn_time()
{
    const int initial_days = get_option<int>( "INITIAL_DAY" );
    return calendar::before_time_starts + 1_days * initial_days;
}

signed int profession::point_cost() const
{
    return _point_cost;
}

std::optional<int> profession::starting_cash() const
{
    return _starting_cash;
}

static void clear_faults( item &it )
{
    if( it.get_var( "dirt", 0 ) > 0 ) {
        it.set_var( "dirt", 0 );
    }
    if( it.is_faulty() ) {
        it.faults.clear();
    }
}

std::vector<detached_ptr<item>> profession::items( bool male,
                             const std::vector<trait_id> &traits ) const
{
    std::vector<detached_ptr<item>> result;
    auto add_legacy_items = [&result]( const itypedecvec & vec ) {
        for( const itypedec &elem : vec ) {
            detached_ptr<item> it = item::spawn( elem.type_id, advanced_spawn_time(), item::default_charges_tag {} );
            if( !elem.snip_id.is_null() ) {
                it->set_snippet( elem.snip_id );
            }
            it = item::in_its_container( std::move( it ) );
            result.push_back( std::move( it ) );
        }
    };

    add_legacy_items( legacy_starting_items );
    add_legacy_items( male ? legacy_starting_items_male : legacy_starting_items_female );

    std::vector<detached_ptr<item>> group_both = item_group::items_from( _starting_items,
                                 advanced_spawn_time() );
    std::vector<detached_ptr<item>> group_gender = item_group::items_from(
                                     male ? _starting_items_male :
                                     _starting_items_female, advanced_spawn_time() );
    result.insert( result.end(),
                   std::make_move_iterator( group_both.begin() ),
                   std::make_move_iterator( group_both.end() ) );
    result.insert( result.end(),
                   std::make_move_iterator( group_gender.begin() ),
                   std::make_move_iterator( group_gender.end() ) );

    std::vector<itype_id> bonus = item_substitutions.get_bonus_items( traits );
    for( const itype_id &elem : bonus ) {
        if( elem != no_bonus ) {
            auto bonus_item = item::spawn( elem, advanced_spawn_time(), item::default_charges_tag {} );
            if( !bonus_item->magazine_current() &&
                bonus_item->magazine_default() != itype_id::NULL_ID() ) {
                bonus_item->put_in( item::spawn( bonus_item->magazine_default(),
                                                 bonus_item->birthday() ) );
            }
            result.push_back( std::move( bonus_item ) );
        }
    }
    for( auto iter = result.begin(); iter != result.end(); ) {
        auto sub = item_substitutions.get_substitution( **iter, traits );
        if( !sub.empty() ) {
            iter = result.erase( iter );
            int offset = std::distance( result.begin(), iter );
            result.insert( result.end(), std::make_move_iterator( sub.begin() ),
                           std::make_move_iterator( sub.end() ) );
            iter = result.begin();
            std::advance( iter, offset );
        } else {
            ++iter;
        }
    }
    for( detached_ptr<item> &it : result ) {
        clear_faults( *it );
        if( it->is_holster() && it->contents.num_item_stacks() == 1 ) {
            clear_faults( it->contents.front() );
        }
        if( it->has_flag( flag_VARSIZE ) ) {
            it->set_flag( flag_FIT );
        }
    }

    if( result.empty() ) {
        // No need to do the below stuff. Plus it would cause said below stuff to crash
        return result;
    }

    // Merge charges for items that stack with each other
    for( auto outer = result.begin(); outer != result.end(); ++outer ) {
        if( !( *outer )->count_by_charges() ) {
            continue;
        }
        for( auto inner = std::next( outer ); inner != result.end(); ) {
            if( ( *outer )->stacks_with( **inner ) ) {
                ( *outer )->merge_charges( std::move( *inner ) );
                inner = result.erase( inner );
            } else {
                ++inner;
            }
        }
    }

    std::ranges::stable_sort( result,
    []( const detached_ptr<item> &first, const detached_ptr<item> &second ) {
        return first->get_layer() < second->get_layer();
    } );
    return result;
}

vproto_id profession::vehicle() const
{
    return _starting_vehicle;
}

std::vector<mtype_id> profession::pets() const
{
    return _starting_pets;
}

std::vector<addiction> profession::addictions() const
{
    return _starting_addictions;
}

std::vector<bionic_id> profession::CBMs() const
{
    return _starting_CBMs;
}

std::vector<trait_id> profession::get_locked_traits() const
{
    return _starting_traits;
}

std::set<trait_id> profession::get_forbidden_traits() const
{
    return _forbidden_traits;
}

std::set<trait_id> profession::get_allowed_traits() const
{
    return _allowed_traits;
}

std::vector<bionic_id> profession::get_locked_bionics() const
{
    return _starting_CBMs;
}

std::set<bionic_id> profession::get_allowed_bionics() const
{
    return _allowed_bionics;
}

bool profession::forbids_bionics() const
{
    return _forbids_bionics;
}
profession::StartingSkillList profession::skills() const
{
    return _starting_skills;
}

std::vector<npc_class_id> profession::npcs() const
{
    return _starting_npcs;
}
bool profession::has_flag( const std::string &flag ) const
{
    return flags.contains( flag );
}

bool profession::is_locked_trait( const trait_id &trait ) const
{
    return std::ranges::find( _starting_traits, trait ) !=
           _starting_traits.end();
}

bool profession::is_forbidden_trait( const trait_id &trait ) const
{
    return _forbidden_traits.contains( trait );
}

bool profession::is_allowed_trait( const trait_id &trait ) const
{
    return _allowed_traits.contains( trait );
}

bool profession::is_locked_bionic( const bionic_id &bionic ) const
{
    return std::ranges::find( _starting_CBMs, bionic ) !=
           _starting_CBMs.end();
}

bool profession::is_forbidden_bionic( const bionic_id &bionic ) const
{
    return _forbidden_bionics.contains( bionic );
}

bool profession::is_allowed_bionic( const bionic_id &bionic ) const
{
    return _allowed_bionics.contains( bionic );
}

std::map<spell_id, int> profession::spells() const
{
    return _starting_spells;
}

// item_substitution stuff:

void profession::load_item_substitutions( const JsonObject &jo )
{
    item_substitutions.load( jo );
}

void json_item_substitution::reset()
{
    substitutions.clear();
    bonuses.clear();
}

json_item_substitution::substitution::info::info( const JsonValue &value )
{
    if( value.test_string() ) {
        value.read( new_item, true );
    } else {
        const JsonObject jo = value.get_object();
        jo.read( "item", new_item, true );
        ratio = jo.get_float( "ratio" );
        if( ratio <= 0.0 ) {
            jo.throw_error( "Ratio must be positive", "ratio" );
        }
    }
}

json_item_substitution::trait_requirements::trait_requirements( const JsonObject &obj )
{
    for( const std::string line : obj.get_array( "present" ) ) {
        present.emplace_back( line );
    }
    for( const std::string line : obj.get_array( "absent" ) ) {
        absent.emplace_back( line );
    }
}

void json_item_substitution::load( const JsonObject &jo )
{
    const bool item_mode = jo.has_string( "item" );
    const std::string title = jo.get_string( item_mode ? "item" : "trait" );

    auto check_duplicate_item = [&]( const itype_id & it ) {
        return substitutions.contains( it ) ||
               std::ranges::find_if( bonuses,
        [&it]( const std::pair<itype_id, trait_requirements> &p ) {
            return p.first == it;
        } ) != bonuses.end();
    };
    if( item_mode && check_duplicate_item( itype_id( title ) ) ) {
        jo.throw_error( "Duplicate definition of item" );
    }

    if( item_mode ) {
        if( jo.has_member( "bonus" ) ) {
            bonuses.emplace_back( itype_id( title ),
                                  trait_requirements( jo.get_object( "bonus" ) ) );
        }

        for( const JsonValue sub : jo.get_array( "sub" ) ) {
            substitution s;
            JsonObject obj = sub.get_object();
            s.trait_reqs = trait_requirements( obj );
            for( const JsonValue info : obj.get_array( "new" ) ) {
                s.infos.emplace_back( info );
            }
            substitutions[itype_id( title )].push_back( s );
        }
    } else {
        for( const JsonObject sub : jo.get_array( "sub" ) ) {
            substitution s;
            itype_id old_it;
            sub.read( "item", old_it, true );
            if( check_duplicate_item( old_it ) ) {
                sub.throw_error( "Duplicate definition of item" );
            }
            s.trait_reqs.present.emplace_back( title );
            for( const JsonValue info : sub.get_array( "new" ) ) {
                s.infos.emplace_back( info );
            }
            substitutions[old_it].push_back( s );
        }
    }
}

void json_item_substitution::check_consistency()
{
    auto check_if_trait = []( const trait_id & t ) {
        if( !t.is_valid() ) {
            debugmsg( "%s is not a trait", t.c_str() );
        }
    };
    auto check_if_itype = []( const itype_id & i ) {
        if( !i.is_valid() ) {
            debugmsg( "%s is not an itype_id", i.c_str() );
        }
    };
    auto check_trait_reqs = [&check_if_trait]( const trait_requirements & tr ) {
        for( const trait_id &str : tr.present ) {
            check_if_trait( str );
        }
        for( const trait_id &str : tr.absent ) {
            check_if_trait( str );
        }
    };

    for( const auto &pair : substitutions ) {
        check_if_itype( pair.first );
        for( const substitution &s : pair.second ) {
            check_trait_reqs( s.trait_reqs );
            for( const substitution::info &inf : s.infos ) {
                check_if_itype( inf.new_item );
            }
        }
    }
    for( const auto &pair : bonuses ) {
        check_if_itype( pair.first );
        check_trait_reqs( pair.second );
    }
}

bool json_item_substitution::trait_requirements::meets_condition( const std::vector<trait_id>
        &traits ) const
{
    const auto pred = [&traits]( const trait_id & s ) {
        return std::ranges::find( traits, s ) != traits.end();
    };
    return std::ranges::all_of( present, pred ) &&
           std::ranges::none_of( absent, pred );
}

std::vector<detached_ptr<item>> json_item_substitution::get_substitution( const item &it,
                             const std::vector<trait_id> &traits ) const
{
    auto iter = substitutions.find( it.typeId() );
    std::vector<detached_ptr<item>> ret;
    if( iter == substitutions.end() ) {
        for( const item *con : it.contents.all_items_top() ) {
            auto sub = get_substitution( *con, traits );
            ret.insert( ret.end(), std::make_move_iterator( sub.begin() ),
                        std::make_move_iterator( sub.end() ) );
        }
        return ret;
    }

    const auto sub = std::find_if( iter->second.begin(),
    iter->second.end(), [&traits]( const substitution & s ) {
        return s.trait_reqs.meets_condition( traits );
    } );
    if( sub == iter->second.end() ) {
        return ret;
    }

    const int old_amt = it.count();
    for( const substitution::info &inf : sub->infos ) {
        const int new_amt = std::max( 1, static_cast<int>( std::round( inf.ratio * old_amt ) ) );
        if( !inf.new_item->count_by_charges() ) {
            for( int i = 0; i < new_amt; i++ ) {
                ret.push_back( item::in_its_container( item::spawn( inf.new_item, advanced_spawn_time() ) ) );
            }
        } else {
            detached_ptr<item> result = item::spawn( inf.new_item, advanced_spawn_time() );
            result->mod_charges( -result->charges + new_amt );
            while( result->charges > 0 ) {
                item &pushed = *result;
                ret.push_back( item::in_its_container( std::move( result ) ) );
                result = item::spawn( pushed );
                result->mod_charges( pushed.contents.empty() ? -pushed.charges :
                                     -pushed.contents.back().charges );
            }
        }
    }
    return ret;
}

std::vector<itype_id> json_item_substitution::get_bonus_items( const std::vector<trait_id>
        &traits ) const
{
    std::vector<itype_id> ret;
    for( const auto &pair : bonuses ) {
        if( pair.second.meets_condition( traits ) ) {
            ret.push_back( pair.first );
        }
    }
    return ret;
}

const std::vector<mission_type_id> &profession::missions() const
{
    return _missions;
}
