#!/usr/bin/env python3
"Extract translatable strings from JSON files into a single POT."
"The resulting file may contain duplicate or conflicting entries."
"Use dedup_pot_file.py to resolve them."

import time
import json
import os
import itertools
import subprocess
from optparse import OptionParser
from sys import platform
from copy import deepcopy

try:
    import polib
except:
    print("You need 'polib' module installed for the script to work.")
    exit(1)

try:
    import luaparser
except:
    print("You need 'luaparser' module installed for the script to work.")
    exit(1)

from luaparser import ast
from luaparser import astnodes

# Must parse command line arguments here
# 'options' variable is referenced in our defined functions below

parser = OptionParser()
parser.add_option("-p", "--project", action="store", dest="project_name", help="project name and (optionally) version. If not specified, uses mod id from first encountered modinfo.")
parser.add_option("-v", "--verbose", action="store_true", default=False, dest="verbose", help="be verbose")
parser.add_option("-i", "--input", action="append",  dest="input_folders", help="list of input folders")
parser.add_option("-e", "--exclude", action="append", default=[], dest="excluded_files", help="exclude individual files from scan")
parser.add_option("-E", "--exclude-dir", action="append", default=[], dest="excluded_dirs", help="exclude individual directories from scan")
parser.add_option("--tracked-only", action="store_true", default=False, dest="tracked_only", help="scan only git tracked files")
parser.add_option("-o", "--output", action="store", dest="output_file", help="output file")
parser.add_option("-s", "--suppress", action="append", default=[], dest="suppress_warning_for_files", help="suppress 'nothing translatable found' warnings for given files")
parser.add_option("--warn-unused-types", action="store_true", default=False, dest="warn_unused_types", help="warn about types defined in script but unused in JSON")
(options, args) = parser.parse_args()

if not options.input_folders or len(options.input_folders) == 0:
    print("Missing input list")
    exit(1)
if not options.output_file:
    print("Missing output file")
    exit(1)

class ExtractorState:
    current_source_file = '' # Source file being processed
    po_entries = []          # Collected entries
    project_name = ''        # Project name for POT


# Exceptions
class WrongJSONItem(Exception):
    def __init__(self, msg, item):
        self.msg = msg
        self.item = item

    def __str__(self):
        return (f"---\nJSON error\n{self.msg}\n--- JSON Item:\n{self.item}\n---")

git_files_list = {os.path.normpath(i) for i in {
    ".",
}}

# no warning will be given if an untranslatable object is found in those files
warning_suppressed_list = {os.path.normpath(i) for i in options.suppress_warning_for_files }

def warning_supressed(filename):
    for i in warning_suppressed_list:
        if filename.startswith(i):
            return True
    return False

# these files will not be parsed. Full related path.
ignore_files = {os.path.normpath(i) for i in options.excluded_files }
ignore_dirs = {os.path.normpath(i) for i in options.excluded_dirs }

# these objects have no translatable strings
ignorable = {
    "ascii_art",
    "ammo_effect",
    "behavior",
    "charge_removal_blacklist",
    "city_building",
    "colordef",
    "construction_sequence",
    "disease_type",
    "emit",
    "enchantment",
    "event_transformation",
    "event_statistic",
    "EXTERNAL_OPTION",
    "hit_range",
    "hunting_data",
    "ITEM_BLACKLIST",
    "item_group",
    "MIGRATION",
    "mod_tileset",
    "monster_adjustment",
    "MONSTER_BLACKLIST",
    "MONSTER_FACTION",
    "monstergroup",
    "MONSTER_WHITELIST",
    "mutation_type",
    "oter_id_migration",
    "overlay_order",
    "overmap_connection",
    "overmap_location",
    "overmap_special",
    "profession_item_substitutions",
    "palette",
    "region_overlay",
    "region_settings",
    "requirement",
    "rotatable_symbol",
    "SCENARIO_BLACKLIST",
    "scent_type",
    "skill_boost",
    "to_cbc_migration",
    "TRAIT_BLACKLIST",
    "trait_group",
    "uncraft",
    "vehicle_group",
    "vehicle_placement",
    "WORLD_OPTION",
}

# these objects can have their strings automatically extracted.
# insert object "type" here IF AND ONLY IF
# all of their translatable strings are in the following form:
#   "name" member
#   "description" member
#   "text" member
#   "sound" member
#   "prompt" member
#   "messages" member containing an array of translatable strings
automatically_convertible = {
    "achievement",
    "activity_type",
    "AMMO",
    "ammunition_type",
    "ARMOR",
    "BATTERY",
    "bionic",
    "BIONIC_ITEM",
    "BOOK",
    "COMESTIBLE",
    "construction_category",
    "construction_group",
    "CONTAINER",
    "dream",
    "ENGINE",
    "event_statistic",
    "faction",
    "furniture",
    "GENERIC",
    "item_action",
    "ITEM_CATEGORY",
    "json_flag",
    "mutation_flag",
    "keybinding",
    "LOOT_ZONE",
    "MAGAZINE",
    "map_extra",
    "MOD_INFO",
    "MONSTER",
    "morale_type",
    "npc",
    "npc_class",
    "overmap_land_use_code",
    "overmap_terrain",
    "PET_ARMOR",
    "score",
    "skill",
    "SPECIES",
    "speech",
    "SPELL",
    "start_location",
    "terrain",
    "TOOL",
    "TOOLMOD",
    "TOOL_ARMOR",
    "tool_quality",
    "vehicle",
    "vehicle_part",
    "vitamin",
    "WHEEL",
    "help",
    "weather_type"
}

# for these objects a plural form is needed
# NOTE: please also change `needs_plural` in `src/item_factory.cpp`
# when changing this list
needs_plural = {
    "AMMO",
    "ARMOR",
    "BATTERY",
    "BIONIC_ITEM",
    "BOOK",
    "COMESTIBLE",
    "CONTAINER",
    "ENGINE",
    "GENERIC",
    "GUN",
    "GUNMOD",
    "MAGAZINE",
    "MONSTER",
    "PET_ARMOR",
    "SPECIES",
    "TOOL",
    "TOOLMOD",
    "TOOL_ARMOR",
    "WHEEL",
}

