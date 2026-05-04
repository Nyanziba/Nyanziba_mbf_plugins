/// @file
/// @brief 高さグリッド版 MapProvider。OccupancyGrid 形式の高さ情報を
///        node 単位のシングルトンで購読する。
///
/// `HeightAwareAStarPlanner` のアダプタが initialize 時に subscribe
/// を呼び、planPath 直前に `latest()` で最新メッセージを `HeightGridView`
/// に変換して `nav_core::HeightProvider` 経由で渡す。

#pragma once

#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <nav_msgs/msg/occupancy_grid.hpp>
#include <rclcpp/rclcpp.hpp>

namespace texnitis::mbf_common {

class HeightMapProvider {
   public:
    using OccupancyGrid    = nav_msgs::msg::OccupancyGrid;
    using OccupancyGridPtr = std::shared_ptr<const OccupancyGrid>;

    [[nodiscard]] static std::shared_ptr<HeightMapProvider>
    get (const rclcpp::Node::SharedPtr &node);

    void subscribe (const std::string &topic, size_t qos_depth = 1);

    [[nodiscard]] OccupancyGridPtr latest (const std::string &topic) const;

    [[nodiscard]] std::vector<std::string> subscribedTopics () const;

   private:
    explicit HeightMapProvider (rclcpp::Node::SharedPtr node) : node_ (std::move (node)) {}

    struct Subscription {
        rclcpp::Subscription<OccupancyGrid>::SharedPtr sub;
        OccupancyGridPtr                                latest;
    };

    rclcpp::Node::SharedPtr                       node_;
    mutable std::shared_mutex                     mutex_;
    std::unordered_map<std::string, Subscription> subs_;
};

}  // namespace texnitis::mbf_common
