# アーキテクチャ概観

## レイヤ図

```mermaid
graph TD
  subgraph "texnitis_nav_core (別リポ・ROS 非依存)"
    core[PlannerBase / ControllerBase / 実装]
  end

  subgraph "texnitis_mbf_plugins (このリポ)"
    common[texnitis_mbf_common<br/>MapProvider / HeightMapProvider<br/>error_codes / pose_conversions / ros_logger_bridge]
    planners[texnitis_mbf_planners<br/>SimplePlanner adapters]
    controllers[texnitis_mbf_controllers<br/>SimpleController adapters]
    bringup[texnitis_mbf_bringup<br/>launch / config]
    tools[texnitis_mbf_tools<br/>waypoint_sender / nav_state_publisher / mbf_action_bridge]
    sim[texnitis_mbf_sim<br/>flat_world_sim_node / run_e2e_check]
    webui[texnitis_mbf_webui<br/>HTML / JS / rosbridge launch]
  end

  subgraph "上流 (third_party)"
    mbf[move_base_flex<br/>mbf_simple_core / mbf_simple_nav]
  end

  planners -- "PRIVATE link" --> core
  controllers -- "PRIVATE link" --> core
  planners --> common
  controllers --> common
  planners -- "pluginlib" --> mbf
  controllers -- "pluginlib" --> mbf
  bringup --> mbf
  bringup --> planners
  bringup --> controllers
  tools -- "ActionClient" --> mbf
  sim -- "/cmd_vel /odom /tf" --> mbf
  webui -- "rosbridge" --> mbf
```

## パッケージの責務

| パッケージ | 責務 | build_type |
|---|---|---|
| `texnitis_mbf_common` | 全アダプタが共有する MapProvider・エラー変換・ロガーブリッジ・型変換 | ament_cmake (SHARED) |
| `texnitis_mbf_planners` | `SimplePlanner` を継承する **薄い** アダプタ。実体は nav_core | ament_cmake (SHARED) |
| `texnitis_mbf_controllers` | `SimpleController` を継承する **薄い** アダプタ。実体は nav_core | ament_cmake (SHARED) |
| `texnitis_mbf_bringup` | mbf を立ち上げる launch + yaml | ament_cmake (純 launch) |
| `texnitis_mbf_tools` | mbf アクションを叩く Python ツール群 | ament_cmake + ament_cmake_python |
| `texnitis_mbf_sim` | 軽量 2D ROS 2 シム + 1 ゴール E2E チェック（CI 用） | ament_cmake + ament_cmake_python |
| `texnitis_mbf_webui` | HTML/JS の WebUI と rosbridge launch | ament_cmake (静的アセット) |

## 1 ゴール走破の流れ

```mermaid
sequenceDiagram
    participant Client
    participant mbf as move_base_flex
    participant Planner as AStarPlanner adapter
    participant Core as nav_core::AStarPlanner
    participant MapProv as MapProvider
    participant Controller as LookaheadController adapter
    participant CCore as nav_core::LookaheadController

    Client->>mbf: MoveBase.send_goal(target_pose, planner=astar, controller=lookahead)
    mbf->>Planner: makePlan(start, goal, ...)
    Planner->>MapProv: latest("/map")
    MapProv-->>Planner: OccupancyGrid (shared_ptr)
    Planner->>Core: planPath(GridMapView, Pose2D, Pose2D, Path2D&)
    Core-->>Planner: PlanResult::Success + path
    Planner-->>mbf: outcome=0, plan
    mbf->>Controller: setPlan(plan)
    Controller->>CCore: setPlan(Path2D)
    loop 20 Hz
        mbf->>Controller: computeVelocityCommands(pose, vel)
        Controller->>CCore: computeCommand(Pose2D, Twist2D&)
        CCore-->>Controller: Twist2D + status
        Controller-->>mbf: TwistStamped + outcome
        mbf-->>+ROS: publish /cmd_vel
    end
    mbf->>Controller: isGoalReached(d_tol, a_tol)
    Controller-->>mbf: true
    mbf-->>Client: outcome=0 (SUCCESS)
```

## 依存方向の原則

- **アダプタは薄く、実装は nav_core**: ROS 依存は型変換・パラメータ宣言・logger 注入だけ
- **costmap_2d 不在の埋め合わせ**: `MapProvider` シングルトンで `/map` を node 単位に 1 回購読
- **エラー変換は 1 箇所**: `error_codes.hpp` にすべての enum→outcome 対応を集約
- **TF 利用は仲介層を通す**: 直接 `tf2_ros::Buffer` を触らず、controller の `initialize` で受け取った
  shared_ptr を保持するだけ