# For handling grammatical gender
all_genders = ["f", "m", "n"]

def gender_options(subject):
    return [f"{subject}:{g}" for g in all_genders]

##
##  SPECIALIZED EXTRACTION FUNCTIONS
##

def extract_harvest(state, item):
    if "message" in item:
        writestr(state, item["message"])


def extract_bodypart(state, item):
    # See comments in `body_part_struct::load` of bodypart.cpp about why xxx and xxx_multiple are not inside a single translation object.
    writestr(state, item["name"])
    if "name_multiple" in item:
        writestr(state, item["name_multiple"])
    writestr(state, item["accusative"])
    if "accusative_multiple" in item:
        writestr(state, item["accusative_multiple"])
    if "encumbrance_text" in item:
        writestr(state, item["encumbrance_text"])
    writestr(state, item["heading"])
    if "heading_multiple" in item:
        writestr(state, item["heading_multiple"])
    if "hp_bar_ui_text" in item:
        writestr(state, item["hp_bar_ui_text"])


def extract_clothing_mod(state, item):
    writestr(state, item["implement_prompt"])
    writestr(state, item["destroy_prompt"])


def extract_construction(state, item):
    if "pre_note" in item:
        writestr(state, item["pre_note"])


def extract_material(state, item):
    writestr(state, item["name"])
    wrote = False
    if "bash_dmg_verb" in item:
        writestr(state, item["bash_dmg_verb"])
        wrote = True
    if "cut_dmg_verb" in item:
        writestr(state, item["cut_dmg_verb"])
        wrote = True
    if "dmg_adj" in item:
        writestr(state, item["dmg_adj"][0])
        writestr(state, item["dmg_adj"][1])
        writestr(state, item["dmg_adj"][2])
        writestr(state, item["dmg_adj"][3])
        wrote = True
    if not wrote and not "copy-from" in item:
        print(f"WARNING: {state.current_source_file}: no mandatory field in item: {item}")


def extract_martial_art(state, item):
    if "name" in item:
        name = item["name"]
        writestr(state, name)
    else:
        name = item["id"]
    if "description" in item:
        writestr(state, item["description"],
                 comment=f"Description for martial art '{name}'")
    if "initiate" in item:
        writestr(state, item["initiate"], format_strings=True,
                 comment=f"Initiate message for martial art '{name}'")
    onhit_buffs = item.get("onhit_buffs", list())
    static_buffs = item.get("static_buffs", list())
    onmove_buffs = item.get("onmove_buffs", list())
    ondodge_buffs = item.get("ondodge_buffs", list())
    buffs = onhit_buffs + static_buffs + onmove_buffs + ondodge_buffs
    for buff in buffs:
        writestr(state, buff["name"])
        if buff["name"] == item["name"]:
            c=f"Description of buff for martial art '{name}'"
        else:
            c=f"Description of buff '{buff['name']}' for martial art '{name}'"
        writestr(state, buff["description"], comment=c)


def extract_effect_type(state, item):
    # writestr will not write string if it is None.
    ctxt_name = item.get("name", ())

    if ctxt_name:
        if len(ctxt_name) == len(item.get("desc", ())):
            for nm_desc in zip(ctxt_name, item.get("desc", ())):
                writestr(state, nm_desc[0])
                writestr(state, nm_desc[1], format_strings=True,
                         comment=f"Description of effect '{nm_desc[0]}'.")
        else:
            for i in ctxt_name:
                writestr(state, i)
            for f in ["desc", "reduced_desc"]:
                for i in item.get(f, ()):
                    writestr(state, i, format_strings=True)

    name = None
    if ctxt_name:
        name = [i["str"] if type(i) is dict else i for i in ctxt_name]
    # apply_message
    msg = item.get("apply_message")
    if not name:
        writestr(state, msg, format_strings=True)
    else:
        writestr(state, msg, format_strings=True,
                 comment=f"Apply message for effect(s) '{', '.join(name)}'.")

    # remove_message
    msg = item.get("remove_message")
    if not name:
        writestr(state, msg, format_strings=True)
    else:
        writestr(state, msg, format_strings=True,
                 comment=f"Remove message for effect(s) '{', '.join(name)}'.")

    # miss messages
    msg = item.get("miss_messages", ())
    if not name:
        for m in msg:
            writestr(state, m[0])
    else:
        for m in msg:
            writestr(state, m[0],
                     comment=f"Miss message for effect(s) '{', '.join(name)}'.")
    msg = item.get("decay_messages", ())
    if not name:
        for m in msg:
            writestr(state, m[0])
    else:
        for m in msg:
            writestr(state, m[0],
                     comment=f"Decay message for effect(s) '{', '.join(name)}'.")

    # speed_name
    if "speed_name" in item:
        if not name:
            writestr(state, item.get("speed_name"))
        else:
            writestr(state, item.get("speed_name"), comment=f"Speed name of effect(s) '{', '.join(name)}'.")

    # apply and remove memorial messages.
    msg = item.get("apply_memorial_log")
    if not name:
        writestr(state, msg, context="memorial_male")
        writestr(state, msg, context="memorial_female")
    else:
        writestr(state, msg, context="memorial_male",
                 comment=f"Male memorial apply log for effect(s) '{', '.join(name)}'.")
        writestr(state, msg, context="memorial_female",
                 comment=f"Female memorial apply log for effect(s) '{', '.join(name)}'.")
    msg = item.get("remove_memorial_log")
    if not name:
        writestr(state, msg, context="memorial_male")
        writestr(state, msg, context="memorial_female")
    else:
        writestr(state, msg, context="memorial_male",
          comment=f"Male memorial remove log for effect(s) '{', '.join(name)}'.")
        writestr(state, msg, context="memorial_female",
          comment=f"Female memorial remove log for effect(s) '{', '.join(name)}'.")


def extract_gun(state, item):
    if "name" in item:
        item_name = item.get("name")
        if item["type"] in needs_plural:
            writestr(state, item_name, pl_fmt=True)
        else:
            writestr(state, item_name)
    if "description" in item:
        description = item.get("description")
        writestr(state, description)
    if "modes" in item:
        modes = item.get("modes")
        for fire_mode in modes:
            writestr(state, fire_mode[1])
    if "skill" in item:
        # legacy code: the "gun type" is calculated in `item::gun_type` and
        # it's basically the skill id, except for archery (which is either
        # bow or crossbow). Once "gun type" is loaded from JSON, it should
        # be extracted directly.
        if not item.get("skill") == "archery":
            writestr(state, item.get("skill"), context="gun_type_type")
    if "reload_noise" in item:
        item_reload_noise = item.get("reload_noise")
        writestr(state, item_reload_noise)


