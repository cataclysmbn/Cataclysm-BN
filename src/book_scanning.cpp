#include "book_scanning.h"

#include "action_time_scale.h"
#include "calendar.h"
#include "character.h"
#include "character_functions.h"
#include "digital_media.h"
#include "flag.h"
#include "item.h"
#include "item_contents.h"
#include "itype.h"
#include "player.h"
#include "player_activity.h"
#include "translations.h"

#include <algorithm>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>

namespace
{
auto validate_book_scan( const Character &who, const item &reader, const item &book ) ->
std::expected<void, std::string>
{
    namespace ranges = std::ranges;
    if( !who.has_item( reader ) || !digital_reader_ready( reader, who ) ) {
        return std::unexpected( _( "You need a powered reader to scan a book." ) );
    }
    if( !who.has_item( book ) || !book.is_book() || is_digital_file( book ) ) {
        return std::unexpected( _( "You need a physical book to scan." ) );
    }
    if( !character_funcs::can_see_fine_details( who ) ) {
        return std::unexpected( _( "You can't see well enough to scan a book." ) );
    }
    if( ranges::any_of( reader.contents.all_items_top(), [&book]( const auto * file ) {
    return is_digital_file( *file ) && file->typeId() == book.typeId();
    } ) ) {
        return std::unexpected( _( "This book is already installed on the reader." ) );
    }
    return {};
}

auto check_scan_targets( player_activity &act, Character &who ) -> bool
{
    if( act.targets.size() != 2 || !act.targets[0] || !act.targets[1] ) {
        who.add_msg_if_player( m_info, _( "You no longer have the book or reader.  You stop scanning." ) );
        act.set_to_null();
        return false;
    }
    if( const auto available = validate_book_scan( who, *act.targets[0], *act.targets[1] );
        !available ) {
        who.add_msg_if_player( m_info, "%s", available.error() );
        act.set_to_null();
        return false;
    }
    return true;
}
} // namespace

auto start_book_scan( Character &who, item &reader, item &book ) -> bool
{
    if( const auto available = validate_book_scan( who, reader, book ); !available ) {
        who.add_msg_if_player( m_info, "%s", available.error() );
        return false;
    }
    // Larger books take longer to photograph, regardless of their reading difficulty.
    const auto duration = std::max<int64_t>( 1,
                          units::to_milliliter( book.volume() ) / 50 ) * 1_minutes;
    auto activity = std::make_unique<player_activity>( activity_id( "ACT_SCAN_BOOK" ),
                    to_moves<int>( duration ) );
    activity->targets.emplace_back( reader );
    activity->targets.emplace_back( book );
    who.assign_activity( std::move( activity ) );
    return true;
}

auto activity_handlers::scan_book_do_turn( player_activity *act, player *who ) -> void
{
    if( !check_scan_targets( *act, *who ) ) {
        return;
    }
    if( action_time_scale::once_every_this_tick( 1_minutes ) ) {
        auto &reader = *act->targets[0];
        who->consume_charges( reader, reader.type->charges_to_use() );
    }
}

auto activity_handlers::scan_book_finish( player_activity *act, player *who ) -> void
{
    if( !check_scan_targets( *act, *who ) ) {
        return;
    }
    auto &reader = *act->targets[0];
    const auto &book = *act->targets[1];
    auto file = item::spawn( book.typeId(), calendar::turn );
    file->set_flag( flag_id( "DIGITAL" ) );
    file->set_flag( flag_IRREMOVABLE );
    file->set_flag( flag_NO_DROP );
    file->set_var( "DIGITAL_FILE_ID", "scanned_book:" + book.typeId().str() );
    reader.put_in( std::move( file ) );
    who->add_msg_if_player( m_good, _( "You save a digital copy of %s on your %s." ),
                            book.type_name(), reader.type_name() );
    act->set_to_null();
}
