# texnitis_mbf_plugins ドキュメント

`naturerobots/move_base_flex`（ros2 ブランチ）に対応する planner / controller
プラグイン群と、bringup・WebUI・waypoint sender などのツールを提供する。
コアアルゴリズムは別リポ
[`texnitis_nav_core`](https://github.com/Nyanziba/texnitis_nav_core)
に置かれており、本リポは **薄いアダプタ** に徹する。

## 読み始め

| 目的 | まず読むページ |
|---|---|
| インストール手順 | [usage/installation.md](usage/installation.md) |
| 最初の 1 ゴール走破 | [usage/quickstart.md](usage/quickstart.md) |
| 旧 yaml からの移行 | [usage/parameter_mapping.md](usage/parameter_mapping.md) |
| WebUI の使い方 | [usage/webui.md](usage/webui.md) |
| ウェイポイント走行 | [usage/waypoint_sender.md](usage/waypoint_sender.md) |
| 自作プラグインを足す | [usage/custom_plugin.md](usage/custom_plugin.md) |
| ROS 2 シム + E2E チェック | [usage/sim_e2e.md](usage/sim_e2e.md) |
| 全体像をつかむ | [architecture.md](architecture.md) |
| なぜこの設計なのか | [design_rationale.md](design_rationale.md) |
| ソースの読み方 | [reading_guide.md](reading_guide.md) |
| エラーコードの意味 | [reference/error_codes.md](reference/error_codes.md) |
| 公開 topic / action / param | [reference/topics_and_actions.md](reference/topics_and_actions.md) |

## 関連リポジトリ

- [`texnitis_nav_core`](https://github.com/Nyanziba/texnitis_nav_core) — コアアルゴリズム（ROS 非依存）
- [`naturerobots/move_base_flex`](https://github.com/naturerobots/move_base_flex) — 上流の mbf 本体（ros2 ブランチ）

## ライセンス

MIT License — リポ root の `LICENSE`。
