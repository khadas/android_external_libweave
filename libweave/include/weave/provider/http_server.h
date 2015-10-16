// Copyright 2015 The Weave Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_INCLUDE_WEAVE_PROVIDER_HTTP_SERVER_H_
#define LIBWEAVE_INCLUDE_WEAVE_PROVIDER_HTTP_SERVER_H_

#include <string>
#include <vector>

#include <base/callback.h>
#include <weave/stream.h>

namespace weave {
namespace provider {

class HttpServer {
 public:
  class Request {
   public:
    virtual ~Request() = default;

    virtual std::string GetPath() const = 0;
    virtual std::string GetFirstHeader(const std::string& name) const = 0;
    virtual std::string GetData() = 0;

    virtual void SendReply(int status_code,
                           const std::string& data,
                           const std::string& mime_type) = 0;
  };

  // Callback type for AddRequestHandler.
  using RequestHandlerCallback =
      base::Callback<void(std::unique_ptr<Request> request)>;

  // Adds callback called on new http/https requests with the given path.
  virtual void AddHttpRequestHandler(
      const std::string& path,
      const RequestHandlerCallback& callback) = 0;
  virtual void AddHttpsRequestHandler(
      const std::string& path,
      const RequestHandlerCallback& callback) = 0;

  virtual uint16_t GetHttpPort() const = 0;
  virtual uint16_t GetHttpsPort() const = 0;
  virtual std::vector<uint8_t> GetHttpsCertificateFingerprint() const = 0;

 protected:
  virtual ~HttpServer() = default;
};

}  // namespace provider
}  // namespace weave

#endif  // LIBWEAVE_INCLUDE_WEAVE_PROVIDER_HTTP_SERVER_H_
