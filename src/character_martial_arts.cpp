#include "character_martial_arts.h"

#include "action.h"
#include "avatar.h"
#include "character.h"
#include "character_display.h"
#include "color.h"
#include "enums.h"
#include "json.h"
#include "martialarts.h"
#include "messages.h"
#include "output.h"
#include "string_id.h"
#include "translations.h"


static const bionic_id bio_cqb( "bio_cqb" );
static const matype_id style_kicks( "style_kicks" );
static const matype_id style_none( "style_none" );

// ids of martial art styles that are available with the bio_cqb bionic.
// TODO: this should be unhardcoded probably
static const std::vector<matype_id> bio_cqb_styles{ {
        matype_id{ "style_aikido" },
        matype_id{ "style_biojutsu" },
        matype_id{ "style_boxing" },
        matype_id{ "style_capoeira" },
        matype_id{ "style_crane" },
        matype_id{ "style_dragon" },
        matype_id{ "style_judo" },
        matype_id{ "style_karate" },
        matype_id{ "style_krav_maga" },
        matype_id{ "style_leopard" },
        matype_id{ "style_muay_thai" },
        matype_id{ "style_ninjutsu" },
        matype_id{ "style_pankration" },
        matype_id{ "style_snake" },
        matype_id{ "style_taekwondo" },
        matype_id{ "style_tai_chi" },
        matype_id{ "style_tiger" },
        matype_id{ "style_wingchun" },
        matype_id{ "style_zui_quan" }
    }};

character_martial_arts::character_martial_arts()
{

    keep_hands_free = false;

    style_selected = style_none;

    ma_styles = { {
            style_none, style_kicks
        }
    };
}

bool character_martial_arts::selected_allow_melee() const
{
    return style_selected->allow_melee;
}

bool character_martial_arts::selected_strictly_melee() const
{
    return style_selected->strictly_melee;
}

std::set<trait_id> character_martial_arts::selected_mutations() const
{
    return style_selected->mutation;
}

bool character_martial_arts::selected_has_weapon( const itype_id &weap ) const
{
    return style_selected->has_weapon( weap );
}

bool character_martial_arts::selected_force_unarmed() const
{
    return style_selected->force_unarmed;
}

/** Creates the UI and handles player input for picking martial arts styles */
bool character_martial_arts::pick_style( const avatar &you )  // Style selection menu
{
    enum style_selection {
        KEEP_HANDS_FREE = 0,
        STYLE_OFFSET
    };

    // If there are style already, cursor starts there
    // if no selected styles, cursor starts from no-style

    // Any other keys quit the menu
    const std::vector<matype_id> &selectable_styles = you.has_active_bionic(
                bio_cqb ) ? bio_cqb_styles :
            ma_styles;

    input_context ctxt( "MELEE_STYLE_PICKER" );
    ctxt.register_action( "SHOW_DESCRIPTION" );

    uilist kmenu;
    kmenu.text = string_format( _( "Select a style.\n"
                                   "\n"
                                   "STR: <color_white>%d</color>, DEX: <color_white>%d</color>, "
                                   "PER: <color_white>%d</color>, INT: <color_white>%d</color>\n"
                                   "Base empty-handed damage: %3d\n"
                                   "Effective dodge rating: %4.1f\n"
                                   "Press [<color_yellow>%s</color>] for more info.\n" ),
                                you.get_str(), you.get_dex(), you.get_per(), you.get_int(),
                                character_display::display_empty_handed_base_damage( you ), you.get_dodge(),
                                ctxt.get_desc( "SHOW_DESCRIPTION" ) );
    ma_style_callback callback( static_cast<size_t>( STYLE_OFFSET ), selectable_styles );
    kmenu.callback = &callback;
    kmenu.input_category = "MELEE_STYLE_PICKER";
    kmenu.additional_actions.emplace_back( "SHOW_DESCRIPTION", translation() );
    kmenu.desc_enabled = true;
    kmenu.addentry_desc( KEEP_HANDS_FREE, true, 'h',
                         keep_hands_free ? _( "Keep hands free (on)" ) : _( "Keep hands free (off)" ),
                         _( "When this is enabled, player won't wield things unless explicitly told to." ) );

    kmenu.selected = STYLE_OFFSET;

    for( size_t i = 0; i < selectable_styles.size(); i++ ) {
        auto &style = selectable_styles[i].obj();
        //Check if this style is currently selected
        const bool selected = selectable_styles[i] == style_selected;
        std::string entry_text = style.name.translated();
        if( selected ) {
            kmenu.selected = i + STYLE_OFFSET;
            entry_text = colorize( entry_text, c_pink );
        }
        kmenu.addentry_desc( i + STYLE_OFFSET, true, -1, entry_text, style.description.translated() );
    }

    kmenu.query();
    int selection = kmenu.ret;

    if( selection >= STYLE_OFFSET ) {
        style_selected = selectable_styles[selection - STYLE_OFFSET];
        martialart_use_message( you );
    } else if( selection == KEEP_HANDS_FREE ) {
        keep_hands_free = !keep_hands_free;
    } else {
        return false;
    }

    return true;
}

