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
#ifdef USE_GST

#include "openauto/Projection/GSTVideoOutput.hpp"
#include "OpenautoLog.hpp"
#include "aasdk/Common/Data.hpp"
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>


namespace openauto {
namespace projection {

GSTVideoOutput::GSTVideoOutput(
    configuration::IConfiguration::Pointer configuration,
    QWidget *videoContainer, std::function<void(bool)> activeCallback)
    : VideoOutput(std::move(configuration)), videoContainer_(videoContainer),
      activeCallback_(activeCallback) {
  this->moveToThread(QApplication::instance()->thread());

  QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

  // Ensure qml6glsink plugin is loaded and registers org.freedesktop.gstreamer.Qt6GLVideoItem before QML loads
  if (GstElement *dummySink = gst_element_factory_make("qml6glsink", nullptr)) {
    gst_object_unref(dummySink);
  }

  videoWidget_ = new QQuickWidget(videoContainer_);
  videoWidget_->setClearColor(Qt::black);

  videoWidget_->setSource(QUrl("qrc:/aa_video.qml"));
  videoWidget_->setResizeMode(QQuickWidget::SizeRootObjectToView);

  if (QQuickWindow *qw = videoWidget_->quickWindow()) {
    connect(qw, &QQuickWindow::sceneGraphInitialized, this,
            &GSTVideoOutput::tryStartPipeline, Qt::QueuedConnection);
  }

  GError *error = nullptr;
  const char *vidLaunchStr =
      "appsrc name=mysrc is-live=true block=false max-latency=100 "
      "do-timestamp=true stream-type=stream ! queue ! h264parse ! "
#ifdef RPI
#ifdef PI4
      "v4l2h264dec ! "
#else
      "omxh264dec ! "
#endif
#else
      "avdec_h264 ! "
#endif
      "videocrop top=0 bottom=0 name=videocropper ! videoconvert ! capsfilter "
      "caps=video/x-raw name=mycapsfilter ! glupload ! qml6glsink name=mysink sync=false async=false";
#ifdef RPI
  OPENAUTO_LOG(info) << "[GSTVideoOutput] RPI Build, running with " <<
#ifdef PI4
      "v4l2h264dec";
#else
      "omxh264dec";
#endif
#endif

  vidPipeline_ = gst_parse_launch(vidLaunchStr, &error);
  GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(vidPipeline_));
  gst_bus_add_watch(bus, (GstBusFunc)&GSTVideoOutput::busCallback, this);
  gst_object_unref(bus);

  GstElement *sink = gst_bin_get_by_name(GST_BIN(vidPipeline_), "mysink");
  QQuickItem *rootObj = videoWidget_->rootObject();
  QQuickItem *videoItem = (rootObj != nullptr)
                              ? rootObj->findChild<QQuickItem *>("videoItem")
                              : qobject_cast<QQuickItem *>(rootObj);
  if (videoItem == nullptr && rootObj != nullptr &&
      rootObj->objectName() == "videoItem") {
    videoItem = rootObj;
  }
  if (sink != nullptr) {
    if (videoItem != nullptr) {
      g_object_set(sink, "widget", videoItem, nullptr);
    } else {
      OPENAUTO_LOG(error)
          << "[GSTVideoOutput] Failed to find QQuickItem 'videoItem'";
    }
    g_object_set(sink, "force-aspect-ratio", false, nullptr);
    gst_object_unref(sink);
  } else {
    OPENAUTO_LOG(error) << "[GSTVideoOutput] Failed to find GstElement 'mysink'";
  }

  vidSrc_ = GST_APP_SRC(gst_bin_get_by_name(GST_BIN(vidPipeline_), "mysrc"));
  gst_app_src_set_stream_type(vidSrc_, GST_APP_STREAM_TYPE_STREAM);

  vidCrop_ = GST_VIDEO_FILTER(
      gst_bin_get_by_name(GST_BIN(vidPipeline_), "videocropper"));

