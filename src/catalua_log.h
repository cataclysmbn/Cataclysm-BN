#pragma once

#include <deque>
#include <string>

namespace cata
{

constexpr size_t DEFAULT_LUA_LOG_CAPACITY = 100;

enum class LuaLogLevel {
    Input,
    Info,
    Warn,
    Error,
    DebugMsg,
};

struct lua_log_msg {
    LuaLogLevel level;
    std::string text;
};

class lua_log_handler
{
    public:
        lua_log_handler();
        ~lua_log_handler() = default;

        void set_log_capacity( size_t lines );

        void add( LuaLogLevel level, std::string &&text );

        void clear();

        const std::deque<lua_log_msg> &get_entries() const {
            return entries;
        }

    private:
        std::deque<lua_log_msg> entries;
        size_t capacity = 0;
};

lua_log_handler &get_lua_log_instance();

} // namespace cata


