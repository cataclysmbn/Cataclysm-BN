#include "string_id.h"

// These are centralized in this file because they must appear in one and only one compilation unit,
// and it's problematic to define them in the respective cpp files for each class.
// Very repetitious, so define them with a macro.
#define MAKE_CLASS_NULL_ID( type, ... ) \
    class type; \
    template<> const string_id<type> &string_id<type>::NULL_ID() { \
        static string_id<type> id = string_id<type>( __VA_ARGS__ ); \
        return id; \
    }

MAKE_CLASS_NULL_ID( activity_type, "ACT_NULL" )
MAKE_CLASS_NULL_ID( ammunition_type, "NULL" )
MAKE_CLASS_NULL_ID( anatomy, "null_anatomy" )
MAKE_CLASS_NULL_ID( disease_type, "null" )
MAKE_CLASS_NULL_ID( effect_type, "null" )
MAKE_CLASS_NULL_ID( emit, "null" )
MAKE_CLASS_NULL_ID( faction, "NULL" )
MAKE_CLASS_NULL_ID( harvest_list, "null" )
MAKE_CLASS_NULL_ID( Item_group, "" )
MAKE_CLASS_NULL_ID( json_flag, "null" )
MAKE_CLASS_NULL_ID( json_trait_flag, "null" )
MAKE_CLASS_NULL_ID( ma_buff, "null" )
MAKE_CLASS_NULL_ID( ma_technique, "tec_none" )
MAKE_CLASS_NULL_ID( map_extra, "" )
MAKE_CLASS_NULL_ID( martialart, "style_none" )
MAKE_CLASS_NULL_ID( material_type, "null" )
MAKE_CLASS_NULL_ID( monfaction, "null" )
MAKE_CLASS_NULL_ID( morale_type_data, "" )
MAKE_CLASS_NULL_ID( npc_class, "NC_NONE" )
MAKE_CLASS_NULL_ID( npc_template, "null" )
MAKE_CLASS_NULL_ID( overmap_connection, "" )
MAKE_CLASS_NULL_ID( overmap_land_use_code, "" )
MAKE_CLASS_NULL_ID( overmap_special, "" )
MAKE_CLASS_NULL_ID( recipe, "null" )
MAKE_CLASS_NULL_ID( SkillDisplayType, "none" )
MAKE_CLASS_NULL_ID( Skill, "none" )
MAKE_CLASS_NULL_ID( spell_type, "spell_none" )
MAKE_CLASS_NULL_ID( ter_furn_transform, "null" )
MAKE_CLASS_NULL_ID( translation, "null" )
MAKE_CLASS_NULL_ID( VehicleGroup, "null" )
MAKE_CLASS_NULL_ID( vpart_info, "null" )
MAKE_CLASS_NULL_ID( zone_type, "null" )
MAKE_CLASS_NULL_ID( profession, "" )



#define MAKE_STRUCT_NULL_ID( type, ... ) \
    struct type; \
    template<> const string_id<type> &string_id<type>::NULL_ID() { \
        static string_id<type> id = string_id<type>( __VA_ARGS__ ); \
        return id; \
    }

MAKE_STRUCT_NULL_ID( ammo_effect, "AE_NULL" )
MAKE_STRUCT_NULL_ID( bionic_data, "" )
MAKE_STRUCT_NULL_ID( body_part_type, "num_bp" )
MAKE_STRUCT_NULL_ID( construction_category, "NULL" )
MAKE_STRUCT_NULL_ID( construction, "constr_null" )
MAKE_STRUCT_NULL_ID( construction_group, "null" )
MAKE_STRUCT_NULL_ID( field_type, "fd_null" )
MAKE_STRUCT_NULL_ID( furn_t, "f_null" )
MAKE_STRUCT_NULL_ID( itype, "null" )
MAKE_STRUCT_NULL_ID( mission_type, "MISSION_NULL" )
MAKE_STRUCT_NULL_ID( MonsterGroup, "GROUP_NULL" )
MAKE_STRUCT_NULL_ID( mtype, "mon_null" )
MAKE_STRUCT_NULL_ID( mutation_branch, "" )
MAKE_STRUCT_NULL_ID( mutation_category_trait, "null" )
MAKE_STRUCT_NULL_ID( oter_t, "" )
MAKE_STRUCT_NULL_ID( oter_type_t, "" )
MAKE_STRUCT_NULL_ID( overmap_location, "" )
MAKE_STRUCT_NULL_ID( requirement_data, "null" )
MAKE_STRUCT_NULL_ID( species_type, "spec_null" )
MAKE_STRUCT_NULL_ID( ter_t, "t_null" )
MAKE_STRUCT_NULL_ID( trap, "tr_null" )
MAKE_STRUCT_NULL_ID( vehicle_prototype, "null" )
MAKE_STRUCT_NULL_ID( weather_type, "null" )
