#include "webrtcmanager.h"

#include <QDebug>
#include <QJsonDocument>

WebRTCManager::WebRTCManager(QObject *parent) : QObject(parent) {
}

WebRTCManager::~WebRTCManager() {
  peers_.clear();
}

void WebRTCManager::initialize(const QString &userId) {
  localUserId_ = userId;
  qDebug() << "WebRTCManager initialized for user:" << userId;
}

rtc::Configuration WebRTCManager::createConfig() {
  rtc::Configuration config;
  config.iceServers.emplace_back("stun:stun.l.google.com:19302");
  config.iceServers.emplace_back("stun:stun1.l.google.com:19302");
  return config;
}

void WebRTCManager::setupPeerCallbacks(
    const QString &peerId, const std::shared_ptr<rtc::PeerConnection> &pc,
    bool isOfferer) {

  pc->onStateChange([this, peerId](rtc::PeerConnection::State state) {
    qDebug() << "PeerConnection state changed:" << peerId << static_cast<int>(state);
    if (state == rtc::PeerConnection::State::Connected) {
      qDebug() << "PeerConnection CONNECTED:" << peerId;
    } else if (state == rtc::PeerConnection::State::Disconnected ||
               state == rtc::PeerConnection::State::Failed ||
               state == rtc::PeerConnection::State::Closed) {
      qDebug() << "PeerConnection DISCONNECTED:" << peerId;
      emit peerDisconnected(peerId);
    }
  });

  pc->onGatheringStateChange(
      [this, peerId](rtc::PeerConnection::GatheringState state) {
        qDebug() << "ICE gathering state:" << peerId << static_cast<int>(state);
      });

  pc->onLocalDescription([this, peerId](rtc::Description description) {
    QJsonObject msg;
    msg["type"] = QString::fromStdString(description.typeString());
    msg["from"] = localUserId_;
    msg["to"] = peerId;
    msg["sdp"] = QString::fromStdString(std::string(description));
    emit signalingMessage(msg);
  });

  pc->onLocalCandidate([this, peerId](rtc::Candidate candidate) {
    QJsonObject msg;
    msg["type"] = "ice-candidate";
    msg["from"] = localUserId_;
    msg["to"] = peerId;
    msg["candidate"] = QString::fromStdString(candidate);
    emit signalingMessage(msg);
  });

  // Создаем DataChannel для аудио (только offerer)
  if (isOfferer) {
    auto dc = pc->createDataChannel("audio");
    qDebug() << "Created DataChannel for peer:" << peerId;
    
    dc->onOpen([this, peerId, dc]() {
      qDebug() << "DataChannel OPENED with peer:" << peerId;
      if (peers_.contains(peerId)) {
        peers_[peerId]->dataChannel = dc;
        peers_[peerId]->connected = true;
      }
    });

    dc->onClosed([this, peerId]() {
      qDebug() << "DataChannel CLOSED with peer:" << peerId;
      if (peers_.contains(peerId)) {
        peers_[peerId]->connected = false;
      }
    });

    dc->onMessage([this, peerId](rtc::message_variant data) {
      if (std::holds_alternative<rtc::binary>(data)) {
        auto &binary = std::get<rtc::binary>(data);
        QByteArray audioData(reinterpret_cast<const char *>(binary.data()), binary.size());
        emit audioReceived(peerId, audioData);
      }
    });
  }

  // Принимаем DataChannel (answerer)
  pc->onDataChannel([this, peerId](std::shared_ptr<rtc::DataChannel> dc) {
    qDebug() << "DataChannel RECEIVED from peer:" << peerId;
    
    dc->onOpen([this, peerId, dc]() {
      qDebug() << "DataChannel OPENED (received) with peer:" << peerId;
      if (peers_.contains(peerId)) {
        peers_[peerId]->dataChannel = dc;
        peers_[peerId]->connected = true;
        emit peerConnected(peerId);
      }
    });

    dc->onClosed([this, peerId]() {
      qDebug() << "DataChannel CLOSED (received) with peer:" << peerId;
      if (peers_.contains(peerId)) {
        peers_[peerId]->connected = false;
      }
    });

    dc->onMessage([this, peerId](rtc::message_variant data) {
      if (std::holds_alternative<rtc::binary>(data)) {
        auto &binary = std::get<rtc::binary>(data);
        QByteArray audioData(reinterpret_cast<const char *>(binary.data()), binary.size());
        emit audioReceived(peerId, audioData);
      }
    });
  });
}

