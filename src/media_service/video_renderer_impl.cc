// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/media/src/media_service/video_renderer_impl.h"

#include "apps/media/lib/timeline/timeline.h"
#include "apps/mozart/services/geometry/cpp/geometry_util.h"
#include "lib/ftl/logging.h"

namespace media {

namespace {
constexpr uint32_t kVideoImageResourceId = 1;
}  // namespace

// static
std::shared_ptr<VideoRendererImpl> VideoRendererImpl::Create(
    fidl::InterfaceRequest<VideoRenderer> video_renderer_request,
    fidl::InterfaceRequest<MediaRenderer> media_renderer_request,
    MediaServiceImpl* owner) {
  return std::shared_ptr<VideoRendererImpl>(
      new VideoRendererImpl(std::move(video_renderer_request),
                            std::move(media_renderer_request), owner));
}

VideoRendererImpl::VideoRendererImpl(
    fidl::InterfaceRequest<VideoRenderer> video_renderer_request,
    fidl::InterfaceRequest<MediaRenderer> media_renderer_request,
    MediaServiceImpl* owner)
    : MediaServiceImpl::Product<VideoRenderer>(
          this,
          std::move(video_renderer_request),
          owner),
      video_frame_source_(std::make_shared<VideoFrameSource>()) {
  video_frame_source_->Bind(std::move(media_renderer_request));
}

VideoRendererImpl::~VideoRendererImpl() {}

void VideoRendererImpl::GetStatus(uint64_t version_last_seen,
                                  const GetStatusCallback& callback) {
  video_frame_source_->GetStatus(version_last_seen, callback);
}

void VideoRendererImpl::CreateView(
    fidl::InterfaceRequest<mozart::ViewOwner> view_owner_request) {
  FTL_DCHECK(video_frame_source_);
  new View(owner()->ConnectToEnvironmentService<mozart::ViewManager>(),
           std::move(view_owner_request), video_frame_source_);
}

VideoRendererImpl::View::View(
    mozart::ViewManagerPtr view_manager,
    fidl::InterfaceRequest<mozart::ViewOwner> view_owner_request,
    std::shared_ptr<VideoFrameSource> video_frame_source)
    : mozart::BaseView(std::move(view_manager),
                       std::move(view_owner_request),
                       "Video Renderer"),
      video_frame_source_(video_frame_source) {
  FTL_DCHECK(video_frame_source_);
  video_frame_source_->RegisterView(this);
}

VideoRendererImpl::View::~View() {
  video_frame_source_->UnregisterView(this);
}

void VideoRendererImpl::View::OnDraw() {
  FTL_DCHECK(properties());

  auto update = mozart::SceneUpdate::New();

  video_frame_source_->AdvanceReferenceTime(
      frame_tracker().frame_info().presentation_time);

  const mozart::Size& view_size = *properties()->view_layout->size;
  mozart::Size video_size = video_frame_source_->GetSize();

  if (view_size.width == 0 || view_size.height == 0 || video_size.width == 0 ||
      video_size.height == 0) {
    // Nothing to show yet.
    update->nodes.insert(mozart::kSceneRootNodeId, mozart::Node::New());
  } else {
    auto children = fidl::Array<uint32_t>::New(0);

    // Scale the video so it fills the view.
    float width_scale = static_cast<float>(view_size.width) /
                        static_cast<float>(video_size.width);
    float height_scale = static_cast<float>(view_size.height) /
                         static_cast<float>(video_size.height);

    mozart::TransformPtr transform =
        mozart::CreateScaleTransform(width_scale, height_scale);

    // Create the image node and apply the transform to it to scale and
    // position it properly.
    auto video_node = MakeVideoNode(std::move(transform), update);
    update->nodes.insert(children.size() + 1, std::move(video_node));
    children.push_back(children.size() + 1);

    // Create the root node.
    auto root = mozart::Node::New();
    root->child_node_ids = std::move(children);
    update->nodes.insert(mozart::kSceneRootNodeId, std::move(root));
  }

  scene()->Update(std::move(update));
  scene()->Publish(CreateSceneMetadata());

  buffer_producer_.Tick();

  if (video_frame_source_->views_should_animate()) {
    Invalidate();
  }
}

mozart::NodePtr VideoRendererImpl::View::MakeVideoNode(
    mozart::TransformPtr transform,
    const mozart::SceneUpdatePtr& update) {
  mozart::Size video_size = video_frame_source_->GetSize();

  if (video_size.width == 0 || video_size.height == 0) {
    return mozart::Node::New();
  }

  mozart::ResourcePtr vid_resource = DrawVideoTexture(video_size);
  FTL_DCHECK(vid_resource);
  update->resources.insert(kVideoImageResourceId, std::move(vid_resource));

  auto video_node = mozart::Node::New();
  video_node->content_transform = std::move(transform);
  video_node->op = mozart::NodeOp::New();
  video_node->op->set_image(mozart::ImageNodeOp::New());
  video_node->op->get_image()->content_rect = mozart::RectF::New();
  video_node->op->get_image()->content_rect->x = 0.0f;
  video_node->op->get_image()->content_rect->y = 0.0f;
  video_node->op->get_image()->content_rect->width = video_size.width;
  video_node->op->get_image()->content_rect->height = video_size.height;
  video_node->op->get_image()->image_resource_id = kVideoImageResourceId;

  return video_node;
}

mozart::ResourcePtr VideoRendererImpl::View::DrawVideoTexture(
    const mozart::Size& size) {
  std::unique_ptr<mozart::ProducedBufferHolder> buffer_holder =
      buffer_producer_.ProduceBuffer(size.height * size.width *
                                     sizeof(uint32_t));

  mozart::ImagePtr image = mozart::Image::New();
  image->size = size.Clone();
  image->stride = size.width * sizeof(uint32_t);
  image->pixel_format = mozart::Image::PixelFormat::B8G8R8A8;
  image->alpha_format = mozart::Image::AlphaFormat::OPAQUE;
  image->buffer = buffer_holder->GetBuffer();

  void* buffer = buffer_holder->shared_vmo()->Map();
  if (buffer == nullptr) {
    FTL_LOG(ERROR) << "Failed to map vmo for video frame";
  } else {
    video_frame_source_->GetRgbaFrame(static_cast<uint8_t*>(buffer), size);
  }

  buffer_holder->SetReadySignal();

  mozart::ResourcePtr resource = mozart::Resource::New();
  resource->set_image(mozart::ImageResource::New());
  resource->get_image()->image = std::move(image);
  return resource;
}

}  // namespace media