def extract_gunmod(state, item):
    if "name" in item:
        item_name = item.get("name")
        if item["type"] in needs_plural:
            writestr(state, item_name, pl_fmt=True)
        else:
            writestr(state, item_name)
    if "description" in item:
        description = item.get("description")
        writestr(state, description)
    if "mode_modifier" in item:
        modes = item.get("mode_modifier")
        for fire_mode in modes:
            writestr(state, fire_mode[1])
    if "location" in item:
        location = item.get("location")
        writestr(state, location)
    if "mod_targets" in item:
        for target in item["mod_targets"]:
            writestr(state, target, context="gun_type_type")


def extract_profession(state, item):
    comment_m = "???"
    comment_f = "???"
    if "name" in item:
        nm = item["name"]
        entry_m = None
        entry_f = None
        if type(nm) == dict and "male" in nm and "female" in nm:
            # 2 different translation entries for male & female
            entry_m = nm["male"]
            entry_f = nm["female"]
        else:
            # 1 translation entry for both male & female
            entry_m = nm
            entry_f = nm
        entry_m = add_context(entry_m, "profession_male")
        entry_f = add_context(entry_f, "profession_female")
        writestr(state, entry_m)
        writestr(state, entry_f)
        comment_m = entry_m["str"]
        comment_f = entry_f["str"]
    if "description" in item:
        writestr(state, add_context(item["description"], "prof_desc_male"),
                    comment=f"Profession (male {comment_m}) description")
        writestr(state, add_context(item["description"], "prof_desc_female"),
                    comment=f"Profession (female {comment_f}) description")


def extract_scenario(state, item):
    # writestr will not write string if it is None.
    name = item.get("name")
    writestr(state,
             name,
             context="scenario_male",
             comment=f"Name for scenario '{name}' for a male character")
    writestr(state,
             name,
             context="scenario_female",
             comment=f"Name for scenario '{name}' for a female character")
    if name:
        msg = item.get("description")
        if msg:
            writestr(state,
                     msg,
                     context="scen_desc_male",
                     comment=f"Description for scenario '{name}' for a male character.")
            writestr(state,
                     msg,
                     context="scen_desc_female",
                     comment=f"Description for scenario '{name}' for a female character.")
        msg = item.get("start_name")
        if msg:
            writestr(state,
                     msg,
                     context="start_name",
                     comment=f"Starting location for scenario '{name}'.")
    else:
        for f in ["description", "start_name"]:
            found = item.get(f, None)
            writestr(state, found)


def extract_mapgen(state, item):
    # writestr will not write string if it is None.
    for (objkey, objval) in sorted(item["object"].items(), key=lambda x: x[0]):
        if objkey == "place_specials" or objkey == "place_signs":
            for special in objval:
                for (speckey, specval) in sorted(special.items(), key=lambda x: x[0]):
                    if speckey == "signage":
                        writestr(state, specval, comment="Sign")
        elif objkey == "signs":
            for (k, v) in sorted(objval.items(), key=lambda x: x[0]):
                sign = v.get("signage", None)
                writestr(state, sign, comment="Sign")
        elif objkey == "computers":
            for (k, v) in sorted(objval.items(), key=lambda x: x[0]):
                if "name" in v:
                    writestr(state, v.get("name"), comment="Computer name")
                if "options" in v:
                    for opt in v.get("options"):
                        writestr(state, opt.get("name"), comment="Computer option")
                if "access_denied" in v:
                    writestr(state, v.get("access_denied"),
                             comment="Computer access denied warning")


def extract_monster_attack(state, item):
    if "hit_dmg_u" in item:
        writestr(state, item.get("hit_dmg_u"))
    if "hit_dmg_npc" in item:
        writestr(state, item.get("hit_dmg_npc"))
    if "no_dmg_msg_u" in item:
        writestr(state, item.get("no_dmg_msg_u"))
    if "no_dmg_msg_npc" in item:
        writestr(state, item.get("no_dmg_msg_npc"))


def extract_recipe(state, item):
    if "book_learn" in item:
        for arr in item["book_learn"]:
            if len(arr) >= 3 and len(arr[2]) > 0:
                writestr(state, arr[2])
    if "description" in item:
        writestr(state, item["description"])
    if "blueprint_name" in item:
        writestr(state, item["blueprint_name"])


def extract_recipe_group(state, item):
    if "recipes" in item:
        for i in item.get("recipes"):
            writestr(state, i.get("description"))


def extract_gendered_dynamic_line_optional(state, line):
    if "gendered_line" in line:
        msg = line["gendered_line"]
        subjects = line["relevant_genders"]
        options = [gender_options(subject) for subject in subjects]
        for context_list in itertools.product(*options):
            context = " ".join(context_list)
            writestr(state, msg, context=context)


def extract_dynamic_line_optional(state, line, member):
    if member in line:
        extract_dynamic_line(state, line[member])


dynamic_line_string_keys = [
# from `simple_string_conds` in `condition.h`
    "u_male", "u_female", "npc_male", "npc_female",
    "has_no_assigned_mission", "has_assigned_mission", "has_many_assigned_missions",
    "has_no_available_mission", "has_available_mission", "has_many_available_missions",
    "mission_complete", "mission_incomplete", "mission_has_generic_rewards",
    "npc_available", "npc_following", "npc_friend", "npc_hostile",
    "npc_train_skills", "npc_train_styles",
    "at_safe_space", "is_day", "npc_has_activity", "is_outside", "u_has_camp",
    "u_can_stow_weapon", "npc_can_stow_weapon", "u_has_weapon", "npc_has_weapon",
    "u_driving", "npc_driving",
    "has_pickup_list", "is_by_radio", "has_reason",
# yes/no strings for complex conditions, 'and' list
    "yes", "no", "and"
]


