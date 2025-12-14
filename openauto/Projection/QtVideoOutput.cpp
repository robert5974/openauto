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

#include "openauto/Projection/QtVideoOutput.hpp"
#include "OpenautoLog.hpp"
#include <QApplication>

namespace openauto {
namespace projection {

QtVideoOutput::QtVideoOutput(
    configuration::IConfiguration::Pointer configuration,
    QWidget *videoContainer)
    : VideoOutput(std::move(configuration)), videoContainer_(videoContainer) {
  this->moveToThread(QApplication::instance()->thread());
  connect(this, &QtVideoOutput::startPlayback, this,
          &QtVideoOutput::onStartPlayback, Qt::QueuedConnection);
  connect(this, &QtVideoOutput::stopPlayback, this,
          &QtVideoOutput::onStopPlayback, Qt::QueuedConnection);

  QMetaObject::invokeMethod(this, "createVideoOutput",
                            Qt::BlockingQueuedConnection);
}

void QtVideoOutput::createVideoOutput() {
  OPENAUTO_LOG(debug) << "[QtVideoOutput] create.";
  videoWidget_ = std::make_unique<QVideoWidget>(videoContainer_);
  mediaPlayer_ = std::make_unique<QMediaPlayer>();
}

bool QtVideoOutput::open() { return videoBuffer_.open(QIODevice::ReadWrite); }

bool QtVideoOutput::init() {
  emit startPlayback();
  return true;
}

void QtVideoOutput::stop() { emit stopPlayback(); }

void QtVideoOutput::write(uint64_t,
                          const aasdk::common::DataConstBuffer &buffer) {
  videoBuffer_.write(reinterpret_cast<const char *>(buffer.cdata), buffer.size);
}

void QtVideoOutput::resize() {
  if (videoWidget_ != nullptr && videoContainer_ != nullptr) {
    videoWidget_->resize(videoContainer_->size());
  }
}

void QtVideoOutput::onStartPlayback() {
  if (videoContainer_ == nullptr) {
    videoWidget_->setAspectRatioMode(Qt::IgnoreAspectRatio);
    videoWidget_->setFocus();
    videoWidget_->setWindowFlags(Qt::WindowStaysOnTopHint);
    videoWidget_->setFullScreen(true);
  } else {
    videoWidget_->resize(videoContainer_->size());
  }
  videoWidget_->show();

  mediaPlayer_->setVideoOutput(videoWidget_.get());
  mediaPlayer_->setSourceDevice(&videoBuffer_, QUrl());
  mediaPlayer_->play();
}

void QtVideoOutput::onStopPlayback() {
  videoWidget_->hide();
  mediaPlayer_->stop();
}

} // namespace projection
} // namespace openauto
