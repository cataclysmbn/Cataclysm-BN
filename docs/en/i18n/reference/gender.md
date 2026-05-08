# Grammatical gender

For NPC dialogue (and potentially other strings) some languages may wish to have alternate
translations depending on the gender of the conversation participants. This two pieces of initial
configuration.

1. The dialogue must have the relevant genders listed in the json file defining it. See
   [the NPC docs](../../mod/json/reference/creatures/npcs).
2. Each language must specify the genders it wishes to use via `genders` list of the language's
   entry in `data/raw/languages.json`. Don't add genders there until you're sure you will need them,
   because it will make more work for you. Current choices are: `m` (male), `f` (female), `n`
   (neuter). If you need different genders than the ones currently supported, see relevant note in
   `src/language.h`.

Having done this, the relevant dialogue lines will appear multiple times for translation, with
different genders specified in the message context. For example, a context of `npc:m` would indicate
that the NPC participant in the conversation is male.

Because of technical limitations, all supported genders will appear as contexts, but you only need
to provide translations for the genders listed in the grammatical gender list for your language.

Other parts of the game have various ad hoc solutions to grammatical gender, so don't be surprised
to see other contexts appearing for other strings.
