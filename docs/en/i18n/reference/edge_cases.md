# Edge cases

There are issues specific to Cataclysm: BN which translators should be aware of. For example:

## Special Symbols

Some translation texts will have special symbols or formatting, such as

- [`%s` and `%3$d` (should be leave them as they are)](../explanation/file_format.md#format-strings-and-newlines)
- [`<name>` (shouldn't be translated)](../explanation/file_format.md#special-tags-in-strings)

To learn more, please read the [file format explanation](../explanation/file_format.md).

## Language Specific Guidelines

Check the following files for specific guidelines:

- [General notes for all translators (mostly english)](../explanation/style_all.md)
- [Notes specific to a language](../explanation/style.md)

Cataclysm: BN has more than 46,000 translatable strings (last updated at 2023-09-22 [^1]), but don't
be discouraged. The more translators there are, the easier it becomes!

[^1]: Transifex API does not expose a public endpoint (at least without bearer token), so we were
    unable to update the number automatically.)
