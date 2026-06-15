:: Extract strings from JSONs and Yarn dialogue files in this folder
@echo off
if not exist lang md lang
python extract_json_strings.py -i .\ -o lang\temp_json.pot
python extract_yarn_strings.py -i .\ -o lang\temp_yarn.pot
python concat_pot_files.py lang\temp_json.pot lang\temp_yarn.pot lang\extracted_strings.pot
del lang\temp_json.pot lang\temp_yarn.pot
python dedup_pot_file.py lang\extracted_strings.pot
echo Done!
pause
@echo on
