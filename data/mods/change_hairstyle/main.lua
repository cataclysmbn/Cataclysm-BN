local mod = game.mod_runtime[game.current_mod]

mod.change_hairstyle_function = function(params)
    local who = params.user

    local function get_hair_trait_type(mut_raw)
        if not mut_raw then return nil end
        
        local success, types = pcall(function() return mut_raw:mutation_types() end)
        if success and types then
            for k, v in pairs(types) do
                local t = (type(v) == "string") and v or k
                if t == "hair_style" then return "hair_style" end
                if t == "hair_color" then return "hair_color" end
            end
        end

        local id_str = mut_raw.id:str()
        if id_str:find("style") then return "hair_style" end
        if id_str:find("color") then return "hair_color" end
        
        return nil
    end

    local main_menu = UiList.new()
    main_menu:title(locale.gettext("Appearance Customization"))
    main_menu:add(1, locale.gettext("Change Hair Style"))
    main_menu:add(2, locale.gettext("Change Hair Color"))
    
    local main_choice = main_menu:query()
    if main_choice < 0 then return 0 end

    local target_type = (main_choice == 1) and "hair_style" or "hair_color"

    local current_trait_id = nil
    local current_muts = who:get_mutations(true)
    for _, mut_id in pairs(current_muts) do
        if get_hair_trait_type(mut_id:obj()) == target_type then
            current_trait_id = mut_id
            break
        end
    end

    local options = {}
    local all_muts_raw = MutationBranchRaw.get_all()
    for _, mut_data in pairs(all_muts_raw) do
        if get_hair_trait_type(mut_data) == target_type then
            table.insert(options, {
                id = mut_data.id,
                name = mut_data:name()
            })
        end
    end

    if #options == 0 then
        gapi.add_msg(MsgType.warning, locale.gettext("No available traits found."))
        return 0
    end

    table.sort(options, function(a, b) return a.name < b.name end)

    local ui = UiList.new()
    ui:title(target_type == "hair_style" and locale.gettext("Select Style:") or locale.gettext("Select Color:"))
    for i, opt in ipairs(options) do
        ui:add(i, opt.name)
    end

    local choice = ui:query()
    if choice > 0 then
        local selected = options[choice]

        if current_trait_id and current_trait_id:str() == selected.id:str() then
            gapi.add_msg(locale.gettext("You already have this style/color."))
            return 0
        end

        if current_trait_id then
            who:remove_mutation(current_trait_id, true)
        end

        who:set_mutation(selected.id)
        gapi.add_msg(MsgType.good, locale.gettext("Success! Your appearance has been updated."))
        
        who:assign_activity(ActivityTypeId.new("ACT_HAIRCUT"), 18000, -1, -1, "")
        return 1
    end

    return 0
end