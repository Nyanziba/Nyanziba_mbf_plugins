# texnitis_mbf_plugins

`naturerobots/move_base_flex`（ros2 ブランチ）対応の planner / controller
プラグインと、bringup・WebUI・waypoint sender、ヘッドレス sim を含むメタリポ。
Ubuntu 24.04 / ROS 2 **Jazzy Jalisco** を想定。

## ハイライト

- AStar / KinematicTimeAStar / Lookahead / DiffDrivePurePursuit
- KinematicTimeAStar（holonomic / differential-drive）と任意のTerrainGrid前処理

`KinematicTimeAStarPlanner` は通常の `/map` (`nav_msgs/OccupancyGrid`) だけで動作する。
任意の `/terrain_grid` (`texnitis_navigation_interfaces/TerrainGrid`) を追加すると、
メートル単位のelevationと`dz/dx`, `dz/dy`から段差・傾斜を共通前処理する。
これらを mbf_simple_core の薄いアダプタとして提供する。
- アルゴリズム本体は ROS 非依存の [`Nyanziba/Nyanziba_nav_core`](https://github.com/Nyanziba/Nyanziba_nav_core)
  に分離（pluginlib `.so` は `find_package(texnitis_nav_core)` で PRIVATE link）
- ヘッドレス 2D シミュレータと 1 ゴール E2E チェックを同梱
  ([`texnitis_mbf_sim`](texnitis_mbf_sim))。Gazebo 不要、CI で実走破
- 3 通りの開発フロー: **CI** / **Docker（macOS でも実行可）** / **pixi（macOS ネイティブ）**

## ディレクトリ

```text
texnitis_mbf_planners/      # AStar / KinematicTimeAStar の SimplePlanner アダプタ
texnitis_mbf_controllers/   # Lookahead / DiffDrivePP のアダプタ
texnitis_mbf_common/        # MapProvider, error_codes, ros_logger_bridge, conversions
texnitis_mbf_bringup/       # launch / config
texnitis_mbf_tools/         # waypoint sender / state publisher / planner benchmark
texnitis_mbf_sim/           # ヘッドレス 2D sim + run_e2e_check + sim launch
texnitis_mbf_webui/         # WebUI (HTML/JS) + rosbridge launch
third_party/                # move_base_flex.repos / texnitis_nav_core.repos
docs/                       # architecture / design_rationale / reading_guide / usage / reference
demo/                       # サンプルマップ + waypoints + 単発ゴールスクリプト
scripts/pixi/               # pixi タスク本体（setup / build / sim_e2e / sim_interactive）
.github/workflows/ci.yml    # 4 ジョブ: structure / python / yaml-launch / sim-e2e
```

## クイックスタート

### 1. ネイティブ（Ubuntu 24.04 / Jazzy）

```bash
mkdir -p ~/ros2_ws/src && cd ~/ros2_ws/src
git clone https://github.com/Nyanziba/Nyanziba_mbf_plugins.git texnitis_mbf_plugins
vcs import < texnitis_mbf_plugins/third_party/texnitis_nav_core.repos
vcs import < texnitis_mbf_plugins/third_party/move_base_flex.repos

cd ~/ros2_ws
rosdep install --from-paths src --ignore-src -r -y
colcon build --packages-up-to texnitis_mbf_sim mbf_simple_nav
source install/setup.bash

# 1 ゴール E2E（成功なら exit 0）
ros2 launch texnitis_mbf_sim texnitis_mbf_sim_demo.launch.py
```

詳細は [`docs/usage/installation.md`](docs/usage/installation.md) と
[`docs/usage/quickstart.md`](docs/usage/quickstart.md)。

### 2. Docker（macOS / Linux 共通、CI と同じ環境）

ROS 2 Jazzy の Docker image でビルド + 実走破:

```bash
# ホスト側でリポをクローン
git clone https://github.com/Nyanziba/Nyanziba_mbf_plugins.git
cd Nyanziba_mbf_plugins

# CI と同じ手順を回すヘルパー（scripts/docker_e2e.sh ※後述で同梱予定）
docker run --rm -v "$PWD:/host_src:ro" -v /tmp/mbf_ws:/ws \
    --platform linux/amd64 osrf/ros:jazzy-desktop bash -c '
        ... # docs/usage/sim_e2e.md の手順を参照
'
```

詳細は [`docs/usage/sim_e2e.md`](docs/usage/sim_e2e.md)。

### 3. pixi（macOS / Linux ネイティブ、Docker 不要）

[pixi](https://pixi.sh) の RoboStack チャンネル経由で ROS 2 Jazzy を取り込み、
Apple Silicon 上でも qemu を介さずに動作（build 約 1m30s）。

```bash
cd Nyanziba_mbf_plugins
pixi install        # ROS 2 Jazzy + colcon + nav2 を解決
pixi run setup      # vcs import で nav_core + mbf を src/ に
pixi run build      # nav_core 一発 → colcon で残り
pixi run test       # ROS 不要の純 Python pytest
pixi run sim-e2e    # 1 ゴール E2E（mbf_msgs typesupport の制約あり、Docker 推奨）
```

詳細は [`docs/usage/local_pixi.md`](docs/usage/local_pixi.md)。

## 起動構成（demo launch）

`texnitis_mbf_sim_demo.launch.py` が以下を一括起動:

| プロセス | 役割 |
|---|---|
| `nav2_map_server` + `nav2_lifecycle_manager` | `/map` を配信 |
| `flat_world_sim_node.py` | `/cmd_vel` 購読 → 中点法積分 → `/odom` + `tf` 配信 |
| `mbf_simple_nav_node` | AStar + Lookahead を pluginlib でロード、MoveBase action サーバ |
| `mbf_e2e_check` (`e2e_check:=true` 時) | 1 ゴール送信 → `outcome=0` を判定し終了コードに反映 |

`e2e_check:=false` で対話モードになり、rviz2 や [`waypoint_sender.py`](texnitis_mbf_tools/scripts/waypoint_sender.py)
から手動でゴールを投げられます。

## CI（GitHub Actions）

`.github/workflows/ci.yml` の 4 ジョブ構成:

| ジョブ | 内容 | 所要時間 |
|---|---|---|
| `package.xml / colcon list` | パッケージ構造 + plugins.xml の妥当性 | ~1 min |
| `python utils + tools` | 全 `*.py` の AST + `pytest` | 13 s |
| `YAML lint + launch.py` | `*.yaml` / `*.repos` / `*.launch.py` 構文 | 10 s |
| `ROS 2 sim end-to-end (jazzy)` | mbf + sim + map_server を Docker で起動して 1 ゴール走破 | warm cache で ~3 min |

最後のジョブは:
1. `nav_core` SHA で `install_nav_core/` をキャッシュ
2. `move_base_flex` SHA で `install_mbf/` をキャッシュ
3. 残りを colcon build（ccache 併用）
4. `ros2 launch` で sim+mbf を立ち上げて `MBF_E2E_RESULT_FILE` 経由で合否判定

## ドキュメント

- 設計判断: [`docs/design_rationale.md`](docs/design_rationale.md)
- アーキテクチャ概観: [`docs/architecture.md`](docs/architecture.md)
- コードリーディング: [`docs/reading_guide.md`](docs/reading_guide.md)
- 使い方:
  - [`docs/usage/installation.md`](docs/usage/installation.md)
  - [`docs/usage/quickstart.md`](docs/usage/quickstart.md)
  - [`docs/usage/sim_e2e.md`](docs/usage/sim_e2e.md) — ヘッドレス sim と E2E チェック
  - [`docs/usage/local_pixi.md`](docs/usage/local_pixi.md) — macOS ネイティブ実行
  - [`docs/usage/parameter_mapping.md`](docs/usage/parameter_mapping.md)
  - [`docs/usage/waypoint_sender.md`](docs/usage/waypoint_sender.md)
  - [`docs/usage/webui.md`](docs/usage/webui.md)
  - [`docs/usage/custom_plugin.md`](docs/usage/custom_plugin.md)
- リファレンス:
  - [`docs/reference/error_codes.md`](docs/reference/error_codes.md)
  - [`docs/reference/topics_and_actions.md`](docs/reference/topics_and_actions.md)

## 上流依存と SHA 固定

- ROS 2: Jazzy Jalisco
- [`Nyanziba/Nyanziba_nav_core`](https://github.com/Nyanziba/Nyanziba_nav_core)
  ROS 非依存のアルゴリズム本体（A\* / Lookahead / Pure Pursuit）
- [`naturerobots/move_base_flex`](https://github.com/naturerobots/move_base_flex)
  `ros2` ブランチ。SHA は [`third_party/move_base_flex.repos`](third_party/move_base_flex.repos) で固定

## ライセンス

MIT License — [`LICENSE`](LICENSE) を参照。
