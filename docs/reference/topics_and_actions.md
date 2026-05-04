# 公開する topic / action / parameter

`texnitis_mbf_plugins` の各パッケージが公開する ROS インターフェースの
カタログです。`mbf_simple_nav` 上で動作します。

## アクション

| 名前 | 型 | 提供元 |
|---|---|---|
| `/move_base_flex/move_base` | `mbf_msgs/action/MoveBase` | mbf_simple_nav |
| `/move_base_flex/get_path` | `mbf_msgs/action/GetPath` | mbf_simple_nav |
| `/move_base_flex/exe_path` | `mbf_msgs/action/ExePath` | mbf_simple_nav |
| `/move_base_flex/recovery` | `mbf_msgs/action/Recovery` | mbf_simple_nav |

## トピック（subscribe される）

| 名前 | 型 | 役割 |
|---|---|---|
| `/map` | `nav_msgs/msg/OccupancyGrid` | 占有グリッド。`MapProvider` が共有購読 |
| `/height_grid` | `nav_msgs/msg/OccupancyGrid` | 高さグリッド。`HeightMapProvider` が共有購読 |
| `/tf`, `/tf_static` | `tf2_msgs/msg/TFMessage` | mbf 内部の TF |

## トピック（publish される）

| 名前 | 型 | 役割 |
|---|---|---|
| `/cmd_vel` | `geometry_msgs/msg/Twist` | mbf node から（`MoveBase` / `ExePath` 実行中） |
| `/move_base_flex/global_plan` | `nav_msgs/msg/Path` | 計画経路 |
| `/move_base_flex/robot_pose` | `geometry_msgs/msg/PoseStamped` | 現在の robot pose（mbf 推定） |
| `/texnitis_mbf/nav_state` | `std_msgs/msg/String` | `nav_state_publisher.py` が出す legacy 互換 JSON |

## パラメータ

`move_base_flex` ノードのパラメータ:

| 名前 | 既定 | 説明 |
|---|---|---|
| `global_frame` | `map` | プランナーの座標系 |
| `robot_frame` | `base_link` | ロボット本体の座標系 |
| `controller_frequency` | 20.0 | controller 呼び出し周波数 [Hz] |
| `planner_frequency` | 1.0 | プランナー呼び出し周波数 [Hz] |
| `planners` | （必須） | プラグインリスト `[{name, type}]` |
| `controllers` | （必須） | プラグインリスト `[{name, type}]` |

各プラグインのパラメータは `<plugin_name>.*` プレフィックスで宣言されます。
完全な一覧は [parameter_mapping.md](../usage/parameter_mapping.md) と
[texnitis_nav_core/docs/usage/parameters.md](https://github.com/Nyanziba/Nyanziba_nav_core/blob/main/docs/usage/parameters.md)。

## tools / webui の topic

| ノード | publish | subscribe | 役割 |
|---|---|---|---|
| `nav_state_publisher.py` | `/texnitis_mbf/nav_state` (String) | `/map`, `/move_base_flex/global_plan`, `/move_base_flex/robot_pose`, `/goal_pose` | legacy `nav_state` 互換 JSON 集約 |
| `mbf_action_bridge.py` | （無） | `texnitis_move_base_like/srv/SetGoal` | 旧 `SetGoal` を mbf アクションへ転送（移行用） |
| `waypoint_sender.py` | （ActionClient のみ） | （無） | YAML を読んで mbf アクションに送る |
| `rosbridge_websocket` | port 9090 | port 9090 | WebUI と ROS の橋渡し |

## TF 要件

`base_link` から計画用フレーム（既定 `map`）への TF が解決できる必要があります。
`ros2 run tf2_ros tf2_echo map base_link` で確認。
