#pragma once

#include <map>
#include <string>
#include <vector>

#include "event_bus.h"
#include "type_id.h"

class JsonIn;
class JsonOut;
namespace cata
{
class event;
}  // namespace cata

class kill_tracker : public event_subscriber
{
        /**
         * to keep track of new kills, we need to access the private members (kills, npc_kills).
         * we may include getter for those later, and remove friend class diary.
         */
        friend class diary;
    public:
        /**
         * @param xp_allowed can be set to false to disable Stats Through Kills regardless of game options.
         *                   Intended to be used when creating multiple kill_trackers, so they don't all grant XP.
         */
        kill_tracker( bool xp_allowed = true );
        void reset( const std::map<mtype_id, int> &kills,
                    const std::vector<std::string> &npc_kills );
        /** returns the number of kills of the given mon_id by the player. */
        int kill_count( const mtype_id & ) const;
        /** returns the number of kills of the given monster species by the player. */
        int kill_count( const species_id & ) const;
        int monster_kill_count() const;
        int npc_kill_count() const;
        // returns player's "kill xp" for monsters via STK
        int kill_xp() const;

        std::string get_kills_text() const;

        void clear();

        void notify( const cata::event & ) override;
        /** directly adds a monster kill to the tracker, bypassing the event bus. */
        void add_monster( mtype_id );
        /** directly adds an NPC kill to the tracker, bypassing the event bus. */
        void add_npc( std::string );

        void serialize( JsonOut & ) const;
        void deserialize( JsonIn & );
    private:
        bool xp_allowed;
        bool option_xp() const;
        std::map<mtype_id, int> kills;         // player's kill count
        std::vector<std::string> npc_kills;    // names of NPCs the player killed
};


