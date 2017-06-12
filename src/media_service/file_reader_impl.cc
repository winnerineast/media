// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/media/src/media_service/file_reader_impl.h"

#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#include "lib/ftl/files/file_descriptor.h"
#include "lib/ftl/logging.h"
#include "lib/mtl/tasks/message_loop.h"

namespace media {

// static
std::shared_ptr<FileReaderImpl> FileReaderImpl::Create(
    const fidl::String& path,
    fidl::InterfaceRequest<SeekingReader> request,
    MediaServiceImpl* owner) {
  return std::shared_ptr<FileReaderImpl>(
      new FileReaderImpl(path, std::move(request), owner));
}

FileReaderImpl::FileReaderImpl(const fidl::String& path,
                               fidl::InterfaceRequest<SeekingReader> request,
                               MediaServiceImpl* owner)
    : MediaServiceImpl::Product<SeekingReader>(this, std::move(request), owner),
      fd_(open(path.get().c_str(), O_RDONLY)),
      buffer_(kBufferSize) {
  result_ = fd_.is_valid() ? MediaResult::OK : MediaResult::NOT_FOUND;

  if (result_ == MediaResult::OK) {
    off_t seek_result = lseek(fd_.get(), 0, SEEK_END);
    if (seek_result >= 0) {
      size_ = static_cast<uint64_t>(seek_result);
    } else {
      size_ = kUnknownSize;
      // TODO(dalesat): More specific error code.
      result_ = MediaResult::UNKNOWN_ERROR;
    }
  }
}

FileReaderImpl::~FileReaderImpl() {
  if (wait_id_ != 0) {
    fidl::GetDefaultAsyncWaiter()->CancelWait(wait_id_);
  }
}

void FileReaderImpl::Describe(const DescribeCallback& callback) {
  callback(result_, size_, true);
}

void FileReaderImpl::ReadAt(uint64_t position, const ReadAtCallback& callback) {
  FTL_DCHECK(position < size_);

  if (result_ != MediaResult::OK) {
    callback(result_, mx::socket());
    return;
  }

  if (socket_) {
    if (wait_id_ != 0) {
      fidl::GetDefaultAsyncWaiter()->CancelWait(wait_id_);
      wait_id_ = 0;
    }
    socket_.reset();
  }

  off_t seek_result = lseek(fd_.get(), position, SEEK_SET);
  if (seek_result < 0) {
    // TODO(dalesat): More specific error code.
    FTL_LOG(ERROR) << "seek failed, result " << seek_result;
    result_ = MediaResult::UNKNOWN_ERROR;
    callback(result_, mx::socket());
    return;
  }

  mx::socket other_socket;
  mx_status_t status = mx::socket::create(0u, &socket_, &other_socket);
  if (status != MX_OK) {
    // TODO(dalesat): More specific error code.
    FTL_LOG(ERROR) << "mx::socket::create failed, status " << status;
    result_ = MediaResult::UNKNOWN_ERROR;
    callback(result_, mx::socket());
    return;
  }

  remaining_buffer_bytes_count_ = 0;
  reached_end_ = false;

  WriteToSocket();

  if (result_ != MediaResult::OK) {
    // Error occurred during WriteToSocket.
    FTL_LOG(ERROR) << "error occurred during WriteToSocket, result_ "
                   << result_;
    callback(result_, mx::socket());
    return;
  }

  callback(result_, std::move(other_socket));
}

// static
void FileReaderImpl::WriteToSocketStatic(mx_status_t status,
                                         mx_signals_t pending,
                                         void* closure) {
  FileReaderImpl* reader = reinterpret_cast<FileReaderImpl*>(closure);
  reader->wait_id_ = 0;
  if (status == MX_ERR_BAD_STATE) {
    // Run loop has aborted...the app is shutting down.
    reader->socket_.reset();
    return;
  }

  if (status != MX_OK) {
    FTL_LOG(ERROR) << "mx::socket::write failed, status " << status;
    reader->socket_.reset();
    return;
  }

  reader->WriteToSocket();
}

void FileReaderImpl::WriteToSocket() {
  while (true) {
    if (remaining_buffer_bytes_count_ == 0 && !reached_end_) {
      ReadFromFile();
    }

    if (remaining_buffer_bytes_count_ == 0) {
      return;
    }

    FTL_DCHECK(remaining_buffer_bytes_ != nullptr);

    size_t byte_count;
    mx_status_t status =
        socket_.write(0u, remaining_buffer_bytes_,
                      remaining_buffer_bytes_count_, &byte_count);

    if (status == MX_OK) {
      FTL_DCHECK(byte_count != 0);
      remaining_buffer_bytes_ += byte_count;
      remaining_buffer_bytes_count_ -= byte_count;
      continue;
    }

    if (status == MX_ERR_SHOULD_WAIT) {
      wait_id_ = fidl::GetDefaultAsyncWaiter()->AsyncWait(
          socket_.get(), MX_SOCKET_WRITABLE | MX_SOCKET_PEER_CLOSED,
          MX_TIME_INFINITE, FileReaderImpl::WriteToSocketStatic, this);
      return;
    }

    if (status == MX_ERR_PEER_CLOSED) {
      // Consumer end was closed. This is normal behavior, depending on what
      // the consumer is up to.
      socket_.reset();
      return;
    }

    FTL_LOG(ERROR) << "mx::socket::write failed, status " << status;
    socket_.reset();
    // TODO(dalesat): More specific error code.
    result_ = MediaResult::UNKNOWN_ERROR;
    return;
  }
}

void FileReaderImpl::ReadFromFile() {
  FTL_DCHECK(buffer_.size() == kBufferSize);
  FTL_DCHECK(!reached_end_);

  ssize_t result =
      ftl::ReadFileDescriptor(fd_.get(), buffer_.data(), kBufferSize);
  if (result < 0) {
    // TODO(dalesat): More specific error code.
    result_ = MediaResult::UNKNOWN_ERROR;
    return;
  }

  if (result < static_cast<ssize_t>(kBufferSize)) {
    reached_end_ = true;
  }

  remaining_buffer_bytes_count_ = static_cast<size_t>(result);
  remaining_buffer_bytes_ = buffer_.data();
}

}  // namespace media
