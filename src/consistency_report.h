#pragma once

#include <vector>
#include <string>

#include "string_formatter.h"

/**
 * Helper class for aggregating warnings reported during
 * consistency check of some object with id.
 */
class consistency_report
{
    private:
        std::vector<std::string> warns;

    public:
        consistency_report() = default;
        ~consistency_report() = default;

        void warn( const char *msg ) {
            warns.emplace_back( msg );
        }

        void warn( const std::string &msg ) {
            warns.push_back( msg );
        }

        template<typename ...Args>
        void warn( const std::string &format, Args &&... args ) {
            warns.push_back( string_format( format, std::forward<Args>( args )... ) );
        }

        template<typename ...Args>
        void warn( const char *format, Args &&... args ) {
            warns.push_back( string_format( format, std::forward<Args>( args )... ) );
        }

        bool is_empty() const {
            return warns.empty();
        }

        /** Format the report for debugmsg. */
        std::string format( const std::string &data_type, const std::string &id ) const {
            std::string ret = string_format( "Warnings for %s \"%s\":", data_type, id );
            for( const std::string &s : warns ) {
                ret += '\n';
                ret += s;
            }
            return ret;
        }
};


