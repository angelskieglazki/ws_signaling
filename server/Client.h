#pragma once

#include <string>
#include <websocketpp/common/connection_hdl.hpp>

namespace signaling {

class Client {
public:
  Client(websocketpp::connection_hdl hdl, std::string userId);

  websocketpp::connection_hdl getHandle() const;

  const std::string &getUserId() const;

  const std::string &getRoomId() const;

  void setRoomId(const std::string &roomId);

  bool isInRoom() const;

private:
  websocketpp::connection_hdl handle_;
  std::string userId_;
  std::string roomId_;
};

} // namespace signaling
