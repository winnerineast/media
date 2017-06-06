// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <memory>
#include <thread>
#include <unordered_set>

#include "application/lib/app/application_context.h"
#include "lib/ftl/logging.h"
#include "lib/ftl/macros.h"
#include "lib/ftl/synchronization/mutex.h"
#include "lib/ftl/synchronization/thread_annotations.h"
#include "lib/ftl/tasks/task_runner.h"
#include "lib/mtl/tasks/message_loop.h"
#include "lib/mtl/threading/create_thread.h"

class FactoryServiceBase {
 public:
  // Provides common behavior for all objects created by the factory service.
  class ProductBase : public std::enable_shared_from_this<ProductBase> {
   public:
    virtual ~ProductBase();

    void QuitOnDestruct() { quit_on_destruct_ = true; }

   protected:
    explicit ProductBase(FactoryServiceBase* owner);

    // Returns the owner.
    FactoryServiceBase* owner() { return owner_; }

    // Tells the factory service to release this product. This method can only
    // be called after the first shared_ptr to the product is created.
    void ReleaseFromOwner() { owner_->RemoveProduct(shared_from_this()); }

   private:
    FactoryServiceBase* owner_;
    bool quit_on_destruct_ = false;
  };

  // A |ProductBase| that exposes FIDL interface |Interface|.
  template <typename Interface>
  class Product : public ProductBase {
   public:
    virtual ~Product() {}

   protected:
    Product(Interface* impl,
            fidl::InterfaceRequest<Interface> request,
            FactoryServiceBase* owner)
        : ProductBase(owner), binding_(impl, std::move(request)) {
      FTL_DCHECK(impl);
      Retain();
      binding_.set_connection_error_handler([this]() {
        binding_.set_connection_error_handler(nullptr);
        binding_.Close();
        Release();
      });
    }

    // Returns the binding established via the request in the constructor.
    const fidl::Binding<Interface>& binding() { return binding_; }

    // Increments the retention count.
    void Retain() { ++retention_count_; }

    // Decrements the retention count and calls UnbindAndReleaseFromOwner if
    // the count has reached zero. This method can only be called after the
    // first shared_ptr to the product is created.
    void Release() {
      if (--retention_count_ == 0) {
        UnbindAndReleaseFromOwner();
      }
    }

    // Closes the binding and calls ReleaseFromOwner. This method can only
    // be called after the first shared_ptr to the product is created.
    void UnbindAndReleaseFromOwner() {
      if (binding_.is_bound()) {
        binding_.Close();
      }

      ReleaseFromOwner();
    }

   private:
    size_t retention_count_ = 0;
    fidl::Binding<Interface> binding_;
  };

  FactoryServiceBase();

  virtual ~FactoryServiceBase();

  // Gets the application context.
  app::ApplicationContext* application_context() {
    return application_context_.get();
  }

  // Connects to a service registered with the application environment.
  template <typename Interface>
  fidl::InterfacePtr<Interface> ConnectToEnvironmentService(
      const std::string& interface_name = Interface::Name_) {
    return application_context_->ConnectToEnvironmentService<Interface>(
        interface_name);
  }

 protected:
  // Adds a product to the factory's collection of products. Threadsafe.
  template <typename ProductImpl>
  void AddProduct(std::shared_ptr<ProductImpl> product) {
    ftl::MutexLocker locker(&mutex_);
    products_.insert(std::static_pointer_cast<ProductBase>(product));
  }

  // Removes a product from the factory's collection of products. Threadsafe.
  void RemoveProduct(std::shared_ptr<ProductBase> product);

  // Creates a new product (by calling |product_creator|) on a new thread. The
  // thread is destroyed when the product is deleted.
  template <typename ProductImpl>
  void CreateProductOnNewThread(
      const std::function<std::shared_ptr<ProductImpl>()>& product_creator) {
    ftl::RefPtr<ftl::TaskRunner> task_runner;
    std::thread thread = mtl::CreateThread(&task_runner);
    task_runner->PostTask([this, product_creator]() {
      std::shared_ptr<ProductImpl> product = product_creator();
      product->QuitOnDestruct();
      AddProduct(product);
    });
    thread.detach();
  }

 private:
  std::unique_ptr<app::ApplicationContext> application_context_;
  ftl::RefPtr<ftl::TaskRunner> task_runner_;
  mutable ftl::Mutex mutex_;
  std::unordered_set<std::shared_ptr<ProductBase>> products_
      FTL_GUARDED_BY(mutex_);

  FTL_DISALLOW_COPY_AND_ASSIGN(FactoryServiceBase);
};

// For use by products when handling fidl requests.
// Checks the condition, and, if it's false, unbinds, releases from the owner
// and calls return. Doesn't support stream arguments.
// TODO(dalesat): Support stream arguments.
#define RCHECK(condition)                                             \
  if (!(condition)) {                                                 \
    FTL_LOG(ERROR) << "request precondition failed: " #condition "."; \
    mtl::MessageLoop::GetCurrent()->task_runner()->PostTask(          \
        [this]() { UnbindAndReleaseFromOwner(); });                   \
    return;                                                           \
  }
