> [!CAUTION]
>
> This guide is quite old and requires manual dependency management.
>
> For modern alternative, see [CMake Visual Studio build with vcpkg](./vs_cmake.md)

some text

> [!NOTE]
>
> - `.md` in `CONTRIBUTING.md` stands for markdown files
> - `.mdx` in `docs.mdx` stands for [MarkDown eXtended](https://mdxjs.com)
>   - it's a superset of markdown with javascript and [jsx component][jsx] support
>   - they are a bit more complicated but allows to use interactive components

[jsx]: https://www.typescriptlang.org/docs/handbook/jsx.html

> [!CAUTION]
>
> #### for X11
>
> ```sh
> $ cmake -DLEGACY=ON -B profiler/build -S profiler # if you're using X11
> ```
>
> tracy uses wayland by default, if you want to use X11, you need to add `LEGACY=1` flag.
