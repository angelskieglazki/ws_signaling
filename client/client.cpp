#include <QApplication>
#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTextEdit>
#include <QUrl>
#include <QVBoxLayout>
#include <QWebSocket>
#include <QWidget>

class SignalingClient : public QWidget {
  Q_OBJECT

public:
  SignalingClient(QWidget *parent = nullptr) : QWidget(parent) {
    setupUI();
    connectSignals();
  }

private:
  void setupUI() {
    setWindowTitle("WebRTC Signaling Client");
    resize(600, 400);

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
    joinBtn_ = new QPushButton("Join");
    joinBtn_->setEnabled(false);
    roomLayout->addWidget(joinBtn_);
    mainLayout->addLayout(roomLayout);

    // Log
    logEdit_ = new QTextEdit();
    logEdit_->setReadOnly(true);
    mainLayout->addWidget(logEdit_);

    // Message input
    auto *msgLayout = new QHBoxLayout();
    msgEdit_ = new QLineEdit();
    msgEdit_->setPlaceholderText("Enter message...");
    msgLayout->addWidget(msgEdit_);
    sendBtn_ = new QPushButton("Send");
    sendBtn_->setEnabled(false);
    msgLayout->addWidget(sendBtn_);
    mainLayout->addLayout(msgLayout);

    // Status
    statusLabel_ = new QLabel("Disconnected");
    mainLayout->addWidget(statusLabel_);
  }

  void connectSignals() {
    connect(connectBtn_, &QPushButton::clicked, this,
            &SignalingClient::onConnect);
    connect(joinBtn_, &QPushButton::clicked, this, &SignalingClient::onJoin);
    connect(sendBtn_, &QPushButton::clicked, this, &SignalingClient::onSend);
    connect(msgEdit_, &QLineEdit::returnPressed, this,
            &SignalingClient::onSend);
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
    log("Connected!");
    statusLabel_->setText("Connected");
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
    log("Disconnected");
    statusLabel_->setText("Disconnected");
    connectBtn_->setText("Connect");
    disconnect(connectBtn_, &QPushButton::clicked, this,
               &SignalingClient::onDisconnect);
    connect(connectBtn_, &QPushButton::clicked, this,
            &SignalingClient::onConnect);
    joinBtn_->setEnabled(false);
    sendBtn_->setEnabled(false);
  }

  void onJoin() {
    if (!socket_ || socket_->state() != QAbstractSocket::ConnectedState)
      return;

    QString roomId = roomEdit_->text();
    if (roomId.isEmpty()) {
      QMessageBox::warning(this, "Error", "Please enter room ID");
      return;
    }

    QString message = QString(R"({"type":"join","room":"%1"})").arg(roomId);
    socket_->sendTextMessage(message);
    log("Joined room: " + roomId);
  }

  void onSend() {
    if (!socket_ || socket_->state() != QAbstractSocket::ConnectedState)
      return;

    QString text = msgEdit_->text();
    if (text.isEmpty())
      return;

    socket_->sendTextMessage(text);
    log("Sent: " + text);
    msgEdit_->clear();
  }

  void onTextMessage(const QString &message) { log("Received: " + message); }

  void onError(QAbstractSocket::SocketError error) {
    log("Error: " + socket_->errorString());
  }

private:
  void log(const QString &msg) {
    logEdit_->append("[" + QDateTime::currentDateTime().toString("hh:mm:ss") +
                     "] " + msg);
  }

  QWebSocket *socket_ = nullptr;
  QLineEdit *urlEdit_;
  QLineEdit *roomEdit_;
  QLineEdit *msgEdit_;
  QTextEdit *logEdit_;
  QPushButton *connectBtn_;
  QPushButton *joinBtn_;
  QPushButton *sendBtn_;
  QLabel *statusLabel_;
};

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  SignalingClient client;
  client.show();

  return app.exec();
}

#include "client.moc"