def extract_dynamic_line(state, line):
    if type(line) == list:
        for l in line:
            extract_dynamic_line(state, l)
    elif type(line) == dict:
        extract_gendered_dynamic_line_optional(state, line)
        for key in dynamic_line_string_keys:
            extract_dynamic_line_optional(state, line, key)
    elif type(line) == str:
        writestr(state, line)


def extract_talk_effects(state, effects):
    if type(effects) != list:
        effects = [effects]
    for eff in effects:
        if type(eff) == dict:
            if "u_buy_monster" in eff and "name" in eff:
                writestr(state, eff["name"], comment=f"Nickname for creature '{eff['u_buy_monster']}'")


def extract_talk_response(state, response):
    if "text" in response:
        writestr(state, response["text"])
    if "truefalsetext" in response:
        writestr(state, response["truefalsetext"]["true"])
        writestr(state, response["truefalsetext"]["false"])
    if "success" in response:
        extract_talk_response(state, response["success"])
    if "failure" in response:
        extract_talk_response(state, response["failure"])
    if "speaker_effect" in response:
        speaker_effects = response["speaker_effect"]
        if type(speaker_effects) != list:
            speaker_effects = [speaker_effects]
        for eff in speaker_effects:
            if "effect" in eff:
                extract_talk_effects(state, eff["effect"])
    if "effect" in response:
        extract_talk_effects(state, response["effect"])


def extract_talk_topic(state, item):
    if "dynamic_line" in item:
        extract_dynamic_line(state, item["dynamic_line"])
    if "responses" in item:
        for r in item["responses"]:
            extract_talk_response(state, r)
    if "effect" in item:
        extract_talk_effects(state, item["effect"])


def extract_technique(state, item):
    writestr(state, item["name"])
    if "description" in item:
        writestr(state, item["description"])
    if "messages" in item:
        for msg in item["messages"]:
            writestr(state, msg, format_strings=True)


def extract_trap(state, item):
    writestr(state, item["name"])
    if "vehicle_data" in item and "sound" in item["vehicle_data"]:
        writestr(state, item["vehicle_data"]["sound"], comment=f"Trap-vehicle collision message for trap '{item['name']}'")


def extract_missiondef(state, item):
    item_name = item.get("name")
    if item_name is None:
        raise WrongJSONItem("JSON item don't contain 'name' field", item)
    writestr(state, item_name)
    if "description" in item:
        writestr(state, item["description"], comment=f"Description for mission '{item_name}'")
    if "dialogue" in item:
        dialogue = item.get("dialogue")
        if "describe" in dialogue:
            writestr(state, dialogue.get("describe"))
        if "offer" in dialogue:
            writestr(state, dialogue.get("offer"))
        if "accepted" in dialogue:
            writestr(state, dialogue.get("accepted"))
        if "rejected" in dialogue:
            writestr(state, dialogue.get("rejected"))
        if "advice" in dialogue:
            writestr(state, dialogue.get("advice"))
        if "inquire" in dialogue:
            writestr(state, dialogue.get("inquire"))
        if "success" in dialogue:
            writestr(state, dialogue.get("success"))
        if "success_lie" in dialogue:
            writestr(state, dialogue.get("success_lie"))
        if "failure" in dialogue:
            writestr(state, dialogue.get("failure"))
    if "start" in item and "effect" in item["start"]:
        extract_talk_effects(state, item["start"]["effect"])
    if "end" in item and "effect" in item["end"]:
        extract_talk_effects(state, item["end"]["effect"])
    if "fail" in item and "effect" in item["fail"]:
        extract_talk_effects(state, item["fail"]["effect"])


def extract_mutation(state, item):
    item_name_or_id = found = item.get("name")
    if found is None:
        if "copy-from" in item:
            item_name_or_id = item["id"]
        else:
            raise WrongJSONItem("JSON item don't contain 'name' field", item)
    else:
        writestr(state, found)

    simple_fields = [ "description" ]

    for f in simple_fields:
        found = item.get(f)
        # Need that check due format string argument
        if found is not None:
            writestr(state, found, comment=f"Description for {item_name_or_id}")

    if "attacks" in item:
        attacks = item.get("attacks")
        if type(attacks) is list:
            for i in attacks:
                if "attack_text_u" in i:
                    writestr(state, i.get("attack_text_u"))
                if "attack_text_npc" in i:
                    writestr(state, i.get("attack_text_npc"))
        else:
            if "attack_text_u" in attacks:
                writestr(state, attacks.get("attack_text_u"))
            if "attack_text_npc" in attacks:
                writestr(state, attacks.get("attack_text_npc"))

    if "spawn_item" in item:
        writestr(state, item.get("spawn_item").get("message"))


def extract_mutation_category(state, item):
    item_name = found = item.get("name")
    if found is None:
        raise WrongJSONItem("JSON item don't contain 'name' field", item)
    writestr(state, found, comment="Mutation class name")

    simple_fields = [ "mutagen_message",
                      "iv_message",
                      "iv_sleep_message",
                      "iv_sound_message",
                      "junkie_message"
                    ]

    for f in simple_fields:
        found = item.get(f)
        # Need that check due format string argument
        if found is not None:
            writestr(state, found, comment=f"Mutation class: {item_name} {f}")

    found = item.get("memorial_message")
    writestr(state, found, context="memorial_male",
             comment=f"Mutation class: {item_name} Male memorial messsage")
    writestr(state, found, context="memorial_female",
             comment=f"Mutation class: {item_name} Female memorial messsage")


def extract_vehspawn(state, item):
    found = item.get("spawn_types")
    if not found:
        return

    for st in found:
        writestr(state, st.get("description"), comment="Vehicle Spawn Description")


def extract_recipe_category(state, item):
    cid = item.get("id", None)
    if cid:
        if cid == 'CC_NONCRAFT':
            return
        cat_name = cid.split("_")[1]
        writestr(state, cat_name, comment="Crafting recipes category name")
    else:
        raise WrongJSONItem("Recipe category must have unique id", item)

    found = item.get("recipe_subcategories", [])
    for subcat in found:
        if subcat == 'CSC_ALL':
            writestr(state, 'ALL', comment="Crafting recipes subcategory all")
            continue
        subcat_name = subcat.split('_')[2]
        writestr(state, subcat_name,
                 comment=f"Crafting recipes subcategory of '{cat_name}' category")


