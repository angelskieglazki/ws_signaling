#include <QApplication>
#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QComboBox>
#include <QDateTime>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMediaDevices>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <QTextEdit>
#include <QUrl>
#include <QVBoxLayout>
#include <QWebSocket>
#include <QWidget>

#include "audiocapture.h"
#include "webrtcmanager.h"

class SignalingClient : public QWidget {
  Q_OBJECT

public:
  SignalingClient(QWidget *parent = nullptr) : QWidget(parent) {
    // Аудио захват (сначала создаем, потом UI)
    audioCapture_ = new AudioCapture(this);
    connect(audioCapture_, &AudioCapture::audioDataReady, this,
            &SignalingClient::onLocalAudioData);
    connect(audioCapture_, &AudioCapture::levelChanged, this,
            [this](int level) {
              // Можно обновить UI с уровнем
            });

    // WebRTC менеджер
    webrtcManager_ = new WebRTCManager(this);
    connect(webrtcManager_, &WebRTCManager::signalingMessage, this,
            &SignalingClient::sendSignalingMessage);
    connect(webrtcManager_, &WebRTCManager::peerConnected, this,
            &SignalingClient::onPeerConnected);
    connect(webrtcManager_, &WebRTCManager::peerDisconnected, this,
            &SignalingClient::onPeerDisconnected);
    connect(webrtcManager_, &WebRTCManager::audioReceived, this,
            &SignalingClient::onAudioReceived);

    // Аудио выход для воспроизведения
    setupAudioOutput();

    // UI (после создания всех объектов)
    setupUI();
    connectSignals();
  }

private:
  void setupUI() {
    setWindowTitle("WebRTC Voice Client");
    resize(800, 600);

    auto *mainLayout = new QVBoxLayout(this);

    // Server URL
    auto *urlLayout = new QHBoxLayout();
    urlLayout->addWidget(new QLabel("Server:"));
    urlEdit_ = new QLineEdit("ws://localhost:9002");
    urlLayout->addWidget(urlEdit_);
    connectBtn_ = new QPushButton("Connect");
    urlLayout->addWidget(connectBtn_);
    mainLayout->addLayout(urlLayout);

    // Room ID
    auto *roomLayout = new QHBoxLayout();
    roomLayout->addWidget(new QLabel("Room ID:"));
    roomEdit_ = new QLineEdit("test-room");
    roomLayout->addWidget(roomEdit_);
    joinBtn_ = new QPushButton("Join Room");
    joinBtn_->setEnabled(false);
    roomLayout->addWidget(joinBtn_);
    mainLayout->addLayout(roomLayout);

    // Настройки аудио устройства
    auto *audioSettingsWidget = new QWidget();
    audioCapture_->createSettingsUI(audioSettingsWidget);
    mainLayout->addWidget(audioSettingsWidget);

    // Управление микрофоном и громкость
    auto *micLayout = new QHBoxLayout();
    micBtn_ = new QPushButton("🎤 Microphone Off");
    micBtn_->setCheckable(true);
    micBtn_->setEnabled(false);
    micLayout->addWidget(micBtn_);

    // Ползунок громкости выхода
    micLayout->addWidget(new QLabel("Output Volume:"));
    volumeSlider_ = new QSlider(Qt::Horizontal);
    volumeSlider_->setRange(0, 100);
    volumeSlider_->setValue(80);
    micLayout->addWidget(volumeSlider_);
    mainLayout->addLayout(micLayout);

    // Список участников
    mainLayout->addWidget(new QLabel("Participants:"));
    participantsList_ = new QListWidget();
    participantsList_->setMaximumHeight(100);
    mainLayout->addWidget(participantsList_);

    // WebRTC статус
    webrtcStatus_ = new QLabel("WebRTC: Not initialized");
    webrtcStatus_->setStyleSheet("color: gray;");
    mainLayout->addWidget(webrtcStatus_);

    // Log
    logEdit_ = new QTextEdit();
    logEdit_->setReadOnly(true);
    mainLayout->addWidget(logEdit_);

    // Message input (для текстовых сообщений)
    auto *msgLayout = new QHBoxLayout();
    msgEdit_ = new QLineEdit();
    msgEdit_->setPlaceholderText("Enter text message...");
    msgLayout->addWidget(msgEdit_);
    sendBtn_ = new QPushButton("Send");
    sendBtn_->setEnabled(false);
    msgLayout->addWidget(sendBtn_);
    mainLayout->addLayout(msgLayout);

    // Status
    statusLabel_ = new QLabel("Disconnected");
    mainLayout->addWidget(statusLabel_);
  }

