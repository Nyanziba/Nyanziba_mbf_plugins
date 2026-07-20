# ROS 2 シム + 自律走行 E2E

`texnitis_mbf_sim` はヘッドレスな 2D シミュレータと、mbf 経由で
1 ゴール走破を確認する E2E チェックを束ねたパッケージです。Gazebo を
使わないので CI コンテナ上でも動き、手元では rviz2 と組み合わせて
インタラクティブに使えます。

## 構成

```text
texnitis_mbf_sim/
├── texnitis_mbf_sim/
│   ├── kinematics.py      # 純 Python の midpoint 積分器
│   └── occupancy.py       # ROS 非依存の占有グリッド衝突判定
├── scripts/
│   ├── flat_world_sim_node.py   # rclpy ノード本体（cmd_vel → odom + tf）
│   └── run_e2e_check.py         # 1 ゴール送って outcome を assert する rclpy ノード
├── launch/
│   └── texnitis_mbf_sim_demo.launch.py  # map_server + sim + mbf + e2e check
├── config/
│   ├── open_field.yaml    # 20x20 m オープンフィールド（既定・高速デモ用）
│   ├── open_field.pgm
│   ├── corridor.yaml      # 4x3 m 狭路（旧デモ、低速向け）
│   └── corridor.pgm
└── test/
    └── test_kinematics.py # pytest（ROS 不要）
```

## 1 コマンドで E2E 走破

`colcon build --packages-up-to texnitis_mbf_sim` 済みの ws で:

```bash
source install/setup.bash
ros2 launch texnitis_mbf_sim texnitis_mbf_sim_demo.launch.py
```

これだけで（既定は 20x20 m オープンフィールドの高速デモ）:

1. `nav2_map_server` が `config/open_field.yaml` を `/map` に流す
2. `flat_world_sim` が `(1.0, 1.0, 0)` から起動して `/cmd_vel` を待つ
3. `mbf_simple_nav` が AStar + Lookahead プラグイン
   （`texnitis_mbf_highspeed.yaml`: `max_speed_xy` 2.0 m/s、
   `rotate_while_moving` 有効）で起動
4. `mbf_e2e_check` が対角 `(18.0, 18.0, yaw=90°)` にゴールを 1 つ送る。
   ロボットは走りながらゴール yaw へ回頭する
5. `outcome=0 (SUCCESS)` が返ったら launch 全体が `Shutdown()` する

旧来の狭路・低速デモは次で再現できます:

```bash
ros2 launch texnitis_mbf_sim texnitis_mbf_sim_demo.launch.py \
    map_yaml:=<share>/texnitis_mbf_sim/config/corridor.yaml \
    bringup_yaml:=<share>/texnitis_mbf_bringup/config/texnitis_mbf.yaml \
    goal_x:=2.5 goal_y:=0.5 goal_yaw:=0.0
```

ノード終了コードがそのまま launch の終了コードになるので、
**CI からはこの 1 launch を `timeout 180s` で起動するだけ** で合否が出ます。

## インタラクティブモード

ゴールを手で投げたいとき:

```bash
ros2 launch texnitis_mbf_sim texnitis_mbf_sim_demo.launch.py e2e_check:=false
```

別ターミナルで rviz2 を上げて 2D Goal Pose を投げるか、
[waypoint_sender.md](waypoint_sender.md) の YAML で連続ゴールが流せます。

## 起動引数

| 引数 | 既定値 | 意味 |
|---|---|---|
| `map_yaml` | `texnitis_mbf_sim/config/open_field.yaml` | `nav2_map_server` に渡す YAML |
| `bringup_yaml` | `texnitis_mbf_bringup/config/texnitis_mbf_highspeed.yaml` | mbf 用パラメータ |
| `e2e_check` | `true` | true なら 1 ゴール送って完了で全体 shutdown |
| `goal_x` / `goal_y` / `goal_yaw` | `18.0` / `18.0` / `1.5708` | 送るゴール |
| `overall_timeout_s` | `90.0` | E2E チェックがあきらめるまでの上限 |

## シミュレータの中身（要点）

- 運動モデル: ホロノミック / メカナム互換。`(vx, vy, wz)` を中点法で積分
  ([kinematics.py](../../texnitis_mbf_sim/texnitis_mbf_sim/kinematics.py))
- ノイズなし。`/odom` は真値。実機相当の擾乱は `texnitis_nav_core` 側の
  Python シムで `disturbance.py` を使う運用にしています
- 衝突: `/map` のセル値が `>= 65` のとき直前 pose に巻き戻して `cmd_vel` の
  影響を相殺（壁にめり込まない）
- `tf` は `map → odom` を identity、`odom → base_link` をシム pose で発行

## CI でどう動かしているか

[`.github/workflows/ci.yml`](../../.github/workflows/ci.yml) の `sim-e2e`
ジョブが次を行います:

1. `osrf/ros:jazzy-desktop` Docker image でチェックアウト
2. `vcs import` で `move_base_flex` と `texnitis_nav_core` を取得
3. `cmake -S src/texnitis_nav_core` で nav_core を先にビルド・install
4. `colcon build --packages-up-to texnitis_mbf_sim` で残り全部
5. `timeout 180s ros2 launch ... e2e_check:=true` で実行
6. exit code が 0 なら PASS、非 0 なら failure

`upload-artifact@v4` で失敗時の `log/` と `build/` を保存しているので、
コケたときは Actions の artifacts から落とせば原因がすぐ追えます。

## トラブルシュート

| 症状 | 原因と対処 |
|---|---|
| `action server never came up` | mbf 側が立ち上がっていない。`ros2 node list` で `/move_base_flex` を確認、`ros2 topic echo /map` で map 配信を確認 |
| `goal did not finish within ...` | コントローラがゴール条件に到達していない。`bringup_yaml` の `lookahead.goal_xy_tolerance` を緩める、または `goal_x/y` を近くにする |
| `collision at (..)` ログ大量 | `inflation_radius` が小さすぎて planner が壁ギリギリの経路を出している。`bringup_yaml` の `astar.inflation_radius` を広げる |
| ローカルで PASS、CI で FAIL | コンテナの `--rmw-implementation` 違いで discovery が遅れることがある。launch の `server_wait_sec` パラメータを伸ばす |