def extract_gate(state, item):
    messages = item.get("messages", {})

    for (k, v) in sorted(messages.items(), key=lambda x: x[0]):
        writestr(state, v,
                 comment=f"'{k}' action message of some gate object.")


def extract_field_type(state, item):
    for fd in item.get("intensity_levels"):
       if "name" in fd:
           writestr(state,fd.get("name"))


def extract_ter_furn_transform(state, item):
    writestr(state,item.get("fail_message"))
    if 'terrain' in item:
        for terrain in item.get("terrain"):
            writestr(state,terrain.get("message"))
    if 'furniture' in item:
        for furniture in item.get("furniture"):
            writestr(state,furniture.get("message"))


def extract_skill_display_type(state, item):
    writestr(state, item["display_string"], comment=f"Display string for skill display type '{item['id']}'")


def extract_fault(state, item):
    writestr(state, item["name"])
    writestr(state, item["description"], comment=f"Description for fault '{item['name']}'")
    for method in item["mending_methods"]:
        if "name" in method:
            writestr(state, method["name"], comment=f"Name of mending method for fault '{item['name']}'")
        if "description" in method:
            writestr(state, method["description"], comment=f"Description for mending method '{method['name']}' of fault '{item['name']}'")
        if "success_msg" in method:
            writestr(state, method["success_msg"], format_strings=True, comment=f"Success message for mending method '{method['name']}' of fault '{item['name']}'")


def extract_snippet(state, item):
    text = item["text"];
    if type(text) is not list:
        text = [text];
    for snip in text:
        if type(snip) is str:
            writestr(state, snip)
        else:
            writestr(state, snip["text"])


def extract_weapon_category(state, item):
    name = item["name"]
    comment = "weapon category name"
    writestr(state, name, comment=comment)


# these objects need to have their strings specially extracted
extract_specials = {
    "body_part": extract_bodypart,
    "clothing_mod": extract_clothing_mod,
    "construction": extract_construction,
    "effect_type": extract_effect_type,
    "fault": extract_fault,
    "field_type": extract_field_type,
    "gate": extract_gate,
    "GUN": extract_gun,
    "GUNMOD": extract_gunmod,
    "harvest" : extract_harvest,
    "mapgen": extract_mapgen,
    "martial_art": extract_martial_art,
    "material": extract_material,
    "mission_definition": extract_missiondef,
    "monster_attack": extract_monster_attack,
    "mutation_category": extract_mutation_category,
    "mutation": extract_mutation,
    "profession": extract_profession,
    "recipe_category": extract_recipe_category,
    "recipe_group": extract_recipe_group,
    "recipe": extract_recipe,
    "scenario": extract_scenario,
    "skill_display_type": extract_skill_display_type,
    "snippet": extract_snippet,
    "talk_topic": extract_talk_topic,
    "technique": extract_technique,
    "ter_furn_transform": extract_ter_furn_transform,
    "trap": extract_trap,
    "vehicle_spawn": extract_vehspawn,
    "weapon_category": extract_weapon_category,
}


def add_context(entry, ctxt):
    """
    Add translation context to entry.
    If string already has context, appends the new value to it.
    Parameter 'entry' can be either a raw string or a translation dict.
    """
    entry = deepcopy(entry)
    if type(entry) == dict:
        if "ctxt" in entry:
            entry["ctxt"] += f"|{ctxt}"
        else:
            entry["ctxt"] = ctxt
        return entry
    else:
        return {"str": entry, "ctxt": ctxt}


def writestr_basic(state, msgid, msgid_plural, msgctxt, comment, check_c_format):
    flags = []
    if check_c_format and ("%" in msgid or (msgid_plural is not None and "%" in msgid_plural)):
        flags.append('c-format')

    # Using None here because we neither know nor care about exact line numbers
    occurrences = [(state.current_source_file, None)]

    if comment:
        # Append `~ ` to help translators distinguish between comments
        comment = f"~ {comment}"

    entry = polib.POEntry(
        msgid=msgid,
        msgid_plural=msgid_plural,
        msgctxt=msgctxt,
        comment=comment,
        flags=flags,
        occurrences=occurrences
    )
    state.po_entries.append(entry)


def writestr(state, string, context=None, format_strings=False, comment=None, pl_fmt=False):
    "Write the string to POT."
    if type(string) is list:
        for entry in string:
            writestr(state, entry, context, format_strings, comment, pl_fmt)
        return
    elif type(string) is dict:
        if "//~" in string:
            if comment is None:
                comment = string["//~"]
            else:
                comment = f"{comment}\n{string['//~']}"
        context = string.get( "ctxt" )
        str_pl = None
        if pl_fmt:
            if "str_pl" in string:
                str_pl = string["str_pl"]
            elif "str_sp" in string:
                str_pl = string["str_sp"]
            else:
                # no "str_pl" entry in json, assuming regular plural form as in translations.cpp
                str_pl = f"{string['str']}s"
        elif "str_pl" in string or "str_sp" in string:
            raise WrongJSONItem("ERROR: 'str_pl' and 'str_sp' not supported here", string)
        if "str" in string:
            str_singular = string["str"]
        elif "str_sp" in string:
            str_singular = string["str_sp"]
        else:
            raise WrongJSONItem("ERROR: 'str' or 'str_sp' not found", string)
    elif type(string) is str:
        if len(string) == 0:
            # empty string has special meaning for gettext, skip it
            return
        str_singular = string
        if pl_fmt:
            # no "str_pl" entry in json, assuming regular plural form as in translations.cpp
            str_pl = f"{string}s"
        else:
            str_pl = None
    elif string is None:
        return;
    else:
        raise WrongJSONItem("ERROR: value is not a string, dict, list, or None", string)

    writestr_basic(state, str_singular, str_pl, context, comment, format_strings)


