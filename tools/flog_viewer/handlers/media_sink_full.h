// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "apps/media/services/logs/media_sink_channel.fidl.h"
#include "apps/media/tools/flog_viewer/channel_handler.h"

namespace flog {
namespace handlers {

// Handler for MediaSinkChannel messages.
class MediaSinkFull : public ChannelHandler,
                      public media::logs::MediaSinkChannel {
 public:
  MediaSinkFull(const std::string& format);

  ~MediaSinkFull() override;

  // ChannelHandler implementation.
  void HandleMessage(fidl::Message* message) override;

  // MediaSinkChannel implementation.
  void BoundAs(uint64_t koid) override;

  void Config(media::MediaTypePtr input_type,
              media::MediaTypePtr output_type,
              fidl::Array<uint64_t> converter_koids,
              uint64_t renderer_koid) override;

 private:
  media::logs::MediaSinkChannelStub stub_;
  bool terse_;
};

}  // namespace handlers
}  // namespace flog
