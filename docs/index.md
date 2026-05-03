# texnitis_mbf_plugins ドキュメント

`naturerobots/move_base_flex`（ros2 ブランチ）に対応する planner / controller プラグイン群と、bringup・WebUI・waypoint sender などのツールを提供する。

## 読み始め

| 目的 | まず読むページ |
|---|---|
| 全体像をつかむ | [architecture.md](architecture.md) |
| なぜこの設計なのかを知る | [design_rationale.md](design_rationale.md) |
| ソースを読み解きたい | [reading_guide.md](reading_guide.md) |
| 試しに動かしたい | [usage/quickstart.md](usage/quickstart.md) |
| インストール手順 | [usage/installation.md](usage/installation.md) |
| 旧 yaml からの移行 | [usage/parameter_mapping.md](usage/parameter_mapping.md) |
| WebUI の使い方 | [usage/webui.md](usage/webui.md) |
| ウェイポイント走行 | [usage/waypoint_sender.md](usage/waypoint_sender.md) |
| 自作プラグインを足したい | [usage/custom_plugin.md](usage/custom_plugin.md) |
| outcome と nav_core enum の対応 | [reference/error_codes.md](reference/error_codes.md) |
| 公開する topic / action / parameter | [reference/topics_and_actions.md](reference/topics_and_actions.md) |

## 関連リポジトリ

- コアアルゴリズム（ROS 非依存）: [Nyanziba/texnitis_nav_core](https://github.com/Nyanziba/texnitis_nav_core)
- 上流: `naturerobots/move_base_flex` の `ros2` ブランチを `third_party/move_base_flex.repos` で SHA 固定