use_action_msgs = {
    "activate_msg",
    "deactive_msg",
    "out_of_power_msg",
    "msg",
    "menu_text",
    "message",
    "friendly_msg",
    "hostile_msg",
    "need_fire_msg",
    "need_charges_msg",
    "non_interactive_msg",
    "unfold_msg",
    "sound_msg",
    "no_deactivate_msg",
    "not_ready_msg",
    "success_message",
    "lacks_fuel_message",
    "failure_message",
    "descriptions",
    "use_message",
    "noise_message",
    "bury_question",
    "done_message",
    "voluntary_extinguish_message",
    "charges_extinguish_message",
    "water_extinguish_message",
    "auto_extinguish_message",
    "activation_message",
    "holster_msg",
    "holster_prompt",
    "verb",
    "gerund"
}


def extract_use_action_msgs(state, use_action, it_name):
    """Extract messages for iuse_actor objects. """
    for f in sorted(use_action_msgs):
        if type(use_action) is dict and f in use_action:
            if it_name:
                writestr(state, use_action[f],
                  comment=f"Use action {f} for {it_name}.")
    # Recursively check sub objects as they may contain more messages.
    if type(use_action) is list:
        for i in use_action:
            extract_use_action_msgs(state, i, it_name)
    elif type(use_action) is dict:
        for (k, v) in sorted(use_action.items(), key=lambda x: x[0]):
            extract_use_action_msgs(state, v, it_name)

found_types = set();
known_types = ignorable | extract_specials.keys() | automatically_convertible


def extract_json(state, item):
    """Find any extractable strings in the given json object,
    and write them to the PO file provided by the state."""
    if not "type" in item:
        return
    object_type = item["type"]
    found_types.add(object_type)
    if object_type in ignorable:
        return
    elif object_type in extract_specials:
        extract_specials[object_type](state, item)
        return
    elif object_type not in automatically_convertible:
        raise WrongJSONItem(f"ERROR: Unrecognized object type '{object_type}'!", item)
    if object_type not in known_types:
        print(f"WARNING: known_types does not contain object type '{object_type}'")
    # Use mod id as project name if project name is not specified
    if object_type == "MOD_INFO" and not state.project_name:
        state.project_name = item.get("id")
    wrote = False
    name = item.get("name") # Used in gettext comments below.
    # Don't extract any record with name = "none".
    if name and name == "none":
        return
    if name:
        if object_type in needs_plural:
            writestr(state, name, pl_fmt=True)
        else:
            writestr(state, name)
        wrote = True
    if "name_suffix" in item:
        writestr(state, item["name_suffix"])
        wrote = True
    if "name_unique" in item:
        writestr(state, item["name_unique"])
        wrote = True
    if "job_description" in item:
        writestr(state, item["job_description"])
        wrote = True
    if "use_action" in item:
        extract_use_action_msgs(state, item["use_action"], item.get("name"))
        wrote = True
    if "conditional_names" in item:
        for cname in item["conditional_names"]:
            c = f"Conditional name for {name} when {cname['type']} matches {cname['condition']}"
            writestr(state, cname["name"], comment=c, format_strings=True, pl_fmt=True)
            wrote = True
    if "description" in item:
        if name:
            c = f"Description for {name}"
        else:
            c = None
        writestr(state, item["description"], comment=c)
        wrote = True
    if "detailed_definition" in item:
        writestr(state, item["detailed_definition"])
        wrote = True
    if "sound" in item:
        writestr(state, item["sound"])
        wrote = True
    if "sound_description" in item:
        writestr(state, item["sound_description"], comment=f"Description for the sound of spell '{name}'")
        wrote = True
    if "snippet_category" in item and type(item["snippet_category"]) is list:
        # snippet_category is either a simple string (the category ident)
        # which is not translated, or an array of snippet texts.
        for entry in item["snippet_category"]:
            # Each entry is a json-object with an id and text
            if type(entry) is dict:
                writestr(state, entry["text"])
                wrote = True
            else:
                # or a simple string
                writestr(state, entry)
                wrote = True
    if "bash" in item and type(item["bash"]) is dict:
        # entries of type technique have a bash member, too.
        # but it's a int, not an object.
        bash = item["bash"]
        if "sound" in bash:
            writestr(state, bash["sound"])
            wrote = True
        if "sound_fail" in bash:
            writestr(state, bash["sound_fail"])
            wrote = True
    if "oxytorch" in item and "message" in item["oxytorch"]:
        c = f"message when oxytorch cutting {name}"
        writestr(state, item["oxytorch"]["message"], comment=c)
        wrote = True
    if "hacksaw" in item:
        hacksaw = item["hacksaw"]
        if "sound" in hacksaw:
            c = f"sound of sawing {name}"
            writestr(state, hacksaw["sound"], comment=c)
            wrote = True
        if "message" in hacksaw:
            c = f"message when finished sawing {name}"
            writestr(state, hacksaw["message"], comment=c)
            wrote = True
    if "boltcut" in item:
        boltcut = item["boltcut"]
        if "sound" in boltcut:
            c = f"sound of bolt cutting {name}"
            writestr(state, boltcut["sound"], comment=c)
            wrote = True
        if "message" in boltcut:
            c = f"message when finished bolt cutting {name}"
            writestr(state, boltcut["message"], comment=c)
            wrote = True
    if "pry" in item:
        pry = item["pry"]
        if "sound" in pry:
            writestr(state, pry["sound"])
            wrote = True
        if "break_sound" in pry:
            writestr(state, pry["break_sound"])
            wrote = True
        if "success_message" in pry:
            writestr(state, pry["success_message"])
            wrote = True
        if "fail_message" in pry:
            writestr(state, pry["fail_message"])
            wrote = True
        if "break_message" in pry:
            writestr(state, pry["break_message"])
            wrote = True
    if "lockpick_message" in item:
        writestr(state, item["lockpick_message"])
        wrote = True
    if "seed_data" in item:
        seed_data = item["seed_data"]
        writestr(state, seed_data["plant_name"])
        wrote = True
    if "relic_data" in item:
        relic_data = item["relic_data"]
        if "name" in relic_data:
            writestr(state, relic_data["name"])
            wrote = True
        if "recharge_scheme" in relic_data:
            for rech in relic_data["recharge_scheme"]:
                if "message" in rech:
                    writestr(state, rech["message"],
                      comment=f"Relic recharge message for {object_type} '{name}'"
                    )
                    wrote = True
    if "text" in item:
        writestr(state, item["text"])
        wrote = True
    if "prompt" in item:
        writestr(state, item["prompt"])
        wrote = True
    if "message" in item:
        writestr(state, item["message"], format_strings=True,
                 comment=f"Message for {object_type} '{name}'" )
        wrote = True
    if "messages" in item:
        for message in item["messages"]:
            writestr(state, message)
            wrote = True
    if "valid_mod_locations" in item:
        for mod_loc in item["valid_mod_locations"]:
            writestr(state, mod_loc[0])
            wrote = True
    if "info" in item:
       c = "Please leave anything in <angle brackets> unchanged."
       writestr(state, item["info"], comment=c)
       wrote = True
    if "verb" in item:
       writestr(state, item["verb"])
       wrote = True
    if "special_attacks" in item:
        special_attacks = item["special_attacks"]
        for special_attack in special_attacks:
            if "description" in special_attack:
                writestr(state, special_attack["description"])
                wrote = True
            if "monster_message" in special_attack:
                writestr(state, special_attack["monster_message"], format_strings=True,
                         comment="Attack message of monster \"{}\"'s spell \"{}\""
                         .format(name, special_attack.get("spell_id")))
                wrote = True
    if "footsteps" in item:
       writestr(state, item["footsteps"])
       wrote = True
    if not wrote and not "copy-from" in item:
        if not warning_supressed(state.current_source_file):
            print(f"WARNING: {state.current_source_file}: nothing translatable found in item: {item}")


