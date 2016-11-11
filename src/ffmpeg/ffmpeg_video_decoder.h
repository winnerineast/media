// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "apps/media/lib/timeline_rate.h"
#include "apps/media/src/ffmpeg/ffmpeg_decoder_base.h"

namespace media {

// Decoder implementation employing and ffmpeg video decoder.
// TODO(dalesat): Complete this.
class FfmpegVideoDecoder : public FfmpegDecoderBase {
 public:
  FfmpegVideoDecoder(AvCodecContextPtr av_codec_context);

  ~FfmpegVideoDecoder() override;

 protected:
  // FfmpegDecoderBase overrides.
  void Flush() override;

  int Decode(const AVPacket& av_packet,
             const ffmpeg::AvFramePtr& av_frame_ptr,
             PayloadAllocator* allocator,
             bool* frame_decoded_out) override;

  PacketPtr CreateOutputPacket(const AVFrame& av_frame,
                               PayloadAllocator* allocator) override;

  PacketPtr CreateOutputEndOfStreamPacket() override;

 private:
  using Extent = VideoStreamType::Extent;

  // Callback used by the ffmpeg decoder to acquire a buffer.
  static int AllocateBufferForAvFrame(AVCodecContext* av_codec_context,
                                      AVFrame* av_frame,
                                      int flags);

  // Callback used by the ffmpeg decoder to release a buffer.
  static void ReleaseBufferForAvFrame(void* opaque, uint8_t* buffer);

  // The allocator used by avcodec_decode_audio4 to provide context for
  // AllocateBufferForAvFrame. This is set only during the call to
  // avcodec_decode_audio4.
  PayloadAllocator* allocator_;

  // Used to supply PTS for end-of-stream.
  int64_t next_pts_ = Packet::kUnknownPts;
  TimelineRate pts_rate_;

  // TODO(dalesat): For investigation only...remove these three fields.
  bool first_frame_ = true;
  AVColorSpace colorspace_;
  Extent coded_size_;
};

}  // namespace media
