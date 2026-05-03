# texnitis_mbf_common ヘッダ

実装は M3 で追加。予定する公開ヘッダ:

- `map_provider.hpp` — /map を node 単位のシングルトンで購読し、複数プラグインで共有
- `height_map_provider.hpp` — 高さグリッド版
- `error_codes.hpp` — `texnitis::nav_core::PlanResult / ControllerResult` ↔ mbf outcome
- `ros_logger_bridge.hpp` — `nav_core::LoggerFn` ↔ `rclcpp::Logger`
- `pose_conversions.hpp` — `PoseStamped ⇄ Pose2D`、`OccupancyGrid → GridMapView`
- `param_loader.hpp` — `name` プレフィックスでの ROS パラメータ宣言・取得ユーティリティ
- `tf_utils.hpp` — TF 直接利用箇所の集約点（将来の TF 型差し替えに備える）
