// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <mojo/system/main.h>

#include "apps/media/cpp/timeline_rate.h"
#include "apps/media/interfaces/media_service.mojom.h"
#include "apps/media/src/media_service/test/fake_renderer.h"
#include "apps/media/src/media_service/test/fake_wav_reader.h"
#include "apps/media/src/mojo/mojo_formatting.h"
#include "lib/ftl/logging.h"
#include "lib/mtl/tasks/message_loop.h"
#include "mojo/public/cpp/application/application_impl_base.h"
#include "mojo/public/cpp/application/connect.h"
#include "mojo/public/cpp/application/run_application.h"

namespace mojo {
namespace media {
namespace test {

class MediaPlayerTester : public ApplicationImplBase {
 public:
  void OnInitialize() override {
    FTL_LOG(INFO) << "MediaPlayerTest starting";

    MediaServicePtr media_service;
    ConnectToService(shell(), "mojo:media_service", GetProxy(&media_service));

    fake_renderer_.SetPtsRate(TimelineRate(48000, 1));

    fake_renderer_.ExpectPackets({{0, false, 4096, 0x20c39d1e31991800},
                                  {1024, false, 4096, 0xeaf137125d313800},
                                  {2048, false, 4096, 0x6162095671991800},
                                  {3072, false, 4096, 0x36e551c7dd41f800},
                                  {4096, false, 4096, 0x23dcbf6fb1991800},
                                  {5120, false, 4096, 0xee0a5963dd313800},
                                  {6144, false, 4096, 0x647b2ba7f1991800},
                                  {7168, false, 4096, 0x39fe74195d41f800},
                                  {8192, false, 4096, 0xb3de76b931991800},
                                  {9216, false, 4096, 0x7e0c10ad5d313800},
                                  {10240, false, 4096, 0xf47ce2f171991800},
                                  {11264, false, 4096, 0xca002b62dd41f800},
                                  {12288, false, 4096, 0xb6f7990ab1991800},
                                  {13312, false, 4096, 0x812532fedd313800},
                                  {14336, false, 4096, 0xf7960542f1991800},
                                  {15360, false, 4052, 0x7308a9824acbd5ea},
                                  {16373, true, 0, 0x0000000000000000}});

    SeekingReaderPtr fake_reader_ptr;
    InterfaceRequest<SeekingReader> reader_request = GetProxy(&fake_reader_ptr);
    fake_reader_.Bind(reader_request.Pass());

    MediaRendererPtr fake_renderer_ptr;
    InterfaceRequest<MediaRenderer> renderer_request =
        GetProxy(&fake_renderer_ptr);
    fake_renderer_.Bind(renderer_request.Pass());

    media_service->CreatePlayer(fake_reader_ptr.Pass(),
                                fake_renderer_ptr.Pass(), nullptr,
                                GetProxy(&media_player_));

    HandleStatusUpdates();
    media_player_->Play();
  }

  void HandleStatusUpdates(uint64_t version = MediaPlayer::kInitialStatus,
                           MediaPlayerStatusPtr status = nullptr) {
    if (status) {
      if (status->end_of_stream) {
        ended_ = true;
        FTL_LOG(INFO) << "MediaPlayerTest "
                      << (fake_renderer_.expected() ? "SUCCEEDED" : "FAILED");
        mtl::MessageLoop::GetCurrent()->PostQuitTask();
      }
    }

    // Request a status update.
    media_player_->GetStatus(
        version, [this](uint64_t version, MediaPlayerStatusPtr status) {
          HandleStatusUpdates(version, status.Pass());
        });
  }

  FakeWavReader fake_reader_;
  FakeRenderer fake_renderer_;
  MediaPlayerPtr media_player_;
  bool ended_ = false;
};

}  // namespace test
}  // namespace media
}  // namespace mojo

MojoResult MojoMain(MojoHandle application_request) {
  FTL_DCHECK(application_request != MOJO_HANDLE_INVALID)
      << "Must be hosted by application_manager";
  mojo::media::test::MediaPlayerTester tester;
  return mojo::RunApplication(application_request, &tester);
}
