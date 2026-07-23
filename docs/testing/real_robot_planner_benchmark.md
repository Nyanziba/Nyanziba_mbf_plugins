# 既存A* / KinematicTime A* 実機到達時間比較

同じ機体、地図、開始姿勢、目標姿勢、controller、積載、バッテリー条件で比較する。
学習効果・路面変化の偏りを減らすため、ツールは A* と KinematicTime A* を交互に実行する。

## 実行

1. コース上に開始姿勢をマーキングし、非常停止と監視者を配置する。
2. MBFを起動し、次を実行する。

```bash
ros2 run texnitis_mbf_tools planner_benchmark.py --ros-args \
  -p planners:="['astar','kinematic_time']" \
  -p trials_per_planner:=10 -p controller:=lookahead \
  -p goal_x:=5.0 -p goal_y:=0.0 -p goal_yaw:=0.0 \
  -p output_prefix:=results/straight_course
```

3. 毎試行前に機体を同一開始姿勢へ戻し、安全確認後に次を呼ぶ。

```bash
ros2 service call /planner_benchmark/ready std_srvs/srv/SetBool "{data: true}"
```

生データはCSV、planner別の成功率・成功試行だけの到達時間中央値・実走行距離中央値はJSONに保存される。
失敗試行を短時間として扱わない。最低10試行を目安とするが、有意差の閾値は機体要件として別途決定する。

## 安全と判定

- 転倒判定は静的近似であり、段差衝撃、タイヤ滑り、動的横加速度を保証しない。実機速度は低速から段階的に上げる。
- 到達時間だけでなく成功率を先に比較する。成功率を落として得た時間短縮を改善とは判定しない。
- `track_width` と `center_of_gravity_height` は実測値を使う。
