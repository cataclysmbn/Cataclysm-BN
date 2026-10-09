#pragma once

#include <memory>

namespace game_client
{
class render_service;

/// Reversible ownership replacement on the presentation service's owning UI thread.
/// Nested scopes restore the exact prior service, rather than a borrowed pointer or fallback.
class presentation_scope
{
    public:
        explicit presentation_scope( std::unique_ptr<render_service> replacement );
        ~presentation_scope() noexcept;
        presentation_scope( const presentation_scope & ) = delete;
        auto operator=( const presentation_scope & ) -> presentation_scope & = delete; // *NOPAD*
    private:
        std::unique_ptr<render_service> previous_;
};
} // namespace game_client
