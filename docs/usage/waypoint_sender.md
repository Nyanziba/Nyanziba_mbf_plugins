# waypoint_sender

`texnitis_mbf_tools/waypoint_sender.py` は YAML で定義した waypoint を
順番に `mbf_msgs/MoveBase` アクションに送るスクリプトです。
旧 `texnitis_move_base_like` のサービス版と yaml フォーマットは互換です。

## YAML フォーマット

```yaml
frame_id: "map"            # waypoint の座標系
loop: false                # true で全到達後に最初に戻る
waypoints:
  - {x: 1.0, y: 0.0, yaw: 0.0}
  - {x: 2.0, y: 1.0, yaw: 1.57}
  - {x: 0.0, y: 0.5, yaw: 3.14}
```

## 実行

```bash
ros2 run texnitis_mbf_tools waypoint_sender.py --ros-args \
    -p waypoints_file:=$PWD/waypoints.yaml
```

## パラメータ

| パラメータ | 既定 | 意味 |
|---|---|---|
| `waypoints_file` | （必須） | YAML ファイルの絶対パス |
| `action_name` | `/move_base_flex/move_base` | mbf アクション名 |
| `planner` | `astar` | 各 goal で使う planner プラグイン名 |
| `controller` | `lookahead` | 各 goal で使う controller プラグイン名 |

## 動作

1. ROS init、ActionClient で mbf に接続
2. YAML を読んで waypoints を配列化
3. 順次 `MoveBase.Goal` を送信
4. result の `outcome` が SUCCESS なら次の waypoint、失敗時はログを出して次へ進む
5. 全部終わったら `loop: true` なら 0 番目に戻る、`false` ならノード終了

## 別 controller / planner で送る

`waypoints.yaml` 内では指定できないので、`-p planner:=...` で全体に適用します。

```bash
ros2 run texnitis_mbf_tools waypoint_sender.py --ros-args \
    -p waypoints_file:=$PWD/waypoints.yaml \
    -p planner:=kinematic_time -p controller:=lookahead
```

## トラブルシュート

- **`Goal rejected by mbf`**: planner / controller 名が yaml に書いた名前と
  一致していない。`ros2 param dump /move_base_flex` で `planners[*].name` を確認
- **`Action server not available`**: mbf node が起動していない、または
  `action_name` が間違っている。`ros2 action list` で確認
- **YAML パースエラー**: `frame_id` または `waypoints` が無い。
  `python -c "import yaml; print(yaml.safe_load(open('waypoints.yaml')))"` で検証
