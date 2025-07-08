#include <algorithm>
#include <map>
#include <sstream>
#include <vector>

#include "calendar.h"
#include "cata_utility.h"
#include "creature.h"
#include "debug.h"
#include "debug_menu.h"
#include "effect.h"
#include "input.h"
#include "json.h"
#include "output.h"
#include "string_input_popup.h"
#include "string_utils.h"
#include "units_serde.h"
#include "ui.h"
#include "uistate.h"

namespace debug_menu
{


namespace
{

struct entry_data {
    entry_data( const bodypart_str_id &body_part )
        : body_part( body_part )
    {}
    entry_data( const bodypart_str_id &body_part, const effect &eff )
        : body_part( body_part )
        , eff( &eff )
    {}
    bodypart_str_id body_part;
    // If not set, we're a body part
    std::optional<const effect *> eff;
};

template<class T, typename F = std::pair<const std::string, void ( T::* )(
             const input_context &,
             const input_event &,
             int,
             uilist & )>>
void register_callback( uilist &menu, T &callback )
{
    menu.callback = &callback;
    for( const F &action : T::get_handled_actions() ) {
        menu.additional_actions.emplace_back( action.first, translation() );
    }
}

void foreach_effect( Creature &c,
                     const entry_data &entry,
                     const std::function<void( const effect & )> &fun )
{
    if( entry.eff ) {
        fun( **entry.eff );
    } else {
        // Body part, so apply to all effects on it
        for( const auto &pr : c.get_all_effects() ) {
            for( const std::pair<const bodypart_str_id, effect> &inner : pr.second ) {
                if( inner.first == entry.body_part ) {
                    fun( inner.second );
                }
            }
        }
    }
}

std::optional<bodypart_str_id> query_body_part( Creature &c )
{
    wisheffect_state &last_val = uistate.debug_menu.effect;
    const std::vector<bodypart_id> &all_bps = c.get_all_body_parts();
    uilist menu;
    size_t i = 0;
    for( const bodypart_id &bp : all_bps ) {
        menu.addentry( i++, true, -1, bp.id().str() );
    }
    menu.query();
    if( menu.ret >= 0 && menu.ret < static_cast<int>( all_bps.size() ) ) {
        last_val.bodypart = all_bps[static_cast<size_t>( menu.ret )].id();
    } else {
        return std::nullopt;
    }
    return last_val.bodypart;
}
std::optional<int> query_intensity()
{
    wisheffect_state &last_val = uistate.debug_menu.effect;
    try {
        last_val.intensity = string_input_popup()
                             .title( _( "Input new intensity" ) )
                             .only_digits( true )
                             .query_int();
    } catch( std::exception & ) {
        return std::nullopt;
    }
    return last_val.intensity;
}
std::optional<time_duration> try_read_time_string( const std::string &ts )
{
    // Ugly hack, but does the job (it's debug anyway)
    // Have to wrap it in quotes because we're reading a raw string
    const std::string quoted_ts = '"' + ts + '"';
    try {
        std::istringstream iss( quoted_ts );
        JsonIn jsin( iss );
        return read_from_json_string<time_duration>( jsin, time_duration::units );
    } catch( const JsonError &e ) {
        // Try unit-less conversion
        try {
            return time_duration::from_turns( std::stoi( ts ) );
        } catch( std::exception & ) {
        }
        popup( e.c_str() );
        return std::nullopt;
    }
}
std::optional<time_duration> query_duration()
{
    wisheffect_state &last_val = uistate.debug_menu.effect;
    string_input_popup popup;
    popup.title( _( "Input new duration followed by unit (s, m, h, d)" ) );
    std::string dur_string = popup.query_string();
    if( !popup.confirmed() ) {
        return std::nullopt;
    }
    std::optional<time_duration> dur = try_read_time_string( dur_string );
    if( dur ) {
        last_val.duration = *dur;
        return dur;
    } else {
        return std::nullopt;
    }
}
bool toggle_effect_force()
{
    uistate.debug_menu.effect.force = !uistate.debug_menu.effect.force;
    return uistate.debug_menu.effect.force;
}

} // namespace

class effect_select_callback : public uilist_callback
{
    private:
        using effect_select_fun = void ( effect_select_callback::* )(
                                      const input_context &,
                                      const input_event &,
                                      int,
                                      uilist & );
        static const std::map<std::string, effect_select_fun> handled_actions;

