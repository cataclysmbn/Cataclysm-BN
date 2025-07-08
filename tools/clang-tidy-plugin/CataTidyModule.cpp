#include <clang/Basic/Version.h>
#include <llvm/ADT/StringRef.h>

#include <clang-tidy/ClangTidyModule.h>
#include <clang-tidy/ClangTidyModuleRegistry.h>
#include "CombineLocalsIntoPointCheck.h"
#include "DeterminismCheck.h"
#include "JsonTranslationInputCheck.h"
#include "NoLongCheck.h"
#include "NoStaticGettextCheck.h"
#include "PointInitializationCheck.h"
#include "SimplifyPointConstructorsCheck.h"
#include "TestFilenameCheck.h"
#include "TextStyleCheck.h"
#include "TranslatorCommentsCheck.h"
#include "UnusedStaticsCheck.h"
#include "UseLocalizedSortingCheck.h"
#include "UseNamedPointConstantsCheck.h"
#include "UsePointApisCheck.h"
#include "UsePointArithmeticCheck.h"
#include "XYCheck.h"

namespace clang
{
namespace tidy
{
namespace cata
{

class CataModule : public ClangTidyModule
{
    public:
        void addCheckFactories( ClangTidyCheckFactories &CheckFactories ) override {
            // Sanity check the clang version to verify that we're loaded into
            // the same version we linked against

            std::string RuntimeVersion = getClangFullVersion();
            if( !StringRef( RuntimeVersion ).contains( "clang version " CLANG_VERSION_STRING ) ) {
                llvm::report_fatal_error(
                    Twine( "clang version mismatch in CataTidyModule.  Compiled against "
                           CLANG_VERSION_STRING " but loaded by ", RuntimeVersion ) );
                abort();
            }
            CheckFactories.registerCheck<CombineLocalsIntoPointCheck>(
                "cata-combine-locals-into-point" );
            CheckFactories.registerCheck<DeterminismCheck>( "cata-determinism" );
            CheckFactories.registerCheck<JsonTranslationInputCheck>( "cata-json-translation-input" );
            CheckFactories.registerCheck<NoLongCheck>( "cata-no-long" );
            CheckFactories.registerCheck<NoStaticGettextCheck>( "cata-no-static-gettext" );
            CheckFactories.registerCheck<PointInitializationCheck>( "cata-point-initialization" );
            CheckFactories.registerCheck<SimplifyPointConstructorsCheck>(
                "cata-simplify-point-constructors" );
            CheckFactories.registerCheck<TestFilenameCheck>( "cata-test-filename" );
            CheckFactories.registerCheck<TextStyleCheck>( "cata-text-style" );
            CheckFactories.registerCheck<TranslatorCommentsCheck>( "cata-translator-comments" );
            CheckFactories.registerCheck<UnusedStaticsCheck>( "cata-unused-statics" );
            CheckFactories.registerCheck<UseLocalizedSortingCheck>( "cata-use-localized-sorting" );
            CheckFactories.registerCheck<UseNamedPointConstantsCheck>(
                "cata-use-named-point-constants" );
            CheckFactories.registerCheck<UsePointApisCheck>( "cata-use-point-apis" );
            CheckFactories.registerCheck<UsePointArithmeticCheck>( "cata-use-point-arithmetic" );
            CheckFactories.registerCheck<XYCheck>( "cata-xy" );
        }
};

} // namespace cata

// Register the MiscTidyModule using this statically initialized variable.
// NOLINTNEXTLINE(cata-unused-statics)
static ClangTidyModuleRegistry::Add<cata::CataModule>
X( "cata-module", "Adds Cataclysm-DDA checks." );

} // namespace tidy
} // namespace clang
