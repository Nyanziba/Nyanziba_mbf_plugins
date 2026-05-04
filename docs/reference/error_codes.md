# エラーコード対応表

mbf アクションの `outcome` (uint32_t) と `texnitis_nav_core` の
`PlanResult` / `ControllerResult` enum の双方向対応表です。
実装は
[`texnitis_mbf_common/include/texnitis_mbf_common/error_codes.hpp`](../../texnitis_mbf_common/include/texnitis_mbf_common/error_codes.hpp)
に集約されています。

## Planner 系 outcome

| outcome | 名称 | nav_core 由来 | 典型的な原因 |
|---|---|---|---|
| 0 | SUCCESS | `Success` | OK |
| 50 | NO_PATH_FOUND | `NoPathFound` | 探索完走したが解なし |
| 51 | CANCELED | `Cancelled` | クライアントが cancel |
| 52 | INVALID_START | `StartOutOfBounds` / `InvalidMap` | start がマップ外 |
| 53 | INVALID_GOAL | `GoalOutOfBounds` | goal がマップ外 |
| 54 | BLOCKED_START | `StartInCollision` | start が壁の中 |
| 55 | BLOCKED_GOAL | `GoalInCollision` | goal が壁の中 |
| 56 | PAT_EXCEEDED | `PatExceeded` | 計画完走時間超過（現状未使用） |
| 57 | EMPTY_PATH | `EmptyPath` | 結果 path が空 |
| 58 | TF_ERROR | `TfError` | TF 解決失敗 |
| 59 | NOT_INITIALIZED | `NotInitialized` | /map 未到着 / HeightProvider 未到着 |
| 60 | INTERNAL_ERROR | `InternalError` | 実装バグ。issue 起票推奨 |

## Controller 系 outcome

| outcome | 名称 | nav_core 由来 | 典型的な原因 |
|---|---|---|---|
| 0 | SUCCESS | `Success` / `GoalReached` | OK |
| 100 | NO_VALID_CMD | `ComputationFailed` | NaN / Inf / アルゴリズム failure |
| 101 | PAT_EXCEEDED | — | 制御完走時間超過（現状未使用） |
| 102 | COLLISION | — | 衝突検知（コア未対応、シミュレータ側で集計） |
| 103 | TIMED_OUT | — | controller 計算がフレーム時間を超えた（mbf 側） |
| 104 | INVALID_PATH | `EmptyPath` / `PathTooFarFromRobot` | setPlan が空 / 経路から離脱 |
| 106 | TF_ERROR | — | TF 解決失敗 |
| 107 | CANCELED | `Cancelled` | controller cancel |
| 108 | NOT_INITIALIZED | `NotInitialized` | 必須パラメータ未設定 |
| 109 | OSCILLATION | — | 振動検知（現状未対応） |
| 110 | INTERNAL_ERROR | `InternalError` | 実装バグ |

## 設計判断

- **`Success` と `GoalReached` を controller では同じ outcome=0 にマップ**
  しています。mbf 上の意味が同じため。区別したい場合は `nav_core::ControllerResult`
  を見るか、controller の `isGoalReached` を別途呼びます。
- **`COLLISION (102)` / `OSCILLATION (109)`** は現状 nav_core 側で検知して
  いません。これらの assert はシミュレータ (`tests_python/test_sim_scenarios.py`)
  で `metrics.collision_count` を見て行います。
- enum を拡張するときは `error_codes.hpp` の `switch` 文を更新すれば
  コンパイラが警告を出すので、対応漏れを機械的に防げます。
