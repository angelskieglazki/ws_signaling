#include "MessageHandler.h"
#include "Client.h"
#include "ClientManager.h"
#include "Room.h"
#include "RoomManager.h"

#include <iostream>

namespace signaling {

MessageHandler::MessageHandler(std::shared_ptr<ClientManager> clientManager,
                               std::shared_ptr<RoomManager> roomManager,
                               SendCallback sendCallback)
    : clientManager_(std::move(clientManager)),
      roomManager_(std::move(roomManager)),
      sendCallback_(std::move(sendCallback)) {}

void MessageHandler::handleMessage(websocketpp::connection_hdl hdl,
                                   const std::string &payload) {
  std::cout << "Received: " << payload << std::endl;

  try {
    json data = json::parse(payload);
    std::string type = data.value("type", "");

    auto client = clientManager_->findClient(hdl);
    if (!client) {
      return;
    }

    if (type == "join") {
      handleJoin(client, data);
    } else if (type == "leave") {
      handleLeave(client);
    } else if (type == "offer" || type == "answer" || type == "ice-candidate") {
      handleWebRTCMessage(client, data);
    } else if (type == "chat") {
      handleChat(client, data);
    }
  } catch (const json::exception &e) {
    std::cerr << "JSON parse error: " << e.what() << std::endl;
    json error = {{"type", "error"}, {"message", "Invalid JSON"}};
    sendToClient(hdl, error);
  }
}

void MessageHandler::handleOpen(websocketpp::connection_hdl hdl) {
  auto client = clientManager_->registerClient(hdl);

  std::cout << "Client " << client->getUserId()
            << " connected. Total: " << clientManager_->getClientCount()
            << std::endl;

  json welcome = {{"type", "welcome"},
                  {"userId", client->getUserId()},
                  {"message", "Signaling ready"}};
  sendToClient(hdl, welcome);
}

void MessageHandler::handleClose(websocketpp::connection_hdl hdl) {
  auto client = clientManager_->findClient(hdl);
  if (!client) {
    return;
  }

  std::string userId = client->getUserId();
  std::string roomId = client->getRoomId();

  // Удаляем из комнаты и уведомляем остальных
  if (client->isInRoom()) {
    bool roomRemoved = roomManager_->removeClientFromRoom(roomId, hdl);

    if (!roomRemoved) {
      json leaveMsg = {{"type", "user-left"}, {"userId", userId}};
      broadcastToRoom(roomId, hdl, leaveMsg);
    }
  }

  clientManager_->removeClient(hdl);

  std::cout << "Client " << userId
            << " disconnected. Total: " << clientManager_->getClientCount()
            << std::endl;
}

void MessageHandler::handleJoin(std::shared_ptr<Client> client,
                                const json &data) {
  std::string userId = client->getUserId();
  std::string currentRoomId = client->getRoomId();

  // Выход из текущей комнаты
  if (client->isInRoom()) {
    roomManager_->removeClientFromRoom(currentRoomId, client->getHandle());
    json leaveMsg = {{"type", "user-left"}, {"userId", userId}};
    broadcastToRoom(currentRoomId, client->getHandle(), leaveMsg);
  }

  // Вход в новую комнату
  std::string newRoomId = data.value("room", "");
  if (newRoomId.empty()) {
    json error = {{"type", "error"}, {"message", "Room ID required"}};
    sendToClient(client->getHandle(), error);
    return;
  }

  roomManager_->addClientToRoom(newRoomId, client);

  // Собираем список участников
  auto room = roomManager_->findRoom(newRoomId);
  std::vector<std::string> participants;
  if (room) {
    for (const auto &conn : room->getClients()) {
      auto participant = clientManager_->findClient(conn);
      if (participant) {
        participants.push_back(participant->getUserId());
      }
    }
  }

  // Отправляем подтверждение
  json response = {{"type", "room-joined"},
                   {"room", newRoomId},
                   {"participants", participants}};
  sendToClient(client->getHandle(), response);

  // Уведомляем других участников
  json joinMsg = {{"type", "user-joined"}, {"userId", userId}};
  broadcastToRoom(newRoomId, client->getHandle(), joinMsg);

  std::cout << "User " << userId << " joined room " << newRoomId << " ("
            << participants.size() << " participants)" << std::endl;
}

void MessageHandler::handleLeave(std::shared_ptr<Client> client) {
  if (!client->isInRoom()) {
    return;
  }

  std::string userId = client->getUserId();
  std::string roomId = client->getRoomId();

  roomManager_->removeClientFromRoom(roomId, client->getHandle());

  json leaveMsg = {{"type", "user-left"}, {"userId", userId}};
  broadcastToRoom(roomId, client->getHandle(), leaveMsg);

  clientManager_->leaveRoom(client->getHandle());

  json response = {{"type", "left"}};
  sendToClient(client->getHandle(), response);
}

void MessageHandler::handleWebRTCMessage(std::shared_ptr<Client> client,
                                         const json &data) {
  if (!client->isInRoom()) {
    return;
  }

  std::string to = data.value("to", "");
  if (to.empty()) {
    return;
  }

  std::string roomId = client->getRoomId();

  // Добавляем from если не указан
  json modifiedData = data;
  if (!modifiedData.contains("from")) {
    modifiedData["from"] = client->getUserId();
  }

  sendToUser(roomId, to, modifiedData);
}

void MessageHandler::handleChat(std::shared_ptr<Client> client,
                                const json &data) {
  if (!client->isInRoom()) {
    return;
  }

  std::cout << "Broadcasting message: " << data.dump() << std::endl;
  broadcastToRoom(client->getRoomId(), client->getHandle(), data);
}

void MessageHandler::broadcastToRoom(const std::string &roomId,
                                     websocketpp::connection_hdl exclude,
                                     const json &msg) {
  auto room = roomManager_->findRoom(roomId);
  if (!room) {
    return;
  }

  for (const auto &client : room->getClients()) {
    if (client.owner_before(exclude) || exclude.owner_before(client)) {
      sendToClient(client, msg);
    }
  }
}

void MessageHandler::sendToUser(const std::string &roomId,
                                const std::string &userId, const json &msg) {
  auto room = roomManager_->findRoom(roomId);
  if (!room) {
    return;
  }

  for (const auto &hdl : room->getClients()) {
    auto client = clientManager_->findClient(hdl);
    if (client && client->getUserId() == userId) {
      sendToClient(hdl, msg);
      return;
    }
  }
}

void MessageHandler::sendToClient(websocketpp::connection_hdl hdl,
                                  const json &msg) {
  if (sendCallback_) {
    sendCallback_(hdl, msg);
  }
}

} // namespace signaling
