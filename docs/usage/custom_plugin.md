# 自作 planner / controller を足す

`texnitis_nav_core` に新しいアルゴリズムを追加し、`texnitis_mbf_plugins`
側で mbf アダプタを書いて pluginlib に登録するまでの手順。

## 全体像

```text
1. texnitis_nav_core 側
   - PlannerBase / ControllerBase を継承したクラスを実装
   - tests/ で GoogleTest を書いて回帰テスト
   - commit + tag

2. texnitis_mbf_plugins 側
   - mbf_simple_core::SimplePlanner / SimpleController を継承したアダプタ
   - plugins.xml に登録
   - bringup の YAML / launch を更新
```

## 例: 「ヒルクライム planner」を足してみる

### 1. nav_core 側にコアを書く

```cpp
// include/texnitis_nav_core/planners/hill_climb_planner.hpp
#pragma once
#include "texnitis_nav_core/planner_base.hpp"

namespace texnitis::nav_core {
struct HillClimbParams { /* ... */ };

class HillClimbPlanner final : public PlannerBase {
public:
    explicit HillClimbPlanner(const HillClimbParams &);
    PlanResult planPath(const GridMapView &, const Pose2D &, const Pose2D &,
                        Path2D &) override;
    void cancel() noexcept override;
private:
    HillClimbParams params_;
    std::atomic<bool> cancel_{false};
};
}  // namespace
```

実装は `src/planners/hill_climb_planner.cpp`、`CMakeLists.txt` の
`add_library(texnitis_nav_core STATIC ...)` のソースリストに追加し、
`tests/test_m4_hill_climb.cpp` で GoogleTest を書く。

### 2. mbf アダプタを書く

```cpp
// texnitis_mbf_planners/include/texnitis_mbf_planners/hill_climb_planner.hpp
#pragma once
#include <atomic>
#include <memory>
#include <mbf_simple_core/simple_planner.h>
#include <texnitis_mbf_common/map_provider.hpp>
#include <texnitis_nav_core/planners/hill_climb_planner.hpp>

namespace texnitis::mbf_planners {
class HillClimbPlanner final : public mbf_simple_core::SimplePlanner {
public:
    void initialize(const std::string name,
                    const rclcpp::Node::SharedPtr &node) override;
    uint32_t makePlan(...) override;
    bool cancel() override;
private:
    std::unique_ptr<::texnitis::nav_core::HillClimbPlanner> core_;
    std::shared_ptr<::texnitis::mbf_common::MapProvider> map_provider_;
    /* name_, node_, cancel_requested_, ... */
};
}
```

実装の構造は既存の `astar_planner.cpp` と全く同じです:

1. `initialize`: `name + ".*"` のパラメータを宣言・取得 → `Params` に充填 →
   `core_` を構築 → `MapProvider::get(node)->subscribe(map_topic)`
2. `makePlan`: `MapProvider::latest()` を取り出して `GridMapView` に変換 →
   `core_->planPath(...)` → 結果を `std::vector<PoseStamped>` に変換 →
   `error_codes::toMbfPlannerOutcome` で outcome を返す
3. `cancel`: `core_->cancel()` を呼び `true` を返す

### 3. plugins.xml に登録

```xml
<library path="texnitis_mbf_planners">
  <class type="texnitis::mbf_planners::AStarPlanner"
         base_class_type="mbf_simple_core::SimplePlanner">...</class>
  <class type="texnitis::mbf_planners::HeightAwareAStarPlanner"
         base_class_type="mbf_simple_core::SimplePlanner">...</class>
  <class type="texnitis::mbf_planners::HillClimbPlanner"
         base_class_type="mbf_simple_core::SimplePlanner">
    <description>Hill-climb planner that ...</description>
  </class>
</library>
```

### 4. CMakeLists.txt に source を追加

```cmake
add_library(texnitis_mbf_planners SHARED
    src/astar_planner.cpp
    src/height_aware_astar_planner.cpp
    src/hill_climb_planner.cpp           # ← 追加
)
```

### 5. 起動時に有効化

```yaml
# texnitis_mbf_bringup/config/texnitis_mbf.yaml に追加
planners:
  - {name: astar,      type: texnitis::mbf_planners::AStarPlanner}
  - {name: hillclimb,  type: texnitis::mbf_planners::HillClimbPlanner}

hillclimb:
  map_topic: /map
  some_param: 0.5
```

## Controller を足す場合

`SimpleController::initialize(name, tf, node)` は **TF (tf2_ros::Buffer の
shared_ptr)** も受け取ります。tf 直接利用は `texnitis_mbf_common::tf_utils`
を介すと将来 mbf API が変わったときに差し替えやすくなります。

`computeVelocityCommands` / `setPlan` / `isGoalReached` / `cancel` の
4 メソッドを実装します。具体例は
`texnitis_mbf_controllers/src/lookahead_controller.cpp` をテンプレートに。

## チェックリスト

- [ ] nav_core 側にコアロジック + GoogleTest
- [ ] mbf_plugins 側にアダプタ（`Params` 充填 + 型変換 + cancel atomic）
- [ ] `plugins.xml` にクラスエントリ
- [ ] `CMakeLists.txt` に source を追加
- [ ] bringup の YAML に登録
- [ ] [error_codes.md](../reference/error_codes.md) に enum 拡張があれば追記
- [ ] CI で `colcon build` が通ることを確認