  void setupAudioOutput() {
    // Настройка аудио выхода для воспроизведения
    QAudioFormat format;
    format.setSampleRate(48000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);

    QAudioDevice outputDevice = QMediaDevices::defaultAudioOutput();
    if (outputDevice.isFormatSupported(format)) {
      audioSink_ = new QAudioSink(outputDevice, format, this);
      audioOutput_ = audioSink_->start();
    }
  }

  void connectSignals() {
    connect(connectBtn_, &QPushButton::clicked, this,
            &SignalingClient::onConnect);
    connect(joinBtn_, &QPushButton::clicked, this, &SignalingClient::onJoin);
    connect(sendBtn_, &QPushButton::clicked, this, &SignalingClient::onSend);
    connect(msgEdit_, &QLineEdit::returnPressed, this,
            &SignalingClient::onSend);
    connect(micBtn_, &QPushButton::toggled, this,
            &SignalingClient::onMicToggled);
    connect(volumeSlider_, &QSlider::valueChanged, this,
            &SignalingClient::onVolumeChanged);
  }

private slots:
  void onConnect() {
    if (!socket_) {
      socket_ = new QWebSocket();
      connect(socket_, &QWebSocket::connected, this,
              &SignalingClient::onConnected);
      connect(socket_, &QWebSocket::disconnected, this,
              &SignalingClient::onDisconnected);
      connect(socket_, &QWebSocket::textMessageReceived, this,
              &SignalingClient::onTextMessage);
      connect(socket_,
              QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
              this, &SignalingClient::onError);
    }

    log("Connecting to " + urlEdit_->text() + "...");
    socket_->open(QUrl(urlEdit_->text()));
  }

  void onConnected() {
    log("Connected to signaling server!");
    statusLabel_->setText("Connected");
    statusLabel_->setStyleSheet("color: green;");
    connectBtn_->setText("Disconnect");
    connect(connectBtn_, &QPushButton::clicked, this,
            &SignalingClient::onDisconnect);
    joinBtn_->setEnabled(true);
    sendBtn_->setEnabled(true);
  }

  void onDisconnect() {
    if (socket_) {
      socket_->close();
    }
  }

  void onDisconnected() {
    log("Disconnected from server");
    statusLabel_->setText("Disconnected");
    statusLabel_->setStyleSheet("color: red;");
    connectBtn_->setText("Connect");
    disconnect(connectBtn_, &QPushButton::clicked, this,
               &SignalingClient::onDisconnect);
    connect(connectBtn_, &QPushButton::clicked, this,
            &SignalingClient::onConnect);
    joinBtn_->setEnabled(false);
    sendBtn_->setEnabled(false);
    micBtn_->setEnabled(false);

    // Останавливаем аудио захват
    audioCapture_->stopCapture();

    // Очищаем список участников
    participantsList_->clear();
    roomParticipants_.clear();
  }

  void onJoin() {
    if (!socket_ || socket_->state() != QAbstractSocket::ConnectedState)
      return;

    QString roomId = roomEdit_->text();
    if (roomId.isEmpty()) {
      QMessageBox::warning(this, "Error", "Please enter room ID");
      return;
    }

    // Отправляем запрос на вход в комнату
    QJsonObject msg;
    msg["type"] = "join";
    msg["room"] = roomId;
    socket_->sendTextMessage(QString::fromUtf8(QJsonDocument(msg).toJson()));

    log("Joining room: " + roomId);
  }

  void onMicToggled(bool checked) {
    if (checked) {
      micBtn_->setText("🎤 Microphone On");
      micBtn_->setStyleSheet("background-color: #4CAF50; color: white;");
      audioCapture_->startCapture();
      log("Microphone enabled");
    } else {
      micBtn_->setText("🎤 Microphone Off");
      micBtn_->setStyleSheet("");
      audioCapture_->stopCapture();
      log("Microphone disabled");
    }
  }

