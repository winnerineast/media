// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/media/tools/flog_viewer/handlers/media_demux.h"

#include <iostream>

#include "apps/media/services/logs/media_demux_channel.fidl.h"
#include "apps/media/tools/flog_viewer/flog_viewer.h"
#include "apps/media/tools/flog_viewer/handlers/media_formatting.h"

namespace flog {
namespace handlers {

std::ostream& operator<<(std::ostream& os,
                         const MediaDemuxAccumulator::Stream& value) {
  if (!value) {
    return os << begl << "NULL STREAM" << std::endl;
  } else {
    os << std::endl;
  }

  os << indent;
  os << begl << "type: " << value.type_;
  if (value.producer_channel_) {
    os << begl << "producer: " << *value.producer_channel_ << " ";
    value.producer_channel_->PrintAccumulator(os);
  } else {
    os << begl << "producer: <none>" << std::endl;
  }

  return os << outdent;
}

MediaDemux::MediaDemux(const std::string& format)
    : ChannelHandler(format),
      accumulator_(std::make_shared<MediaDemuxAccumulator>()) {
  stub_.set_sink(this);
}

MediaDemux::~MediaDemux() {}

void MediaDemux::HandleMessage(fidl::Message* message) {
  stub_.Accept(message);
}

std::shared_ptr<Accumulator> MediaDemux::GetAccumulator() {
  return accumulator_;
}

void MediaDemux::BoundAs(uint64_t koid) {
  terse_out() << entry() << "MediaDemux.BoundAs" << std::endl;
  terse_out() << indent;
  terse_out() << begl << "koid: " << AsKoid(koid) << std::endl;
  terse_out() << outdent;

  BindAs(koid);
}

void MediaDemux::NewStream(uint32_t index,
                           media::MediaTypePtr type,
                           uint64_t producer_address) {
  FTL_DCHECK(type);

  terse_out() << entry() << "MediaDemux.NewStream" << std::endl;
  terse_out() << indent;
  terse_out() << begl << "index: " << index << std::endl;
  terse_out() << begl << "type: " << type;
  terse_out() << begl << "producer_address: " << *AsChannel(producer_address)
              << std::endl;
  terse_out() << outdent;

  while (accumulator_->streams_.size() <= index) {
    accumulator_->streams_.emplace_back();
  }

  if (accumulator_->streams_[index]) {
    ReportProblem() << "NewStream index " << index << " already in use";
  }

  accumulator_->streams_[index].type_ = std::move(type);
  accumulator_->streams_[index].producer_channel_ = AsChannel(producer_address);
  accumulator_->streams_[index].producer_channel_->SetHasParent();
}

MediaDemuxAccumulator::MediaDemuxAccumulator() {}

MediaDemuxAccumulator::~MediaDemuxAccumulator() {}

void MediaDemuxAccumulator::Print(std::ostream& os) {
  os << "MediaDemux" << std::endl;
  os << indent;
  os << begl << "streams: " << streams_;
  Accumulator::Print(os);
  os << outdent;
}

MediaDemuxAccumulator::Stream::Stream() {}

MediaDemuxAccumulator::Stream::~Stream() {}

}  // namespace handlers
}  // namespace flog
