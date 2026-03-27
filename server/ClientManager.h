#pragma once

#include <map>
#include <memory>
#include <string>
#include <websocketpp/common/connection_hdl.hpp>

namespace signaling {

class Client;

/**
 * @brief Менеджер клиентов - управление подключениями и генерация ID
 */
class ClientManager {
public:
  ClientManager();

  std::shared_ptr<Client> registerClient(websocketpp::connection_hdl hdl);

  std::shared_ptr<Client> removeClient(websocketpp::connection_hdl hdl);

  std::shared_ptr<Client> findClient(websocketpp::connection_hdl hdl) const;

  std::shared_ptr<Client> findClientByUserId(const std::string &userId) const;

  size_t getClientCount() const;

  void leaveRoom(websocketpp::connection_hdl hdl);

  void setClientRoom(websocketpp::connection_hdl hdl,
                     const std::string &roomId);

private:
  std::string generateUserId();

  std::map<websocketpp::connection_hdl, std::shared_ptr<Client>,
           std::owner_less<websocketpp::connection_hdl>>
      clients_;
  int userCounter_;
};

} // namespace signaling
