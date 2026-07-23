# ROS 2 KinematicTimeAStar結合テスト根拠

Test Basisは、`KinematicTimeAStarPlanner` MBFアダプタの公開契約、
`TerrainGrid.msg`、`Trajectory2D.msg`、および
`docs/testing/real_robot_planner_benchmark.md`とする。

| Persona | 観点 | 対応テスト |
|---|---|---|
| P1 新人ユーザー | `/map`だけで計画できる | `plans_from_plain_occupancy_grid_and_publishes_trajectory` |
| P2 ベテラン | ROS message内の全点時刻とTerrain layerを直接検証 | `plans_from_plain_occupancy_grid_and_publishes_trajectory`, `plans_when_required_terrain_grid_arrives` |
| P3 悪意ある操作者 | map未到着、必須Slope未到着 | `returns_not_initialized_before_map_arrives`, `returns_not_initialized_when_required_slope_is_missing` |
| P4 整合性監査役 | MBF cost、Path、Trajectoryの姿勢・時刻・終点速度、Terrain有効bitを確認 | `plans_from_plain_occupancy_grid_and_publishes_trajectory`, `plans_when_required_terrain_grid_arrives` |
| P5 移行担当者 | TerrainGridなしの従来OccupancyGrid | `plans_from_plain_occupancy_grid_and_publishes_trajectory` |
| P6 デグレ番人 | pluginlib対象ライブラリを直接linkした結合テスト | ROS 2 CTest全件 |
| P7 仕様懐疑者 | core成功だけでなくROS publishまで到達するか | trajectory購読assert |

## 要確認

- DDS実装を跨いだ複数process launch test
- 実機時刻とROS simulated timeの選択
- controllerがTrajectory2D速度を直接追従する契約

未決定項目は推測で期待値を置かない。
