#include "proc_ui.h"

#include <algorithm>
#include <map>
#include <optional>
#include <ranges>
#include <string>
#include <vector>

#include "character.h"
#include "cursesdef.h"
#include "game.h"
#include "input.h"
#include "inventory.h"
#include "item.h"
#include "item_factory.h"
#include "output.h"
#include "proc_fact.h"
#include "recipe.h"
#include "string_formatter.h"
#include "translations.h"
#include "ui.h"
#include "ui_manager.h"

namespace
{

static const trait_id trait_DEBUG_HS( "DEBUG_HS" );

struct source_entry {
    item *src = nullptr;
    std::string where;
    proc::part_fact fact;
};

struct source_pool {
    std::vector<detached_ptr<item>> owned;
    std::vector<source_entry> entries;
};

struct source_options {
    item *src = nullptr;
    std::string where;
    proc::part_ix ix = proc::invalid_part_ix;
    int charges = 0;
    int uses = 1;
};

auto make_source_entry( const source_options &opts ) -> source_entry
{
    return source_entry{
        .src = opts.src,
        .where = opts.where,
        .fact = proc::normalize_part_fact( *opts.src, {
            .ix = opts.ix,
            .charges = opts.charges,
            .uses = opts.uses
        } )
    };
}

auto source_for_ix( const std::vector<source_entry> &sources,
                    const proc::part_ix ix ) -> const source_entry * // *NOPAD*
{
    if( ix < 0 || static_cast<size_t>( ix ) >= sources.size() ) {
        return nullptr;
    }
    return &sources[static_cast<size_t>( ix )];
}

auto gather_inventory_sources( Character &who, const proc::schema &sch ) -> source_pool
{
    auto ret = source_pool {};
    auto ix = proc::part_ix{ 0 };
    const auto &inv = who.crafting_inventory();
    std::ranges::for_each( inv.const_slice(), [&]( const std::vector<item *> *stack ) {
        if( stack == nullptr ) {
            return;
        }
        std::ranges::for_each( *stack, [&]( item * it ) {
            if( it == nullptr || it->is_craft() ) {
                return;
            }
            const auto entry = make_source_entry( {
                .src = it,
                .where = it->describe_location( &who ),
                .ix = ix,
                .charges = it->count_by_charges() ? 1 : 0,
                .uses = it->count_by_charges() ? std::max( it->charges, 1 ) : 1
            } );
            if( std::ranges::none_of( sch.slots, [&]( const proc::slot_data & slot ) {
            return proc::matches_slot( slot, entry.fact );
            } ) ) {
                return;
            }
            ret.entries.push_back( entry );
            ix++;
        } );
    } );
    return ret;
}

auto matching_slot_uses( const proc::schema &sch, const proc::part_fact &fact ) -> int
{
    auto uses = 0;
    std::ranges::for_each( sch.slots, [&]( const proc::slot_data & slot ) {
        if( proc::matches_slot( slot, fact ) ) {
            uses += std::max( slot.max, 1 );
        }
    } );
    return uses;
}

auto gather_debug_sources( const proc::schema &sch ) -> source_pool
{
    auto ret = source_pool {};
    auto ix = proc::part_ix{ 0 };
    const auto candidates = item_controller->all();
    std::ranges::for_each( candidates, [&]( const itype * candidate ) {
        if( candidate == nullptr ) {
            return;
        }
        ret.owned.push_back( item::spawn( candidate->get_id(), calendar::turn ) );
        auto *temp = &*ret.owned.back();
        auto entry = make_source_entry( {
            .src = temp,
            .where = _( "debug hammerspace" ),
            .ix = ix,
            .charges = temp->count_by_charges() ? 1 : 0,
            .uses = 1
        } );
        const auto uses = matching_slot_uses( sch, entry.fact );
        if( uses <= 0 ) {
            ret.owned.pop_back();
            return;
        }
        entry.fact.uses = uses;
        ret.entries.push_back( entry );
        ix++;
    } );
    return ret;
}

auto missing_required_slots( const proc::schema &sch,
                             const proc::builder_state &state ) -> std::vector<std::string>
{
    auto ret = std::vector<std::string> {};
    std::ranges::for_each( sch.slots, [&]( const proc::slot_data & slot ) {
        const auto iter = state.cand.find( slot.id );
        const auto missing = slot.min > 0 && ( iter == state.cand.end() || iter->second.empty() );
        if( missing ) {
            ret.push_back( slot.role );
        }
    } );
    return ret;
}

auto current_slot( const proc::schema &sch, const int slot_cursor ) -> const proc::slot_data &
{
    return sch.slots[static_cast<size_t>( slot_cursor )];
}

auto current_candidates( const proc::builder_state &state,
                         const proc::slot_id &slot ) -> const std::vector<proc::part_ix> & // *NOPAD*
{
    static const auto empty = std::vector<proc::part_ix> {};
    const auto iter = state.cand.find( slot );
    return iter == state.cand.end() ? empty : iter->second;
}

auto slot_pick_text( const proc::builder_state &state, const proc::slot_data &slot,
                     const std::vector<source_entry> &sources ) -> std::string
{
    auto picks = std::vector<std::string> {};
    std::ranges::for_each( state.picks_for( slot.id ), [&]( const proc::part_ix ix ) {
        if( const auto *source = source_for_ix( sources, ix ) ) {
            picks.push_back( source->src->tname() );
        }
    } );
    const auto count = static_cast<int>( picks.size() );
    const auto body = picks.empty() ? _( "empty" ) : enumerate_as_string( picks,
                      enumeration_conjunction::none );
    return string_format( "%s %d/%d %s", slot.role, count, slot.max, body );
}

auto candidate_text( const source_entry &source, const proc::builder_state &state ) -> std::string
{
    const auto remaining = std::max( source.fact.uses - proc::pick_count( state, source.fact.ix ), 0 );
    const auto count_suffix = remaining > 1 ? string_format( " x%d", remaining ) : std::string();
    return string_format( "%s [%s]%s", source.src->tname(), source.where, count_suffix );
}

auto preview_lines( const proc::schema &sch, const proc::builder_state &state,
                    const std::vector<source_entry> &sources ) -> std::vector<std::string>
{
    auto lines = std::vector<std::string> {};
    const auto title = state.fast.name.empty() ? sch.id.str() : state.fast.name;
    lines.push_back( title );
    lines.push_back( string_format( _( "Mass: %d g" ), state.fast.mass_g ) );
    lines.push_back( string_format( _( "Volume: %d ml" ), state.fast.volume_ml ) );
    if( sch.cat == "food" ) {
        lines.push_back( string_format( _( "Calories: %d" ), state.fast.kcal ) );
    }
    if( !state.fast.vit.empty() ) {
        auto vitamins = std::vector<std::string> {};
        std::ranges::for_each( state.fast.vit, [&]( const std::pair<const vitamin_id, int> &entry ) {
            vitamins.push_back( string_format( "%s:%d", entry.first.str(), entry.second ) );
        } );
        lines.push_back( string_format( _( "Vitamins: %s" ), enumerate_as_string( vitamins,
                                        enumeration_conjunction::none ) ) );
    }
    if( !state.fast.melee.empty() ) {
        lines.push_back( string_format( _( "Bash: %d" ), state.fast.melee.bash ) );
        lines.push_back( string_format( _( "Cut: %d" ), state.fast.melee.cut ) );
        lines.push_back( string_format( _( "Stab: %d" ), state.fast.melee.stab ) );
        lines.push_back( string_format( _( "To-hit: %+d" ), state.fast.melee.to_hit ) );
        lines.push_back( string_format( _( "Durability: %d" ), state.fast.melee.dur ) );
    }
    lines.push_back( std::string() );
    lines.push_back( _( "Selected parts:" ) );
    std::ranges::for_each( sch.slots, [&]( const proc::slot_data & slot ) {
        std::ranges::for_each( state.picks_for( slot.id ), [&]( const proc::part_ix ix ) {
            if( const auto *source = source_for_ix( sources, ix ) ) {
                lines.push_back( string_format( "%s: %s", slot.role, source->src->tname() ) );
            }
        } );
    } );
    return lines;
}

auto clear_slot( proc::builder_state &state, const proc::slot_id &slot ) -> void
{
    while( proc::remove_last_pick( state, slot ) ) {
    }
}

} // namespace

