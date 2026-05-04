# macOS / Linux ローカル開発環境（pixi + RoboStack）

Docker を経由せず、Apple Silicon でネイティブに動かすための pixi セットアップ。
RoboStack の conda チャンネルで ROS 2 Jazzy を取り込みます。

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

## トラブルシュート

| 症状 | 原因と対処 |
|---|---|
| `pixi install` が失敗（チャンネル解決） | RoboStack チャンネルが古い可能性。`pixi update` で再解決 |
| `colcon build` が `find_package(mbf_simple_core)` で失敗 | `pixi run setup` を再実行して `src/move_base_flex` が消えていないか確認 |
| 起動するが `outcome=58 (TF_ERROR)` | ホスト時計と launch 内の `use_sim_time` が噛み合っていない。`pixi run sim-e2e` を `--use_sim_time:=false` で（既定値） |
| AppleSilicon でクラッシュ | RoboStack の osx-arm64 build に当該パッケージがあるか確認。無ければ条件付きで `pixi run sim-interactive --controller=lookahead` で MPPI を回避 |
| `UnsupportedTypeSupport: Could not import 'rosidl_typesupport_c' for package 'mbf_msgs'` | RoboStack の rosidl_typesupport ABI が workspace 生成のメッセージと噛み合わない既知の制限。回避策は Docker (`docs/usage/sim_e2e.md`) で動かすか、workspace 生成時に `--cmake-args -DROSIDL_TYPESUPPORT_IMPL=...` で RoboStack 側に揃える（実験的） |

## 既知の制限

- `pixi run sim-e2e` / `sim-interactive`: 上記の typesupport 問題で macOS では完全には動かない可能性があります。CI と同じ Docker 経由なら確実です（[sim_e2e.md](sim_e2e.md)）。
- `pixi run build` と `pixi run test`（純 Python ユニット）は安定して動きます。ローカルでロジック確認したいときに使ってください。

## なぜ Docker ではないのか

CI は Linux x86 の固定環境を再現するため `osrf/ros:jazzy-desktop` を使いますが、
Apple Silicon 上では QEMU エミュレーションになり 3-5x 遅くなります。
ローカル開発はネイティブの方が iteration が速く、pixi は conda 環境を
シェルに被せず `pixi run <task>` で隔離できるので副作用も少ないです。
