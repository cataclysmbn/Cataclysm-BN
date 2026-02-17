#include "lua_tiles.h"

#include <algorithm>

#include "calendar.h"
#include "map.h"
#include "game.h"

lua_tile_manager &lua_tile_manager::get()
{
    static lua_tile_manager instance;
    return instance;
}

int lua_tile_manager::add_tile( const std::string &tile_id, const color_tint_pair &tint,
                                const tripoint &abs_pos, int priority,
                                int duration_turns, bool cleanup_outside_bubble )
{
    int h = next_handle_++;
    lua_tile_entry entry;
    entry.tile_id = tile_id;
    entry.tint = tint;
    entry.pos = tripoint_abs_ms( abs_pos );
    entry.priority = priority;
    entry.duration_turns = duration_turns;
    entry.cleanup_outside_bubble = cleanup_outside_bubble;
    entry.created_at = calendar::turn;
    entry.handle = h;
    entries_.push_back( std::move( entry ) );
    return h;
}

bool lua_tile_manager::remove_tile( int handle )
{
    auto it = std::find_if( entries_.begin(), entries_.end(),
    [handle]( const lua_tile_entry & e ) {
        return e.handle == handle;
    } );
    if( it != entries_.end() ) {
        entries_.erase( it );
        return true;
    }
    return false;
}

int lua_tile_manager::remove_tiles_at( const tripoint &abs_pos )
{
    tripoint_abs_ms target( abs_pos );
    int removed = 0;
    entries_.erase(
        std::remove_if( entries_.begin(), entries_.end(),
    [&target, &removed]( const lua_tile_entry & e ) {
        if( e.pos == target ) {
            ++removed;
            return true;
        }
        return false;
    } ),
    entries_.end() );
    return removed;
}

void lua_tile_manager::clear_all()
{
    entries_.clear();
}

void lua_tile_manager::get_tiles_at_local( const tripoint &local_pos,
        std::vector<const lua_tile_entry *> &out ) const
{
    out.clear();
    map &here = get_map();
    tripoint abs = here.getabs( local_pos );
    tripoint_abs_ms abs_ms( abs );

    for( const lua_tile_entry &e : entries_ ) {
        if( e.pos == abs_ms ) {
            out.push_back( &e );
        }
    }
    // Sort by priority ascending so highest priority draws last (on top)
    std::stable_sort( out.begin(), out.end(),
    []( const lua_tile_entry * a, const lua_tile_entry * b ) {
        return a->priority < b->priority;
    } );
}

void lua_tile_manager::cleanup()
{
    map &here = get_map();
    time_point now = calendar::turn;

    entries_.erase(
        std::remove_if( entries_.begin(), entries_.end(),
    [&here, &now]( const lua_tile_entry & e ) {
        // Check duration expiry
        if( e.duration_turns >= 0 ) {
            time_point expiry = e.created_at + time_duration::from_turns( e.duration_turns );
            if( now >= expiry ) {
                return true;
            }
        }
        // Check reality bubble cleanup
        if( e.cleanup_outside_bubble ) {
            tripoint local = here.getlocal( e.pos );
            if( !here.inbounds( local ) ) {
                return true;
            }
        }
        return false;
    } ),
    entries_.end() );
}