auto proc::open_builder( Character &who, const recipe &rec ) -> std::optional<ui_result>
{
    if( !rec.is_proc() || !proc::has( rec.proc_id() ) ) {
        return std::nullopt;
    }

    const auto &sch = proc::get( rec.proc_id() );
    if( sch.slots.empty() ) {
        popup( _( "Procedural builder has no slots." ) );
        return std::nullopt;
    }

    auto source_data = who.has_trait( trait_DEBUG_HS ) ? gather_debug_sources( sch ) :
                       gather_inventory_sources( who, sch );

    const auto facts = source_data.entries
    | std::views::transform( []( const source_entry & entry ) {
        return entry.fact;
    } )
    | std::ranges::to<std::vector>();
    auto state = proc::build_state( sch, facts );
    if( source_data.entries.empty() ) {
        popup( _( "No nearby items match this procedural recipe." ) );
        return std::nullopt;
    }
    if( const auto missing = missing_required_slots( sch, state ); !missing.empty() ) {
        popup( _( "Missing candidates for required slots: %s" ),
               enumerate_as_string( missing, enumeration_conjunction::none ) );
        return std::nullopt;
    }
    auto candidate_cursor = std::map<slot_id, int> {};
    std::ranges::for_each( sch.slots, [&]( const slot_data & slot ) {
        candidate_cursor.emplace( slot.id, 0 );
    } );
    auto slot_cursor = 0;
    auto slots_focused = true;
    auto status = std::string {};

    auto w = catacurses::window {};
    auto ui = ui_adaptor( ui_adaptor::disable_uis_below {} );
    auto width = FULL_SCREEN_WIDTH;
    auto height = TERMY;

    const auto resize_cb = [&]( ui_adaptor & adaptor ) {
        width = std::min( TERMX - 2, FULL_SCREEN_WIDTH * 2 );
        height = std::min( TERMY, FULL_SCREEN_HEIGHT );
        const auto start = point( ( TERMX - width ) / 2, ( TERMY - height ) / 2 );
        w = catacurses::newwin( height, width, start );
        adaptor.position_from_window( w );
    };
    resize_cb( ui );
    ui.on_screen_resize( resize_cb );

    ui.on_redraw( [&]( ui_adaptor & ) {
        werase( w );
        draw_border( w );

        const auto left_width = 28;
        const auto middle_width = 38;
        const auto right_width = width - left_width - middle_width - 4;
        const auto header = rec.builder_name().translated().empty() ? rec.result_name() :
                            rec.builder_name().translated();
        trim_and_print( w, point( 2, 1 ), width - 4, c_white, header );
        if( !rec.builder_desc().translated().empty() ) {
            trim_and_print( w, point( 2, 2 ), width - 4, c_light_gray, rec.builder_desc().translated() );
        }

        const auto content_top = 4;
        const auto content_height = height - 8;
        mvwvline( w, point( left_width + 1, content_top ), LINE_XOXO, content_height );
        mvwvline( w, point( left_width + middle_width + 2, content_top ), LINE_XOXO, content_height );
        trim_and_print( w, point( 2, content_top - 1 ), left_width - 1,
                        slots_focused ? c_yellow : c_light_gray, _( "Slots" ) );
        trim_and_print( w, point( left_width + 3, content_top - 1 ), middle_width - 2,
                        slots_focused ? c_light_gray : c_yellow, _( "Candidates" ) );
        trim_and_print( w, point( left_width + middle_width + 4, content_top - 1 ), right_width - 1,
                        c_light_gray, _( "Preview" ) );

        auto slot_start = 0;
        calcStartPos( slot_start, slot_cursor, content_height, static_cast<int>( sch.slots.size() ) );
        const auto slot_end = std::min( static_cast<int>( sch.slots.size() ), slot_start + content_height );
        std::ranges::for_each( std::views::iota( slot_start, slot_end ), [&]( const int row ) {
            const auto &slot = sch.slots[static_cast<size_t>( row )];
            const auto color = row == slot_cursor ? c_yellow : proc::slot_complete( state, sch, slot.id ) ?
                               c_light_green : c_white;
            trim_and_print( w, point( 2, content_top + row - slot_start ), left_width - 2, color,
                            slot_pick_text( state, slot, source_data.entries ) );
        } );

        const auto &slot = current_slot( sch, slot_cursor );
        const auto &candidates = current_candidates( state, slot.id );
        auto &cand_cursor = candidate_cursor[slot.id];
        if( !candidates.empty() ) {
            cand_cursor = std::clamp( cand_cursor, 0, static_cast<int>( candidates.size() ) - 1 );
        } else {
            cand_cursor = 0;
        }
        auto cand_start = 0;
        calcStartPos( cand_start, cand_cursor, content_height, static_cast<int>( candidates.size() ) );
        const auto cand_end = std::min( static_cast<int>( candidates.size() ),
                                        cand_start + content_height );
        std::ranges::for_each( std::views::iota( cand_start, cand_end ), [&]( const int row ) {
            const auto *source = source_for_ix( source_data.entries, candidates[static_cast<size_t>( row )] );
            if( source == nullptr ) {
                return;
            }
            const auto color = row == cand_cursor && !slots_focused ? c_yellow : c_white;
            trim_and_print( w, point( left_width + 3, content_top + row - cand_start ), middle_width - 2,
                            color, candidate_text( *source, state ) );
        } );

        const auto preview = preview_lines( sch, state, source_data.entries );
        const auto preview_end = std::min( static_cast<int>( preview.size() ), content_height );
        std::ranges::for_each( std::views::iota( 0, preview_end ), [&]( const int row ) {
            trim_and_print( w, point( left_width + middle_width + 4, content_top + row ), right_width - 1,
                            c_white, preview[static_cast<size_t>( row )] );
        } );

        const auto ready_color = proc::complete( state, sch ) ? c_light_green : c_light_red;
        trim_and_print( w, point( 2, height - 3 ), width - 4, ready_color,
                        proc::complete( state, sch ) ? _( "Ready to craft" ) : _( "Missing required slots" ) );
        trim_and_print( w, point( 2, height - 2 ), width - 4, c_light_gray,
                        status.empty() ? _( "Arrows move, Enter adds, r removes, c clears, f crafts, Esc cancels" ) :
                        status );
        wnoutrefresh( w );
    } );
    ui.mark_resize();
    g->invalidate_main_ui_adaptor();

    auto ctxt = input_context( "PROC_BUILDER" );
    ctxt.register_action( "UP", to_translation( "Previous entry" ) );
    ctxt.register_action( "DOWN", to_translation( "Next entry" ) );
    ctxt.register_action( "LEFT", to_translation( "Switch to slots" ) );
    ctxt.register_action( "RIGHT", to_translation( "Switch to candidates" ) );
    ctxt.register_action( "PAGE_UP", to_translation( "Fast scroll up" ) );
    ctxt.register_action( "PAGE_DOWN", to_translation( "Fast scroll down" ) );
    ctxt.register_action( "HOME", to_translation( "Go to first entry" ) );
    ctxt.register_action( "END", to_translation( "Go to last entry" ) );
    ctxt.register_action( "CONFIRM", to_translation( "Add selected candidate" ) );
    ctxt.register_action( "QUIT", to_translation( "Cancel" ) );
    ctxt.register_action( "HELP_KEYBINDINGS" );

    while( true ) {
        ui_manager::redraw();
        const auto action = ctxt.handle_input();
        const auto evt = ctxt.get_raw_input();
        const auto ch = evt.get_first_input();
        const auto &slot = current_slot( sch, slot_cursor );
        const auto &candidates = current_candidates( state, slot.id );
        auto &cand_cursor = candidate_cursor[slot.id];
        status.clear();

        if( action == "QUIT" ) {
            return std::nullopt;
        }
        if( action == "LEFT" ) {
            slots_focused = true;
            continue;
        }
        if( action == "RIGHT" ) {
            slots_focused = false;
            continue;
        }
        if( action == "UP" ) {
            if( slots_focused ) {
                slot_cursor = std::max( slot_cursor - 1, 0 );
            } else if( !candidates.empty() ) {
                cand_cursor = std::max( cand_cursor - 1, 0 );
            }
            continue;
        }
        if( action == "DOWN" ) {
            if( slots_focused ) {
                slot_cursor = std::min( slot_cursor + 1, static_cast<int>( sch.slots.size() ) - 1 );
            } else if( !candidates.empty() ) {
                cand_cursor = std::min( cand_cursor + 1, static_cast<int>( candidates.size() ) - 1 );
            }
            continue;
        }
        if( action == "PAGE_UP" && !slots_focused && !candidates.empty() ) {
            cand_cursor = std::max( cand_cursor - 8, 0 );
            continue;
        }
        if( action == "PAGE_DOWN" && !slots_focused && !candidates.empty() ) {
            cand_cursor = std::min( cand_cursor + 8, static_cast<int>( candidates.size() ) - 1 );
            continue;
        }
        if( action == "HOME" ) {
            if( slots_focused ) {
                slot_cursor = 0;
            } else {
                cand_cursor = 0;
            }
            continue;
        }
        if( action == "END" ) {
            if( slots_focused ) {
                slot_cursor = static_cast<int>( sch.slots.size() ) - 1;
            } else if( !candidates.empty() ) {
                cand_cursor = static_cast<int>( candidates.size() ) - 1;
            }
            continue;
        }
        if( ch == 'r' ) {
            status = proc::remove_last_pick( state, slot.id ) ? _( "Removed last pick." ) :
                     _( "Slot is already empty." );
            continue;
        }
        if( ch == 'c' ) {
            clear_slot( state, slot.id );
            status = _( "Cleared slot." );
            continue;
        }
        if( ch == 'f' ) {
            if( !proc::complete( state, sch ) ) {
                status = _( "Fill all required slots before crafting." );
                continue;
            }
            auto result = ui_result{};
            result.preview = state.fast;
            const auto picks = proc::selected_picks( state, sch );
            std::ranges::for_each( picks, [&]( const proc::craft_pick & pick ) {
                if( const auto *source = source_for_ix( source_data.entries, pick.ix ) ) {
                    result.picks.push_back( ui_pick{ .slot = pick.slot, .src = source->src, .fact = source->fact } );
                }
            } );
            return result;
        }
        if( action == "CONFIRM" ) {
            slots_focused = false;
            if( candidates.empty() ) {
                status = _( "No candidates for this slot." );
                continue;
            }
            if( slot.max == 1 && !state.picks_for( slot.id ).empty() ) {
                clear_slot( state, slot.id );
            }
            const auto picked_ix = candidates[static_cast<size_t>( cand_cursor )];
            status = proc::add_pick( state, sch, slot.id, picked_ix ) ? _( "Added pick." ) :
                     _( "That candidate can not be used again." );
            continue;
        }
    }
}
