#pragma once

#include <deque>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "activity_type.h"
#include "calendar.h"
#include "type_id.h"
#include "units.h"

class avatar;
class Character;
class JsonIn;
class JsonOut;
class player_activity;
class inventory;

struct bench_loc;

using metric = std::pair<units::mass, units::volume>;

struct simple_task {
    // Name of the target that's being processed
    const std::string target_name;
    /* Total number of moves required for this target/task to complete */
    const int moves_total = 0;
    /* The number of moves remaining for this target/task to complete */
    int moves_left = 0;

    inline bool complete() const {
        return moves_left <= 0;
    }

    inline bool not_started() const {
        return moves_left == moves_total;
    }

    int to_counter() const;

    //Json stuff
    void serialize( JsonOut &json ) const;
};

/*
 * Special class to track progress of current activity
 * Outside is a queue with slighly altered functionality
*/
class progress_counter
{
    private:
        /** Total number of moves required to complete the activity aka all the tasks */
        int moves_total = 0;
        /** The number of moves remaining in this activity before it is complete aka all the tasks */
        int moves_left = 0;
        //Index of current task - 1-based, since expected to be used only for printing
        int idx = 1;
        //Counts total amount of tasks - done and in queue
        int total_tasks = 0;

        std::deque<simple_task> targets;

    public:
        inline void emplace( std::string name, int moves_total_ ) {
            moves_total += moves_total_;
            moves_left += moves_total_;
            targets.emplace_back( simple_task {
                .target_name = name,
                .moves_total = moves_total_,
                .moves_left = moves_total_
            } );
            total_tasks++;
        }
        inline void emplace( std::string name, int moves_total_, int moves_left_ ) {
            moves_total += moves_total_;
            moves_left += moves_left_;
            targets.emplace_back( simple_task {
                .target_name = name,
                .moves_total = moves_total_,
                .moves_left = moves_left_
            } );
            total_tasks++;
        }
        void pop();
        void purge();
        inline bool empty() const {
            return targets.empty();
        }
        inline bool complete() const {
            return total_tasks > 0 && moves_left <= 0;
        }
        inline bool invalid() const {
            return total_tasks == 0 && empty();
        }
        inline int get_index() const {
            return idx;
        }
        inline int get_total_tasks() const {
            return total_tasks;
        }
        inline int get_moves_total() const {
            return moves_total;
        }
        inline int get_moves_left() const {
            return moves_left;
        }
        inline size_t size() const {
            return targets.size();
        }
        inline const simple_task &front() const {
            return targets.front();
        }
        inline const simple_task &back() const {
            return targets.back();
        }
        //Modifies move_left of the first task(and total progress)
        inline void mod_moves_left( int moves ) {
            moves_left += moves;
            targets.front().moves_left += moves;
        }
        /*
        * Creates a dummy task, ends it instantaneously
        * For very certain and rare occasions, use cautiously
        * Basically to properly process "unique" activities, like autodrive
        */
        inline void dummy() {
            emplace( "If you see this it's a bug", calendar::INDEFINITELY_LONG );
        }

        //Json stuff

        void serialize( JsonOut &json ) const;
        void deserialize( JsonIn &jsin );
};


struct act_progress_message {
    /**
     * Whether activity actor implements the method.
     * TODO: remove once migration to actors is complete.
     */
    bool implemented = true;

    std::optional<std::string> msg_extra_info;
    std::optional<std::string> msg_full;

    /**
     * The text will completely overwrite default message.
     */
    static act_progress_message make_full( std::string &&text ) {
        act_progress_message ret;
        ret.msg_full = std::move( text );
        return ret;
    }

    /**
     * The text will be appended to default message.
     */
    static act_progress_message make_extra_info( std::string &&text ) {
        act_progress_message ret;
        ret.msg_extra_info = std::move( text );
        return ret;
    }

    /**
     * There will be no message shown.
     */
    static act_progress_message make_empty() {
        return act_progress_message{};
    }
};


