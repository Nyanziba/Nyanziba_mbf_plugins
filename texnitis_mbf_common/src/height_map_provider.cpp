// HeightMapProvider: MapProvider と同型のシングルトン実装。occupancy
// grid 入力を高さグリッドとして扱う（int8 [0,100] を高さ離散値）。

#include "texnitis_mbf_common/height_map_provider.hpp"

#include <utility>

namespace texnitis::mbf_common {

namespace {

struct Registry {
    std::mutex                                                                 mutex;
    std::unordered_map<rclcpp::Node *, std::weak_ptr<HeightMapProvider>>       table;
};

Registry &registry () {
    static Registry r;
    return r;
}

}  // namespace

std::shared_ptr<HeightMapProvider>
HeightMapProvider::get (const rclcpp::Node::SharedPtr &node) {
    if (!node) {
        return nullptr;
    }
    auto &reg = registry ();
    std::lock_guard<std::mutex> guard (reg.mutex);
    auto &entry = reg.table[node.get ()];
    if (auto live = entry.lock ()) {
        return live;
    }
    auto provider = std::shared_ptr<HeightMapProvider> (new HeightMapProvider (node));
    entry         = provider;
    return provider;
}

void HeightMapProvider::subscribe (const std::string &topic, size_t qos_depth) {
    {
        std::shared_lock<std::shared_mutex> read (mutex_);
        if (subs_.find (topic) != subs_.end ()) {
            return;
        }
    }

    Subscription sub;
    auto qos = rclcpp::QoS (rclcpp::KeepLast (qos_depth)).reliable ().transient_local ();
    sub.sub = node_->create_subscription<OccupancyGrid> (
        topic, qos,
        [this, topic] (OccupancyGrid::ConstSharedPtr msg) {
            std::unique_lock<std::shared_mutex> write (mutex_);
            auto it = subs_.find (topic);
            if (it == subs_.end ()) {
                return;
            }
            it->second.latest = msg;
        });

    std::unique_lock<std::shared_mutex> write (mutex_);
    subs_.emplace (topic, std::move (sub));
}

HeightMapProvider::OccupancyGridPtr
HeightMapProvider::latest (const std::string &topic) const {
    std::shared_lock<std::shared_mutex> read (mutex_);
    auto it = subs_.find (topic);
    if (it == subs_.end ()) {
        return nullptr;
    }
    return it->second.latest;
}

std::vector<std::string> HeightMapProvider::subscribedTopics () const {
    std::shared_lock<std::shared_mutex> read (mutex_);
    std::vector<std::string> out;
    out.reserve (subs_.size ());
    for (const auto &kv : subs_) {
        out.push_back (kv.first);
    }
    return out;
}

}  // namespace texnitis::mbf_common
