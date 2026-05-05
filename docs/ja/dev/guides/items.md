# アイテムと弾薬のクックブック

ここでは、アイテムを扱うときによく使う作業と、アイテム、弾薬、マガジン、チャージの
処理を一貫させるための規則を説明します。アイテムのゲームオブジェクトについては
[ゲームオブジェクト](../explanation/game_objects.md)を参照してください。JSON のアイテム
フィールドについては、Mod 用リファレンスの
[アイテム作成](../../mod/json/reference/items/item_creation.md)、
[アイテム生成](../../mod/json/reference/items/item_spawn.md)、
[レシピ](../../mod/json/reference/items/recipes.md)を参照してください。

## 基本モデル

`item` は複数の概念を表すことがあります。

- ドライバーやバックパックのような通常の物体。
- 弾薬、食料の一部、液体、部品のような charge で数えるスタック。
- 弾薬やエネルギーを保持している銃や道具。
- 弾薬を中身の弾薬アイテムとして保持するマガジン。
- 他のアイテムを入れているコンテナ。

`item::charges` が常に同じ意味だと仮定しないでください。アイテムの種類によって、
charges はスタック数、道具のエネルギー、残弾数、または意味のない値になります。
下にある高水準のアイテム API を優先して使ってください。

## 適切な API を選ぶ

| 作業                                           | 推奨 API                                            | 避けるもの                                 |
| ---------------------------------------------- | --------------------------------------------------- | ------------------------------------------ |
| 装填中の弾薬タイプを調べる                     | `item::ammo_current()`                              | `curammo` を直接読む                       |
| 装填量を調べる                                 | `item::ammo_remaining()`                            | `charges` を直接読む                       |
| 容量を調べる                                   | `item::ammo_capacity()`                             | JSON フィールドから再計算する              |
| 銃/道具/マガジンへ装填する                     | `item::ammo_set()`                                  | `charges` と弾薬フィールドを直接設定する   |
| 銃/道具/マガジンを空にする                     | `item::ammo_unset()` または `recover_stored_ammo()` | `charges` だけを消す                       |
| 装填済みアイテムから弾薬を消費する             | `item::ammo_consume()`                              | `charges` から直接引く                     |
| インベントリ/マップの charges を消費する       | `Character::use_charges()`, `map::use_charges()`    | インベントリを手動で走査する               |
| 個数指定の部品アイテムを消費する               | `Character::use_amount()`, `map::use_amount()`      | 手動の detach ループを書く                 |
| 保持された弾薬をインベントリアイテムへ変換する | `stored_ammo.h` のヘルパー                          | `item::spawn()` と場当たり的な charge 計算 |

直接のフィールドアクセスは、不変条件がその場で明確な低水準のアイテム実装コードだけで
使ってください。

## ある tripoint から別の tripoint へアイテムを移動する

```cpp
auto move_item( map &here, const tripoint &src, const tripoint &dest ) -> void
{
    auto items_src = here.i_at( src );
    auto items_dest = here.i_at( dest );

    items_src.move_all_to( &items_dest );
}
```

## 弾薬や charges つきのアイテムを生成する

通常の count-by-charges アイテムであれば、charges を指定して生成してかまいません。

```cpp
auto nails = item::spawn( "nail", calendar::turn, 50 );
```

銃、道具、マガジンでは、生成後に `ammo_set()` を使うことを優先してください。この関数は、
対象が内蔵弾薬ストレージを使うのか、中身のマガジン/弾薬アイテムを使うのかを把握しています。

```cpp
auto cell = item::spawn( "medium_battery_cell" );
cell->ammo_set( itype_id( "battery" ), 500 );
```

アイテム内部の実装で関連する弾薬の不変条件をすべて同時に保つ場合を除き、`charges` を
直接書き換えて装填済み弾薬を初期化しないでください。

## 銃、道具、マガジン

装填済みアイテムには同じ公開クエリを使ってください。

```cpp
auto describe_loaded_ammo( const item &it ) -> std::string
{
    if( it.ammo_current().is_null() || it.ammo_remaining() <= 0 ) {
        return "empty";
    }
    return string_format( "%s: %d", it.ammo_current().str(), it.ammo_remaining() );
}
```

保存方法はアイテムの種類ごとに異なります。

