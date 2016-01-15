// Copyright 2015 The Weave Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "examples/provider/file_config_store.h"

#include <sys/stat.h>
#include <sys/utsname.h>

#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace weave {
namespace examples {

const char kSettingsDir[] = "/var/lib/weave/";

FileConfigStore::FileConfigStore(bool disable_security,
                                 const std::string& model_id)
    : disable_security_{disable_security}, model_id_{model_id} {}

std::string FileConfigStore::GetPath(const std::string& name) const {
  std::string path{kSettingsDir};
  path += path + "weave_settings_" + model_id_;
  if (!name.empty())
    path += "_" + name;
  return path + ".json";
}

bool FileConfigStore::LoadDefaults(Settings* settings) {
  char host_name[HOST_NAME_MAX] = {};
  gethostname(host_name, HOST_NAME_MAX);

  settings->name = host_name;
  settings->description = "";

  utsname uname_data;
  uname(&uname_data);

  settings->firmware_version = uname_data.sysname;
  settings->oem_name = "Unknown";
  settings->model_name = "Unknown";
  settings->model_id = model_id_;
  settings->pairing_modes = {PairingType::kEmbeddedCode};
  settings->embedded_code = "0000";

  // Keys owners:
  //   avakulenko@google.com
  //   gene@chromium.org
  //   vitalybuka@chromium.org
  settings->client_id =
      "338428340000-vkb4p6h40c7kja1k3l70kke8t615cjit.apps.googleusercontent."
      "com";
  settings->client_secret = "LS_iPYo_WIOE0m2VnLdduhnx";
  settings->api_key = "AIzaSyACK3oZtmIylUKXiTMqkZqfuRiCgQmQSAQ";

  settings->disable_security = disable_security_;
  return true;
}

std::string FileConfigStore::LoadSettings() {
  return LoadSettings("");
}

std::string FileConfigStore::LoadSettings(const std::string& name) {
  LOG(INFO) << "Loading settings from " << GetPath(name);
  std::ifstream str(GetPath(name));
  return std::string(std::istreambuf_iterator<char>(str),
                     std::istreambuf_iterator<char>());
}

void FileConfigStore::SaveSettings(const std::string& name,
                                   const std::string& settings) {
  CHECK(mkdir(kSettingsDir, S_IRWXU) == 0 || errno == EEXIST);
  LOG(INFO) << "Saving settings to " << GetPath(name);
  std::ofstream str(GetPath(name));
  str << settings;
}

}  // namespace examples
}  // namespace weave