def assert_num_args(node, args, n):
    if len(args) != n:
        raise Exception(f"invalid amount of arguments in translation call (found {len(args)}, expected {n}). Error source:   {ast.to_lua_source(node)}")


def get_string_literal(node, args, pos):
    if isinstance(args[pos], astnodes.String):
        return args[pos].s.decode('utf-8')
    else:
        raise Exception(f"argument to translation call should be string. Error source:   {ast.to_lua_source(node)}")


class LuaComment:
    def __init__(self, source, comment_node):
        first_char = comment_node.first_token.start
        last_char = comment_node.first_token.stop
        text = source[first_char:last_char+1]
        line = comment_node.first_token.line
        source = comment_node.first_token.source

        self.is_trans_comment = self.__check_is_trans_comment(text)
        self.text = self.__strip_comment(text)
        self.line = line
        self.source = source
        self.used = False

    def mark_used(self):
        self.used = True

    def __strip_comment(self, text):
        if text.startswith("--[[") and text.endswith("]]"):
            # Regular multiline comment
            return text[4:len(text)-2].strip()
        elif text.startswith("--~"):
            # Translation comment
            return text[3:].strip()
        elif text.startswith("--"):
            # Regular comment
            return text[2:].strip()
        else:
            return text

    def __check_is_trans_comment(self, text):
        return text.startswith("--~")


class LuaCallVisitor(ast.ASTVisitor):
    def register_state(self, state):
        self.state = state

    def load_comments(self, comments):
        self.trans_comments = []
        self.regular_comments = []
        for comment in comments:
            if comment.is_trans_comment:
                self.trans_comments.append(comment)
            else:
                self.regular_comments.append(comment)

    def report_unused_comments(self):
        for comment in self.trans_comments:
            if not comment.used:
                print(f"WARNING: unused translator comment at {self.state.current_source_file}:{comment.line}")

    def __find_trans_comments_before(self, line):
        for comment in self.trans_comments:
            if comment.line == line - 1:
                comment.mark_used()
                ret = self.__find_trans_comments_before( line - 1 )
                ret.append(comment.text)
                return ret
        return []

    def __find_regular_comment_before(self, line):
        for comment in self.regular_comments:
            if comment.line == line - 1:
                return comment
        return None

    def __find_comment(self, line):
        if line is None:
            return None
        comments = self.__find_trans_comments_before(line)
        if len(comments) != 0:
            return '\n'.join(comments)
        else:
            comment = self.__find_regular_comment_before(line)
            if comment:
                print(f"WARNING: regular comment used when translation comment may be intended. Error source: {self.state.current_source_file}:{comment.line}")
            return None

    def visit_Call(self, node):
        try:
            found = False
            if isinstance(node.func, astnodes.Name):
                func_id = node.func.id
                first_token = node.func.first_token
                func_line = first_token.line if first_token else None
                func_args = node.args
                found = True
            elif isinstance(node.func, astnodes.Index):
                if isinstance(node.func.idx, astnodes.Name):
                    func_id = node.func.idx.id
                    first_token = node.func.idx.first_token
                    func_line = first_token.line if first_token else None
                    func_args = node.args
                    found = True
            if not found:
                return
        except Exception as E:
            print(f"WARNING: {E}")
            return
        write = False
        msgctxt = None
        msgid = None
        msgid_plural = None
        try:
            if func_id == "gettext":
                assert_num_args(node, func_args, 1)
                msgid = get_string_literal(node, func_args, 0)
                write = True
            elif func_id == "pgettext":
                assert_num_args(node, func_args, 2)
                msgctxt = get_string_literal(node, func_args, 0)
                msgid = get_string_literal(node, func_args, 1)
                write = True
            elif func_id == "vgettext":
                assert_num_args(node, func_args, 3)
                msgid = get_string_literal(node, func_args, 0)
                msgid_plural = get_string_literal(node, func_args, 1)
                write = True
            elif func_id == "vpgettext":
                assert_num_args(node, func_args, 4)
                msgctxt = get_string_literal(node, func_args, 0)
                msgid = get_string_literal(node, func_args, 1)
                msgid_plural = get_string_literal(node, func_args, 2)
                write = True
        except Exception as E:
            print(f"WARNING: {E}")
        if write:
            comment = self.__find_comment(func_line)
            writestr_basic(self.state, msgid, msgid_plural, msgctxt, comment, check_c_format = True)


# https://github.com/boolangery/py-lua-parser/pull/62
from luaparser.builder import BuilderVisitor  # noqa: E402
from luaparser.parser.LuaLexer import LuaLexer  # noqa: E402
from luaparser.parser.LuaParser import LuaParser  # noqa: E402
from luaparser.ast import SyntaxException  # noqa: E402
from antlr4.error.ErrorListener import ConsoleErrorListener  # noqa: E402
from antlr4 import InputStream, CommonTokenStream, Token  # noqa: E402

