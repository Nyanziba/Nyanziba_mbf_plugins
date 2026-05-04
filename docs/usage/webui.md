# WebUI

`texnitis_mbf_webui` は、mbf の状態を rosbridge_websocket 経由で閲覧する
HTML/JS ダッシュボードです。`/map`, 計画経路, ロボット pose, ゴール pose,
nav_state JSON を 1 画面に表示します。

## 起動

```bash
ros2 launch texnitis_mbf_webui webui.launch.py
```

これは内部で 2 つを起動します:

| ノード | ポート | 役割 |
|---|---|---|
| `rosbridge_websocket` | 9090 | ROS topic / action を WebSocket 経由で公開 |
| `python -m http.server` | 8000 | `webui/` の静的ファイル配信 |

ブラウザで <http://localhost:8000> を開きます。

## ポート変更

```bash
ros2 launch texnitis_mbf_webui webui.launch.py http_port:=8080 rosbridge_port:=9091
```

## nav_state を受け取る

WebUI は legacy 互換の `nav_state` JSON topic を購読します。
mbf 単体ではこの topic を出さないので、`texnitis_mbf_tools` の
`nav_state_publisher.py` を別ターミナルで起動してください:

```bash
ros2 run texnitis_mbf_tools nav_state_publisher.py
```

これが `/map`, `/move_base_flex/global_plan`, `/move_base_flex/robot_pose`,
`/goal_pose` を集約して `/texnitis_mbf/nav_state` に JSON を流します。
WebUI 側でこの topic 名（または旧 `/move_base_like_node/nav_state` への remap）
を購読する設定にしてください。

## 同梱されているページ

- `index.html` — ダッシュボード（マップ + ロボット位置 + 経路 + 状態 JSON）
- `map-editor.html` — 占有グリッドエディタ。CSV / YAML 出力可能

## 改修方針

WebUI 自体は legacy 時代のものをそのまま流用しています。topic 名の
remap だけで動くよう設計されており、必要なら以下のいずれかで対応:

- `nav_state_publisher.py` のパラメータで topic 名を上書き
- `webui/app.js` の `nav_state_topic` 変数を直接書き換え

カスタム topic を増やす場合は `app.js` 内の `subscribers` ブロックに
追記すれば、`ROSLIB.Topic` で購読できます。
