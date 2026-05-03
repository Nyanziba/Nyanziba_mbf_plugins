# 設計判断と根拠

「**問い → 検討した代替案 → 採用した案と理由**」の形で記録する。

> このファイルはマイルストーン進行に合わせて追記される。骨格段階では各セクションのテーマと書く観点だけ置く。

---

## なぜ `mbf_simple_core` を選んだのか（vs `mbf_abstract_core` を直接継承）

- ros2 ブランチに `mbf_costmap_core` が存在しないため、costmap_2d 経由の継承パスは取れない
- `mbf_simple_core` は `initialize(name, tf, node)` だけ受け取れば動き、コストマップなし運用に整合
- **採用理由**: TBD（M3 完了時に書く）

## なぜ planners と controllers を別 .so にするのか

- pluginlib は 1 .so 内に複数 class を許すが、planner と controller を分けるとリスク隔離（片方の crash 隔離）と CI 並列度で有利
- **採用理由**: TBD（M3 完了時に書く）

## なぜ `MapProvider` をシングルトンにするのか

- mbf 側に共有マップ入口がないので、各プラグインが /map を独自購読すると帯域・メモリの無駄
- node 単位の唯一インスタンスを `weak_ptr` テーブルで管理し、複数プラグインが同じ topic を要求しても購読は 1 本
- **採用理由**: TBD（M3 完了時に書く）

## なぜ ROS 1 / ROS 2 統合ではなく ROS 2 専用パッケージなのか

- 既存運用が ROS 2 (Jazzy on Ubuntu 24.04) であること
- ROS 1 は EOL に近く、二重メンテのコストに見合わない
- nav_core 側は ROS 非依存なので、ROS 1 が必要になればそちらは別リポでアダプタを書く方針
- **採用理由**: TBD（M3 完了時に書く）

## なぜ `tools` を ament_python ではなく ament_cmake で書くのか

- 既存スタックの大半が ament_cmake で統一されており、`ament_cmake_python` を使えば Python パッケージも CMakeLists.txt 一本で扱える
- `find_package` 系のビルドフックが他パッケージと足並み揃うので CI 設定が単純化する
- **採用理由**: TBD（M6 完了時に書く）

## なぜ webui を別パッケージに切り出すのか

- フィールド機（HMI 無し）では install 不要にしたいので、`SKIP_PACKAGES` で切り離せる構造が望ましい
- 静的ファイルだけのパッケージは ament_cmake の install ルール 1 行で済む
- **採用理由**: TBD（M6 完了時に書く）

## なぜ `move_base_flex` を vcs import で取り込むのか（vs git submodule / fork）

- vcs import は ros2 ワークスペース運用と相性がよく、SHA 固定も `*.repos` ファイル 1 行で表せる
- submodule 比で「親リポをクローンしただけでは取り込まない」点はワーストケースだが、CI で自動取得すれば実害なし
- 上流の更新追従は repos の SHA をバンプする PR を別途立てる運用
- **採用理由**: TBD（M3 完了時に書く）
