/// @file
/// @brief node 単位で /map を共有購読するシングルトン。
///
/// mbf ros2 ブランチには `mbf_costmap_core` が存在しないため、各
/// プラグインが /map を独自に購読すると帯域・メモリの無駄が発生する。
/// `MapProvider::get(node)` は node ポインタごとに唯一のインスタンス
/// を `weak_ptr` テーブルで管理し、複数プラグインが同じ topic を要求
/// しても購読を 1 本に集約する。
///
/// @see docs/design_rationale.md の「なぜ MapProvider をシングルトン
///      にするのか」節

#pragma once

#include <rclcpp/rclcpp.hpp>

#include <nav_msgs/msg/occupancy_grid.hpp>

#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace texnitis::mbf_common {

/// @brief /map の最新値を共有するシングルトン。
class MapProvider {
   public:
    using OccupancyGrid    = nav_msgs::msg::OccupancyGrid;
    using OccupancyGridPtr = std::shared_ptr<const OccupancyGrid>;

    /// @brief node に紐付くシングルトンを返す。
    ///
    /// 同じ node ポインタに対しては毎回同じインスタンスを返す。
    /// node が破棄されると `weak_ptr` が expire し、次回呼び出し時に
    /// 自動的に新しいインスタンスを生成する。
    [[nodiscard]] static std::shared_ptr<MapProvider>
    get (const rclcpp::Node::SharedPtr &node);

    /// @brief 指定 topic の購読を開始する。すでに購読していれば
    ///        no-op。プラグインの `initialize()` から呼ぶ。
    ///
    /// @param topic     購読する OccupancyGrid topic 名。
    /// @param qos_depth 直近何件をキャッシュするか（既定 1 で十分）。
    void subscribe (const std::string &topic, size_t qos_depth = 1);

    /// @brief 指定 topic の最新メッセージを返す。未到着なら nullptr。
    [[nodiscard]] OccupancyGridPtr latest (const std::string &topic) const;

    /// @brief 購読中の全 topic 名（テスト用）。
    [[nodiscard]] std::vector<std::string> subscribedTopics () const;

   private:
    explicit MapProvider (rclcpp::Node::SharedPtr node) : node_ (std::move (node)) {}

    struct Subscription {
        rclcpp::Subscription<OccupancyGrid>::SharedPtr sub;
        OccupancyGridPtr                                latest;
    };

    rclcpp::Node::SharedPtr                                  node_;
    mutable std::shared_mutex                                mutex_;
    std::unordered_map<std::string, Subscription>            subs_;
};

}  // namespace texnitis::mbf_common
