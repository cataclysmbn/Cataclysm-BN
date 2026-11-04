#if defined(CATA_MCP)

#    include "catch/catch.hpp"
#    include "engine_client_session.h"
#    include "language.h"
#    include "mcp_server.h"

#    include <algorithm>
#    include <nlohmann/json.hpp>
#    include <ranges>
#    include <sstream>
#    include <string>
#    include <vector>

namespace {
namespace ec = engine_client;
using json = nlohmann::json;

auto rpc(const std::string& method, const std::string& params, const std::string& id)
    -> std::string {
    return R"({"jsonrpc":"2.0","method":")" + method + R"(","params":)" + params + R"(,"id":)" + id
         + "}\n";
}
auto hello(const std::string& locale = {}) -> std::string {
    return rpc(
        "bn.hello",
        R"({"versions":["0.2"],"client":{"name":"t","version":"1"})"
            + (locale.empty() ? "" : R"(,"locale":")" + locale + '"') + "}",
        "1");
}
/// One connection to a session, which reads whatever it is sent and whatever the session publishes.
struct connection {
    std::ostringstream output;
    std::ostringstream errors;
    std::vector<bn::mcp::key_event> queued;
    bn::mcp::server server;
    explicit connection(ec::session& authority)
        : server{{
              .observe = [] { return bn::mcp::screen_snapshot{.json = "{}"}; },
              .submit =
                  [this](const auto& input) {
                      queued = input;
                      return true;
                  },
              .has_input = [this] { return !queued.empty(); },
              .contract_session = &authority,
          }} {}
    auto send(const std::string& text) -> void {
        auto input = std::istringstream{text};
        for (auto lines = std::ranges::count(text, '\n'); lines > 0; --lines) {
            REQUIRE(server.poll_input(input, output, errors));
        }
    }
    /// Every frame written so far.
    auto frames() const -> std::vector<json> {
        auto result = std::vector<json>{};
        auto lines = std::istringstream{output.str()};
        for (auto line = std::string{}; std::getline(lines, line);) {
            if (!line.empty()) { result.push_back(json::parse(line)); }
        }
        return result;
    }
    /// The result of the request with this id.
    auto result(const int id) const -> json {
        for (const auto& frame : frames()) {
            if (frame.contains("id") && frame["id"] == id) { return frame.at("result"); }
        }
        FAIL("no response with id " << id << " in " << output.str());
        return {};
    }
};
} // namespace

TEST_CASE(
    "a hello asking for another language is answered in the engine's own",
    "[engine_client_locale][engine_client_server]") {
    auto authority = ec::session{};
    REQUIRE(authority.publish_boundary());
    for (const auto* locale : {"xx_XX", "de_DE", "ko_KR"}) {
        auto asker = connection{authority};
        asker.send(hello(locale));
        const auto answer = asker.result(1);
        INFO(asker.output.str());
        CHECK(answer["locale"] == answer["engine_locale"]);
        CHECK(answer["engine_locale"].get<std::string>() == get_language().id);
        CHECK(answer["version"] == "0.2");
    }
}
#endif