  void onVolumeChanged(int value) {
    if (audioSink_) {
      audioSink_->setVolume(value / 100.0);
    }
  }

  void onSend() {
    if (!socket_ || socket_->state() != QAbstractSocket::ConnectedState)
      return;

    QString text = msgEdit_->text();
    if (text.isEmpty())
      return;

    QJsonObject msg;
    msg["type"] = "chat";
    msg["from"] = localUserId_;
    msg["text"] = text;
    QString jsonStr =
        QString::fromUtf8(QJsonDocument(msg).toJson(QJsonDocument::Compact));
    socket_->sendTextMessage(jsonStr);

    log("Me: " + text);
    msgEdit_->clear();
  }

  void onTextMessage(const QString &message) {
    log("Received: " + message);

    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject())
      return;

    QJsonObject obj = doc.object();
    QString type = obj["type"].toString();

    if (type == "welcome")
      handleWelcome(obj);
    else if (type == "room-joined")
      handleRoomJoined(obj);
    else if (type == "user-joined")
      handleUserJoined(obj);
    else if (type == "user-left")
      handleUserLeft(obj);
    else if (type == "offer")
      handleOffer(obj);
    else if (type == "answer")
      handleAnswer(obj);
    else if (type == "ice-candidate")
      handleIceCandidate(obj);
    else if (type == "chat")
      handleChat(obj);
    else {
      log("Unknown message type: " + type);
    }
  }

  void handleWelcome(const QJsonObject &obj) {
    localUserId_ = obj["userId"].toString();
    webrtcManager_->initialize(localUserId_);

    webrtcStatus_->setText("WebRTC: Initialized (" + localUserId_ + ")");
    webrtcStatus_->setStyleSheet("color: blue;");
    log("Assigned userId: " + localUserId_);
  }

  void handleRoomJoined(const QJsonObject &obj) {
    currentRoomId_ = obj["room"].toString();
    QJsonArray participants = obj["participants"].toArray();

    log("Room joined: " + currentRoomId_ + " with " +
        QString::number(participants.size()) + " participants");

    updateParticipantsList(participants);

    QStringList participantIds;
    for (const auto &p : participants) {
      QString id = p.toString();
      if (id != localUserId_)
        participantIds.append(id);
    }

    webrtcManager_->joinRoom(currentRoomId_, participantIds);
    micBtn_->setEnabled(true);
    webrtcStatus_->setText("WebRTC: Ready for calls");
  }

  void handleUserJoined(const QJsonObject &obj) {
    QString userId = obj["userId"].toString();
    log("User joined: " + userId);

    if (roomParticipants_.contains(userId))
      return;

    roomParticipants_.append(userId);
    participantsList_->addItem(userId + " (connecting...)");

    // Mesh-топология: меньший userId создаёт offer
    if (localUserId_ < userId) {
      webrtcManager_->createOffer(userId);
    }
  }

  void handleUserLeft(const QJsonObject &obj) {
    QString userId = obj["userId"].toString();
    log("User left: " + userId);

    roomParticipants_.removeAll(userId);
    webrtcManager_->removePeer(userId);
    updateParticipantsListDisplay();
  }

  void handleOffer(const QJsonObject &obj) {
    QString from = obj["from"].toString();
    QString sdp = obj["sdp"].toString();
    log("Received offer from: " + from);
    webrtcManager_->handleOffer(from, sdp);
  }
  void handleAnswer(QJsonObject &obj) {
    // Получили SDP answer
    QString from = obj["from"].toString();
    QString sdp = obj["sdp"].toString();
    log("Received answer from: " + from);
    webrtcManager_->handleAnswer(from, sdp);
  }
  void handleIceCandidate(QJsonObject &obj) {
    // Получили ICE candidate
    QString from = obj["from"].toString();
    QString candidate = obj["candidate"].toString();
    webrtcManager_->handleIceCandidate(from, candidate);
  }

  void handleChat(QJsonObject &obj) {
    // Получили текстовое сообщение
    QString text = obj["text"].toString();
    QString from = obj.value("from").toString();
    if (from.isEmpty()) {
      log("Chat: " + text);
    } else {
      log("[" + from + "]: " + text);
    }
  }

  void onError(QAbstractSocket::SocketError error) {
    log("Error: " + socket_->errorString());
    statusLabel_->setText("Error: " + socket_->errorString());
    statusLabel_->setStyleSheet("color: red;");
  }

  void sendSignalingMessage(const QJsonObject &msg) {
    if (socket_ && socket_->state() == QAbstractSocket::ConnectedState) {
      socket_->sendTextMessage(QString::fromUtf8(QJsonDocument(msg).toJson()));
    }
  }

  void onPeerConnected(const QString &peerId) {
    log("Peer connected: " + peerId);
    updateParticipantStatus(peerId, "connected");
    webrtcStatus_->setText("WebRTC: Connected to " + peerId);
    webrtcStatus_->setStyleSheet("color: green;");

    // Автоматически включаем микрофон при первом подключении
    if (!micBtn_->isChecked()) {
      micBtn_->setChecked(true);
      onMicToggled(true);
    }
  }

  void onPeerDisconnected(const QString &peerId) {
    log("Peer disconnected: " + peerId);
    updateParticipantStatus(peerId, "disconnected");
  }

  void onLocalAudioData(const QByteArray &data) {
    // Отправляем локальное аудио всем пирам
    webrtcManager_->sendAudioData(data);
  }

  void onAudioReceived(const QString &peerId, const QByteArray &data) {
    // Воспроизводим полученное аудио
    qDebug() << "Playing audio from" << peerId << ":" << data.size() << "bytes";
    if (audioOutput_) {
      int written = audioOutput_->write(data);
      qDebug() << "Written to audio output:" << written << "bytes";
    }

    // Обновляем индикатор активности
    updateParticipantStatus(peerId, "speaking");
  }

