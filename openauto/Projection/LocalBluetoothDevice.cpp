/*
*  This file is part of openauto project.
*  Copyright (C) 2018 f1x.studio (Michal Szwaj)
*
*  openauto is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 3 of the License, or
*  (at your option) any later version.

*  openauto is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with openauto. If not, see <http://www.gnu.org/licenses/>.
*/

#include "openauto/Projection/LocalBluetoothDevice.hpp"
#include "OpenautoLog.hpp"
#include <QApplication>


namespace openauto {
namespace projection {

LocalBluetoothDevice::LocalBluetoothDevice() {
  qRegisterMetaType<IBluetoothDevice::PairingPromise::Pointer>(
      "PairingPromise::Pointer");

  this->moveToThread(QApplication::instance()->thread());
  connect(this, &LocalBluetoothDevice::startPairing, this,
          &LocalBluetoothDevice::onStartPairing, Qt::QueuedConnection);
  QMetaObject::invokeMethod(this, "createBluetoothLocalDevice",
                            Qt::BlockingQueuedConnection);
}

void LocalBluetoothDevice::createBluetoothLocalDevice() {
  OPENAUTO_LOG(debug) << "[LocalBluetoothDevice] create.";

  localDevice_ = std::make_unique<QBluetoothLocalDevice>(QBluetoothAddress());

  connect(localDevice_.get(), &QBluetoothLocalDevice::errorOccurred, this,
          &LocalBluetoothDevice::onError);
  connect(localDevice_.get(), &QBluetoothLocalDevice::hostModeStateChanged,
          this, &LocalBluetoothDevice::onHostModeStateChanged);
  localDevice_->setHostMode(QBluetoothLocalDevice::HostDiscoverable);
}

void LocalBluetoothDevice::stop() {
  std::lock_guard<decltype(mutex_)> lock(mutex_);

  if (pairingPromise_ != nullptr) {
    pairingPromise_->reject();
    pairingPromise_.reset();
    pairingAddress_ = QBluetoothAddress();
  }
}

bool LocalBluetoothDevice::isPaired(const std::string &address) const {
  std::lock_guard<decltype(mutex_)> lock(mutex_);

  return false;
}

void LocalBluetoothDevice::pair(const std::string &address,
                                PairingPromise::Pointer promise) {
  emit startPairing(QString::fromStdString(address), std::move(promise));
}

std::string LocalBluetoothDevice::getLocalAddress() const {
  std::lock_guard<decltype(mutex_)> lock(mutex_);
  return localDevice_->isValid()
             ? localDevice_->address().toString().toStdString()
             : "";
}

bool LocalBluetoothDevice::isAvailable() const {
  std::lock_guard<decltype(mutex_)> lock(mutex_);
  return localDevice_->isValid();
}

void LocalBluetoothDevice::onStartPairing(const QString &address,
                                          PairingPromise::Pointer promise) {
  OPENAUTO_LOG(debug) << "[LocalBluetoothDevice] onStartPairing, address: "
                      << address.toStdString();

  std::lock_guard<decltype(mutex_)> lock(mutex_);

  promise->reject();
}
}

void LocalBluetoothDevice::onError(QBluetoothLocalDevice::Error error) {
  OPENAUTO_LOG(debug) << "[LocalBluetoothDevice] onError, error: " << error;

  std::lock_guard<decltype(mutex_)> lock(mutex_);

  if (pairingPromise_ != nullptr) {
    pairingPromise_->reject();
    pairingPromise_.reset();
    pairingAddress_ = QBluetoothAddress();
  }
}

void LocalBluetoothDevice::onHostModeStateChanged(
    QBluetoothLocalDevice::HostMode state) {
  std::lock_guard<decltype(mutex_)> lock(mutex_);

  if (state == QBluetoothLocalDevice::HostPoweredOff &&
      pairingPromise_ != nullptr) {
    pairingPromise_->reject();
    pairingPromise_.reset();
    pairingAddress_ = QBluetoothAddress();
  }
}

}
}
