#pragma once

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "calendar.h"
#include "cata_variant.h"
#include "enums.h"
#include "event_statistics.h"
#include "json.h"
#include "string_id.h"
#include "translations.h"

class JsonIn;
class JsonObject;
class JsonOut;
class achievements_tracker;
class stats_tracker;
class kill_tracker;
struct achievement_requirement;
template <typename E> struct enum_traits;

enum class achievement_comparison {
    less_equal,
    greater_equal,
    anything,
    last,
};

template<>
struct enum_traits<achievement_comparison> {
    static constexpr achievement_comparison last = achievement_comparison::last;
};

enum class achievement_completion {
    pending,
    completed,
    failed,
    last
};

template<>
struct enum_traits<achievement_completion> {
    static constexpr achievement_completion last = achievement_completion::last;
};

class achievement
{
    public:
        achievement() = default;

        void load( const JsonObject &, const std::string & );
        void check() const;
        static void load_achievement( const JsonObject &, const std::string & );
        static void finalize();
        static void check_consistency();
        static const std::vector<achievement> &get_all();
        static void reset();
        std::string ui_text( achievement_completion completion, const kill_tracker &kt ) const;
        std::string skill_ui_text() const;
        std::string kill_ui_text( achievement_completion completion, const kill_tracker &kt ) const;

        string_id<achievement> id;
        bool was_loaded = false;

        const translation &name() const {
            return name_;
        }

        const translation &description() const {
            return description_;
        }

        const std::vector<string_id<achievement>> &hidden_by() const {
            return hidden_by_;
        }

        class time_bound
        {
            public:
                friend class achievement;
                enum class epoch {
                    cataclysm,
                    game_start,
                    last
                };

                void deserialize( JsonIn & );
                void check( const string_id<achievement> & ) const;
                std::string time_ui_text( const achievement_completion ) const;

                time_point target() const;
                achievement_comparison comparison() const;
            private:
                achievement_comparison comparison_;
                epoch epoch_;
                time_duration period_;
        };

        const std::optional<time_bound> &time_constraint() const {
            return time_constraint_;
        }
        const std::map<mtype_id, std::pair<achievement_comparison, int>> &kill_requirements() const {
            return kill_requirements_;
        }
        const std::map<species_id, std::pair<achievement_comparison, int>> &species_kill_requirements()
        const {
            return species_kill_requirements_;
        }
        const std::map<skill_id, std::pair<achievement_comparison, int>> &skill_requirements() const {
            return skill_requirements_;
        }
        const std::vector<achievement_requirement> &requirements() const {
            return requirements_;
        }
    private:
        translation name_;
        translation description_;
        std::vector<string_id<achievement>> hidden_by_;
        std::optional<time_bound> time_constraint_;
        std::map<skill_id, std::pair<achievement_comparison, int>> skill_requirements_;
        std::map<mtype_id, std::pair<achievement_comparison, int>> kill_requirements_;
        std::map <species_id, std::pair<achievement_comparison, int>> species_kill_requirements_;
        std::vector<achievement_requirement> requirements_;

        /** Retrieves kill requirement JsonObjects and feeds it to add_skill_requirement*/
        void add_kill_requirements( const JsonObject &jo, const std::string &src );
        /** Organizes variables provided and adds kill_requirements to achievements*/
        void add_kill_requirement( const JsonObject &inner, const std::string &src );
        /** Retrieves skill requirement JsonObjects and feeds it to add_skill_requirement*/
        void add_skill_requirements( const JsonObject &jo, const std::string &src );
        /** Organizes variables provided and adds skill_requirements to achievements*/
        void add_skill_requirement( const JsonObject &inner, const std::string &src );
};


struct achievement_requirement {
    string_id<event_statistic> statistic;
    achievement_comparison comparison;
    int target = 0;
    bool becomes_false = false;

    void deserialize( JsonIn &jin );
    void finalize();
    void check( const string_id<achievement> &id ) const;
    bool satisifed_by( const cata_variant &v ) const;
};


template<>
struct enum_traits<achievement::time_bound::epoch> {
    static constexpr achievement::time_bound::epoch last = achievement::time_bound::epoch::last;
};

// Once an achievement is either completed or failed it is stored as an
// achievement_state
struct achievement_state {
    // The final state
    achievement_completion completion;

    // When it became that state
    time_point last_state_change;

    // The values for each requirement at the time of completion or failure
    std::vector<cata_variant> final_values;

    std::string ui_text( const achievement *, const kill_tracker &kt ) const;

    void serialize( JsonOut & ) const;
    void deserialize( JsonIn & );
};

class achievements_tracker
{
    public:
        achievements_tracker( const achievements_tracker & ) = delete;
        achievements_tracker &operator=( const achievements_tracker & ) = delete;

        achievements_tracker(
            stats_tracker &, kill_tracker &,
            const std::function<void( const achievement * )> &achievement_attained_callback );
        ~achievements_tracker();

        auto stats() -> stats_tracker &; // *NOPAD*
        auto stats() const -> const stats_tracker &; // *NOPAD*

        // Return kill tracker pointer (only matters for testing)
        auto kills() const -> const kill_tracker *; // *NOPAD*

        // Return all scores which are valid now and existed at game start
        auto valid_achievements() const -> std::vector<const achievement *>;

        auto report_achievement( const achievement *, achievement_completion ) -> void;
        auto report_achievement( const string_id<achievement> &, achievement_completion ) -> void;

        auto recorded_completion( const string_id<achievement> & ) const -> achievement_completion;
        auto is_completed( const string_id<achievement> & ) const -> achievement_completion;
        auto is_hidden( const achievement * ) const -> bool;
        auto ui_text_for( const achievement * ) const -> std::string;

        auto clear() -> void;

        auto serialize( JsonOut & ) const -> void;
        auto deserialize( JsonIn & ) -> void;
    private:
        auto activate_statistics() -> void;
        auto current_values_for( const achievement & ) const -> std::vector<cata_variant>;
        auto has_failed( const achievement & ) const -> bool;
        auto pending_ui_text_for( const achievement & ) const -> std::string;

        stats_tracker *stats_ = nullptr;
        kill_tracker *kill_tracker_ = nullptr;
        std::function<void( const achievement * )> achievement_attained_callback_;
        achievements_tracker *previous_active_tracker_ = nullptr;

        std::unordered_map<string_id<achievement>, achievement_state> achievements_status_;
};

/** Checks if time requirements for achievements are satisfied, have failed, or are pending.*/
achievement_completion time_req_completed( const achievement &ach );
/** Checks if kill requirements for achievements are satisfied, have failed, or are pending.*/
achievement_completion kill_req_completed( const achievement &ach, const kill_tracker &kt );
/** Checks if skill requirements for achievements are satisfied, or are pending.*/
achievement_completion skill_req_completed( const achievement &ach );

/** Uses comparator supplied to compare target and supplied value. Only works on integers.*/
bool ach_compare( const achievement_comparison symbol, const int target, const int to_compare );