  connect(this, &GSTVideoOutput::startPlayback, this,
          &GSTVideoOutput::onStartPlayback, Qt::QueuedConnection);
  connect(this, &GSTVideoOutput::stopPlayback, this,
          &GSTVideoOutput::onStopPlayback, Qt::QueuedConnection);
}

GSTVideoOutput::~GSTVideoOutput() {
  gst_object_unref(vidPipeline_);
  gst_object_unref(vidSrc_);
}

gboolean GSTVideoOutput::busCallback(GstBus *, GstMessage *message,
                                     gpointer *) {
  gchar *debug;
  GError *err;
  gchar *name;

  switch (GST_MESSAGE_TYPE(message)) {
  case GST_MESSAGE_ERROR:
    gst_message_parse_error(message, &err, &debug);
    OPENAUTO_LOG(info) << "[GSTVideoOutput] Error " << err->message;
    g_error_free(err);
    g_free(debug);
    break;
  case GST_MESSAGE_WARNING:
    gst_message_parse_warning(message, &err, &debug);
    OPENAUTO_LOG(info) << "[GSTVideoOutput] Warning " << err->message
                       << " | Debug " << debug;
    name = (gchar *)GST_MESSAGE_SRC_NAME(message);
    OPENAUTO_LOG(info) << "[GSTVideoOutput] Name of src "
                       << (name ? name : "nil");
    g_error_free(err);
    g_free(debug);
    break;
  case GST_MESSAGE_EOS:
    OPENAUTO_LOG(info) << "[GSTVideoOutput] End of stream";
    break;
  case GST_MESSAGE_STATE_CHANGED:
  default:
    break;
  }

  return TRUE;
}

bool GSTVideoOutput::open() {
  {
    std::lock_guard<std::mutex> lock(bufferMutex_);
    pendingBuffers_.clear();
    playbackRequested_ = true;
    pipelineActive_ = false;
  }

  if (!probeAdded_) {
    GstElement *capsFilter =
        gst_bin_get_by_name(GST_BIN(vidPipeline_), "mycapsfilter");
    if (capsFilter != nullptr) {
      GstPad *convertPad = gst_element_get_static_pad(capsFilter, "sink");
      if (convertPad != nullptr) {
        gst_pad_add_probe(convertPad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
                          &GSTVideoOutput::convertProbe, this, nullptr);
        gst_object_unref(convertPad);
      }
      gst_object_unref(capsFilter);
    }
    probeAdded_ = true;
  }

  QMetaObject::invokeMethod(this, "onStartPlayback", Qt::QueuedConnection);
  return true;
}

void GSTVideoOutput::tryStartPipeline() {
  std::lock_guard<std::mutex> lock(bufferMutex_);
  if (!playbackRequested_) {
    return;
  }
  if (!pipelineActive_) {
    GstStateChangeReturn ret =
        gst_element_set_state(vidPipeline_, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
      OPENAUTO_LOG(debug) << "[GSTVideoOutput] Waiting for QML scene graph "
                             "before transitioning pipeline to PLAYING";
      QTimer::singleShot(40, this, &GSTVideoOutput::tryStartPipeline);
      return;
    }
    OPENAUTO_LOG(info)
        << "[GSTVideoOutput] Pipeline transitioned to PLAYING (ret=" << ret
        << "), flushing " << pendingBuffers_.size() << " buffered packets";
    pipelineActive_ = true;
  }

  for (const auto &pkt : pendingBuffers_) {
    GstBuffer *buf = gst_buffer_new_and_alloc(pkt.size());
    gst_buffer_fill(buf, 0, pkt.data(), pkt.size());
    int ret = gst_app_src_push_buffer((GstAppSrc *)vidSrc_, buf);
    if (ret != GST_FLOW_OK) {
      OPENAUTO_LOG(info) << "[GSTVideoOutput] flush buffer returned " << ret
                         << " for " << pkt.size() << "bytes";
    }
  }
  pendingBuffers_.clear();
}

GstPadProbeReturn GSTVideoOutput::convertProbe(GstPad *pad,
                                               GstPadProbeInfo *info, void *) {
  GstEvent *event = GST_PAD_PROBE_INFO_EVENT(info);
  if (GST_PAD_PROBE_INFO_TYPE(info) & GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM) {
    if (GST_EVENT_TYPE(event) == GST_EVENT_SEGMENT) {
      GstCaps *caps = gst_pad_get_current_caps(pad);
      if (caps != nullptr) {
        GstVideoInfo *vinfo = gst_video_info_new();
        gst_video_info_from_caps(vinfo, caps);
        OPENAUTO_LOG(info) << "[GSTVideoOutput] Video Width: " << vinfo->width;
        OPENAUTO_LOG(info) << "[GSTVideoOutput] Video Height: "
                           << vinfo->height;
      }

      return GST_PAD_PROBE_REMOVE;
    }
  }

  return GST_PAD_PROBE_OK;
}

bool GSTVideoOutput::init() {
  OPENAUTO_LOG(info) << "[GSTVideoOutput] init";
  emit startPlayback();

  return true;
}

void GSTVideoOutput::write(uint64_t timestamp,
                           const aasdk::common::DataConstBuffer &buffer) {
  std::lock_guard<std::mutex> lock(bufferMutex_);
  if (!pipelineActive_) {
    if (pendingBuffers_.size() < 512) {
      pendingBuffers_.emplace_back(buffer.cdata, buffer.cdata + buffer.size);
    }
    return;
  }

  GstBuffer *buffer_ = gst_buffer_new_and_alloc(buffer.size);
  gst_buffer_fill(buffer_, 0, buffer.cdata, buffer.size);
  int ret = gst_app_src_push_buffer((GstAppSrc *)vidSrc_, buffer_);
  if (ret != GST_FLOW_OK) {
    OPENAUTO_LOG(info) << "[GSTVideoOutput] push buffer returned " << ret
                       << " for " << buffer.size << "bytes";
  }
}

void GSTVideoOutput::onStartPlayback() {
  if (activeCallback_ != nullptr) {
    activeCallback_(true);
  }

  if (videoContainer_ == nullptr) {
    OPENAUTO_LOG(info)
        << "[GSTVideoOutput] No video container, setting projection fullscreen";
    videoWidget_->setFocus();
    videoWidget_->setWindowFlags(Qt::WindowStaysOnTopHint |
                                 Qt::FramelessWindowHint);
    videoWidget_->showFullScreen();
  } else {
    OPENAUTO_LOG(info) << "[GSTVideoOutput] Resizing to video container";
    this->resize();
  }
  videoWidget_->show();
  QCoreApplication::processEvents();
  this->tryStartPipeline();
}

void GSTVideoOutput::stop() { emit stopPlayback(); }

void GSTVideoOutput::onStopPlayback() {
  {
    std::lock_guard<std::mutex> lock(bufferMutex_);
    playbackRequested_ = false;
    pipelineActive_ = false;
    pendingBuffers_.clear();
  }

  if (activeCallback_ != nullptr) {
    activeCallback_(false);
  }

  OPENAUTO_LOG(info) << "[GSTVideoOutput] stop.";
  gst_element_set_state(vidPipeline_, GST_STATE_NULL);
  videoWidget_->hide();
}

void GSTVideoOutput::resize() {
  OPENAUTO_LOG(info) << "[GSTVideoOutput] Got resize request to "
                     << videoContainer_->width() << "x"
                     << videoContainer_->height();

  if (videoWidget_ != nullptr && videoContainer_ != nullptr) {
    videoWidget_->resize(videoContainer_->size());
  }

  int width = 0;
  int height = 0;
  int containerWidth = videoContainer_->width();
  int containerHeight = videoContainer_->height();

  switch (this->getVideoResolution()) {
  case aasdk::proto::enums::VideoResolution_Enum__1080p:
    width = 1920;
    height = 1080;
    break;
  case aasdk::proto::enums::VideoResolution_Enum__720p:
    width = 1280;
    height = 720;
    break;
  case aasdk::proto::enums::VideoResolution_Enum__480p:
    width = 800;
    height = 480;
    break;
  default:
    break;
  }

  double marginWidth = 0;
  double marginHeight = 0;

  double widthRatio = (double)containerWidth / width;
  double heightRatio = (double)containerHeight / height;

  if (widthRatio > heightRatio) {
    // cropping height
    marginHeight = (widthRatio * height - containerHeight) / widthRatio;
    marginHeight /= 2;
  } else {
    // cropping width
    marginWidth = (heightRatio * width - containerWidth) / heightRatio;
    marginWidth /= 2;
  }

  OPENAUTO_LOG(info) << "[GSTVideoOutput] Android Auto is " << width << "x"
                     << height << ", calculated margins of: " << marginWidth
                     << "x" << marginHeight;
  g_object_set(vidCrop_, "top", (int)marginHeight, nullptr);
  g_object_set(vidCrop_, "bottom", (int)marginHeight, nullptr);
  g_object_set(vidCrop_, "left", (int)marginWidth, nullptr);
  g_object_set(vidCrop_, "right", (int)marginWidth, nullptr);
  this->configuration_->setVideoMargins(
      QRect(0, 0, (int)(marginWidth * 2), (int)(marginHeight * 2)));
}

} // namespace projection
} // namespace openauto

#endif
