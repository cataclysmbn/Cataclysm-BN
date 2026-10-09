#if defined(CATA_INPUT_LIFECYCLE_TEST)
#    include "input.h"

#    include <cstdlib>
#    include <iostream>
#    include <ranges>
#    include <vector>

#    if !defined(__ANDROID__)
#        error This target must exercise the production Android publication branch.
#    endif

namespace {
auto assertions = 0;
auto expect_stack(const std::vector<input_context*>& expected) -> void {
    ++assertions;
    if (!std::ranges::equal(input_context::input_context_stack, expected)) {
        std::cerr << "Android publication assertion " << assertions << " failed\n";
        std::abort();
    }
}
auto expect_category(const input_context& context, const std::string& expected) -> void {
    ++assertions;
    if (context.category_name() != expected) { std::abort(); }
}
} // namespace

auto main() -> int {
    expect_stack({});
    {
        auto root = input_context{};
        expect_stack({&root});
        {
            auto native = input_context("native");
            expect_stack({&root, &native});
            auto metadata = input_context(
                input_context_options{.category = "metadata", .mode = input_context_mode::metadata});
            expect_stack({&root, &native});
            auto metadata_copy = input_context(metadata);
            expect_stack({&root, &native});
            expect_category(metadata_copy, "metadata");
            {
                auto native_copy = input_context(native);
                expect_stack({&root, &native, &native_copy});
                native_copy = metadata;
                expect_category(native_copy, "metadata");
                expect_stack({&root, &native, &native_copy});
                metadata_copy = native;
                expect_category(metadata_copy, "native");
                expect_stack({&root, &native, &native_copy});
                auto& native_alias = native_copy;
                native_copy = native_alias;
                auto& metadata_alias = metadata_copy;
                metadata_copy = metadata_alias;
                expect_stack({&root, &native, &native_copy});
                {
                    auto after_assignment = input_context(metadata_copy);
                    expect_stack({&root, &native, &native_copy});
                    auto still_native = input_context(native_copy);
                    expect_stack({&root, &native, &native_copy, &still_native});
                    after_assignment = still_native;
                    expect_stack({&root, &native, &native_copy, &still_native});
                }
                expect_stack({&root, &native, &native_copy});
            }
            expect_stack({&root, &native});
            metadata = root;
            native = metadata_copy;
            expect_stack({&root, &native});
            expect_category(native, "native");
        }
        expect_stack({&root});
        try {
            auto metadata = input_context(
                input_context_options{.mode = input_context_mode::metadata});
            auto native = input_context("exception");
            expect_stack({&root, &native});
            throw 7;
        } catch (const int value) {
            if (value != 7) { std::abort(); }
        }
        expect_stack({&root});
    }
    expect_stack({});
    std::cout
        << assertions
        << " assertions passed: actual Android input_context publication branch; host source-matched lifecycle target, not an Android product build\n";
}
#endif
