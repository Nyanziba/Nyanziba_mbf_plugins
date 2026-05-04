# 設計判断と根拠

「**問い → 検討した代替案 → 採用した案と理由**」の形で記録する。

---

## なぜ `mbf_simple_core` を選んだのか（vs `mbf_abstract_core` を直接継承）

- ros2 ブランチに `mbf_costmap_core` が存在しないため、costmap_2d 経由の継承パスは取れない
- **検討した代替案**:
  - (A) `mbf_abstract_core::AbstractPlanner` を直接継承
  - (B) ★採用：`mbf_simple_core::SimplePlanner` を継承し、`(name, node)` を受ける `initialize` を使う
- **採用理由**: SimplePlanner は ROS パラメータ宣言と logger 取得をそのまま実現できる
  最小 API。AbstractPlanner だとそれらを別途プラグインに実装する手間が増えるだけで、
  得るものがない

## なぜ planners と controllers を別 .so にするのか

- pluginlib は 1 .so 内に複数 class を許す
- **検討した代替案**:
  - (A) 1 つの `texnitis_mbf_plugins` .so にまとめる
  - (B) ★採用：`texnitis_mbf_planners.so` と `texnitis_mbf_controllers.so` に分離
- **採用理由**:
  - リスク隔離（片方の crash がもう一方を巻き込まない）
  - CI 並列度が上がる
  - 上流の mbf も planners / controllers を別 plugin namespace で扱うので、足並み揃う

## なぜ `MapProvider` をシングルトンにするのか

- mbf に共有マップ入口がなく、各プラグインが個別購読すると帯域・メモリの無駄
- **検討した代替案**:
  - (A) 各プラグインが独自に subscribe
  - (B) ★採用：node 単位の `weak_ptr` シングルトン
- **採用理由**:
  - mbf_simple_nav は同一 node に複数プラグインをロードする運用が前提なので、node ポインタを
    キーにすれば共有できる
  - node 破棄で `weak_ptr` が expire し、エントリは自動的にクリア
  - reliable + transient_local QoS で、後発のプラグインも最新マップを取得できる

## なぜ ROS 1 / ROS 2 統合ではなく ROS 2 専用パッケージなのか

- 既存運用が ROS 2 (Jazzy on Ubuntu 24.04)
- ROS 1 は EOL に近く、二重メンテのコストに見合わない
- nav_core 側は ROS 非依存なので、ROS 1 が必要になればそちらに別リポでアダプタを書ける

## なぜ `texnitis_mbf_tools` を ament_python ではなく ament_cmake で書くのか

- 既存スタックの大半が ament_cmake で統一されており、`ament_cmake_python` を使えば
  Python パッケージも CMakeLists.txt 一本で扱える
- `find_package` 系のビルドフックが他パッケージと足並み揃うので CI 設定が単純化する
- ament_pythonは非推奨となったため。

## なぜ webui を別パッケージに切り出すのか

- フィールド機（HMI 無し）では install 不要にしたいので、`SKIP_PACKAGES` で切り離せる
  構造が望ましい
- 静的ファイルだけのパッケージは ament_cmake の install ルール 1 行で済む

## なぜ `move_base_flex` を vcs import で取り込むのか（vs git submodule / fork）

- vcs import は ros2 ワークスペース運用と相性がよく、SHA 固定も `*.repos` ファイル
  1 行で表せる
- submodule 比で「親リポをクローンしただけでは取り込まない」点はワーストケースだが、CI で
  自動取得すれば実害なし
- 上流の更新追従は repos の SHA をバンプする PR を別途立てる運用

## なぜ legacy `texnitis_move_base_like` を即削除しなかったのか

- `R2_packages` は organization リポで、削除には PR レビューが要る
- nav_core / mbf_plugins が安定し、運用切替が完了するまで legacy も残しておくのが安全
- `mbf_action_bridge.py` は `SetGoal.srv` を import するため、legacy 削除と同時に
  自動 deprecate される設計
