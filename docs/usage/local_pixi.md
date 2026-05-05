# macOS / Linux ローカル開発環境（pixi + RoboStack）

Docker を経由せず、Apple Silicon でネイティブに動かすための pixi セットアップ。
RoboStack の conda チャンネルで ROS 2 Jazzy を取り込みます。

## 重要: macOS では `scripts/pixi/activate.sh` が必須

`pixi run` は PATH と `CONDA_PREFIX` を立てるだけで、conda-forge の
`activate.d` スクリプト群（`CC`, `CXX`, `SDKROOT`, `MACOSX_DEPLOYMENT_TARGET`
を設定）を必ずしも source しません。さらに macOS の SIP は
`/usr/bin/env python3` shebang で `DYLD_LIBRARY_PATH` を strip するため、
rclpy が ROS の typesupport dylib を dlopen できず
`UnsupportedTypeSupport: Could not import 'rosidl_typesupport_c'` で落ちます。

[`scripts/pixi/activate.sh`](../../scripts/pixi/activate.sh) は:
- `activate.d/*.sh` を一括 source して `CC` / `CXX` / `SDKROOT` を立てる
- `Python3_EXECUTABLE` を env の `python3.12` に固定
- `MACOSX_DEPLOYMENT_TARGET=11.0` を保証
- `fix_shebangs()` で `install/<pkg>/lib/*.py` の shebang を env の python に書き換え（colcon が毎回 `/usr/bin/env` に戻すので build 後に必ず実行）
- 便利関数 `cb` （= `colcon build` + `fix_shebangs`）を提供

`pixi run build` / `pixi run sim-e2e` / `pixi run sim-interactive` は
このスクリプトを内部で source するので、通常は意識せず使えます。
手動で `colcon build` するときだけ `source scripts/pixi/activate.sh` してから。

## 前提

- [pixi](https://pixi.sh) (>= 0.40)
- 100 GB 程度の空き（Jazzy 一式 + colcon ws）

## 初回セットアップ

```bash
cd texnitis_mbf_plugins
pixi install                         # ROS 2 Jazzy + colcon + nav2 など解決
pixi run setup                       # vcs import で nav_core と mbf を src/ に
pixi run build                       # nav_core → mbf → texnitis 全部 build
```

`pixi run build` は依存に応じて差分ビルドします。

## 実行

```bash
# 1 ゴール走破の E2E チェック（成功なら exit 0）
pixi run sim-e2e

# rviz2 や waypoint_sender から手動で goal を投げたいとき
pixi run sim-interactive

# 純 Python ユニットテスト（ROS 不要）
pixi run test
```

## 対話的に操作したいとき

```bash
# env + workspace install + DYLD_LIBRARY_PATH を整えたサブシェルに入る
# ($SHELL を尊重するので zsh 利用者は zsh のまま)
pixi run shell
ros2 topic list
ros2 topic echo /move_base_flex/astar/plan
exit  # サブシェルを抜ける

# あるいは現在のシェルに env を注入したい場合（rc を汚さない）
eval "$(pixi run activate-cmd)"

# 単発で ros2 を呼びたいだけのとき
pixi run ros2 topic list
pixi run ros2 action send_goal /move_base_flex/move_base mbf_msgs/action/MoveBase \
    "{target_pose: {header: {frame_id: 'map'}, pose: {position: {x: 1.0}}}, planner: 'astar', controller: 'lookahead'}"
```

## トラブルシュート

| 症状 | 原因と対処 |
|---|---|
| `pixi install` が失敗（チャンネル解決） | RoboStack チャンネルが古い可能性。`pixi update` で再解決 |
| `colcon build` が `find_package(mbf_simple_core)` で失敗 | `pixi run setup` を再実行して `src/move_base_flex` が消えていないか確認 |
| 起動するが `outcome=58 (TF_ERROR)` | ホスト時計と launch 内の `use_sim_time` が噛み合っていない。`pixi run sim-e2e` を `--use_sim_time:=false` で（既定値） |
| AppleSilicon でクラッシュ | RoboStack の osx-arm64 build に当該パッケージがあるか確認。無ければ条件付きで `pixi run sim-interactive --controller=lookahead` で MPPI を回避 |
| `UnsupportedTypeSupport: Could not import 'rosidl_typesupport_c' for package 'mbf_msgs'` | macOS SIP が `/usr/bin/env python3` shebang で `DYLD_LIBRARY_PATH` を strip するのが原因。`scripts/pixi/activate.sh` の `fix_shebangs` が自動で対処するので、`colcon build` を直接叩いた場合のみ手動で `fix_shebangs` を実行してください |
| 終了時 `mbf_simple_nav_node ... exit code -11` | E2E PASS 判定後のシャットダウン時に発生する SEGV。launch shutdown 中のクリーンアップ順に起因し、E2E 結果には影響しません |

## ネイティブ実行の検証済み所要時間（M1 Pro）

| ステップ | 時間 |
|---|---|
| `pixi install` (初回) | ~30 s |
| `pixi run setup` | ~10 s |
| `pixi run build` (cold) | ~2 min 18 s |
| `pixi run sim-e2e` | ~14 s（goal 送信から到達まで） |

## なぜ Docker ではないのか

CI は Linux x86 の固定環境を再現するため `osrf/ros:jazzy-desktop` を使いますが、
Apple Silicon 上では QEMU エミュレーションになり 3-5x 遅くなります。
ローカル開発はネイティブの方が iteration が速く、pixi は conda 環境を
シェルに被せず `pixi run <task>` で隔離できるので副作用も少ないです。
