#include "procgen/proc_ui.h"

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
#include "procgen/proc_builder.h"
#include "procgen/proc_fact.h"
#include "procgen/proc_item.h"
#include "procgen/proc_recipe.h"
#include "recipe.h"
#include "procgen/proc_ui_candidates.h"
#include "procgen/proc_ui_input.h"
#include "procgen/proc_ui_slot_indicator.h"
#include "procgen/proc_ui_text.h"
#include "string_input_popup.h"
#include "string_formatter.h"
#include "string_utils.h"
#include "translations.h"
#include "ui.h"
#include "procgen/proc_ui_navigation.h"
#include "ui_manager.h"

namespace
{

static const trait_id trait_DEBUG_HS( "DEBUG_HS" );

using panel_focus = proc::builder_focus;

struct source_entry {
    item *src = nullptr;
    std::string prefix;
    std::string where;
    proc::part_fact fact;
};

struct source_pool {
    std::vector<detached_ptr<item>> owned;
    std::vector<source_entry> entries;
};

struct source_options {
    item *src = nullptr;
    std::string prefix;
    std::string where;
    proc::part_ix ix = proc::invalid_part_ix;
    int charges = 0;
    int uses = 1;
};

struct recipe_requirement_status {
    bool ready = true;
    std::string missing;
};

auto make_source_entry( const source_options &opts ) -> source_entry
{
    return source_entry{
        .src = opts.src,
        .prefix = opts.prefix,
        .where = opts.where,
        .fact = proc::normalize_part_fact( *opts.src, {
            .ix = opts.ix,
            .charges = opts.charges,
            .uses = opts.uses
        } )
    };
}

auto current_recipe_requirement_status( Character &who, const recipe &rec,
                                        const std::vector<proc::part_fact> &facts ) -> recipe_requirement_status
{
    const auto reqs = proc::recipe_requirements( rec, facts );
    const auto ready = reqs.can_make_with_inventory( who.crafting_inventory(),
                       rec.get_component_filter(), 1, cost_adjustment::start_only );
    const auto missing = ready ? std::string {} :
                         reqs.list_missing();
    return recipe_requirement_status{
        .ready = ready,
        .missing = missing,
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

auto candidate_meta( const source_entry &source, const proc::schema &sch ) -> std::string;

auto candidate_line( const source_entry &source, const proc::schema &sch ) -> std::string
{
    return string_format( "%s %s  [%s]", source.prefix, source.src->tname(),
                          candidate_meta( source, sch ) );
}

auto filtered_candidates( const proc::builder_state &state, const proc::slot_id &slot,
                          const proc::schema &sch,
                          const std::vector<source_entry> &sources,
                          const std::string &query ) -> std::vector<proc::grouped_candidate_entry>
{
    auto candidate_sources = std::vector<proc::candidate_source_entry> {};
    candidate_sources.reserve( sources.size() );
    std::ranges::transform( sources,
    std::back_inserter( candidate_sources ), [&]( const source_entry & source ) {
        return proc::candidate_source_entry{
            .label = candidate_line( source, sch ),
            .name = source.src->tname(),
            .where = source.where,
            .fact = source.fact,
        };
    } );
    return proc::filter_grouped_candidates( state, slot, candidate_sources, query );
}

auto slot_summary( const proc::builder_state &state, const proc::slot_data &slot,
                   const std::vector<source_entry> &sources ) -> std::string
{
    auto picks = std::vector<std::string> {};
    std::ranges::for_each( state.picks_for( slot.id ), [&]( const proc::part_ix ix ) {
        if( const auto *source = source_for_ix( sources, ix ) ) {
            picks.push_back( source->src->tname() );
        }
    } );
    return proc::grouped_label_summary( picks, _( "empty" ) );
}

auto candidate_meta( const source_entry &source, const proc::schema &sch ) -> std::string
{
    if( sch.cat == "food" ) {
        return string_format( "+%d kcal", source.fact.kcal );
    }
    if( sch.cat == "weapon" ) {
        return string_format( "%d g", source.fact.mass_g );
    }
    return string_format( "%d g", source.fact.mass_g );
}

auto result_state_label( const proc::schema &sch ) -> std::string
{
    return sch.cat == "food" ? _( "Fresh" ) : _( "New" );
}

auto clear_slot( proc::builder_state &state, const proc::slot_id &slot ) -> void;

auto highlighted_preview( proc::builder_state state, const proc::schema &sch,
                          const proc::slot_id &slot,
                          const std::vector<proc::grouped_candidate_entry> &candidates,
                          const int cursor ) -> proc::fast_blob
{
    if( candidates.empty() || cursor < 0 || static_cast<size_t>( cursor ) >= candidates.size() ) {
        return state.fast;
    }
    const auto chosen = proc::first_grouped_candidate_ix( candidates[static_cast<size_t>( cursor )] );
    if( chosen == proc::invalid_part_ix ) {
        return state.fast;
    }
    if( const auto slot_data = std::ranges::find_if( sch.slots, [&]( const proc::slot_data & entry ) {
    return entry.id == slot;
} ); slot_data != sch.slots.end() && slot_data->max == 1 && !state.picks_for( slot ).empty() ) {
        clear_slot( state, slot );
    }
    if( !proc::add_pick( state, sch, slot, chosen ) ) {
        return state.fast;
    }
    return state.fast;
}

auto diff_line( const std::string &label, const int current, const int preview,
                const std::string &suffix ) -> std::string
{
    if( current == preview ) {
        return string_format( "%-10s %d%s", label, current, suffix );
    }
    if( current == 0 ) {
        return string_format( "%-10s %d%s", label, preview, suffix );
    }
    return string_format( "%-10s %d%s -> [ %d%s ]", label, current, suffix, preview, suffix );
}

auto selected_ingredient_lines( const proc::builder_state &state, const proc::schema &sch,
                                const std::vector<source_entry> &sources ) -> std::vector<std::string>
{
    auto picks = std::vector<std::string> {};
    std::ranges::for_each( proc::selected_picks( state, sch ), [&]( const proc::craft_pick & pick ) {
        if( const auto *source = source_for_ix( sources, pick.ix ) ) {
            picks.push_back( source->src->tname() );
        }
    } );
    return proc::grouped_label_lines( picks );
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
                .prefix = "(I)",
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
        const auto fact = proc::debug_part_fact( sch, *temp, ix );
        if( !fact.has_value() ) {
            ret.owned.pop_back();
            return;
        }
        ret.entries.push_back( source_entry{
            .src = temp,
            .prefix = "(D)",
            .where = _( "debug hammerspace" ),
            .fact = *fact,
        } );
        ix++;
    } );
    return ret;
}

auto missing_required_slots( const proc::schema &sch,
                             const proc::builder_state &state ) -> std::vector<std::string>
{
    auto ret = std::vector<std::string> {};
    std::ranges::for_each( sch.slots, [&]( const proc::slot_data & slot ) {
        if( !proc::slot_can_meet_minimum( state, sch, slot.id ) ) {
            ret.push_back( slot.role );
        }
    } );
    return ret;
}

auto current_slot( const proc::schema &sch, const int slot_cursor ) -> const proc::slot_data &
{
    return sch.slots[static_cast<size_t>( slot_cursor )];
}

auto preview_lines( const proc::schema &sch, const proc::builder_state &state,
                    const proc::fast_blob &preview_blob,
                    const std::vector<source_entry> &sources ) -> std::vector<std::string>
{
    auto lines = std::vector<std::string> {};
    lines.push_back( string_format( _( "Result: %s" ), preview_blob.name.empty() ? sch.res.str() :
                                    preview_blob.name ) );
    lines.push_back( string_format( _( "State:  %s" ), result_state_label( sch ) ) );
    lines.push_back( "----------------------" );
    lines.push_back( diff_line( _( "Mass:" ), state.fast.mass_g, preview_blob.mass_g, " g" ) );
    lines.push_back( diff_line( _( "Vol:" ), state.fast.volume_ml, preview_blob.volume_ml, " ml" ) );
    if( sch.cat == "food" ) {
        lines.push_back( diff_line( _( "Cal:" ), state.fast.kcal, preview_blob.kcal, "" ) );
    }
    if( !preview_blob.vit.empty() ) {
        auto vitamins = std::vector<std::string> {};
        std::ranges::for_each( preview_blob.vit, [&]( const std::pair<const vitamin_id, int> &entry ) {
            vitamins.push_back( string_format( "%s:%d", entry.first.str(), entry.second ) );
        } );
        lines.push_back( string_format( _( "Vit: %s" ), enumerate_as_string( vitamins,
                                        enumeration_conjunction::none ) ) );
    }
    if( !preview_blob.melee.empty() ) {
        lines.push_back( diff_line( _( "Bash:" ), state.fast.melee.bash, preview_blob.melee.bash, "" ) );
        lines.push_back( diff_line( _( "Cut:" ), state.fast.melee.cut, preview_blob.melee.cut, "" ) );
        lines.push_back( diff_line( _( "Stab:" ), state.fast.melee.stab, preview_blob.melee.stab, "" ) );
        lines.push_back( diff_line( _( "To-hit:" ), state.fast.melee.to_hit, preview_blob.melee.to_hit,
                                    "" ) );
        lines.push_back( diff_line( _( "Dur:" ), state.fast.melee.dur, preview_blob.melee.dur, "" ) );
    }
    lines.push_back( "----------------------" );
    lines.push_back( _( "[ Ingredients Added ]" ) );
    const auto ingredients = selected_ingredient_lines( state, sch, sources );
    if( ingredients.empty() ) {
        lines.push_back( string_format( "- %s", _( "None" ) ) );
    } else {
        std::ranges::copy( ingredients, std::back_inserter( lines ) );
    }
    return lines;
}

auto preview_slots( const proc::builder_state &state,
                    const proc::schema &sch ) -> std::vector<proc::slot_id>
{
    auto slots = std::vector<proc::slot_id> {};
    const auto picks = proc::selected_picks( state, sch );
    slots.reserve( picks.size() );
    std::ranges::for_each( picks, [&]( const proc::craft_pick & pick ) {
        slots.push_back( pick.slot );
    } );
    return slots;
}

auto preview_item_from_state( const proc::builder_state &state, const proc::schema &sch,
                              const recipe &rec ) -> detached_ptr<item>
{
    auto facts = proc::selected_facts( state );
    if( facts.empty() ) {
        auto fallback = item::spawn( sch.res, calendar::turn );
        fallback->set_var( "name", rec.builder_name().translated().empty() ? rec.result_name() :
                           rec.builder_name().translated() );
        fallback->set_var( "description", rec.builder_desc().translated().empty() ?
                           _( "Select parts to preview the crafted item." ) :
                           rec.builder_desc().translated() );
        return fallback;
    }
    return proc::make_item( sch, facts, {
        .mode = proc::hist::none,
        .rec = &rec,
        .used = {},
        .slots = preview_slots( state, sch )
    } );
}

auto preview_item_info( const proc::schema &sch, const proc::builder_state &state,
                        const proc::fast_blob &preview_blob,
                        const std::vector<source_entry> &,
                        const recipe &rec, int &preview_scroll ) -> item_info_data
{
    auto preview_state = state;
    preview_state.fast = preview_blob;
    auto display_item = preview_item_from_state( preview_state, sch, rec );
    auto compare_info = std::vector<iteminfo> {};
    if( !proc::selected_facts( state ).empty() ) {
        const auto compare_item = preview_item_from_state( state, sch, rec );
        compare_info = compare_item->info();
    }

    auto data = item_info_data( display_item->tname(), std::string {}, display_item->info(),
                                compare_info,
                                preview_scroll );
    data.without_getch = true;
    data.without_border = true;
    data.use_full_win = true;
    data.scrollbar_left = false;
    data.any_input = false;
    data.padding = 0;
    return data;
}

auto clear_slot( proc::builder_state &state, const proc::slot_id &slot ) -> void
{
    while( proc::remove_last_pick( state, slot ) ) {
    }
}

auto draw_slot_indicator( const catacurses::window &w, const point &pos,
                          const proc::slot_data &slot, const int picked,
                          const bool selected, const nc_color &frame_color ) -> int
{
    const auto cells = proc::slot_indicator_cells( slot, picked, selected );
    auto x = pos.x;
    mvwputch( w, point( x, pos.y ), frame_color, '[' );
    x++;
    std::ranges::for_each( std::views::iota( size_t{ 0 }, cells.size() ), [&]( const size_t idx ) {
        const auto &cell = cells[idx];
        mvwputch( w, point( x, pos.y ), cell.color, cell.glyph );
        x++;
        if( idx + 1 < cells.size() ) {
            mvwputch( w, point( x, pos.y ), cell.color, ' ' );
            x++;
        }
    } );
    mvwputch( w, point( x, pos.y ), frame_color, ']' );
    return x - pos.x + 1;
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
    auto focus = panel_focus::slots;
    auto search_query = std::string {};
    auto status = std::string {};
    auto w = catacurses::window {};
    auto ui = ui_adaptor( ui_adaptor::disable_uis_below {} );
    auto width = TERMX;
    auto height = TERMY;
    auto window_origin = point_zero;
    auto preview_scroll = 0;

    const auto resize_cb = [&]( ui_adaptor & adaptor ) {
        width = std::clamp( TERMX - 2, 80, TERMX );
        height = std::clamp( TERMY - 2, 24, TERMY );
        const auto start = point( ( TERMX - width ) / 2, ( TERMY - height ) / 2 );
        window_origin = start;
        w = catacurses::newwin( height, width, start );
        adaptor.position_from_window( w );
    };
    resize_cb( ui );
    ui.on_screen_resize( resize_cb );

    ui.on_redraw( [&]( ui_adaptor & ) {
        werase( w );
        draw_border( w );

        auto left_width = std::max( 28, width * 3 / 10 );
        auto middle_width = std::max( 34, width * 33 / 100 );
        if( left_width + middle_width + 32 > width - 6 ) {
            middle_width = std::max( 28, width - left_width - 32 - 6 );
        }
        if( left_width + middle_width + 32 > width - 6 ) {
            left_width = std::max( 24, width - middle_width - 32 - 6 );
        }
        const auto right_width = width - left_width - middle_width - 6;
        const auto header = rec.builder_name().translated().empty() ? rec.result_name() :
                            rec.builder_name().translated();
        trim_and_print( w, point( 2, 1 ), width - 4, c_white, header );
        if( !rec.builder_desc().translated().empty() ) {
            trim_and_print( w, point( 2, 2 ), width - 4, c_light_gray, rec.builder_desc().translated() );
        }

        const auto &slot = current_slot( sch, slot_cursor );
        auto candidates = filtered_candidates( state, slot.id, sch, source_data.entries, search_query );
        auto &cand_cursor = candidate_cursor[slot.id];
        if( !candidates.empty() ) {
            cand_cursor = std::clamp( cand_cursor, 0, static_cast<int>( candidates.size() ) - 1 );
        } else {
            cand_cursor = 0;
        }
        const auto preview_blob = highlighted_preview( state, sch, slot.id, candidates, cand_cursor );

        const auto content_top = 5;
        const auto search_row = content_top;
        const auto list_top = content_top + 2;
        const auto content_height = std::max( height - list_top - 4, 1 );
        mvwvline( w, point( left_width + 1, content_top ), LINE_XOXO, content_height );
        mvwvline( w, point( left_width + middle_width + 3, content_top ), LINE_XOXO, content_height );
        trim_and_print( w, point( 2, content_top - 1 ), left_width - 1,
                        focus == panel_focus::slots ? c_yellow : c_light_gray, _( "[1. Slots]" ) );
        trim_and_print( w, point( left_width + 3, content_top - 1 ), middle_width - 2,
                        focus != panel_focus::slots ? c_yellow : c_light_gray,
                        string_format( _( "[2. Candidates: %s ]" ), search_query.empty() ? slot.role : search_query ) );
        trim_and_print( w, point( left_width + middle_width + 5, content_top - 1 ), right_width - 1,
                        c_light_gray, _( "[3. Preview ]" ) );
        trim_and_print( w, point( left_width + 3, search_row ), middle_width - 2,
                        c_light_gray, string_format( _( "Search: /%s" ), search_query ) );

        auto slot_start = 0;
        calcStartPos( slot_start, slot_cursor, content_height, static_cast<int>( sch.slots.size() ) );
        const auto slot_end = std::min( static_cast<int>( sch.slots.size() ), slot_start + content_height );
        std::ranges::for_each( std::views::iota( slot_start, slot_end ), [&]( const int row ) {
            const auto &slot_entry = sch.slots[static_cast<size_t>( row )];
            const auto picked = static_cast<int>( state.picks_for( slot_entry.id ).size() );
            const auto selected = row == slot_cursor;
            const auto color = selected ? c_yellow : proc::slot_complete( state, sch,
                               slot_entry.id ) ?
                               c_light_green : c_white;
            const auto y = list_top + row - slot_start;
            const auto prefix = string_format( "%s ", selected ? ">" : " " );
            trim_and_print( w, point( 2, y ), left_width - 2, color, prefix );
            const auto indicator_width = draw_slot_indicator( w, point( 2 + utf8_width( prefix ), y ),
                                         slot_entry, picked, selected, color );
            trim_and_print( w, point( 2 + utf8_width( prefix ) + indicator_width, y ),
                            left_width - 2 - utf8_width( prefix ) - indicator_width, color,
                            string_format( " %s %s", slot_entry.role,
                                           slot_summary( state, slot_entry, source_data.entries ) ) );
        } );

        auto cand_start = 0;
        calcStartPos( cand_start, cand_cursor, content_height, static_cast<int>( candidates.size() ) );
        const auto cand_end = std::min( static_cast<int>( candidates.size() ),
                                        cand_start + content_height );
        std::ranges::for_each( std::views::iota( cand_start, cand_end ), [&]( const int row ) {
            const auto color = row == cand_cursor && focus != panel_focus::slots ? c_yellow : c_white;
            trim_and_print( w, point( left_width + 3, list_top + row - cand_start ), middle_width - 2,
                            color, proc::grouped_candidate_label( candidates[static_cast<size_t>( row )] ) );
        } );

        const auto recipe_requirements = current_recipe_requirement_status( who, rec,
                                         proc::selected_facts( state ) );

        const auto readiness = !proc::complete( state,
                                                sch ) ? proc::builder_readiness::missing_required_slots :
                               recipe_requirements.ready ? proc::builder_readiness::ready_to_craft :
                               proc::builder_readiness::missing_recipe_requirements;
        const auto ready_color = readiness == proc::builder_readiness::ready_to_craft ? c_light_green :
                                 c_light_red;
        trim_and_print( w, point( 2, height - 3 ), width - 4, ready_color,
                        proc::builder_readiness_label( readiness ) );
        const auto idle_status = readiness == proc::builder_readiness::missing_recipe_requirements ?
                                 proc::compact_requirement_text( recipe_requirements.missing ) :
                                 _( "[Arrows] Navigate  [/] Search  [Enter] Add  [r] Remove  [c] Clear  [f] Craft  [Esc] Cancel" );
        trim_and_print( w, point( 2, height - 2 ), width - 4, c_light_gray,
                        status.empty() ? idle_status :
                        status );
        wnoutrefresh( w );

        auto preview_data = preview_item_info( sch, state, preview_blob, source_data.entries, rec,
                                               preview_scroll );
        draw_item_info( window_origin.x + left_width + middle_width + 4, right_width + 1,
                        window_origin.y + list_top, content_height, preview_data );
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
    ctxt.register_action( "ANY_INPUT" );

    while( true ) {
        ui_manager::redraw();
        const auto action = ctxt.handle_input();
        const auto evt = ctxt.get_raw_input();
        const auto ch = evt.get_first_input();
        const auto &slot = current_slot( sch, slot_cursor );
        auto candidates = filtered_candidates( state, slot.id, sch, source_data.entries, search_query );
        auto &cand_cursor = candidate_cursor[slot.id];
        if( !candidates.empty() ) {
            cand_cursor = std::clamp( cand_cursor, 0, static_cast<int>( candidates.size() ) - 1 );
        } else {
            cand_cursor = 0;
        }
        status.clear();

        if( action == "QUIT" ) {
            return std::nullopt;
        }
        if( action == "ANY_INPUT" && ch == '/' ) {
            search_query = string_input_popup()
                           .title( _( "Search term:" ) )
                           .description( _( "Filter candidates by item text or tag atoms like tag:fish." ) )
                           .identifier( "proc_builder" )
                           .text( search_query )
                           .query_string();
            focus = panel_focus::candidates;
            continue;
        }
        if( action == "LEFT" ) {
            focus = panel_focus::slots;
            continue;
        }
        if( action == "RIGHT" ) {
            focus = panel_focus::candidates;
            continue;
        }
        if( action == "UP" ) {
            if( focus == panel_focus::slots ) {
                const auto navigation = proc::handle_builder_slot_navigation( {
                    .focus = focus,
                    .action = action,
                    .slot_cursor = slot_cursor,
                    .slot_count = static_cast<int>( sch.slots.size() ),
                    .search_query = search_query,
                } );
                slot_cursor = navigation.slot_cursor;
                search_query = navigation.search_query;
            } else if( !candidates.empty() ) {
                cand_cursor = proc::wrap_cursor( cand_cursor, -1, static_cast<int>( candidates.size() ) );
            }
            continue;
        }
        if( action == "DOWN" ) {
            if( focus == panel_focus::slots ) {
                const auto navigation = proc::handle_builder_slot_navigation( {
                    .focus = focus,
                    .action = action,
                    .slot_cursor = slot_cursor,
                    .slot_count = static_cast<int>( sch.slots.size() ),
                    .search_query = search_query,
                } );
                slot_cursor = navigation.slot_cursor;
                search_query = navigation.search_query;
            } else if( !candidates.empty() ) {
                cand_cursor = proc::wrap_cursor( cand_cursor, 1, static_cast<int>( candidates.size() ) );
            }
            continue;
        }
        if( action == "PAGE_UP" && focus != panel_focus::slots && !candidates.empty() ) {
            cand_cursor = std::max( cand_cursor - 8, 0 );
            continue;
        }
        if( action == "PAGE_DOWN" && focus != panel_focus::slots && !candidates.empty() ) {
            cand_cursor = std::min( cand_cursor + 8, static_cast<int>( candidates.size() ) - 1 );
            continue;
        }
        if( action == "HOME" ) {
            if( focus == panel_focus::slots ) {
                const auto navigation = proc::handle_builder_slot_navigation( {
                    .focus = focus,
                    .action = action,
                    .slot_cursor = slot_cursor,
                    .slot_count = static_cast<int>( sch.slots.size() ),
                    .search_query = search_query,
                } );
                slot_cursor = navigation.slot_cursor;
                search_query = navigation.search_query;
            } else {
                cand_cursor = 0;
            }
            continue;
        }
        if( action == "END" ) {
            if( focus == panel_focus::slots ) {
                const auto navigation = proc::handle_builder_slot_navigation( {
                    .focus = focus,
                    .action = action,
                    .slot_cursor = slot_cursor,
                    .slot_count = static_cast<int>( sch.slots.size() ),
                    .search_query = search_query,
                } );
                slot_cursor = navigation.slot_cursor;
                search_query = navigation.search_query;
            } else if( !candidates.empty() ) {
                cand_cursor = static_cast<int>( candidates.size() ) - 1;
            }
            continue;
        }
        if( ch == 'r' || ch == KEY_BACKSPACE || ch == KEY_DC ) {
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
            const auto recipe_requirements = current_recipe_requirement_status( who, rec,
                                             proc::selected_facts( state ) );
            if( !proc::complete( state, sch ) ) {
                status = _( "Fill all required slots before crafting." );
                continue;
            }
            if( !recipe_requirements.ready ) {
                status = proc::compact_requirement_text( recipe_requirements.missing );
                if( status.empty() ) {
                    status = _( "Missing required tools or qualities for this recipe." );
                }
                continue;
            }
            if( const auto valid = proc::validate_selection( sch, proc::selected_facts( state ),
                                   state.fast ); !valid ) {
                status = valid.error();
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
            if( focus == panel_focus::slots ) {
                focus = panel_focus::candidates;
                continue;
            }
            if( candidates.empty() ) {
                status = _( "No candidates for this slot." );
                continue;
            }
            const auto picked_ix = proc::first_grouped_candidate_ix( candidates[static_cast<size_t>
                                   ( cand_cursor )] );
            if( picked_ix == proc::invalid_part_ix ) {
                status = _( "No candidates for this slot." );
                continue;
            }
            if( slot.max == 1 && !state.picks_for( slot.id ).empty() &&
                std::ranges::find( state.picks_for( slot.id ), picked_ix ) == state.picks_for( slot.id ).end() ) {
                clear_slot( state, slot.id );
            }
            if( proc::add_pick( state, sch, slot.id, picked_ix ) ) {
                status = _( "Added selection." );
                continue;
            }
            status = proc::remove_pick( state, slot.id, picked_ix ) ? _( "Removed selection." ) :
                     _( "That candidate can not be used here." );
            continue;
        }
    }
}
