#pragma once

#include <memory>

class JsonIn;
class JsonOut;
class player_activity;

class activity_ptr
{
    private:
        std::unique_ptr<player_activity> act;

        /** This releases the activity's memory if it's still active, it will now manage it's own memory */
        void check_active();
    public:
        activity_ptr();
        activity_ptr( const activity_ptr & ) = delete;
        activity_ptr( activity_ptr && ) noexcept ;
        activity_ptr( std::unique_ptr<player_activity> && );
        activity_ptr &operator=( const activity_ptr & ) = delete;
        activity_ptr &operator=( activity_ptr && ) noexcept ;
        activity_ptr &operator=( std::unique_ptr<player_activity> && );

        ~activity_ptr();

        player_activity *get() const {
            return act.get();
        }

        explicit operator bool() const;

        player_activity &operator*() const {
            return *get();
        }

        player_activity *operator->() const {
            return get();
        }

        std::unique_ptr<player_activity> release();

        void serialize( JsonOut &json ) const;
        void deserialize( JsonIn &jsin );
};