        // This is "almost static", might be cached on finalize
        const std::vector<efftype_id> all_effects = find_all_effect_types();

        Creature &c;

    public:
        effect_select_callback( Creature &c )
            : c( c )
        {}

        static const std::map<std::string, effect_select_fun> &get_handled_actions() {
            return handled_actions;
        }

        bool key( const input_context &ctx, const input_event &key, int entnum,
                  uilist *parent_menu ) override {
            const std::string &action = ctx.input_to_action( key );
            auto iter = handled_actions.find( action );
            if( iter != handled_actions.end() ) {
                // This ugly thing is a call to method using a pointer-to-member-method
                ( ( *this ).*( iter->second ) )( ctx, key, entnum, *parent_menu );
                return true;
            }
            return false;

        }

        void refresh( uilist *menu ) override {
            wisheffect_state &last_val = uistate.debug_menu.effect;
            size_t selected = clamp<size_t>( menu->selected, 0, all_effects.size() - 1 );
            input_context ctxt( menu->input_category );

            const point start( menu->w_width - menu->pad_right, 3 );
            const int width = menu->w_width - start.x;
            std::stringstream ss;
            ss << string_format( "ID: %s\n", all_effects[selected].str() );
            const auto eff_type = all_effects[ selected];
            ss << string_format( "[%s] <bold>Body part</bold>: %s\n",
                                 ctxt.get_desc( "CHANGE_BODY_PART" ),
                                 last_val.bodypart
                                 ? last_val.bodypart->name.translated().c_str()
                                 : "Global" );

            time_duration dur = last_val.duration <= 0_seconds
                                ? eff_type->get_max_duration()
                                : last_val.duration;
            ss << string_format( "[%s] <bold>Duration</bold>: %10d (max: %d)\n",
                                 ctxt.get_desc( "CHANGE_DURATION" ),
                                 to_turns<int>( dur ),
                                 to_turns<int>( eff_type->get_max_duration() ) );

            ss << string_format( "[%s] <bold>Intensity</bold>: %d\n",
                                 ctxt.get_desc( "CHANGE_INTENSITY" ),
                                 std::max( last_val.intensity, 1 ) );

            ss << string_format( "[%s] <bold>Force</bold>: %s\n",
                                 ctxt.get_desc( "TOGGLE_FORCE" ),
                                 last_val.force
                                 ? "Yes"
                                 : "No" );

            ss << '\n';
            ss << string_format( "Permanent: %s\n", eff_type->is_permanent()
                                 ? "Yes"
                                 : "No" );
            fold_and_print_from( menu->window, start, width, 0, c_light_gray, replace_colors( ss.str() ) );
            wnoutrefresh( menu->window );
        }

        void change_body_part( const input_context &, const input_event &, int,
                               uilist & ) {
            query_body_part( c );
        }
        void change_intensity( const input_context &, const input_event &, int,
                               uilist & ) {
            query_intensity();
        }
        void change_duration( const input_context &, const input_event &, int,
                              uilist & ) {
            query_duration();
        }
        void toggle_force( const input_context &, const input_event &, int,
                           uilist & ) {
            toggle_effect_force();
        }
};

const std::map<std::string, effect_select_callback::effect_select_fun>
effect_select_callback::handled_actions = {{
        { "CHANGE_BODY_PART", &effect_select_callback::change_body_part },
        { "CHANGE_INTENSITY", &effect_select_callback::change_intensity },
        { "CHANGE_DURATION", &effect_select_callback::change_duration },
        { "TOGGLE_FORCE", &effect_select_callback::toggle_force },
    }
};

class bodypart_select_callback : public uilist_callback
{

};

class effect_edit_callback : public uilist_callback
{
    private:
        using effect_edit_fun = void ( effect_edit_callback::* )(
                                    const input_context &,
                                    const input_event &,
                                    int,
                                    uilist & );
        static const std::map<std::string, effect_edit_fun> handled_actions;

        Creature &c;

