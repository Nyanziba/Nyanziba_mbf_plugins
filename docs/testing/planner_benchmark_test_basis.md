# Planner benchmark テスト根拠

Test Basisは[実機比較手順](real_robot_planner_benchmark.md)の「失敗試行を速度統計に含めない」、
成功率・成功時到達時間中央値・実走行距離中央値を保存するという契約、および
`planner_benchmark.py`の公開データ型とする。

| Persona | 観点 | 対応テスト |
|---|---|---|
| P1 新人ユーザー | データがまだない状態 | `test_summarize_empty_input_is_empty` |
| P2 ベテラン | plannerごとの複数反復を集計 | `test_summarize_reports_success_rate_and_median_arrival_time` |
| P3 悪意ある操作者 | 失敗を短い到達時間として混入 | `test_summarize_does_not_treat_failed_trial_as_fast_arrival` |
| P4 整合性監査役 | 件数、成功数、中央値を内部辞書で直接確認 | `test_summarize_reports_success_rate_and_median_arrival_time` |
| P5 移行担当者 | N/A: 既存結果ファイル形式からの移行要件なし | N/A |
| P6 デグレ番人 | 既存tools/sim pytestと一括実行 | Python全テスト |
| P7 仕様懐疑者 | 成功率低下を時間短縮と誤認しない | 成功率と成功試行中央値を別集計する全ケース |

## 要確認

- 改善と判定する到達時間差、有意水準、必要試行数
- 実機の開始姿勢許容誤差とバッテリー電圧の許容範囲
- 動的転倒リスクを比較結果へ含めるセンサと閾値

根拠未確定のため、比較ツールはこれらを推測して合否判定しない。
