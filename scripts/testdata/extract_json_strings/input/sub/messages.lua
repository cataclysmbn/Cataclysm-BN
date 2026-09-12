--~ lua gettext note
-- regular Lua comment between translator note and call
gettext("Lua message")

--~ lua context note
pgettext("menu", "Lua context message")

--~ lua plural note
vgettext("Lua apple", "Lua apples", count)