        // This is "almost static", might be cached on finalize
        const std::vector<efftype_id> all_effects = find_all_effect_types();
        // Should be aligned with entries
        const std::vector<entry_data> meta;
        std::function<void()> on_creature_changed;

    public:
        effect_edit_callback( Creature &c,
                              const std::vector<entry_data> &meta,
                              const std::function<void()> &on_creature_changed )
            : c( c )
            , meta( meta )
            , on_creature_changed( on_creature_changed )
        {}

        static std::map<std::string, effect_edit_fun> get_handled_actions() {
            return handled_actions;
        }


        bool key( const input_context &ctx, const input_event &key, int entnum,
                  uilist *parent_menu ) override {
            const std::string &action = ctx.input_to_action( key );
            auto iter = handled_actions.find( action );
            if( iter != handled_actions.end() ) {
                // This ugly thing is a call to method using a pointer-to-member-method
                ( ( *this ).*( iter->second ) )( ctx, key, entnum, *parent_menu );
                return true;
            }
            return false;

        }

    private:

        void add_effect_submenu( const input_context &, const input_event &, int,
                                 uilist &menu ) {
            wisheffect_state &last_val = uistate.debug_menu.effect;
            uilist submenu;
            submenu.w_x_setup = 0;
            submenu.w_width_setup = []() -> int {
                return TERMX;
            };
            submenu.pad_right_setup = []() -> int {
                return TERMX - 40;
            };
            effect_select_callback callback( c );
            register_callback( submenu, callback );
            submenu.input_category = menu.input_category;
            int i = 0;
            for( const efftype_id &eff : all_effects ) {
                submenu.addentry( i++, true, -1, eff.str() );
            }
            submenu.set_selected( last_val.last_type_selected_index );

            submenu.query();
            if( submenu.ret >= 0 && submenu.ret < static_cast<int>( all_effects.size() ) ) {
                const efftype_id eff_type = all_effects[static_cast<size_t>( submenu.ret )];
                last_val.last_type_selected_index = submenu.ret;
                time_duration duration = last_val.duration <= 0_seconds ? eff_type->get_max_duration() :
                                         last_val.duration;
                c.add_effect( eff_type, duration, last_val.bodypart, last_val.intensity, last_val.force );
                on_creature_changed();
            }
        }

