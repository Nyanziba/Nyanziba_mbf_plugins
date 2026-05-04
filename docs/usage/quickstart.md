# クイックスタート

[installation.md](installation.md) でビルド済みであることを前提に、最初の 1 ゴール
を走らせるまでの最短経路を示します。

## 起動

ターミナル 1: マップを供給する何かを上げる（rviz2 でも `nav2_map_server` でも何でも）

```bash
ros2 launch texnitis_mbf_bringup line_map_publisher.launch.xml
# もしくは
ros2 run nav2_map_server map_server --ros-args -p yaml_filename:=my_map.yaml
```

ターミナル 2: mbf 本体を起動

```bash
source ~/ros2_ws/install/setup.bash
ros2 launch texnitis_mbf_bringup texnitis_mbf.launch.py
```

起動ログに次のような行が出れば planner / controller プラグインの初期化に
成功しています:

```text
[move_base_flex]: AStarPlanner 'astar' initialized (map_topic=/map, global_frame=map)
[move_base_flex]: LookaheadController 'lookahead' initialized (diff_drive=false)
```

## ゴールを送る

### rviz2 から

```bash
ros2 run rviz2 rviz2
```

ツールバーの「2D Goal Pose」をクリックしてマップ上で goal を指定。

### Python アクションクライアントから

```python
import rclpy
from rclpy.action import ActionClient
from rclpy.node import Node
from mbf_msgs.action import MoveBase
from geometry_msgs.msg import PoseStamped


class Sender(Node):
    def __init__(self):
        super().__init__("sender")
        self.client = ActionClient(self, MoveBase, "/move_base_flex/move_base")
        self.client.wait_for_server()
        goal = MoveBase.Goal()
        goal.target_pose = self._pose(1.0, 0.5)
        goal.planner = "astar"
        goal.controller = "lookahead"
        self.client.send_goal_async(goal).add_done_callback(self._accepted)

    def _pose(self, x, y):
        p = PoseStamped()
        p.header.frame_id = "map"
        p.pose.position.x = x
        p.pose.position.y = y
        p.pose.orientation.w = 1.0
        return p

    def _accepted(self, fut):
        fut.result().get_result_async().add_done_callback(
            lambda f: self.get_logger().info(f"outcome={f.result().result.outcome}"))


rclpy.init(); rclpy.spin(Sender())
```

### waypoint_sender でループ走行

`waypoints.yaml` 例:

```yaml
frame_id: "map"
loop: false
waypoints:
  - {x: 1.0, y: 0.0, yaw: 0.0}
  - {x: 2.0, y: 1.0, yaw: 1.57}
  - {x: 0.0, y: 0.5, yaw: 3.14}
```

```bash
ros2 run texnitis_mbf_tools waypoint_sender.py --ros-args \
    -p waypoints_file:=$PWD/waypoints.yaml
```

詳細は [waypoint_sender.md](waypoint_sender.md)。

## 期待される動き

1. mbf がプランナー (`astar`) に `makePlan(start, goal)` を要求 → 経路が返る
2. mbf がコントローラ (`lookahead`) に `setPlan(...)` を渡す
3. `controller_frequency`（既定 20 Hz）で `computeVelocityCommands` が呼ばれ
   `/cmd_vel` が出る
4. `LookaheadController::isGoalReached(d_tol, a_tol)` が True になったら
   mbf アクションは `outcome = 0 (SUCCESS)` を返す

## 失敗時の典型挙動

| 症状 | 原因と対処 |
|---|---|
| `outcome=50 (NO_PATH_FOUND)` | start / goal 周辺に inflation 後の空きセルが無い。`<planner>.inflation_radius` を下げる |
| `outcome=58 (TF_ERROR)` | `map → base_link` の TF が来ていない。`ros2 run tf2_ros tf2_echo map base_link` |
| `outcome=104 (INVALID_PATH)` | controller の `setPlan` 失敗 / 経路長 0 |
| `outcome=59 (NOT_INITIALIZED)` | プランナーが `/map` を受信していない。`ros2 topic echo /map` で確認 |
| 動かない / cmd_vel 0 ばかり | `goal_stateful: true` のとき、過去の到達フラグが残っている。`reset` してから再ゴール |

詳細は [../reference/error_codes.md](../reference/error_codes.md)。
