#include "service/MeshSimulatorService.h"
#include <iostream>

namespace upimesh {
namespace service {

// --- VirtualDevice ---
void VirtualDevice::hold(const model::MeshPacket &packet) {
  std::lock_guard<std::mutex> lock(mutex_);
  heldPackets_[packet.packetId] = packet;
}

bool VirtualDevice::holds(const std::string &packetId) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return heldPackets_.find(packetId) != heldPackets_.end();
}

int VirtualDevice::packetCount() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return heldPackets_.size();
}

std::vector<model::MeshPacket> VirtualDevice::getHeldPackets() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<model::MeshPacket> result;
  result.reserve(heldPackets_.size());
  for (const auto &[id, pkt] : heldPackets_) {
    result.push_back(pkt);
  }
  return result;
}

void VirtualDevice::clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  heldPackets_.clear();
}

// --- MeshSimulatorService ---
MeshSimulatorService::MeshSimulatorService() { seedDefaultDevices(); }

void MeshSimulatorService::seedDefaultDevices() {
  devices_["phone-alice"] =
      std::make_shared<VirtualDevice>("phone-alice", false);
  devices_["phone-stranger1"] =
      std::make_shared<VirtualDevice>("phone-stranger1", false);
  devices_["phone-stranger2"] =
      std::make_shared<VirtualDevice>("phone-stranger2", false);
  devices_["phone-stranger3"] =
      std::make_shared<VirtualDevice>("phone-stranger3", false);
  devices_["phone-bridge"] =
      std::make_shared<VirtualDevice>("phone-bridge", true);
}

std::vector<std::shared_ptr<VirtualDevice>>
MeshSimulatorService::getDevices() const {
  std::vector<std::shared_ptr<VirtualDevice>> result;
  for (const auto &[id, dev] : devices_)
    result.push_back(dev);
  return result;
}

std::shared_ptr<VirtualDevice>
MeshSimulatorService::getDevice(const std::string &id) const {
  auto it = devices_.find(id);
  return it != devices_.end() ? it->second : nullptr;
}

void MeshSimulatorService::inject(const std::string &senderDeviceId,
                                  const model::MeshPacket &packet) {
  auto sender = getDevice(senderDeviceId);
  if (!sender)
    throw std::invalid_argument("Unknown device: " + senderDeviceId);
  sender->hold(packet);
  std::cout << "Packet " << packet.packetId.substr(0, 8) << " injected at "
            << senderDeviceId << " (TTL=" << packet.ttl << ")\n";
}

GossipResult MeshSimulatorService::gossipOnce() {
  int transfers = 0;
  auto deviceList = getDevices();

  // Snapshot what each device holds
  std::unordered_map<std::string, std::vector<model::MeshPacket>> snapshot;
  for (const auto &d : deviceList) {
    snapshot[d->deviceId] = d->getHeldPackets();
  }

  for (const auto &src : deviceList) {
    for (const auto &pkt : snapshot[src->deviceId]) {
      if (pkt.ttl <= 0)
        continue;
      for (const auto &dst : deviceList) {
        if (dst == src)
          continue;
        if (dst->holds(pkt.packetId))
          continue;

        model::MeshPacket copy = pkt;
        copy.ttl -= 1; // Decrement TTL
        dst->hold(copy);
        transfers++;
      }
    }
  }

  std::cout << "Gossip round complete: " << transfers << " packet transfers\n";
  return {transfers, snapshotMap()};
}

std::map<std::string, int> MeshSimulatorService::snapshotMap() const {
  std::map<std::string, int> m;
  for (const auto &[id, d] : devices_) {
    m[id] = d->packetCount();
  }
  return m;
}

std::vector<BridgeUpload> MeshSimulatorService::collectBridgeUploads() const {
  std::vector<BridgeUpload> out;
  for (const auto &[id, d] : devices_) {
    if (!d->hasInternet)
      continue;
    for (const auto &pkt : d->getHeldPackets()) {
      out.push_back({d->deviceId, pkt});
    }
  }
  return out;
}

void MeshSimulatorService::resetMesh() {
  for (const auto &[id, d] : devices_) {
    d->clear();
  }
}

} // namespace service
} // namespace upimesh
