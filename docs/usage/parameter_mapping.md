# 旧 `texnitis_move_base_like.yaml` から `texnitis_mbf.yaml` への移行表

旧パッケージのパラメータをそのまま mbf 構成に持ち込むときの対応表。

## トップレベル

| 旧 yaml キー | 新 yaml の場所 | 備考 |
|---|---|---|
| `planner_plugin: ...AStarPlanner` | `planners` リストに `texnitis::mbf_planners::AStarPlanner` |
| `controller_plugin: ...LookaheadController` | `controllers` リストに `texnitis::mbf_controllers::LookaheadController` |
| `accept_external_path: true` | `ExePath` アクションに直接 path を投げる方式へ移行 |
| `external_path_timeout_sec` | クライアント側のタイムアウトに任せる |
| `control_hz` | `move_base_flex.controller_frequency` |
| `global_frame` | `move_base_flex.global_frame` |
| `base_frame` | `move_base_flex.robot_frame` |
| `map_topic` | `<planner_name>.map_topic` |
| `cmd_vel_topic` | mbf node 起動時の `--remap` |
| `goal_pose_topic` | 廃止（mbf アクション or 自前 ActionClient へ） |

## A\* / KinematicTime A\* 系

| 旧 yaml | 新 yaml (`<planner_name>.*`) |
|---|---|
| `occupied_threshold` | `occupied_threshold` |
| `unknown_is_obstacle` | `unknown_is_obstacle` |
| `inflation_radius` | `inflation_radius` |
| `heading_bins` | `heading_bins` (KinematicTime A*) |
| `heading_weight` | （現状未対応） |
| `height_lethal_threshold` | 廃止。TerrainGridと`max_step_height`を使用 |
| `height_grid_topic` | 廃止。`<planner_name>.terrain_topic`を使用 |

## Lookahead Controller

| 旧 yaml | 新 yaml (`<controller_name>.*`) |
|---|---|
| `kp_xy` | `kp_xy` |
| `kp_yaw` | `kp_yaw` |
| `max_speed_xy` | `max_speed_xy` |
| `max_speed_yaw` | `max_speed_yaw` |
| `lookahead_dist` | `lookahead_dist` |
| `linear_threshold_for_wz` | `linear_threshold_for_wz` |
| `max_wz_when_moving` | `max_wz_when_moving` |
| `use_diff_drive` | `use_diff_drive` |
| `goal_xy_tolerance` | `goal_xy_tolerance`（mbf アクションの `dist_tolerance` も上書きするので注意） |
| `goal_yaw_tolerance` | `goal_yaw_tolerance` |
| `goal_stateful` | `goal_stateful` |
| （旧実装になし） | `rotate_while_moving`（移動中にゴール yaw へ補間回頭。既定 false） |
| （旧実装になし） | `rotate_while_moving_exponent`（補間カーブの冪指数。既定 2.0） |

## Differential Drive Pure Pursuit

| 旧 yaml (`pp_*`) | 新 yaml (`<controller_name>.*`) |
|---|---|
| `pp_max_speed_xy` | `max_linear_velocity` |
| `pp_max_speed_yaw` | `max_angular_velocity` |
| `pp_max_acc_xy` | `max_acceleration` |
| `pp_max_acc_yaw` | `max_angular_acceleration` |
| `pp_lookahead_dist` | `min_lookahead_distance` / `max_lookahead_distance` の組へ拡張 |
| （新規）`lookahead_time` | 速度ベースの動的 lookahead [s] |
| `pp_curvature_p` | （現状未対応 — M9 以降） |

## 完全な実例

`texnitis_mbf_bringup/config/texnitis_mbf.yaml` を編集元として使うのが
最短です。2 つの controller を宣言して切替可能にする例:

```yaml
move_base_flex:
  ros__parameters:
    global_frame: map
    robot_frame: base_link
    controller_frequency: 20.0
    planner_frequency: 1.0

    planners:
      - {name: astar,    type: texnitis::mbf_planners::AStarPlanner}
    controllers:
      - {name: lookahead, type: texnitis::mbf_controllers::LookaheadController}
      - {name: pursuit,   type: texnitis::mbf_controllers::DiffDrivePurePursuitController}

    astar:
      map_topic: /map
      occupied_threshold: 65
      inflation_radius: 0.30

    lookahead:
      kp_xy: 1.0
      kp_yaw: 1.2
      max_speed_xy: 0.25
      max_speed_yaw: 0.8
      lookahead_dist: 0.40
      goal_xy_tolerance: 0.05
      goal_yaw_tolerance: 0.10
      goal_stateful: true

    pursuit:
      max_linear_velocity: 0.50
      max_acceleration: 0.50
      lookahead_time: 0.80
      min_lookahead_distance: 0.30
      max_lookahead_distance: 1.00
      goal_xy_tolerance: 0.06

```

mbf アクション送信時に `goal.controller = "pursuit"` を指定すれば
runtime に切り替わります。
