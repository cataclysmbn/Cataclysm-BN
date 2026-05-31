#pragma once

#include <functional>
#include <iosfwd>
#include <string>
#include <unordered_map>
#include <vector>

#include "enums.h"
#include "item_search.h"

class JsonIn;
class JsonOut;
class item;
struct itype;

namespace auto_pickup
{

class user_interface
{
    public:
        class tab
        {
            public:
                std::string title;
                rule_list new_rules;
                std::reference_wrapper<rule_list> rules;

                tab( const std::string &t, rule_list &r ) : title( t ), new_rules( r ), rules( r ) { }
        };

        std::string title;
        std::vector<tab> tabs;
        bool is_autopickup = false;

        void show();
        void test_pattern( const rule &rule ) const;

        bool bStuffChanged = false;
};

class base_settings
{
    protected:
        mutable item_search_cache map_items;
        bool cache_is_valid = false;
    private:
        virtual void refresh_map_items( item_search_cache &map_items ) const = 0;

        void recreate() const;

    public:
        virtual ~base_settings() = default;
        rule_state check_item( const item &item );
        void invalidate();
};

class player_settings : public base_settings
{
    private:
        void load( bool bCharacter );
        bool save( bool bCharacter );

        rule_list global_rules;
        rule_list character_rules;

        void refresh_map_items( item_search_cache &map_items ) const override;

    public:
        ~player_settings() override = default;
        bool has_rule( const item *it );
        void add_rule( const item *it );
        void remove_rule( const item *it );

        void clear_character_rules();

        void show();
        bool save_character();
        bool save_global();
        void load_character();
        void load_global();

        bool empty() const;
};

class npc_settings : public base_settings
{
    private:
        rule_list rules;

        void refresh_map_items( item_search_cache &map_items ) const override;

    public:
        ~npc_settings() override = default;

        void show( const std::string &name );

        void serialize( JsonOut &jsout ) const;
        void deserialize( JsonIn &jsin );

        bool empty() const;
};

} // namespace auto_pickup

auto_pickup::player_settings &get_auto_pickup();


