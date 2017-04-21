// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <memory>

#include "apps/media/lib/flog/flog.h"
#include "apps/media/services/logs/media_sink_channel.fidl.h"
#include "apps/media/services/media_sink.fidl.h"
#include "apps/media/services/timeline_controller.fidl.h"
#include "apps/media/src/fidl/fidl_conversion_pipeline_builder.h"
#include "apps/media/src/media_service/media_service_impl.h"
#include "apps/media/src/util/incident.h"
#include "lib/fidl/cpp/bindings/binding.h"

namespace media {

// Fidl agent that consumes a stream and delivers it to a destination specified
// by URL.
class MediaSinkImpl : public MediaServiceImpl::Product<MediaSink>,
                      public MediaSink {
 public:
  static std::shared_ptr<MediaSinkImpl> Create(
      fidl::InterfaceHandle<MediaRenderer> renderer_handle,
      fidl::InterfaceRequest<MediaSink> sink_request,
      MediaServiceImpl* owner);

  ~MediaSinkImpl() override;

  // MediaSink implementation.
  void GetTimelineControlPoint(
      fidl::InterfaceRequest<MediaTimelineControlPoint> req) override;

  void ConsumeMediaType(MediaTypePtr media_type,
                        const ConsumeMediaTypeCallback& callback) override;

 private:
  MediaSinkImpl(fidl::InterfaceHandle<MediaRenderer> renderer_handle,
                fidl::InterfaceRequest<MediaSink> sink_request,
                MediaServiceImpl* owner);

  // Builds the conversion pipeline.
  void BuildConversionPipeline();

  MediaServicePtr media_service_;
  MediaRendererPtr renderer_;
  ConsumeMediaTypeCallback consume_media_type_callback_;
  MediaTypePtr original_media_type_;
  std::unique_ptr<StreamType> stream_type_;
  std::unique_ptr<std::vector<std::unique_ptr<StreamTypeSet>>>
      supported_stream_types_;
  Incident got_supported_stream_types_;

  FLOG_INSTANCE_CHANNEL(logs::MediaSinkChannel, log_channel_);
};

}  // namespace media