        void clear_effects( const input_context &, const input_event &, int,
                            uilist & ) {
            on_creature_changed();
            c.clear_effects();
            if( !c.get_all_effects().empty() ) {
                debugmsg( "Couldn't clear effects" );
            }
        }
        void remove( const input_context &, const input_event &, int entnum,
                     uilist & ) {
            on_creature_changed();
            foreach_effect( c, meta[entnum], [&]( const effect & eff ) {
                bool removed = c.remove_effect( eff.get_id(), eff.get_bp() );
                if( !removed ) {
                    debugmsg( "Couldn't remove %s from %s",
                              eff.get_id().str(),
                              eff.get_bp().str() );
                }
            } );

        }
        void change_body_part( const input_context &, const input_event &, int entnum,
                               uilist &parent_menu ) {
            std::optional<bodypart_str_id> bp = query_body_part( c );
            if( bp ) {
                on_creature_changed();
                const bodypart_str_id bp_from = meta[entnum].body_part;
                std::vector<effect> to_move;
                foreach_effect( c, meta[entnum], [&]( const effect & eff ) {
                    to_move.push_back( eff );
                } );
                for( const effect &eff : to_move ) {
                    c.remove_effect( eff.get_id(), bp_from );
                    c.add_effect( eff.get_id(), eff.get_duration(), *bp, eff.get_intensity(), true );
                }
                auto iter_to = std::ranges::find_if( meta, [bp]( const entry_data & ed ) {
                    return ed.body_part == *bp;
                } );
                size_t bp_index = std::distance( iter_to, meta.begin() ) % meta.size();
                parent_menu.set_selected( bp_index );
            }
        }
        void change_intensity( const input_context &, const input_event &, int entnum,
                               uilist & ) {
            std::optional<int> new_intensity = query_intensity();
            if( new_intensity ) {
                on_creature_changed();
                foreach_effect( c, meta[entnum], [&]( const effect & eff ) {
                    effect &e = c.get_effect( eff.get_id(), eff.get_bp() );
                    if( !e.is_null() ) {
                        e.set_intensity( *new_intensity );
                    }
                } );
            }
        }
        void change_duration( const input_context &, const input_event &, int entnum,
                              uilist & ) {
            std::optional<time_duration> dur = query_duration();
            if( dur ) {
                on_creature_changed();
                foreach_effect( c, meta[entnum], [&]( const effect & eff ) {
                    effect &e = c.get_effect( eff.get_id(), eff.get_bp() );
                    if( !e.is_null() ) {
                        e.set_duration( *dur );
                    }
                } );
            }
        }
        void toggle_force( const input_context &, const input_event &, int,
                           uilist & ) {
            toggle_effect_force();
        }
};

const std::map<std::string, effect_edit_callback::effect_edit_fun>
effect_edit_callback::handled_actions = {{
        { "ADD_NEW_EFFECT", &effect_edit_callback::add_effect_submenu },
        { "CLEAR_EFFECTS", &effect_edit_callback::clear_effects },
        { "REMOVE", &effect_edit_callback::remove },
        { "CHANGE_BODY_PART", &effect_edit_callback::change_body_part },
        { "CHANGE_INTENSITY", &effect_edit_callback::change_intensity },
        { "CHANGE_DURATION", &effect_edit_callback::change_duration },
        { "TOGGLE_FORCE", &effect_edit_callback::toggle_force },
    }
};

void effect_edit_menu( Creature &c )
{
    bool not_dirty = false;
    bool stay_in_menu = true;
    int last_selected = 0;
    while( stay_in_menu ) {
        const effects_map &em = c.get_all_effects();
        uilist menu;
        menu.input_category = "edit_creature_effects";
        std::map<bodypart_str_id, std::vector<const effect *>> bp_effects;
        const std::vector<bodypart_id> &all_bps = c.get_all_body_parts();
        for( const auto &pr : em ) {
            for( const auto &inner : pr.second ) {
                bp_effects[inner.first].push_back( &inner.second );
                if( !inner.first.is_null() &&
                    std::count( all_bps.begin(), all_bps.end(), inner.first ) == 0 ) {
                    debugmsg( "Creature %s has effect on body part %s, not in anatomy",
                              c.get_name(), inner.first.str() );
                }
            }
        }

        std::vector<entry_data> meta;
        size_t i = 0;
        const auto add_effect_entry = [&menu, &i]( const effect & eff ) {
            // TODO: Columns
            std::string effect_description = string_format(
                                                 "- %s %d, %d s",
                                                 eff.get_id().str(),
                                                 eff.get_intensity(),
                                                 to_seconds<int>( eff.get_duration() ) );
            menu.addentry( i++, true, 0, effect_description );
        };
        menu.addentry( i++, true, -1, _( "Global" ) );
        auto global_iter = bp_effects.find( bodypart_str_id::NULL_ID() );
        meta.emplace_back( bodypart_str_id::NULL_ID() );
        if( global_iter != bp_effects.end() ) {
            for( const effect *eff : global_iter->second ) {
                add_effect_entry( *eff );
                meta.emplace_back( bodypart_str_id::NULL_ID(), *eff );
            }

        }
        for( const bodypart_id &bp : all_bps ) {
            menu.addentry( i++, true, -1, bp.id().str() );
            meta.emplace_back( bp.id() );
            auto iter = bp_effects.find( bp.id() );
            if( iter != bp_effects.end() ) {
                for( const effect *eff : iter->second ) {
                    add_effect_entry( *eff );
                    meta.emplace_back( bp.id(), *eff );
                }
            }
        }

        const auto on_creature_changed = [&not_dirty]() {
            not_dirty = false;
        };
        effect_edit_callback callback( c, meta, on_creature_changed );
        register_callback( menu, callback );
        not_dirty = true;
        while( not_dirty ) {
            // Have to do this because menus don't support entry removal
            menu.set_selected( last_selected );
            menu.query( false );
            if( menu.ret == UILIST_CANCEL ) {
                stay_in_menu = false;
                break;
            }
            last_selected = menu.selected;
        }
    }
}

} // namespace debug_menu
