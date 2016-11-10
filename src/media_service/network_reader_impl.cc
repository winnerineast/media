// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/media/src/media_service/network_reader_impl.h"

#include <mx/datapipe.h>

#include "apps/modular/lib/app/connect.h"
#include "apps/network/services/network_service.fidl.h"
#include "lib/ftl/logging.h"

namespace media {

const char* NetworkReaderImpl::kContentLengthHeaderName = "Content-Length";
const char* NetworkReaderImpl::kAcceptRangesHeaderName = "Accept-Ranges";
const char* NetworkReaderImpl::kAcceptRangesHeaderBytesValue = "bytes";
const char* NetworkReaderImpl::kRangeHeaderName = "Range";

// static
std::shared_ptr<NetworkReaderImpl> NetworkReaderImpl::Create(
    const fidl::String& url,
    fidl::InterfaceRequest<SeekingReader> request,
    MediaServiceImpl* owner) {
  return std::shared_ptr<NetworkReaderImpl>(
      new NetworkReaderImpl(url, std::move(request), owner));
}

NetworkReaderImpl::NetworkReaderImpl(
    const fidl::String& url,
    fidl::InterfaceRequest<SeekingReader> request,
    MediaServiceImpl* owner)
    : MediaServiceImpl::Product<SeekingReader>(this, std::move(request), owner),
      url_(url) {
  network::NetworkServicePtr network_service =
      owner->ConnectToEnvironmentService<network::NetworkService>();

  network_service->CreateURLLoader(GetProxy(&url_loader_));

  network::URLRequestPtr url_request(network::URLRequest::New());
  url_request->url = url_;
  url_request->method = "HEAD";

  url_loader_->Start(
      std::move(url_request), [this](network::URLResponsePtr response) {
        // TODO(dalesat): Handle redirects.
        if (response->status_code != kStatusOk) {
          FTL_LOG(WARNING) << "HEAD response status code "
                           << response->status_code;
          result_ = response->status_code == kStatusNotFound
                        ? MediaResult::NOT_FOUND
                        : MediaResult::UNKNOWN_ERROR;
          ready_.Occur();
          return;
        }

        for (const network::HttpHeaderPtr& header : response->headers) {
          if (header->name == kContentLengthHeaderName) {
            size_ = std::stoull(header->value);
          } else if (header->name == kAcceptRangesHeaderName &&
                     header->value == kAcceptRangesHeaderBytesValue) {
            can_seek_ = true;
          }
        }

        ready_.Occur();
      });
}

NetworkReaderImpl::~NetworkReaderImpl() {}

void NetworkReaderImpl::Describe(const DescribeCallback& callback) {
  ready_.When([this, callback]() { callback(result_, size_, can_seek_); });
}

void NetworkReaderImpl::ReadAt(uint64_t position,
                               const ReadAtCallback& callback) {
  ready_.When([this, position, callback]() {
    if (result_ != MediaResult::OK) {
      callback(result_, mx::datapipe_consumer());
      return;
    }

    if (!can_seek_ && position != 0) {
      callback(MediaResult::INVALID_ARGUMENT, mx::datapipe_consumer());
      return;
    }

    network::URLRequestPtr request(network::URLRequest::New());
    request->url = url_;
    request->method = "GET";

    if (position != 0) {
      std::ostringstream value;
      value << kAcceptRangesHeaderBytesValue << "=" << position << "-";

      network::HttpHeaderPtr header(network::HttpHeader::New());
      header->name = kRangeHeaderName;
      header->value = value.str();

      request->headers = fidl::Array<network::HttpHeaderPtr>::New(1);
      request->headers[0] = std::move(header);
    }

    url_loader_->Start(
        std::move(request), [this, callback](network::URLResponsePtr response) {
          if (response->status_code != kStatusOk &&
              response->status_code != kStatusPartialContent) {
            FTL_LOG(WARNING) << "GET response status code "
                             << response->status_code;
            result_ = MediaResult::UNKNOWN_ERROR;
            callback(result_, mx::datapipe_consumer());
            return;
          }

          FTL_DCHECK(response->body);
          FTL_DCHECK(response->body->get_stream());
          callback(result_, std::move(response->body->get_stream()));
        });
  });
}

}  // namespace media
