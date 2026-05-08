# Failed to create glyph error

There were similar issues reported in BN and DDA for MSYS Windows builds and some Mac builds.
https://github.com/CleverRaven/Cataclysm-DDA/issues/50115

It's something in newer versions of SDL2/freetype not agreeing with with our default font, and
apparently vcpkg updated both of them 20 and 9 days ago respectively
(https://github.com/microsoft/vcpkg/pull/19509, https://github.com/microsoft/vcpkg/pull/19284). I'm
using month old vcpkg, and there's no glyph issue, so that looks to me like the root of the problem.

Unfortunately, vcpkg being Microsoft's official C++ library manager does not allow you to install
libraries of some specific version. Thus, you have two options:

1. Grab an older version of vcpkg, from before the updates. I've installed from this one, it should
   work: https://github.com/microsoft/vcpkg/tree/6bc4362fb49e53f1fff7f51e4e27e1946755ecc6

2. Open config/fonts.json and remove Terminus.ttf entries. I don't remember why we're not using
   unifont by default, but if you're not planning on working on i18n/fonts code, it shouldn't affect
   you beyond visually.
