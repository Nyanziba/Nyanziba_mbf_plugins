# アーキテクチャ概観

## レイヤ

```mermaid
graph TD
  subgraph "texnitis_nav_core (別リポ)"
    core[PlannerBase / ControllerBase / 実装]
  end

  subgraph "texnitis_mbf_plugins (このリポ)"
    common[texnitis_mbf_common<br/>MapProvider / error_codes / logger bridge]
    planners[texnitis_mbf_planners<br/>SimplePlanner adapters]
    controllers[texnitis_mbf_controllers<br/>SimpleController adapters]
    bringup[texnitis_mbf_bringup<br/>launch / config]
    tools[texnitis_mbf_tools<br/>waypoint_sender / nav_state_publisher]
    webui[texnitis_mbf_webui<br/>HTML / JS / rosbridge launch]
  end

  subgraph "上流 (third_party)"
    mbf[move_base_flex<br/>mbf_simple_core / mbf_simple_nav]
  end

  planners -- PRIVATE link --> core
  controllers -- PRIVATE link --> core
  planners --> common
  controllers --> common
  planners -- pluginlib --> mbf
  controllers -- pluginlib --> mbf
  bringup --> mbf
  bringup --> planners
  bringup --> controllers
  tools -- action client --> mbf
  webui -- rosbridge --> mbf
```

## パッケージの責務

| パッケージ | 責務 |
|---|---|
| `texnitis_mbf_common` | 全アダプタが共有する MapProvider・エラー変換・ロガーブリッジ・型変換 |
| `texnitis_mbf_planners` | `mbf_simple_core::SimplePlanner` を継承する **薄い** アダプタ。実体は nav_core |
| `texnitis_mbf_controllers` | `mbf_simple_core::SimpleController` を継承する **薄い** アダプタ。実体は nav_core |
| `texnitis_mbf_bringup` | mbf を立ち上げる launch + yaml |
| `texnitis_mbf_tools` | mbf アクションを叩く Python ツール群（ament_cmake + ament_cmake_python） |
| `texnitis_mbf_webui` | HTML/JS の WebUI と rosbridge launch |

## 依存方向の原則

- **アダプタは薄く、実装は nav_core**: ROS 依存は `package.xml` のレイヤ（type 変換・パラメータ・logger）に閉じる
- **costmap_2d 不在の埋め合わせ**: `MapProvider` シングルトンで /map を node 単位に 1 回だけ購読
- **エラー変換は 1 箇所**: `texnitis_mbf_common::error_codes` にすべての enum→outcome 対応を集約
