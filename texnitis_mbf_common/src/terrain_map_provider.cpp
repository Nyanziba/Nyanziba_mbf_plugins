#include "texnitis_mbf_common/terrain_map_provider.hpp"

namespace texnitis::mbf_common {
namespace {
struct Registry {
    std::mutex                                                            mutex;
    std::unordered_map<rclcpp::Node *, std::weak_ptr<TerrainMapProvider>> entries;
};
Registry &registry () {
    static Registry value;
    return value;
}
}  // namespace

std::shared_ptr<TerrainMapProvider> TerrainMapProvider::get (const rclcpp::Node::SharedPtr &node) {
    if (!node) return nullptr;
    auto                       &r = registry ();
    std::lock_guard<std::mutex> lock (r.mutex);
    auto                       &entry = r.entries[node.get ()];
    if (auto existing = entry.lock ()) return existing;
    auto created = std::shared_ptr<TerrainMapProvider> (new TerrainMapProvider (node));
    entry        = created;
    return created;
}

void TerrainMapProvider::subscribe (const std::string &topic, size_t depth) {
    {
        std::shared_lock<std::shared_mutex> lock (mutex_);
        if (entries_.count (topic)) return;
    }
    Entry entry;
    entry.sub = node_->create_subscription<Message> (topic, rclcpp::QoS (rclcpp::KeepLast (depth)).reliable ().transient_local (), [this, topic] (Message::ConstSharedPtr message) {
        std::unique_lock<std::shared_mutex> lock (mutex_);
        auto                                found = entries_.find (topic);
        if (found != entries_.end ()) found->second.latest = std::move (message);
    });
    std::unique_lock<std::shared_mutex> lock (mutex_);
    entries_.emplace (topic, std::move (entry));
}

TerrainMapProvider::MessagePtr TerrainMapProvider::latest (const std::string &topic) const {
    std::shared_lock<std::shared_mutex> lock (mutex_);
    const auto                          found = entries_.find (topic);
    return found == entries_.end () ? nullptr : found->second.latest;
}

}  // namespace texnitis::mbf_common
