# コードリーディングガイド

> このファイルは骨格段階。実装が積まれるたびに「初見の読者が迷子にならない順序」を更新する。

## 1 リクエストの流れ

mbf 経由でゴールが入ったときの想定トレース（実装後に Mermaid シーケンスで詳細化）:

1. `mbf_simple_nav` の MoveBase アクションサーバがゴールを受信
2. planner プラグイン（例: `texnitis_mbf_planners::AStarPlanner`）の `makePlan` が呼ばれる
3. アダプタは `texnitis_mbf_common::MapProvider::latest()` で最新 OccupancyGrid を取り出す
4. `OccupancyGrid → texnitis::nav_core::GridMapView`、`PoseStamped → Pose2D` に変換
5. `texnitis::nav_core::PlannerBase::planPath(...)` が走る
6. `Path2D` を `std::vector<PoseStamped>` に詰め直し、outcome を `texnitis_mbf_common::error_codes::toMbf` で変換して返す
7. mbf が controller プラグインの `setPlan` を呼ぶ
8. 周期的に `computeVelocityCommands` → `nav_core::ControllerBase::computeCommand`
9. `isGoalReached` が true → mbf アクション完了

## 最初に読むべき 5 ファイル（実装後）

```mermaid
graph LR
  A[texnitis_mbf_common/error_codes.hpp] --> B[texnitis_mbf_common/map_provider.hpp]
  B --> C[texnitis_mbf_planners/astar_planner.hpp]
  C --> D[texnitis_mbf_controllers/lookahead_controller.hpp]
  D --> E[texnitis_mbf_bringup/launch/texnitis_mbf.launch.py]
```

## 内部用語集

| 用語 | 意味 |
|---|---|
| **アダプタ層** | `mbf_simple_core::SimplePlanner / SimpleController` を継承し、内部で nav_core に委譲する薄いラッパ |
| **MapProvider** | mbf プラグイン全体で共有する /map 購読シングルトン。HeightMapProvider は別系統 |
| **outcome** | mbf アクションが返す `uint32_t` ステータスコード。0=SUCCESS, 50=NO_PATH_FOUND など |
| **logger bridge** | `nav_core::LoggerFn`（`std::function`）と `rclcpp::Logger` をつなぐ薄いユーティリティ |
| **vcs import** | `*.repos` で記述したリポ群を `colcon` workspace に取り込むコマンド |
