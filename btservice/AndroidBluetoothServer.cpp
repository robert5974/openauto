#include "btservice/AndroidBluetoothServer.hpp"
#include "OpenautoLog.hpp"
#include <QNetworkInterface>
#include <QThread>


namespace openauto {
namespace btservice {

AndroidBluetoothServer::AndroidBluetoothServer(
    openauto::configuration::IConfiguration::Pointer config)
    : rfcommServer_(std::make_unique<QBluetoothServer>(
          QBluetoothServiceInfo::RfcommProtocol, this)),
      socket_(nullptr), config_(std::move(config)) {
  connect(rfcommServer_.get(), &QBluetoothServer::newConnection, this,
          &AndroidBluetoothServer::onClientConnected);
}

bool AndroidBluetoothServer::start(const QBluetoothAddress &address,
                                   uint16_t portNumber) {
  OPENAUTO_LOG(info) << "[AndroidBluetoothServer] listening on " 
                     << address.toString().toStdString() << ":" << portNumber;
  
  bool success = rfcommServer_->listen(address, portNumber);
  if (!success) {
      OPENAUTO_LOG(error) << "[AndroidBluetoothServer] listen failed! Error code: " 
                          << rfcommServer_->error();
      
      // Try fallback to QBluetoothAddress::Any and port 0 if we passed a specific address/port
      if (!address.isNull() || portNumber != 0) {
          OPENAUTO_LOG(info) << "[AndroidBluetoothServer] Retrying listen with Any address and auto-port...";
          success = rfcommServer_->listen(QBluetoothAddress(), 0);
          if (!success) {
              OPENAUTO_LOG(error) << "[AndroidBluetoothServer] fallback listen also failed! Error code: " 
                                  << rfcommServer_->error();
          }
      }
  }
  return success;
}

void AndroidBluetoothServer::onClientConnected() {
  socket_ = rfcommServer_->nextPendingConnection();
  if (socket_ != nullptr) {
    OPENAUTO_LOG(info) << "[AndroidBluetoothServer] Device Connected: "
                       << socket_->peerName().toStdString();
    connect(socket_, SIGNAL(readyRead()), this, SLOT(readSocket()));
    writeSocketInfoRequest();
  } else {
    OPENAUTO_LOG(error) << "[AndroidBluetoothServer] received null socket "
                           "during client connection.";
  }
}

bool AndroidBluetoothServer::writeProtoMessage(
    uint16_t messageType, google::protobuf::Message &message) {
  QByteArray byteArray(message.SerializeAsString().c_str(),
                       (int)message.ByteSizeLong());
  uint16_t messageLength = (int)message.ByteSizeLong();
  byteArray.prepend(messageType & 0x000000ff);
  byteArray.prepend((messageType & 0x0000ff00) >> 8);
  byteArray.prepend(messageLength & 0x000000ff);
  byteArray.prepend((messageLength & 0x0000ff00) >> 8);

  if (socket_->write(byteArray) != byteArray.length()) {
    return false;
  }

  return true;
}

static QString resolveWirelessHeadUnitIPv4() {
  // Prefer the wlan0 Wi-Fi hotspot interface IPv4 address (e.g., 10.42.0.1)
  QNetworkInterface wlan0 = QNetworkInterface::interfaceFromName("wlan0");
  if (wlan0.isValid()) {
    for (const QNetworkAddressEntry &entry : wlan0.addressEntries()) {
      QHostAddress addr = entry.ip();
      if (!addr.isLoopback() &&
          addr.protocol() == QAbstractSocket::IPv4Protocol) {
        return addr.toString();
      }
    }
  }

  // Fallback: check any wireless/AP interface (wlan*, uap*, ap*)
  for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
    QString name = iface.name().toLower();
    if (name.startsWith("wlan") || name.startsWith("uap") ||
        name.startsWith("ap")) {
      for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
        QHostAddress addr = entry.ip();
        if (!addr.isLoopback() &&
            addr.protocol() == QAbstractSocket::IPv4Protocol) {
          return addr.toString();
        }
      }
    }
  }

  // Fallback: any non-loopback IPv4 address
  for (const QHostAddress &addr : QNetworkInterface::allAddresses()) {
    if (!addr.isLoopback() &&
        addr.protocol() == QAbstractSocket::IPv4Protocol) {
      return addr.toString();
    }
  }

  return QStringLiteral("10.42.0.1");
}

void AndroidBluetoothServer::writeSocketInfoRequest() {
  OPENAUTO_LOG(info) << "[AndroidBluetoothServer] Sending SocketInfoRequest.";

  QString ipAddr = resolveWirelessHeadUnitIPv4();

  btservice::proto::SocketInfoRequest socketInfoRequest;
  socketInfoRequest.set_ip_address(ipAddr.toStdString());
  OPENAUTO_LOG(info) << "[AndroidBluetoothServer] ipAddress: "
                     << ipAddr.toStdString();

  socketInfoRequest.set_port(5000);
  OPENAUTO_LOG(info) << "[AndroidBluetoothServer] port: " << 5000;

  if (this->writeProtoMessage(1, socketInfoRequest)) {
    OPENAUTO_LOG(info) << "[AndroidBluetoothServer] Sent SocketInfoRequest.";
  } else {
    OPENAUTO_LOG(error)
        << "[AndroidBluetoothServer] Error sending SocketInfoRequest.";
  }
}
void AndroidBluetoothServer::writeSocketInfoResponse() {
  OPENAUTO_LOG(info) << "[AndroidBluetoothServer] Sending SocketInfoResponse.";
  QString ipAddr = resolveWirelessHeadUnitIPv4();

  btservice::proto::SocketInfoResponse socketInfoResponse;
  socketInfoResponse.set_ip_address(ipAddr.toStdString());
  OPENAUTO_LOG(info) << "[AndroidBluetoothServer] ipAddress: "
                     << ipAddr.toStdString();

  socketInfoResponse.set_port(5000);
  OPENAUTO_LOG(info) << "[AndroidBluetoothServer] port: " << 5000;

  socketInfoResponse.set_status(btservice::proto::Status::STATUS_SUCCESS);
  OPENAUTO_LOG(info) << "[AndroidBluetoothServer] status: "
                     << btservice::proto::Status::STATUS_SUCCESS;

  if (this->writeProtoMessage(7, socketInfoResponse)) {
    OPENAUTO_LOG(info) << "[AndroidBluetoothServer] Sent SocketInfoResponse.";
  } else {
    OPENAUTO_LOG(error)
        << "[AndroidBluetoothServer] Error sending SocketInfoResponse.";
  }
}

