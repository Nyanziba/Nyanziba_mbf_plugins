# texnitis_mbf_plugins

`naturerobots/move_base_flex`（ros2 ブランチ）に対応する planner / controller プラグイン群と、それらを束ねる bringup・WebUI・waypoint sender などのツールを提供するメタリポ。

> このリポジトリは現時点では **骨格（scaffold）のみ** です。実装は `docs/architecture.md` のマイルストーン順に追加していきます。

## 構成

```text
texnitis_mbf_planners/      # AStar / HeightAwareAStar の SimplePlanner アダプタ（1 .so）
texnitis_mbf_controllers/   # Lookahead / DiffDrivePP / MecanumMPPI の SimpleController アダプタ（1 .so）
texnitis_mbf_common/        # MapProvider, error_codes, ros_logger_bridge, conversions など共有ユーティリティ
texnitis_mbf_bringup/       # launch / config（純 launch パッケージ）
texnitis_mbf_tools/         # waypoint_sender / nav_state_publisher / mbf_action_bridge (ament_python)
texnitis_mbf_webui/         # WebUI (HTML/JS) と rosbridge launch
third_party/move_base_flex.repos  # naturerobots/move_base_flex を SHA 固定で取り込むための vcs ファイル
docs/                       # architecture / design_rationale / reading_guide / usage / reference
```

## 上流依存

- ROS 2: **Jazzy Jalisco** 想定（Ubuntu 24.04）
- 別リポ: [`Nyanziba/texnitis_nav_core`](https://github.com/Nyanziba/Nyanziba_nav_core) を `find_package(texnitis_nav_core)` で取り込む（PRIVATE link）
- 上流: `naturerobots/move_base_flex` の `ros2` ブランチを `third_party/move_base_flex.repos` で SHA 固定

## ビルド

```bash
mkdir -p ~/ros2_ws/src && cd ~/ros2_ws/src
git clone https://github.com/Nyanziba/Nyanziba_nav_core.git
git clone <this-repo-url>
vcs import < texnitis_mbf_plugins/third_party/move_base_flex.repos

cd ~/ros2_ws
rosdep install --from-paths src --ignore-src -r -y
colcon build --symlink-install --packages-up-to texnitis_mbf_bringup
source install/setup.bash
```

## 起動例

```bash
ros2 launch texnitis_mbf_bringup line_map_publisher.launch.xml &
ros2 launch texnitis_mbf_bringup texnitis_mbf.launch.py
# rviz2 で 2D Goal を投げると mbf 経由で 1 ゴール走破できる
```

## ドキュメント

- 設計判断: `docs/design_rationale.md`
- アーキテクチャ概観: `docs/architecture.md`
- コードリーディング: `docs/reading_guide.md`
- 使い方:
  - `docs/usage/installation.md`
  - `docs/usage/quickstart.md`
  - `docs/usage/parameter_mapping.md`
  - `docs/usage/webui.md`
  - `docs/usage/waypoint_sender.md`
  - `docs/usage/custom_plugin.md`
- リファレンス:
  - `docs/reference/error_codes.md`
  - `docs/reference/topics_and_actions.md`

## ライセンス

MIT License — `LICENSE` を参照。
