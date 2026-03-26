#ifndef AUDIOCAPTURE_H
#define AUDIOCAPTURE_H

#include <QAudioFormat>
#include <QAudioSource>
#include <QComboBox>
#include <QIODevice>
#include <QLabel>
#include <QObject>
#include <QProgressBar>
#include <QTimer>
#include <memory>

class AudioCapture : public QObject {
  Q_OBJECT

public:
  explicit AudioCapture(QObject *parent = nullptr);
  ~AudioCapture();

  bool initialize();
  void startCapture();
  void stopCapture();
  void setMuted(bool muted);
  bool isMuted() const { return muted_; }
  
  // Получить список устройств
  QStringList getAvailableDevices() const;
  void setDevice(const QString &deviceName);
  
  // UI для настроек
  void createSettingsUI(QWidget *parent);
  void updateVU(); // Обновить индикатор уровня

signals:
  // Вызывается при готовности аудио данных (16-bit PCM, 48kHz, mono)
  void audioDataReady(const QByteArray &data);
  // Уровень громкости 0-100 для VU meter
  void levelChanged(int level);

private slots:
  void onAudioDataAvailable();
  void onDeviceChanged(int index);

private:
  void calculateLevel(const QByteArray &data);
  void setupAudioDevice();

  QAudioFormat format_;
  std::unique_ptr<QAudioSource> audioSource_;
  QIODevice *audioDevice_ = nullptr;
  bool muted_ = false;
  QByteArray buffer_;
  
  // Выбранное устройство
  QString currentDeviceName_;
  
  // UI элементы
  QComboBox *deviceCombo_ = nullptr;
  QProgressBar *vuMeter_ = nullptr;
  QLabel *statusLabel_ = nullptr;
  
  // Для расчета уровня
  int currentLevel_ = 0;

  static constexpr int SAMPLE_RATE = 48000;
  static constexpr int CHANNELS = 1;
  static constexpr int SAMPLE_SIZE = 16;
};

#endif // AUDIOCAPTURE_H