void AndroidBluetoothServer::handleSocketInfoRequestResponse(QByteArray data) {
  btservice::proto::SocketInfoResponse socketInfoResponse;
  socketInfoResponse.ParseFromArray(data, data.size());
  OPENAUTO_LOG(info)
      << "[AndroidBluetoothServer] Received SocketInfoRequestResponse, status: "
      << socketInfoResponse.status();
  if (socketInfoResponse.status() == 0) {
    // A status of 0 should be successful handshake (unless phone later reports
    // an error, aw well) save this phone so we can autoconnect to it next time
    config_->setLastBluetoothPair(
        socket_->peerAddress().toString().toStdString());
    config_->save();
  }
}

void AndroidBluetoothServer::handleSocketInfoRequest(QByteArray data) {
  OPENAUTO_LOG(info) << "[AndroidBluetoothServer] Reading SocketInfoRequest.";

  btservice::proto::SocketInfoRequest socketInfoRequest;

  writeSocketInfoResponse();
}

void AndroidBluetoothServer::writeNetworkInfoMessage() {
  OPENAUTO_LOG(info) << "[AndroidBluetoothServer] Sending NetworkInfoMessage.";

  std::string ssid = config_->getWifiSSID();
  if (ssid.empty()) {
    ssid = "OpenDash2";
    config_->setWifiSSID(ssid);
  }
  std::string psk = config_->getWifiPassword();
  if (psk.size() < 8) {
    psk = "opendash2";
    config_->setWifiPassword(psk);
  }

  btservice::proto::NetworkInfo networkMessage;
  networkMessage.set_ssid(ssid);
  OPENAUTO_LOG(info) << "[AndroidBluetoothServer] SSID: " << ssid;

  networkMessage.set_psk(psk);
  OPENAUTO_LOG(info) << "[AndroidBluetoothServer] PSKEY: " << psk;

  if (config_->getWifiMAC().empty()) {
    networkMessage.set_mac_addr(QNetworkInterface::interfaceFromName("wlan0")
                                    .hardwareAddress()
                                    .toStdString());
    OPENAUTO_LOG(info) << "[AndroidBluetoothServer] MAC: "
                       << QNetworkInterface::interfaceFromName("wlan0")
                              .hardwareAddress()
                              .toStdString();
  } else {
    networkMessage.set_mac_addr(config_->getWifiMAC());
    OPENAUTO_LOG(info) << "[AndroidBluetoothServer] MAC: "
                       << config_->getWifiMAC();
  }

  networkMessage.set_security_mode(
      btservice::proto::SecurityMode::WPA2_PERSONAL);
  OPENAUTO_LOG(info) << "[AndroidBluetoothServer] Security: "
                     << btservice::proto::SecurityMode::WPA2_PERSONAL;

  networkMessage.set_ap_type(btservice::proto::AccessPointType::STATIC);
  OPENAUTO_LOG(info) << "[AndroidBluetoothServer] AP Type: "
                     << btservice::proto::AccessPointType::STATIC;

  if (this->writeProtoMessage(3, networkMessage)) {
    OPENAUTO_LOG(info) << "[AndroidBluetoothServer] Sent NetworkInfoMessage";
  } else {
    OPENAUTO_LOG(info)
        << "[AndroidBluetoothServer] Error sending NetworkInfoMessage.";
  }
}

void AndroidBluetoothServer::handleUnknownMessage(int messageType,
                                                  QByteArray data) {
  OPENAUTO_LOG(info)
      << "[AndroidBluetoothServer] Received unknown MessageType of "
      << messageType;
  OPENAUTO_LOG(info) << "[AndroidBluetoothServer] Unknown Message Data: "
                     << data.toHex(' ').toStdString();
}

void AndroidBluetoothServer::readSocket() {
  if (!socket_) {
    return;
  }

  auto data = socket_->readAll();
  if (data.length() == 0) {
    return;
  }

  while (data.length() >= 4) {
    uint16_t payloadLen = (static_cast<uint8_t>(data[0]) << 8) |
                          static_cast<uint8_t>(data[1]);
    uint16_t messageType = (static_cast<uint8_t>(data[2]) << 8) |
                           static_cast<uint8_t>(data[3]);
    int packetSize = 4 + static_cast<int>(payloadLen);
    QByteArray packet = (data.length() >= packetSize)
                            ? data.left(packetSize)
                            : data;
    QByteArray payload = packet.mid(4);

    switch (messageType) {
    case 1:
      handleSocketInfoRequest(packet);
      break;
    case 2:
      writeNetworkInfoMessage();
      break;
    case 7:
      handleSocketInfoRequestResponse(payload);
      break;
    default:
      handleUnknownMessage(messageType, payload);
      break;
    }

    if (data.length() >= packetSize && packetSize > 0) {
      data.remove(0, packetSize);
    } else {
      break;
    }
  }
}

} // namespace btservice
} // namespace openauto