# workaround till 3.3.1 is released, see:
# https://github.com/boolangery/py-lua-parser/issues/71
# https://github.com/boolangery/py-lua-parser/pull/62
def parse(source: str) -> astnodes.Chunk:
    """Parse Lua source to a Chunk."""
    lexer = LuaLexer(InputStream(source))
    lexer.removeErrorListeners()
    lexer.addErrorListener(ConsoleErrorListener())
    token_stream = CommonTokenStream(lexer, channel=Token.DEFAULT_CHANNEL)
    parser = LuaParser(token_stream)
    parser.addErrorListener(ConsoleErrorListener())
    tree = parser.start_()
    if parser.getNumberOfSyntaxErrors() > 0:
        raise SyntaxException("syntax errors")
    else:
        v = BuilderVisitor(token_stream)
        val = v.visit(tree)
        return val

def extract_lua(state, source):
    """Find any extractable strings in the given Lua source code,
    and write them to the PO file provided by the state."""

    tree = parse(source)

    #print(ast.to_pretty_str(tree))

    comments = []

    for node in ast.walk(tree):
        for comment_node in node.comments:
            comments.append(LuaComment( source, comment_node ))

    for comment in comments:
        if comment.is_trans_comment:
            print(f"Line {comment.line}: {comment.text}")

    visitor = LuaCallVisitor()
    visitor.register_state(state)
    visitor.load_comments(comments)
    visitor.visit(tree)
    visitor.report_unused_comments()


def log_verbose(msg):
    if options.verbose:
        print(msg)


def extract_all_from_dir(state, dir):
    """Extract strings from every applicable file in the specified directory,
    recursing into any subdirectories."""
    allfiles = os.listdir(dir)
    allfiles.sort()
    dirs = []
    skiplist = [ os.path.normpath(".gitkeep") ]
    for f in allfiles:
        full_name = os.path.join(dir, f)
        if os.path.isdir(full_name):
            if os.path.normpath(full_name) in ignore_dirs:
                log_verbose(f"Skipping dir (ignored): {f}")
            else:
                dirs.append(f)
        elif f in skiplist:
            log_verbose(f"Skipping file (skiplist): '{f}'")
        elif full_name in ignore_files:
            log_verbose(f"Skipping file (ignored): '{f}'")
        elif f.endswith(".json"):
            if not options.tracked_only or full_name in git_files_list:
                extract_all_from_json_file(state, full_name)
            else:
                log_verbose(f"Skipping file (untracked): '{full_name}'")
        elif f.endswith(".lua"):
            if not options.tracked_only or full_name in git_files_list:
                extract_all_from_lua_file(state, full_name)
            else:
                log_verbose(f"Skipping file (untracked): '{full_name}'")
        else:
            log_verbose(f"Skipping file (not applicable): '{f}'")
    for d in dirs:
        extract_all_from_dir(state, os.path.join(dir, d))


def extract_all_from_json_file(state, json_file):
    "Extract translatable strings from every object in the specified JSON file."
    state.current_source_file = json_file
    log_verbose(f"Loading {json_file}")

    with open(json_file, encoding="utf-8") as fp:
        jsondata = json.load(fp)
    # it's either an array of objects, or a single object
    try:
        if hasattr(jsondata, "keys"):
            extract_json(state, jsondata)
        else:
            for jsonobject in jsondata:
                extract_json(state, jsonobject)
    except WrongJSONItem as E:
        print(f"---\nFile: '{json_file}'")
        print(E)
        exit(1)


def extract_all_from_lua_file(state, lua_file):
    "Extract translatable strings from lua code in the specified file."
    state.current_source_file = lua_file
    log_verbose(f"Loading {lua_file}")

    with open(lua_file, encoding="utf-8") as fp:
        luadata_raw = fp.read()

    extract_lua(state, luadata_raw)

def prepare_git_file_list():
    command_str = "git ls-files"
    res = None;
    if platform == "win32":
        res = subprocess.Popen(command_str, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    else:
        res = subprocess.Popen(command_str, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, close_fds=True)
    output = res.stdout.readlines()
    res.communicate()
    if res.returncode != 0:
        print(f"'git ls-files' command exited with non-zero exit code: {res.returncode}")
        exit(1)
    for f in output:
        if len(f) > 0:
            git_files_list.add(os.path.normpath(f[:-1].decode('utf8')))


def write_pot(entries, output_path, project_name):
    pot = polib.POFile()
    pot.metadata = {
        'Project-Id-Version': project_name,
        'POT-Creation-Date': time.strftime('%Y-%m-%d %H:%M%z'),
        'Language': '',
        'MIME-Version': '1.0',
        'Content-Type': 'text/plain; charset=UTF-8',
        'Content-Transfer-Encoding': '8bit',
    }
    for entry in entries:
        # Entries with plural forms use 1+ `msgstr[x]` fields instead of single `msgstr`.
        # We have to explicitly specify which one we want,
        # as polib by default always writes `msgstr`.
        if entry.msgid_plural:
            entry.msgstr = None
            entry.msgstr_plural = {0:'', 1:''}
        else:
            entry.msgstr = ''
            entry.msgstr_plural = None
        pot.append(entry)
    pot.save(output_path, newline='\n')

##
##  PREPARATION
##

directories = {os.path.normpath(i) for i in options.input_folders}
output_pot_file_name = os.path.normpath(options.output_file)

##
##  EXTRACTION
##

if options.tracked_only:
    print("==> Generating the list of all Git tracked files")
    prepare_git_file_list()

state = ExtractorState()
state.project_name = options.project_name

print("==> Parsing JSON")
for i in sorted(directories):
    print(f"----> Traversing directory {i}")
    extract_all_from_dir(state, i)

if options.warn_unused_types:
    print("==> Checking types")
    if len(known_types - found_types) != 0:
        print(f"WARNING: type {known_types - found_types} not found in any JSON objects")
    if len(needs_plural - found_types) != 0:
        print(f"WARNING: type {needs_plural - found_types} from needs_plural not found in any JSON objects")

print("==> Writing POT")
if not state.project_name:
    state.project_name = "Unknown Mod"
write_pot(state.po_entries, output_pot_file_name, state.project_name)

# done.