private:
  void log(const QString &msg) {
    logEdit_->append("[" + QDateTime::currentDateTime().toString("hh:mm:ss") +
                     "] " + msg);
  }

  void updateParticipantsList(const QJsonArray &participants) {
    roomParticipants_.clear();
    participantsList_->clear();

    for (const auto &p : participants) {
      QString userId = p.toString();
      if (userId != localUserId_) {
        roomParticipants_.append(userId);
        participantsList_->addItem(userId + " (waiting...)");
      }
    }
  }

  void updateParticipantsListDisplay() {
    participantsList_->clear();
    for (const auto &userId : roomParticipants_) {
      participantsList_->addItem(userId);
    }
  }

  void updateParticipantStatus(const QString &peerId, const QString &status) {
    for (int i = 0; i < participantsList_->count(); ++i) {
      QListWidgetItem *item = participantsList_->item(i);
      if (item->text().startsWith(peerId)) {
        item->setText(peerId + " (" + status + ")");

        if (status == "connected") {
          item->setBackground(QBrush(QColor(200, 255, 200)));
        } else if (status == "speaking") {
          item->setBackground(QBrush(QColor(255, 255, 150)));
          // Сбросить через 200ms - не захватываем указатель на item
          QTimer::singleShot(200, [this, peerId]() {
            // Ищем item заново по peerId
            for (int j = 0; j < participantsList_->count(); ++j) {
              QListWidgetItem *currentItem = participantsList_->item(j);
              if (currentItem && currentItem->text().startsWith(peerId)) {
                currentItem->setBackground(QBrush(QColor(200, 255, 200)));
                break;
              }
            }
          });
        } else {
          item->setBackground(QBrush(Qt::white));
        }
        break;
      }
    }
  }

private:
  // WebSocket
  QWebSocket *socket_ = nullptr;

  // WebRTC
  WebRTCManager *webrtcManager_;
  AudioCapture *audioCapture_;

  // Аудио выход
  QAudioSink *audioSink_ = nullptr;
  QIODevice *audioOutput_ = nullptr;

  // UI
  QLineEdit *urlEdit_;
  QLineEdit *roomEdit_;
  QLineEdit *msgEdit_;
  QTextEdit *logEdit_;
  QListWidget *participantsList_;
  QPushButton *connectBtn_;
  QPushButton *joinBtn_;
  QPushButton *sendBtn_;
  QPushButton *micBtn_;
  QSlider *volumeSlider_;
  QLabel *statusLabel_;
  QLabel *webrtcStatus_;

  // Состояние
  QString localUserId_;
  QString currentRoomId_;
  QStringList roomParticipants_;
};

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  SignalingClient client;
  client.show();

  return app.exec();
}

#include "client.moc"
