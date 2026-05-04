# texnitis_mbf_plugins デモ

ROS 2 Jazzy + `move_base_flex` でナビゲーションを 1 ゴール走破するための
最短手順をまとめた、ステップバイステップのデモ。

```text
demo/
├── README.md                  ← このファイル
├── maps/
│   └── corridor.yaml          ← nav2_map_server 互換のサンプルマップ
├── waypoints/
│   ├── single_goal.yaml       ← 1 点だけのゴール
│   └── loop.yaml              ← 4 点を周回するシナリオ
└── scripts/
    └── send_single_goal.py    ← Python ActionClient で 1 ゴール送信
```

`demo/` 配下のファイルはすべて pkg install されないので、リポを
clone した位置からそのまま使う想定です。

---

## 0. 前提

- [`docs/usage/installation.md`](../docs/usage/installation.md) のステップが完了
  していること（`ros2 pkg list | grep texnitis` で 6 パッケージ見える）
- ターミナルで `source ~/ros2_ws/install/setup.bash` が済んでいること

---

## 1. マップを起動する

`map_server` で `demo/maps/corridor.yaml` を提供します。

```bash
ros2 run nav2_map_server map_server --ros-args \
    -p yaml_filename:=$PWD/demo/maps/corridor.yaml \
    -p frame_id:=map
ros2 run nav2_lifecycle_manager lifecycle_manager --ros-args \
    -p autostart:=true -p node_names:='[map_server]'
```

`/map` topic が出ることを確認:

```bash
ros2 topic echo /map --once | head
```

`base_link → map` の TF がない場合は、シミュレータ無しでもデモするために
静的 TF を流しておきます:

```bash
ros2 run tf2_ros static_transform_publisher \
    --x 0.5 --y 0.5 --z 0 --roll 0 --pitch 0 --yaw 0 \
    --frame-id map --child-frame-id base_link
```

## 2. mbf を起動する

```bash
ros2 launch texnitis_mbf_bringup texnitis_mbf.launch.py
```

ログに次のような行が出れば成功:

```text
[move_base_flex]: AStarPlanner 'astar' initialized (map_topic=/map, global_frame=map)
[move_base_flex]: LookaheadController 'lookahead' initialized (diff_drive=false)
```

## 3. ゴールを送る

3 通りから好きなものを選んでください。

### 3a. Python ActionClient（最速確認）

```bash
python3 demo/scripts/send_single_goal.py
```

成功すれば次のような出力が出ます:

```text
[demo_goal_sender] sending goal x=2.0 y=1.0 yaw=0.0
[demo_goal_sender] outcome=0 message=
```

### 3b. waypoint_sender（複数点を順番に）

```bash
ros2 run texnitis_mbf_tools waypoint_sender.py --ros-args \
    -p waypoints_file:=$PWD/demo/waypoints/loop.yaml
```

`loop: true` の YAML なので Ctrl-C で抜けるまで永遠に周回します。

### 3c. rviz2

```bash
ros2 run rviz2 rviz2
```

ツールバーの「2D Goal Pose」で goal を指定。

## 4. WebUI（任意）

別ターミナルで:

```bash
ros2 run texnitis_mbf_tools nav_state_publisher.py    # nav_state JSON 集約
ros2 launch texnitis_mbf_webui webui.launch.py        # rosbridge + http.server
```

ブラウザで <http://localhost:8000> を開く。

---

## トラブルシュート

| 症状 | 対処 |
|---|---|
| `outcome=59 NOT_INITIALIZED` | `/map` が transient_local で publish されていない。`map_server` が `lifecycle_manager` で `active` 状態か確認 |
| `outcome=58 TF_ERROR` | `map → base_link` TF が無い。step 1 の static_transform_publisher を起動 |
| `outcome=50 NO_PATH_FOUND` | start / goal が壁の中。`<planner>.inflation_radius` を 0 にして再試行 |
| `Action server not available` | `texnitis_mbf.launch.py` が起動していない、または mbf node 名が違う |
| `Goal rejected by mbf` | YAML の planner / controller 名が `texnitis_mbf.yaml` のリストに無い |

詳細は [../docs/reference/error_codes.md](../docs/reference/error_codes.md)。

---

## カスタム化

- ゴールを変える: [`demo/waypoints/single_goal.yaml`](waypoints/single_goal.yaml) や
  [`demo/scripts/send_single_goal.py`](scripts/send_single_goal.py) を編集
- マップを差し替える: `nav2_map_server` の引数 `yaml_filename` を変える
  （`demo/maps/corridor.yaml` をベースに自作 YAML を用意）
- planner / controller を切り替える: `MoveBase.Goal.planner / .controller` に
  別の名前を入れる（例: `pursuit`, `mppi`、ただし `texnitis_mbf.yaml` の
  `controllers:` リストにあらかじめ宣言しておく必要あり）
