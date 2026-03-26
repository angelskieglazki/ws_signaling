#ifndef WEBRTCMANAGER_H
#define WEBRTCMANAGER_H

#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QTimer>

#include <rtc/rtc.hpp>

// Структура для хранения информации о peer
struct PeerConnection {
  std::shared_ptr<rtc::PeerConnection> pc;
  std::shared_ptr<rtc::DataChannel> dataChannel;
  QString userId;
  bool connected = false;
};

class WebRTCManager : public QObject {
  Q_OBJECT

public:
  explicit WebRTCManager(QObject *parent = nullptr);
  ~WebRTCManager();

  // Инициализация с нашим userId
  void initialize(const QString &userId);

  // Присоединение к комнате со списком участников
  void joinRoom(const QString &roomId, const QStringList &participants);

  // Создание offer для нового участника (вызывается когда мы узнали о новом
  // peer)
  void createOffer(const QString &peerId);

  // Обработка сигнальных сообщений
  void handleOffer(const QString &from, const QString &sdp);
  void handleAnswer(const QString &from, const QString &sdp);
  void handleIceCandidate(const QString &from, const QString &candidate);

  // Удаление peer
  void removePeer(const QString &peerId);

  // Отправка аудио данных
  void sendAudioData(const QByteArray &data);

  // Проверка инициализации
  bool isInitialized() const { return !localUserId_.isEmpty(); }

signals:
  // Отправка signaling сообщения через WebSocket
  void signalingMessage(const QJsonObject &msg);

  // События peer
  void peerConnected(const QString &peerId);
  void peerDisconnected(const QString &peerId);

  // Получение аудио от peer
  void audioReceived(const QString &peerId, const QByteArray &data);

private:
  rtc::Configuration createConfig();
  void setupPeerCallbacks(const QString &peerId,
                          const std::shared_ptr<rtc::PeerConnection> &pc,
                          bool isOfferer);

  QString localUserId_;
  QString currentRoomId_;
  QMap<QString, std::shared_ptr<PeerConnection>> peers_;
};

#endif // WEBRTCMANAGER_H
