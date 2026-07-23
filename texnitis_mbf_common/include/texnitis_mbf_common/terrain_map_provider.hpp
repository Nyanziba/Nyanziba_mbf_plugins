#pragma once

#include <rclcpp/rclcpp.hpp>
#include <texnitis_navigation_interfaces/msg/terrain_grid.hpp>

#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace texnitis::mbf_common {

class TerrainMapProvider {
   public:
    using Message    = texnitis_navigation_interfaces::msg::TerrainGrid;
    using MessagePtr = std::shared_ptr<const Message>;
    [[nodiscard]] static std::shared_ptr<TerrainMapProvider> get (const rclcpp::Node::SharedPtr &node);
    void                                                     subscribe (const std::string &topic, size_t qos_depth = 1);
    [[nodiscard]] MessagePtr                                 latest (const std::string &topic) const;

   private:
    explicit TerrainMapProvider (rclcpp::Node::SharedPtr node) : node_ (std::move (node)) {}
    struct Entry {
        rclcpp::Subscription<Message>::SharedPtr sub;
        MessagePtr                               latest;
    };
    rclcpp::Node::SharedPtr                node_;
    mutable std::shared_mutex              mutex_;
    std::unordered_map<std::string, Entry> entries_;
};

}  // namespace texnitis::mbf_common
