# Extract strings from JSONs and Yarn dialogue files in this folder
mkdir -p lang
python3 extract_json_strings.py -i ./ -o lang/temp_json.pot
python3 extract_yarn_strings.py -i ./ -o lang/temp_yarn.pot
python3 concat_pot_files.py lang/temp_json.pot lang/temp_yarn.pot lang/extracted_strings.pot
rm lang/temp_json.pot lang/temp_yarn.pot
python3 dedup_pot_file.py lang/extracted_strings.pot
echo Done!
