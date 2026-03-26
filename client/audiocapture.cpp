#include "audiocapture.h"

#include <QAudioDevice>
#include <QDebug>
#include <QHBoxLayout>
#include <QMediaDevices>
#include <QVBoxLayout>

AudioCapture::AudioCapture(QObject *parent) : QObject(parent) {
  format_.setSampleRate(SAMPLE_RATE);
  format_.setChannelCount(CHANNELS);
  format_.setSampleFormat(QAudioFormat::Int16);
}

AudioCapture::~AudioCapture() { stopCapture(); }

QStringList AudioCapture::getAvailableDevices() const {
  QStringList devices;
  QMediaDevices *mediaDevices = new QMediaDevices(const_cast<AudioCapture *>(this));
  for (const auto &device : mediaDevices->audioInputs()) {
    devices.append(device.description());
  }
  return devices;
}

void AudioCapture::setDevice(const QString &deviceName) {
  currentDeviceName_ = deviceName;
  if (audioSource_) {
    // Перезапускаем с новым устройством
    stopCapture();
    setupAudioDevice();
    startCapture();
  }
}

void AudioCapture::setupAudioDevice() {
  QMediaDevices *devices = new QMediaDevices(this);
  QAudioDevice inputDevice;

  if (currentDeviceName_.isEmpty()) {
    inputDevice = devices->defaultAudioInput();
    currentDeviceName_ = inputDevice.description();
  } else {
    for (const auto &device : devices->audioInputs()) {
      if (device.description() == currentDeviceName_) {
        inputDevice = device;
        break;
      }
    }
    if (inputDevice.isNull()) {
      inputDevice = devices->defaultAudioInput();
      currentDeviceName_ = inputDevice.description();
    }
  }

  if (!inputDevice.isFormatSupported(format_)) {
    qWarning() << "Default format not supported, trying to use nearest";
    format_ = inputDevice.preferredFormat();
    qDebug() << "Preferred format:" << format_.sampleRate() << "Hz"
             << format_.channelCount() << "channels";
  }

  audioSource_ = std::make_unique<QAudioSource>(inputDevice, format_, this);
  audioSource_->setBufferSize(960 * 2); // 20ms at 48kHz, 16-bit mono

  qDebug() << "Audio capture initialized:" << format_.sampleRate() << "Hz,"
           << format_.channelCount() << "channels," << SAMPLE_SIZE << "bits";
  qDebug() << "Using device:" << inputDevice.description();

  if (statusLabel_) {
    statusLabel_->setText("Device: " + inputDevice.description());
  }
}

bool AudioCapture::initialize() {
  setupAudioDevice();
  return audioSource_ != nullptr;
}

void AudioCapture::startCapture() {
  if (!audioSource_) {
    if (!initialize()) {
      qWarning() << "Failed to initialize audio capture";
      return;
    }
  }

  audioDevice_ = audioSource_->start();
  connect(audioDevice_, &QIODevice::readyRead, this,
          &AudioCapture::onAudioDataAvailable);
  qDebug() << "Audio capture started, bytes available:" << audioDevice_->bytesAvailable();
}

void AudioCapture::stopCapture() {
  if (audioSource_) {
    audioSource_->stop();
  }
  audioDevice_ = nullptr;
  qDebug() << "Audio capture stopped";
  
  if (vuMeter_) {
    vuMeter_->setValue(0);
  }
}

void AudioCapture::setMuted(bool muted) {
  muted_ = muted;
  if (audioSource_) {
    audioSource_->setVolume(muted ? 0.0f : 1.0f);
  }
}

void AudioCapture::calculateLevel(const QByteArray &data) {
  // Вычисляем RMS уровень
  const int16_t *samples = reinterpret_cast<const int16_t *>(data.data());
  int sampleCount = data.size() / sizeof(int16_t);
  
  if (sampleCount == 0) return;
  
  double sum = 0;
  for (int i = 0; i < sampleCount; ++i) {
    double sample = samples[i] / 32768.0; // Нормализуем в [-1, 1]
    sum += sample * sample;
  }
  
  double rms = std::sqrt(sum / sampleCount);
  // Конвертируем в децибелы и затем в проценты
  double db = 20 * std::log10(rms + 0.0001);
  int level = static_cast<int>((db + 60) * 100 / 60); // -60dB = 0%, 0dB = 100%
  level = qBound(0, level, 100);
  
  currentLevel_ = level;
  
  if (vuMeter_) {
    vuMeter_->setValue(level);
  }
  
  emit levelChanged(level);
  
  // Логируем только если уровень значительный
  if (level > 10) {
    // qDebug() << "Audio level:" << level << "% (" << db << "dB)";
  }
}

void AudioCapture::onAudioDataAvailable() {
  if (!audioDevice_ || muted_)
    return;

  QByteArray data = audioDevice_->readAll();
  if (!data.isEmpty()) {
    calculateLevel(data);
    emit audioDataReady(data);
  }
}

void AudioCapture::createSettingsUI(QWidget *parent) {
  auto *layout = new QVBoxLayout(parent);
  
  // Выбор устройства
  auto *deviceLayout = new QHBoxLayout();
  deviceLayout->addWidget(new QLabel("Input Device:"));
  deviceCombo_ = new QComboBox();
  deviceLayout->addWidget(deviceCombo_);
  layout->addLayout(deviceLayout);
  
  // Заполняем список устройств
  for (const auto &deviceName : getAvailableDevices()) {
    deviceCombo_->addItem(deviceName);
  }
  deviceCombo_->setCurrentText(currentDeviceName_);
  connect(deviceCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &AudioCapture::onDeviceChanged);
  
  // VU Meter
  auto *vuLayout = new QHBoxLayout();
  vuLayout->addWidget(new QLabel("Input Level:"));
  vuMeter_ = new QProgressBar();
  vuMeter_->setRange(0, 100);
  vuMeter_->setTextVisible(true);
  vuMeter_->setFormat("%p%");
  vuLayout->addWidget(vuMeter_);
  layout->addLayout(vuLayout);
  
  // Статус
  statusLabel_ = new QLabel("Not initialized");
  layout->addWidget(statusLabel_);
}

void AudioCapture::onDeviceChanged(int index) {
  if (deviceCombo_) {
    setDevice(deviceCombo_->itemText(index));
  }
}

void AudioCapture::updateVU() {
  // Обновляется автоматически в onAudioDataAvailable
}
