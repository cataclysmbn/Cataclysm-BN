# Extract strings from JSON, JSONC, and Lua files in this folder
mkdir -p lang
deno run --allow-read --allow-write --allow-run scripts/extract_json_strings.ts -i ./ -o lang/extracted_strings.pot
deno run --allow-read --allow-write scripts/pot_tools.ts dedup lang/extracted_strings.pot
echo Done!
