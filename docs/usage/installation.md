# インストール

ROS 2 Jazzy + Ubuntu 24.04 をターゲットに、`texnitis_nav_core` と
`texnitis_mbf_plugins` の両方をビルドして使えるようにする手順。

## 前提

| 項目 | バージョン |
|---|---|
| OS | Ubuntu 24.04（推奨） / macOS + pixi global |
| ROS | ROS 2 Jazzy Jalisco |
| C++ | C++20 (GCC 13 / Apple Clang 15+) |
| CMake | 3.20 以降 |

## 1. ROS 2 環境を sourcing

- **Linux (Ubuntu 24.04)**:
  ```bash
  source /opt/ros/jazzy/setup.bash
  ```
- **macOS (pixi global)**:
  ```bash
  source ~/.pixi/envs/default/setup.zsh
  ```
  pixi-global.toml の `default` env に `ros-jazzy-desktop` が入っている前提。

## 2. ワークスペース作成

```bash
mkdir -p ~/ros2_ws/src && cd ~/ros2_ws/src
git clone https://github.com/Nyanziba/Nyanziba_nav_core.git
git clone https://github.com/Nyanziba/Nyanziba_mbf_plugins.git
```

## 3. 上流の move_base_flex を取り込む

`texnitis_mbf_plugins/third_party/move_base_flex.repos` で SHA を固定済み:

```bash
cd ~/ros2_ws
vcs import src < src/texnitis_mbf_plugins/third_party/move_base_flex.repos
```

`src/move_base_flex/` に上流ソースが取り込まれます。

## 4. texnitis_nav_core を CMake で先に install

`texnitis_nav_core` は ament パッケージではないので、**colcon の前に**
CMake で個別にビルド・install する必要があります。

```bash
cd ~/ros2_ws
cmake -S src/texnitis_nav_core -B build/texnitis_nav_core \
    -DNAV_CORE_BUILD_TESTS=OFF \
    -DCMAKE_INSTALL_PREFIX=$PWD/install/texnitis_nav_core
cmake --build build/texnitis_nav_core -j --target install
```

## 5. rosdep で残依存を入れる

```bash
rosdep install --from-paths src --ignore-src -r -y --rosdistro jazzy
```

Eigen3 が無い場合は別途:

```bash
sudo apt-get install -y libeigen3-dev   # Ubuntu
brew install eigen                       # macOS
```

## 6. colcon build

```bash
export texnitis_nav_core_DIR=$PWD/install/texnitis_nav_core/lib/cmake/texnitis_nav_core
colcon build --symlink-install \
    --packages-up-to texnitis_mbf_bringup \
    --cmake-args -Dtexnitis_nav_core_DIR=$texnitis_nav_core_DIR
source install/setup.bash
```

`--packages-up-to texnitis_mbf_bringup` で必要な依存だけビルドします。
全パッケージを入れたいときは `--packages-up-to texnitis_mbf_webui` まで。

## 動作確認

```bash
ros2 pkg list | grep texnitis
# texnitis_mbf_bringup
# texnitis_mbf_common
# texnitis_mbf_controllers
# texnitis_mbf_planners
# texnitis_mbf_tools
# texnitis_mbf_webui

ros2 launch texnitis_mbf_bringup texnitis_mbf.launch.py
# move_base_flex node が起動し、planner / controller プラグインがロードされる
```

ノードが正常起動したら [quickstart.md](quickstart.md) で実際にゴールを送ります。

## トラブルシュート

### `find_package(texnitis_nav_core)` で `Could not find a package configuration file`

CMake の install を忘れているか、`texnitis_nav_core_DIR` を設定していません。
ステップ 4 と 6 を確認。

### `Could not find a package configuration file provided by "Eigen3"`

Eigen3 が無い。`apt install libeigen3-dev` または `brew install eigen`。

### `mbf_simple_core / mbf_simple_nav` が見つからない

`vcs import` のステップ 3 が失敗しています。`ls src/move_base_flex/` で
ディレクトリが存在することを確認。

### colcon build で `'mbf_msgs' not found`

`rosdep install` が走っていない、または `mbf_msgs` 依存が解決されていません。
`apt list --installed | grep ros-jazzy-mbf` で確認。
