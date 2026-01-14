# Lua スクリプティングクックブック

Lua API に慣れて、使い方を学ぶのに役立つコードスニペットです。
これらのサンプルをテストするには、バッククォート(\`\`)キーを押してゲーム内Luaコンソールにコードを貼り付けてください。

## アイテム

### インベントリ内の全ての装備および着用アイテムのリストを取得する

```lua
local you = gapi.get_avatar()
local items = you:all_items(false)

for _, item in pairs(items) do
  local status = ""
  if you:is_wielding(item) then
    status = "装備中: "
  elseif you:is_wearing(item) then
    status = "着用中: "
  end
  print(status .. item:tname(1, false, 0))
end
```

<details>
<summary>出力例</summary>

```
装備中: スマートフォン
着用中: ブラジャー
着用中: パンティー
着用中: ソックス
着用中: ジーンズ
着用中: 長袖シャツ
着用中: スニーカー
着用中: メッセンジャーバッグ
着用中: 腕時計
ポケットナイフ
マッチ本
きれいな水（プラスチック製ボトル）
きれいな水
```

</details>

## モンスター

### プレイヤーの近くに犬をスポーンする

```lua
local avatar = gapi.get_avatar()
local coords = avatar:get_pos_ms()
local dog_mtype = MtypeId.new("mon_dog_bcollie")
local doggy = gapi.place_monster_around(dog_mtype, coords, 5)
if doggy == nil then
    gdebug.log_info("犬をスポーンできませんでした :(")
else
    gdebug.log_info(string.format("犬を %s にスポーンしました", doggy:get_pos_ms()))
end
```

## 戦闘

### 格闘技が使用された時に詳細情報を表示する

まずは関数を定義します。

```lua
on_creature_performed_technique = function(params)
  local char = params.char
  local technique = params.technique
  local target = params.target
  local damage_instance = params.damage_instance
  local move_cost = params.move_cost
  gdebug.log_info(
          string.format(
                  "%s が %s を %s に対して使用しました (DI: %s , MC: %s)",
                  char:get_name(),
                  technique.name,
                  target:get_name(),
                  damage_instance:total_damage(),
                  move_cost
          )
  )
end
```

その後、フックを関数に一度だけ接続します。

```lua
table.insert(game.hooks.on_creature_performed_technique, function(...) return on_creature_performed_technique(...) end)
```

<details>
<summary>出力例</summary>

```
Ramiro Waters が Power Hit を zombie に対して使用しました (DI: 27.96 , MC: 58)
```

</details>

## アイテムの耐久性

### アイテムのダメージをチェックして修正する

```lua
local you = gapi.get_avatar()
local wielding = you:all_items(false)[1]
print(wielding:get_damage())
print(wielding:get_damage_level(4))
wielding:mod_damage(2000)
print(wielding:get_damage_level(4))
```

## モンスター

### モンスターのインベントリにアイテムを追加する

```lua
local target_monster = -- [[ あなたのモンスター参照 ]]
local scraps = gapi.create_item(ItypeId.new("scrap"), 3)
target_monster:as_monster():add_item(scraps)
```

## 天気フック

### 天気の変化に反応する

まず、preload.lua でフックをセットアップします:

```lua
local mod = game.mod_runtime[game.current_mod]
table.insert(game.hooks.on_weather_changed, function(...) mod.weather_changed_alert(...) end)
table.insert(game.hooks.on_weather_updated, function(...) mod.weather_report(...) end)
```

次に、main.lua でハンドラーを定義します:

```lua
local mod = game.mod_runtime[game.current_mod]

-- 天気が変わる時に呼び出される(例: 晴れ -> 雨)
mod.weather_changed_alert = function(params)
    local msg = string.format(
        "天気が %s から %s に変わりました!",
        params.old_weather_id,
        params.weather_id
    )
    gdebug.log_info(msg)
end

-- 5分ごとに現在の天気データと共に呼び出される
mod.weather_report = function(params)
    local msg = string.format(
        "現在の天気: %s, 温度: %.1f°C, 風: %d, 湿度: %d%%",
        params.weather_id,
        params.temperature,
        params.windspeed,
        params.humidity
    )
    gdebug.log_info(msg)
end
```

## 遠距離戦闘

### 発砲と投げたアイテムに反応する

まず、preload.lua でフックをセットアップします:

```lua
local mod = game.mod_runtime[game.current_mod]
table.insert(game.hooks.on_shoot, function(...) return mod.on_shoot_fun(...) end)
table.insert(game.hooks.on_throw, function(...) return mod.on_throw_fun(...) end)
```

次に、main.lua でハンドラーを定義します:

```lua
local mod = game.mod_runtime[game.current_mod]

mod.on_shoot_fun = function(params)
    ---@type Item
    local gun = params.gun
    ---@type Item
    local ammo_item = params.ammo
    local ammo = ItypeId.NULL_ID()
    if not ammo_item then
        ammo = gun:ammo_current()
    else
        ammo = ammo_item:get_type()
    end
    local shoot_noise = ammo:obj():slot_ammo().loudness
    gdebug.log_info(string.format("銃音: %d.", shoot_noise))
end

mod.on_throw_fun = function(params)
    ---@type Character
    local thrower = params.thrower
    ---@type Item
    local thrown = params.item
    if thrown:is_gun() then
        gdebug.log_info("おい！銃は投げるものではないぞ!")
    end
end
```

## オーバーマップクエリ

### オーバーマップ上のアイテムを検索して操作する

```lua
-- 特定の場所のオーバーマップ上の全てのアイテムを検索
local om_pos = OmPos.new(0, 0, 0)
local items = gapi.overmap_find_items_around(om_pos, 0)

-- マップからアイテムを取得して、マップがアンロードされても Lua に保持
local map_pos = MapPos.new(100, 100, 0)
local item = gapi.get_map():find_item_at(map_pos)
if item then
    local detached = gapi.create_detached_item(item)
    -- 後で位置に再度アタッチできます
    local reattached = gapi.reattach_item(detached, map_pos)
end

-- 同じマップ内でアイテムを移動
local source_pos = MapPos.new(100, 100, 0)
local dest_pos = MapPos.new(110, 110, 0)
gapi.get_map():move_item_at(source_pos, dest_pos)
```

## 死亡フック

### モンスターの死亡をトラッキング

```lua
-- preload.lua 内
local mod = game.mod_runtime[game.current_mod]
table.insert(game.hooks.on_mon_death, function(...) return mod.on_mon_death(...) end)
```

```lua
-- main.lua 内
local mod = game.mod_runtime[game.current_mod]

mod.on_mon_death = function(params)
    ---@type Creature
    local monster = params.creature
    ---@type Character|nil
    local killer = params.killer

    local killer_name = killer and killer:get_name() or "不明"
    gdebug.log_info(string.format("%s は %s に殺されました", monster:get_name(), killer_name))
end
```

### キャラクターの死亡をトラッキング

```lua
-- preload.lua 内
local mod = game.mod_runtime[game.current_mod]
table.insert(game.hooks.on_char_death, function(...) return mod.on_char_death(...) end)
```

```lua
-- main.lua 内
local mod = game.mod_runtime[game.current_mod]

mod.on_char_death = function(params)
    ---@type Character
    local char = params.char
    ---@type Character|nil
    local killer = params.killer

    if char == gapi.get_avatar() then
        gdebug.log_info("あなたは死にました！")
    end
end
```

## キャラクターのコンバットスタッツ

### 攻撃とスタミナのコストを取得する

```lua
local you = gapi.get_avatar()
local items = you:all_items(false)

for _, item in pairs(items) do
    print(
        item:tname(1, false, 0) 
        .. " { 攻撃コスト: " .. item:attack_cost() 
        .. ", スタミナコスト: " .. item:stamina_cost()
        .. ", 近接スタミナコスト: " .. you:get_melee_stamina_cost(item)
        .. " }"
    )
end

-- 特殊能力をチェック
print("Uncanny dodge: " .. (you:uncanny_dodge() and "はい" or "いいえ"))
```

## ダイナミックアイテムアクション

### Lua でカスタムアイテム使用関数を作成する

```lua
-- tick と can_use 関数でアイテムの使用動作を定義
game.iuse_functions["my_custom_item"] = {
    use = function(params)
        local user = params.user
        local item = params.item
        gdebug.log_info("使用中: " .. item:tname(1))
        return 0  -- 移動単位での時間コストを返す
    end,

    can_use = function(params)
        local user = params.user
        local item = params.item
        -- 使用を許可する場合は true、禁止する場合は false を返す
        return true
    end,

    tick = function(params)
        local user = params.user
        local item = params.item
        -- アイテムがアクティブ状態の間、定期的に呼び出される
        if item:get_countdown() == 0 then
            gdebug.log_info("アイテムのカウントダウンが完了しました!")
        end
    end
}

-- 周期的なティックをトリガーするためにアイテムにカウントダウンを設定
local item = gapi.create_item(ItypeId.new("some_item"), 1)
item:set_countdown(100)  -- 100ターンティック
```

## より多くのコンバットフック

### 回避、ブロック、および技のイベントに反応する

```lua
-- preload.lua 内
local mod = game.mod_runtime[game.current_mod]
table.insert(game.hooks.on_creature_dodged, function(...) return mod.on_creature_dodged(...) end)
table.insert(game.hooks.on_creature_blocked, function(...) return mod.on_creature_blocked(...) end)
table.insert(game.hooks.on_creature_performed_technique, function(...) return mod.on_creature_performed_technique(...) end)
table.insert(game.hooks.on_creature_melee_attacked, function(...) return mod.on_creature_melee_attacked(...) end)
```

```lua
-- main.lua 内
local mod = game.mod_runtime[game.current_mod]

mod.on_creature_dodged = function(params)
    ---@type Character
    local char = params.char
    ---@type Creature
    local source = params.source
    local difficulty = params.difficulty
    gdebug.log_info(string.format("%s は %s を回避しました (DC: %d)", char:get_name(), source:get_name(), difficulty))
end

mod.on_creature_blocked = function(params)
    ---@type Character
    local char = params.char
    ---@type Creature
    local source = params.source
    local bodypart_id = params.bodypart_id
    local damage_blocked = params.damage_blocked
    gdebug.log_info(string.format(
        "%s は %s を %s でブロックしました (ブロック: %.1f ダメージ)",
        char:get_name(),
        source:get_name(),
        bodypart_id,
        damage_blocked
    ))
end

mod.on_creature_melee_attacked = function(params)
    ---@type Character
    local char = params.char
    ---@type Creature
    local target = params.target
    if params.success then
        gdebug.log_info(string.format("%s は %s にヒットしました", char:get_name(), target:get_name()))
    else
        gdebug.log_info(string.format("%s は %s をミスしました", char:get_name(), target:get_name()))
    end
end
```

## アイテムタイプ情報

### ItypeId 経由のアイテムタイププロパティのクエリ

```lua
local item_type = ItypeId.new("9mm")

-- アイテムタイプオブジェクト(ItypeRaw)を取得
local itype_raw = item_type:obj()

-- アイテムタイプ固有のデータにアクセス(例: 弾薬)
if itype_raw:slot_ammo() then
    local ammo_data = itype_raw:slot_ammo()
    print("弾薬ダメージ: " .. ammo_data.damage)
    print("弾薬射程: " .. ammo_data.range)
end

-- コンテナの場合
if itype_raw:slot_container() then
    local container_data = itype_raw:slot_container()
    print("容量: " .. container_data.capacity)
end

-- ツールの場合
if itype_raw:slot_tool() then
    local tool_data = itype_raw:slot_tool()
    print("ツール品質: " .. tool_data.quality)
end
```
