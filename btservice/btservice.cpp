#include "btservice/btservice.hpp"

namespace openauto {
namespace btservice {

btservice::btservice(openauto::configuration::IConfiguration::Pointer config)
    : androidBluetoothServer_(config) {
  QBluetoothAddress address;
  auto adapters = QBluetoothLocalDevice::allDevices();
  if (adapters.size() > 0) {
    address = adapters.at(0).address();
  } else {
    OPENAUTO_LOG(error) << "[btservice] No adapter found.";
  }

  // Pass 0 to automatically assign an available RFCOMM port
  if (!androidBluetoothServer_.start(address, 0)) {
    OPENAUTO_LOG(error) << "[btservice] Server start failed.";
    return;
  }

  uint16_t assignedPort = androidBluetoothServer_.serverPort();
  OPENAUTO_LOG(info) << "[btservice] Listening for connections, address: "
                     << address.toString().toStdString()
                     << ", port: " << assignedPort;

  // Initialize service with the auto-assigned port
  androidBluetoothService_ = std::make_unique<openauto::btservice::AndroidBluetoothService>(assignedPort);

  if (!androidBluetoothService_->registerService(address)) {
    OPENAUTO_LOG(error) << "[btservice] Service registration failed.";
  } else {
    OPENAUTO_LOG(info) << "[btservice] Service registered, port: "
                       << assignedPort;
  }
  if (config->getAutoconnectBluetooth() &&
      !config->getLastBluetoothPair().empty()) {
    connectToBluetooth(QBluetoothAddress(QString::fromStdString(
                           config->getLastBluetoothPair())),
                       address);
  }
}

btservice::~btservice() {
  if (androidBluetoothService_) {
    androidBluetoothService_->unregisterService();
  }
  if (btConnectProcess) {
    if (btConnectProcess->state() != QProcess::NotRunning) {
      btConnectProcess->kill();
      btConnectProcess->waitForFinished(500);
    }
    delete btConnectProcess;
    btConnectProcess = nullptr;
  }
}

void btservice::connectToBluetooth(QBluetoothAddress addr,
                                   QBluetoothAddress controller) {
  if (addr.isNull()) {
    return;
  }

  // Non-blocking Bluetooth auto-connect to last paired Android phone
  btConnectProcess = new QProcess();
  btConnectProcess->setProcessChannelMode(QProcess::SeparateChannels);
  OPENAUTO_LOG(info)
      << "[btservice] Attempting to connect to last bluetooth device: "
      << addr.toString().toStdString();

  QString targetAddr = addr.toString();
  QString ctrlCmd = controller.isNull()
                        ? QString()
                        : QString("select %1\n").arg(controller.toString());
  QString cmd = QString("printf '%1connect %2\\nquit\\n' | bluetoothctl "
                        ">/dev/null 2>&1 || sudo -n rfcomm connect hci0 %2 2 "
                        ">/dev/null 2>&1 || true")
                    .arg(ctrlCmd, targetAddr);
  btConnectProcess->start("/bin/sh", {"-c", cmd});
}

} // namespace btservice
} // namespace openauto
