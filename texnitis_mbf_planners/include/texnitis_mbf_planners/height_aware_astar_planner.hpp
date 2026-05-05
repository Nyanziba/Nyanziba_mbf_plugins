/// @file
/// @brief HeightAwareAStarPlanner mbf アダプタ。
///
/// `MapProvider` で /map を、`HeightMapProvider` で /height_grid を
/// それぞれ共有購読し、planPath 直前に `nav_core::HeightProvider` の
/// 実装に最新メッセージを差し込んで core を呼び出す。

#pragma once

#include <atomic>
#include <memory>
#include <string>

#include <mbf_simple_core/simple_planner.h>
#include <nav_msgs/msg/path.hpp>
#include <rclcpp/rclcpp.hpp>

#include <texnitis_mbf_common/height_map_provider.hpp>
#include <texnitis_mbf_common/map_provider.hpp>
#include <texnitis_nav_core/height_provider.hpp>
#include <texnitis_nav_core/planners/height_aware_astar_planner.hpp>

namespace texnitis::mbf_planners {

class HeightAwareAStarPlanner final : public mbf_simple_core::SimplePlanner {
   public:
    HeightAwareAStarPlanner ()           = default;
    ~HeightAwareAStarPlanner () override = default;

    void initialize (const std::string             name,
                     const rclcpp::Node::SharedPtr &node_handle) override;

    uint32_t makePlan (const geometry_msgs::msg::PoseStamped               &start,
                       const geometry_msgs::msg::PoseStamped               &goal,
                       double                                                tolerance,
                       std::vector<geometry_msgs::msg::PoseStamped>         &plan,
                       double                                                &cost,
                       std::string                                           &message) override;

    bool cancel () override;

   private:
    /// HeightMapProvider に問い合わせて latest メッセージを HeightGridView
    /// に変換する HeightProvider 実装。`makePlan` 中だけ生存する。
    class CachedHeightProvider final : public ::texnitis::nav_core::HeightProvider {
       public:
        void set (::texnitis::mbf_common::HeightMapProvider::OccupancyGridPtr msg) noexcept {
            msg_ = std::move (msg);
        }

        [[nodiscard]] bool getLatest (::texnitis::nav_core::HeightGridView &out_view) const override {
            if (!msg_) {
                return false;
            }
            out_view.data       = msg_->data.data ();
            out_view.width      = static_cast<int> (msg_->info.width);
            out_view.height     = static_cast<int> (msg_->info.height);
            out_view.resolution = msg_->info.resolution;
            out_view.origin_x   = msg_->info.origin.position.x;
            out_view.origin_y   = msg_->info.origin.position.y;
            return true;
        }

       private:
        ::texnitis::mbf_common::HeightMapProvider::OccupancyGridPtr msg_;
    };

    std::string                                                       name_;
    rclcpp::Node::WeakPtr                                              node_;
    std::unique_ptr<::texnitis::nav_core::HeightAwareAStarPlanner>      core_;
    std::shared_ptr<::texnitis::mbf_common::MapProvider>                map_provider_;
    std::shared_ptr<::texnitis::mbf_common::HeightMapProvider>          height_provider_;
    CachedHeightProvider                                                cached_height_;

    std::string map_topic_{"/map"};
    std::string height_topic_{"/height_grid"};
    std::string global_frame_{"map"};

    /// rviz2 / 監視向けに `<planner_name>/plan` (`nav_msgs/Path`) を発行。
    rclcpp::Publisher<nav_msgs::msg::Path>::SharedPtr plan_pub_;

    std::atomic<bool> cancel_requested_{false};
};

}  // namespace texnitis::mbf_planners
