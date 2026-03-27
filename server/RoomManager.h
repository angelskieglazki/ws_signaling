#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>
#include <websocketpp/common/connection_hdl.hpp>

namespace signaling {

class Room;
class Client;

class RoomManager {
public:
  std::shared_ptr<Room> getOrCreateRoom(const std::string &roomId);

  void addClientToRoom(const std::string &roomId,
                       std::shared_ptr<Client> client);

  bool removeClientFromRoom(const std::string &roomId,
                            websocketpp::connection_hdl hdl);

  std::shared_ptr<Room> findRoom(const std::string &roomId) const;

private:
  std::map<std::string, std::shared_ptr<Room>> rooms_;
};

} // namespace signaling