- 内蔵式の銃/道具は、多くの場合、弾薬量を `charges` に保存し、弾薬タイプは別に保存します。
- 着脱式マガジンを使う銃/道具は、弾薬の問い合わせをマガジンへ転送します。
- マガジンは弾薬を中身の弾薬アイテムとして保存します。

そのため、呼び出し側は保存レイアウトを調べず、`ammo_current()`、`ammo_remaining()`、
`ammo_set()`、`ammo_unset()`、`ammo_consume()` を使うべきです。

## アイテム内に保持された弾薬や charges を回収する

一部のアイテムは弾薬を抽象的な charges として保持しますが、インベントリに現れる物理
アイテムは別の単位を使うことがあります。重要な特殊例はプルトニウム燃料です。道具と車両
部品はプルトニウムを `PLUTONIUM_CHARGES` 単位で保存しますが、インベントリの燃料電池は
完全なアイテムです。部分的なプルトニウム charges を生の `item::spawn` と手動の `charges`
代入で変換してはいけません。0-charge のゴーストアイテムを作る可能性があります。

銃、道具、マガジン、車両燃料ストアの保持弾薬を物理アイテムへ変換するコードでは、
`stored_ammo.h` を使ってください。

```cpp
for( auto &ammo : recover_stored_ammo( source_item, stored_ammo_remainder_handling::discard ) ) {
    drop_or_handle( std::move( ammo ), who );
}
```

余りの扱いを明示的に選んでください。

- `stored_ammo_remainder_handling::discard`: クラフト、分解、解体のように元アイテムが破壊される
  ときに使います。完全な弾薬アイテムを回収し、アイテムとして存在できない余りを消します。
- `stored_ammo_remainder_handling::preserve`: 車両から完全な燃料アイテムだけを取り出し、部分燃料を
  部品に残す場合のように、元のものが世界に残るときに使います。

物理表現を調べる、または生成するだけのコードでは、`stored_ammo_item_charges`、
`stored_ammo_charges_for_items`、`spawn_stored_ammo` を使ってください。これにより、charge から
アイテムへの変換が文書化された 1 つの経路にまとまり、呼び出し側がプルトニウムの丸め規則を
再実装しなくて済みます。

## 車両燃料ストア

車両部品には独自の弾薬/燃料ストレージがあります。部品内部を直接読まず、
`vehicle::fuel_left()`、`vehicle::fuels_left()`、`vehicle::drain()`、部品の弾薬 API などの
車両 API を使ってください。

固体の車両燃料をインベントリアイテムとして取り出す場合も、最後の保持 charge から
アイテム charge への変換には `stored_ammo.h` を使ってください。これにより、完全なアイテムで
表せない部分燃料を保持できます。

```cpp
const auto stored_charges = veh.fuel_left( fuel );
auto fuel_item = spawn_stored_ammo( fuel, stored_charges );
if( fuel_item ) {
    veh.drain( fuel, stored_ammo_charges_for_items( fuel, fuel_item->charges ) );
}
```

## `NO_UNLOAD` と回収ポリシー

アイテムの `NO_UNLOAD` は、通常の unload UI でプレイヤーがそのアイテムを空にできないという
意味です。クラフト、分解、解体でそのアイテムを含むものが破壊されるときに何が起きるべきかは
自動的には決まりません。そのような経路ではポリシーを明示し、テストを追加してください。

- 元アイテムが残る場合は、アイテムとして表せない余りを保持します。
- 元アイテムが消費される場合は、完全な弾薬アイテムを回収し、インベントリアイテムとして
  存在できない余りを捨てます。

## テストチェックリスト

アイテム、弾薬、燃料コードを変更するときは、保存レイアウトと境界条件をテストしてください。

- 内蔵弾薬を使う道具または銃。
- 充電式バッテリーセルのような、中身の弾薬を持つマガジン。
- クラフト/分解の部品として消費される元アイテム。
- unload 後も世界に残る元アイテム。
- 弾薬なし、および物理アイテム 1 個未満の部分弾薬、特にプルトニウム燃料。
- 複数の完全な物理アイテムと部分的な余りが同時にある場合。

回収されたアイテムスタックと、元に残った弾薬の両方を確認するテストを推奨します。
