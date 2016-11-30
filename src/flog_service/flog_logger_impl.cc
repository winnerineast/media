// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/media/src/flog_service/flog_logger_impl.h"

#include "lib/ftl/files/file_descriptor.h"
#include "lib/ftl/logging.h"

namespace flog {

// static
std::shared_ptr<FlogLoggerImpl> FlogLoggerImpl::Create(
    fidl::InterfaceRequest<FlogLogger> request,
    uint32_t log_id,
    const std::string& label,
    std::shared_ptr<FlogDirectory> directory,
    FlogServiceImpl* owner) {
  return std::shared_ptr<FlogLoggerImpl>(
      new FlogLoggerImpl(std::move(request), log_id, label, directory, owner));
}

FlogLoggerImpl::FlogLoggerImpl(fidl::InterfaceRequest<FlogLogger> request,
                               uint32_t log_id,
                               const std::string& label,
                               std::shared_ptr<FlogDirectory> directory,
                               FlogServiceImpl* owner)
    : FlogServiceImpl::ProductBase(owner),
      id_(log_id),
      label_(label),
      fd_(directory->GetFile(log_id, label, true)) {
  fidl::internal::MessageValidatorList validators;
  router_.reset(new fidl::internal::Router(request.PassChannel(),
                                           std::move(validators),
                                           fidl::GetDefaultAsyncWaiter()));
  router_->set_incoming_receiver(this);
  router_->set_connection_error_handler([this]() { ReleaseFromOwner(); });
}

FlogLoggerImpl::~FlogLoggerImpl() {}

bool FlogLoggerImpl::Accept(fidl::Message* message) {
  FTL_DCHECK(message != nullptr);
  FTL_DCHECK(message->data_num_bytes() > 0);
  FTL_DCHECK(message->data() != nullptr);

  uint32_t message_size = message->data_num_bytes();

  WriteData(sizeof(message_size), &message_size);
  WriteData(message_size, message->data());

  return true;
}

bool FlogLoggerImpl::AcceptWithResponder(
    fidl::Message* message,
    fidl::MessageReceiverWithStatus* responder) {
  FTL_DCHECK(false) << "FlogLogger has no methods with responses";
  abort();
}

void FlogLoggerImpl::WriteData(uint32_t data_size, const void* data) {
  FTL_DCHECK(data_size > 0);
  FTL_DCHECK(data != nullptr);

  if (!ftl::WriteFileDescriptor(fd_.get(), reinterpret_cast<const char*>(data),
                                data_size)) {
    // TODO(dalesat): Handle error.
  }
}

}  // namespace flog