class activity_actor
{
    protected:
        /**
         * Returns true if `this` activity is resumable, and `this` and @p other
         * are "equivalent" i.e. similar enough that `this` activity
         * can be resumed instead of starting @p other.
         * Many activities are not resumable, so the default is returning
         * false.
         * @pre @p other is the same type of actor as `this`
         */
        virtual bool can_resume_with_internal( const activity_actor &,
                                               const Character & ) const {
            return false;
        }

    public:
        virtual ~activity_actor() = default;
        //List of task with names of ACTUAL targets, like a wall you mine or a grave you dig
        //Also tracks number of moves left and total
        progress_counter progress;
        /**
         * Should return the activity id of the corresponding activity
         */
        virtual activity_id get_type() const = 0;


        /**
         * Actor specific behaviour to recalc all speed values
         * Expected to be called once per target and on game load
         */
        virtual void calc_all_moves( player_activity &act, Character &who );

        /**
         * Called once at the start of the activity.
         * This may be used to preform setup actions and/or set
         * player_activity::moves_left/moves_total.
         */
        virtual void start( player_activity &act, Character &who ) = 0;

        /**
         * Called on every turn of the activity
         * It may be used to stop the activity prematurely by setting it to null.
         */
        virtual void do_turn( player_activity &act, Character &who ) = 0;

        /**
         * Called when the activity runs out of moves, assuming that it has not
         * already been set to null
         */
        virtual void finish( player_activity &act, Character &who ) = 0;

        /**
         * Called just before Character::cancel_activity() executes.
         * This may be used to perform cleanup
         */
        virtual void canceled( player_activity &/*act*/, Character &/*who*/ ) {};

        /**
         * Called in player_activity::can_resume_with
         * which allows suspended activities to be resumed instead of
         * starting a new activity in certain cases.
         * Checks that @p other has the same type as `this` so that
         * `can_resume_with_internal` can safely `static_cast` @p other.
         */
        bool can_resume_with( const activity_actor &other, const Character &who ) const {
            if( other.get_type() == get_type() ) {
                return can_resume_with_internal( other, who );
            }

            return false;
        }

        /**
         * Must write any custom members of the derived class to json
         * Note that a static member function for deserialization must also be created and
         * added to the `activity_actor_deserializers` hashmap in activity_actor.cpp
         */
        virtual void serialize( JsonOut &jsout ) const = 0;

        virtual act_progress_message get_progress_message(
            const player_activity &, const Character & ) const {
            // TODO: make it create default message once migration to actors is complete.
            act_progress_message msg;
            msg.implemented = false;
            return msg;
        }

        /*
         * actor specific formula for speed factor based on skills
         * anything above 0 is a valid number
         * anything below 0 is invalid, promting to use default formula
        */
        virtual float calc_skill_factor( const Character &/*who*/,
                                         const std::vector<activity_req<skill_id>> &/*skills*/ ) const {
            return -1.0f;
        }

        /*
         * actor specific formula for speed factor based on tools' qualities
         * anything above 0 is a valid number
         * anything below 0 is invalid, promting to use default formula
        */
        virtual float calc_tools_factor( const std::vector<activity_req<quality_id>> &/*qualities*/,
                                         const inventory &/*inv*/ ) const {
            return -1.0f;
        }

        /*
         * actor specific formula for speed factor based on player's morale
         * anything above 0 is a valid number
         * anything below 0 is invalid, promting to use default formula
        */
        virtual float calc_morale_factor( const Character &/*who*/ ) const {
            return -1.0f;
        }

        /*
         * actor specific formula for speed factor based on stats
         * anything above 0 is a valid number
         * anything below 0 is invalid, promting to use default formula
        */
        std::vector<std::pair<character_stat, float>> calc_stats_factors( const Character &/*who*/,
        const std::vector<activity_req<character_stat>> &/*stats*/ ) const {
            return {};
        }
};

namespace activity_actors
{

// defined in activity_actor.cpp
extern const std::unordered_map<activity_id, std::unique_ptr<activity_actor>( * )( JsonIn & )>
deserialize_functions;

} // namespace activity_actors

void serialize( const std::unique_ptr<activity_actor> &actor, JsonOut &jsout );
void deserialize( std::unique_ptr<activity_actor> &actor, JsonIn &jsin );