void WebRTCManager::joinRoom(const QString &roomId,
                             const QStringList &participants) {
  currentRoomId_ = roomId;
  qDebug() << "Joining room:" << roomId << "with" << participants.size()
           << "participants";

  for (const QString &participantId : participants) {
    if (participantId != localUserId_) {
      if (localUserId_ < participantId) {
        createOffer(participantId);
      }
    }
  }
}

void WebRTCManager::createOffer(const QString &peerId) {
  if (peers_.contains(peerId)) {
    qDebug() << "PeerConnection already exists for:" << peerId;
    return;
  }

  qDebug() << "Creating PeerConnection and offer for:" << peerId;

  auto config = createConfig();
  auto pc = std::make_shared<rtc::PeerConnection>(config);

  setupPeerCallbacks(peerId, pc, true);

  auto peerInfo = std::make_shared<PeerConnection>();
  peerInfo->pc = pc;
  peerInfo->userId = peerId;
  peers_[peerId] = peerInfo;

  pc->setLocalDescription();
}

void WebRTCManager::handleOffer(const QString &from, const QString &sdp) {
  qDebug() << "Received offer from:" << from;

  if (peers_.contains(from)) {
    qDebug() << "Already have PeerConnection for:" << from << "- ignoring duplicate offer";
    return;
  }

  auto config = createConfig();
  auto pc = std::make_shared<rtc::PeerConnection>(config);

  setupPeerCallbacks(from, pc, false);

  auto peerInfo = std::make_shared<PeerConnection>();
  peerInfo->pc = pc;
  peerInfo->userId = from;
  peers_[from] = peerInfo;

  pc->setRemoteDescription(rtc::Description(sdp.toStdString()));
}

void WebRTCManager::handleAnswer(const QString &from, const QString &sdp) {
  qDebug() << "Received answer from:" << from;

  if (!peers_.contains(from)) {
    qWarning() << "No PeerConnection found for:" << from;
    return;
  }

  auto &peer = peers_[from];
  peer->pc->setRemoteDescription(rtc::Description(sdp.toStdString()));
}

void WebRTCManager::handleIceCandidate(const QString &from,
                                       const QString &candidate) {
  if (!peers_.contains(from)) {
    qWarning() << "No PeerConnection found for ICE candidate:" << from;
    return;
  }

  auto &peer = peers_[from];
  peer->pc->addRemoteCandidate(rtc::Candidate(candidate.toStdString()));
}

void WebRTCManager::removePeer(const QString &peerId) {
  qDebug() << "Removing peer:" << peerId;
  peers_.remove(peerId);
  emit peerDisconnected(peerId);
}

void WebRTCManager::sendAudioData(const QByteArray &data) {
  int sentCount = 0;
  for (auto it = peers_.begin(); it != peers_.end(); ++it) {
    auto &peer = it.value();
    if (peer->dataChannel && peer->dataChannel->isOpen()) {
      std::vector<std::byte> packet(
          reinterpret_cast<const std::byte *>(data.data()),
          reinterpret_cast<const std::byte *>(data.data() + data.size()));
      peer->dataChannel->send(packet);
      sentCount++;
    }
  }
  if (sentCount > 0) {
    qDebug() << "Sent audio to" << sentCount << "peers:" << data.size() << "bytes";
  } else {
    static int counter = 0;
    if (++counter % 100 == 0) {  // Логируем каждые 100 раз чтобы не спамить
      qDebug() << "No peers with open DataChannel";
    }
  }
}
