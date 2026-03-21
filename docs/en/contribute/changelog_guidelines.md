# Changelog Guidelines

PR title follows [Convensional Commits](https://www.conventionalcommits.org/en/v1.0.0/) for easier
changelog generation. The format is one of:

```
<Type>: <PR subject>
<Type>(<Scope>, <Scope>, ...): <PR subject>
```

For Example, a PR title can be:

```
feat: add new mutation
feat(port): port mutation description from DDA
```

The PR title should be easy to understand for players at a glance. It's recommended to use imperative and descriptive title (`<verb> <noun>`) like:

```diff
- feat: rebalancing some rifles
+ feat: nerf jam chance of m16 and m4
```

before: it's hard to know whether they are buffed or nerfed, and which rifles are changed unless reading the full PR description.
after: it's easy to understand what exactly is changed from the title itself.

## Type

The type is the first word in the PR title. They specify the type of change being made. When in
doubt, use `feat` for new features, and `fix` for bugfixes. Here are some frequently used
categories:

### `feat`: Features

New features, additions, or balance changes.

### `fix`: Bugfixes

Anything that fixes a bug or makes the game more stable.

### `refactor`: Infrastructure

Make development easier without changing its behavior. For example:

- `C++` refactorings and overhaul
- `Json` reorganizations
- `docs/`, `.github/` and repository changes
- other development tools

### `build`: Build

Improve build process:

- more robust
- easier to use
- faster compile time

### Others

- `docs`: Documentation changes
- `style`: Code style changes (whitespace, formatting, etc), usu. fixing JSON formatting.
- `perf`: Performance Improvements
- `test`: Adding missing tests or correcting existing tests
- `ci`: Changes to CI process
- `chore`: Other changes that don't fit into any of the above categories
- `revert`: Reverts a previous commit

## Scopes

1. Use them inside parentheses after the category to further narrow the scope of your PR.
2. There are no limits to number of scopes.
3. They are optional, but recommended.
4. these are only guidelines, not rules. choose the best one for your PR freely!

### `<None>`: General Features

For example,

Changes related to player:

- player can do something new (e.g: mutations, skills)
- something new can happen to the player (e.g: new disease)

New contents like:

- new monsters
- new map areas
- new items
- new vehicles
- new doohickeys

Example PR title:

```
feat: strength training activity
feat: mutation system overhaul
feat: semi-plausible smokeless gunpowder recipe
feat(port): game store
```

### `lua`: Changes to Lua API

Changes to the Lua API, such as:

- [adding new bindings](../mod/lua/guides/binding.md)
- improving lua documentation/API generation
- [migrating hardcoded C++ features to lua](https://github.com/cataclysmbn/Cataclysm-BN/pull/6901)

Example PR title:

```
feat(lua): add dialogue bindings
```

### `UI`: Interfaces

UI/UX changes like:

- adding mouse support
- adding/adjusting menus
- change shortcuts
- streamlining workflows
- quality of life improvements

Example PR title:

```
feat(UI): More info about items dropped with bags
feat(UI): overhaul encumbrance UI
```

### `i18n`: Internationalization

Improve translation and other languages support.

```
fix(UI,i18n): recipe names not translated unless learned
```

### `mods` or `mods/<MOD_ID>`: Mods

- changes contained within a mod
- extends what is capable within a mod

Example PR title:

```
feat(mods/Magical_Nights): add missing owlbear pelts recipe
fix(mods): No Hope doesn't make the world freezing
```

### `balance`: Balance Changes

Changes to game balance.

Example PR title:

```
feat(balance): Give moose pelts instead of hides
```

### `port`: Ports from DDA or other forks

Example PR title:

```
feat(port): game shop
```
