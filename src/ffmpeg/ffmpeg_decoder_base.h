// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <limits>

#include "apps/media/src/decode/decoder.h"
#include "apps/media/src/ffmpeg/av_codec_context.h"
#include "apps/media/src/ffmpeg/av_frame.h"
#include "apps/media/src/ffmpeg/av_packet.h"
extern "C" {
#include "third_party/ffmpeg/libavcodec/avcodec.h"
}

namespace media {

// Abstract base class for ffmpeg-based decoders.
class FfmpegDecoderBase : public Decoder {
 public:
  FfmpegDecoderBase(AvCodecContextPtr av_codec_context);

  ~FfmpegDecoderBase() override;

  // Decoder implementation.
  std::unique_ptr<StreamType> output_stream_type() override;

  // Transform implementation.
  void Flush() override;

  bool TransformPacket(const PacketPtr& input,
                       bool new_input,
                       PayloadAllocator* allocator,
                       PacketPtr* output) override;

 protected:
  class DecoderPacket : public Packet {
   public:
    static PacketPtr Create(int64_t pts,
                            TimelineRate pts_rate,
                            bool keyframe,
                            AVBufferRef* av_buffer_ref) {
      return PacketPtr(
          new DecoderPacket(pts, pts_rate, keyframe, av_buffer_ref));
    }

   protected:
    ~DecoderPacket() override;

    void Release() override;

   private:
    DecoderPacket(int64_t pts,
                  TimelineRate pts_rate,
                  bool keyframe,
                  AVBufferRef* av_buffer_ref)
        : Packet(pts,
                 pts_rate,
                 keyframe,
                 false,
                 static_cast<size_t>(av_buffer_ref->size),
                 av_buffer_ref->data),
          av_buffer_ref_(av_buffer_ref) {
      FTL_DCHECK(av_buffer_ref->size >= 0);
    }

    AVBufferRef* av_buffer_ref_;
  };

  // Called when a new input packet is about to be processed. The default
  // implementation does nothing.
  virtual void OnNewInputPacket(const PacketPtr& packet);

  // Fills in |av_frame|, probably using an |AVBuffer| allocated via
  // CreateAVBuffer. |av_codec_context| may be distinct from context() and
  // should be used when a codec context is required.
  virtual int BuildAVFrame(const AVCodecContext& av_codec_context,
                           AVFrame* av_frame,
                           PayloadAllocator* allocator) = 0;

  // Creates a Packet from av_frame.
  virtual PacketPtr CreateOutputPacket(const AVFrame& av_frame,
                                       PayloadAllocator* allocator) = 0;

  // The ffmpeg codec context.
  const AvCodecContextPtr& context() { return av_codec_context_; }

  // Gets the current 'next PTS' value.
  int64_t next_pts() { return next_pts_; }

  // Sets the next PTS value. This is used by this class to create an
  // end-of-stream packet. Subclasses may also use it as needed.
  void set_next_pts(int64_t value) { next_pts_ = value; }

  // Gets the current PTS rate value.
  TimelineRate pts_rate() { return pts_rate_; }

  // Gets the PTS rate value.
  void set_pts_rate(TimelineRate value) { pts_rate_ = value; }

  // Creates an AVBuffer.
  AVBufferRef* CreateAVBuffer(uint8_t* payload_buffer,
                              size_t payload_buffer_size,
                              PayloadAllocator* allocator) {
    FTL_DCHECK(payload_buffer_size <=
               static_cast<size_t>(std::numeric_limits<int>::max()));
    return av_buffer_create(payload_buffer,
                            static_cast<int>(payload_buffer_size),
                            ReleaseBufferForAvFrame, allocator, /* flags */ 0);
  }

 private:
  // Callback used by the ffmpeg decoder to acquire a buffer.
  static int AllocateBufferForAvFrame(AVCodecContext* av_codec_context,
                                      AVFrame* av_frame,
                                      int flags);

  // Callback used by the ffmpeg decoder to release a buffer.
  static void ReleaseBufferForAvFrame(void* opaque, uint8_t* buffer);

  AvCodecContextPtr av_codec_context_;
  ffmpeg::AvFramePtr av_frame_ptr_;
  int64_t next_pts_ = Packet::kUnknownPts;
  TimelineRate pts_rate_;

  // The allocator used by avcodec_send_packet and avcodec_receive_frame to
  // provide context for AllocateBufferForAvFrame. This is set only during
  // those calls.
  PayloadAllocator* allocator_ = nullptr;
};

}  // namespace media
