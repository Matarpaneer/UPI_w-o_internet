#pragma once

#include "model/MeshPacket.h"
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace upimesh {
namespace service {

class VirtualDevice {
public:
  VirtualDevice(const std::string &id, bool internet)
      : deviceId(id), hasInternet(internet) {}

  std::string deviceId;
  bool hasInternet;

  void hold(const model::MeshPacket &packet);
  bool holds(const std::string &packetId) const;
  int packetCount() const;
  std::vector<model::MeshPacket> getHeldPackets() const;
  void clear();

private:
  std::unordered_map<std::string, model::MeshPacket> heldPackets_;
  mutable std::mutex mutex_;
};

struct GossipResult {
  int transfers;
  std::map<std::string, int> deviceCounts;
};

struct BridgeUpload {
  std::string bridgeNodeId;
  model::MeshPacket packet;
};

class MeshSimulatorService {
public:
  MeshSimulatorService();

  std::vector<std::shared_ptr<VirtualDevice>> getDevices() const;
  std::shared_ptr<VirtualDevice> getDevice(const std::string &id) const;

  void inject(const std::string &senderDeviceId,
              const model::MeshPacket &packet);
  GossipResult gossipOnce();
  std::map<std::string, int> snapshotMap() const;
  std::vector<BridgeUpload> collectBridgeUploads() const;
  void resetMesh();

private:
  std::unordered_map<std::string, std::shared_ptr<VirtualDevice>> devices_;
  void seedDefaultDevices();
};

} // namespace service
} // namespace upimesh