bool character_martial_arts::knows_selected_style() const
{
    return has_martialart( style_selected );
}

bool character_martial_arts::selected_is_none() const
{
    return style_selected == style_none;
}

void character_martial_arts::learn_current_style_CQB( bool is_avatar )
{
    add_martialart( style_selected );
    if( is_avatar ) {
        add_msg( m_good, _( "You have learned %s from extensive practice with the CQB Bionic." ),
                 style_selected->name );
    }
}

void character_martial_arts::learn_style( const matype_id &mastyle, bool is_avatar )
{
    add_martialart( mastyle );

    if( is_avatar ) {
        add_msg( m_good, _( "You learn %s." ),
                 mastyle->name );
        add_msg( m_info, _( "%s to select martial arts style." ),
                 press_x( ACTION_PICK_STYLE ) );
    }
}

void character_martial_arts::set_style( const matype_id &mastyle, bool force )
{
    if( force || has_martialart( mastyle ) ) {
        style_selected = mastyle;
    }
}

void character_martial_arts::reset_style()
{
    style_selected = style_none;
}

void character_martial_arts::selected_style_check()
{
    // check if player knows current style naturally, otherwise drop them back to style_none
    if( style_selected != style_none && style_selected != style_kicks ) {
        bool has_style = false;
        for( const matype_id &elem : ma_styles ) {
            if( elem == style_selected ) {
                has_style = true;
            }
        }
        if( !has_style ) {
            reset_style();
        }
    }
}

std::string character_martial_arts::enumerate_known_styles( const itype_id &weap ) const
{
    return enumerate_as_string( ma_styles.begin(), ma_styles.end(),
    [weap]( const matype_id & mid ) {
        return mid->has_weapon( weap ) ? colorize( mid->name.translated(), c_cyan ) : std::string();
    } );
}

std::string character_martial_arts::selected_style_name( const Character &owner ) const
{
    if( style_selected->force_unarmed || style_selected->weapon_valid( owner.primary_weapon() ) ) {
        return style_selected->name.translated();
    } else if( owner.is_armed() ) {
        return _( "Normal" );
    } else {
        return _( "No Style" );
    }
}

std::vector<matype_id> character_martial_arts::get_unknown_styles( const character_martial_arts
        &from ) const
{
    std::vector<matype_id> ret;
    for( const matype_id &i : from.ma_styles ) {
        if( !has_martialart( i ) ) {
            ret.push_back( i );
        }
    }
    return ret;
}

std::vector<matype_id> character_martial_arts::get_known_styles() const
{
    return ma_styles;
}

void character_martial_arts::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "ma_styles", ma_styles );
    json.member( "keep_hands_free", keep_hands_free );
    json.member( "style_selected", style_selected );
    json.end_object();
}

void character_martial_arts::deserialize( JsonIn &jsin )
{
    JsonObject data = jsin.get_object();
    data.read( "ma_styles", ma_styles );
    data.read( "keep_hands_free", keep_hands_free );
    data.read( "style_selected", style_selected );
}
