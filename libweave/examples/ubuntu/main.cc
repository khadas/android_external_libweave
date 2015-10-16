// Copyright 2015 The Weave Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <bitset>

#include <base/bind.h>
#include <base/values.h>
#include <weave/device.h>
#include <weave/error.h>

#include "examples/ubuntu/avahi_client.h"
#include "examples/ubuntu/bluez_client.h"
#include "examples/ubuntu/curl_http_client.h"
#include "examples/ubuntu/event_http_server.h"
#include "examples/ubuntu/event_task_runner.h"
#include "examples/ubuntu/file_config_store.h"
#include "examples/ubuntu/network_manager.h"

namespace {

// Supported LED count on this device
const size_t kLedCount = 3;

void ShowUsage(const std::string& name) {
  LOG(ERROR) << "\nUsage: " << name << " <option(s)>"
             << "\nOptions:\n"
             << "\t-h,--help                    Show this help message\n"
             << "\t--v=LEVEL                    Logging level\n"
             << "\t-b,--bootstrapping           Force WiFi bootstrapping\n"
             << "\t-d,--disable_security        Disable privet security\n"
             << "\t--registration_ticket=TICKET Register device with the "
                                                "given ticket\n";
}

class CommandHandler {
 public:
  CommandHandler(weave::Device* device,
                 weave::provider::TaskRunner* task_runner)
      : device_{device}, task_runner_(task_runner) {
    device->AddStateDefinitionsFromJson(R"({
      "_greeter": {"_greetings_counter":"integer"},
      "_ledflasher": {"_leds": {"items": "boolean"}}
    })");

    device->SetStatePropertiesFromJson(R"({
      "_greeter": {"_greetings_counter": 0},
      "_ledflasher":{"_leds": [false, false, false]}
    })",
                                       nullptr);

    device->AddCommandDefinitionsFromJson(R"({
      "_greeter": {
        "_greet": {
          "minimalRole": "user",
          "parameters": {
            "_name": "string",
            "_count": {"minimum": 1, "maximum": 100}
          },
          "progress": { "_todo": "integer"},
          "results": { "_greeting": "string" }
        }
      },
      "_ledflasher": {
         "_set":{
           "parameters": {
             "_led": {"minimum": 1, "maximum": 3},
             "_on": "boolean"
           }
         },
         "_toggle":{
           "parameters": {
             "_led": {"minimum": 1, "maximum": 3}
           }
        }
      }
    })");
    device->AddCommandHandler(
        "_ledflasher._toggle",
        base::Bind(&CommandHandler::OnFlasherToggleCommand,
                   weak_ptr_factory_.GetWeakPtr()));
    device->AddCommandHandler("_greeter._greet",
                              base::Bind(&CommandHandler::OnGreetCommand,
                                         weak_ptr_factory_.GetWeakPtr()));
    device->AddCommandHandler("_ledflasher._set",
                              base::Bind(&CommandHandler::OnFlasherSetCommand,
                                         weak_ptr_factory_.GetWeakPtr()));
    device->AddCommandHandler("",
                              base::Bind(&CommandHandler::OnUnhandledCommand,
                                         weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  void DoGreet(const std::weak_ptr<weave::Command>& command, int todo) {
    auto cmd = command.lock();
    if (!cmd)
      return;

    std::string name;
    if (!cmd->GetParameters()->GetString("_name", &name)) {
      weave::ErrorPtr error;
      weave::Error::AddTo(&error, FROM_HERE, "example",
                          "invalid_parameter_value", "Name is missing");
      cmd->Abort(error.get(), nullptr);
      return;
    }

    if (todo-- > 0) {
      LOG(INFO) << "Hello " << name;

      base::DictionaryValue progress;
      progress.SetInteger("_todo", todo);
      cmd->SetProgress(progress, nullptr);

      base::DictionaryValue state;
      state.SetInteger("_greeter._greetings_counter", ++counter_);
      device_->SetStateProperties(state, nullptr);
    }

    if (todo > 0) {
      task_runner_->PostDelayedTask(
          FROM_HERE, base::Bind(&CommandHandler::DoGreet,
                                weak_ptr_factory_.GetWeakPtr(), command, todo),
          base::TimeDelta::FromSeconds(1));
      return;
    }

    base::DictionaryValue result;
    result.SetString("_greeting", "Hello " + name);
    cmd->Complete(result, nullptr);
    LOG(INFO) << cmd->GetName() << " command finished: " << result;
    LOG(INFO) << "New state: " << *device_->GetState();
  }

  void OnGreetCommand(const std::weak_ptr<weave::Command>& command) {
    auto cmd = command.lock();
    if (!cmd)
      return;
    LOG(INFO) << "received command: " << cmd->GetName();

    int todo = 1;
    cmd->GetParameters()->GetInteger("_count", &todo);
    DoGreet(command, todo);
  }

  void OnFlasherSetCommand(const std::weak_ptr<weave::Command>& command) {
    auto cmd = command.lock();
    if (!cmd)
      return;
    LOG(INFO) << "received command: " << cmd->GetName();
    int32_t led_index = 0;
    bool cmd_value = false;
    if (cmd->GetParameters()->GetInteger("_led", &led_index) &&
        cmd->GetParameters()->GetBoolean("_on", &cmd_value)) {
      // Display this command in terminal
      LOG(INFO) << cmd->GetName() << " _led: " << led_index
                << ", _on: " << (cmd_value ? "true" : "false");

      led_index--;
      int new_state = cmd_value ? 1 : 0;
      int cur_state = led_status_[led_index];
      led_status_[led_index] = new_state;

      if (cmd_value != cur_state) {
        UpdateLedState();
      }
      cmd->Complete({}, nullptr);
      return;
    }
    weave::ErrorPtr error;
    weave::Error::AddTo(&error, FROM_HERE, "example", "invalid_parameter_value",
                        "Invalid parameters");
    cmd->Abort(error.get(), nullptr);
  }

  void OnFlasherToggleCommand(const std::weak_ptr<weave::Command>& command) {
    auto cmd = command.lock();
    if (!cmd)
      return;
    LOG(INFO) << "received command: " << cmd->GetName();
    int32_t led_index = 0;
    if (cmd->GetParameters()->GetInteger("_led", &led_index)) {
      LOG(INFO) << cmd->GetName() << " _led: " << led_index;
      led_index--;
      led_status_[led_index] = ~led_status_[led_index];

      UpdateLedState();
      cmd->Complete({}, nullptr);
      return;
    }
    weave::ErrorPtr error;
    weave::Error::AddTo(&error, FROM_HERE, "example", "invalid_parameter_value",
                        "Invalid parameters");
    cmd->Abort(error.get(), nullptr);
  }

  void OnUnhandledCommand(const std::weak_ptr<weave::Command>& command) {
    auto cmd = command.lock();
    if (!cmd)
      return;
    LOG(INFO) << cmd->GetName() << " unimplemented command: ignored";
  }

  void UpdateLedState(void) {
    base::ListValue list;
    for (uint32_t i = 0; i < led_status_.size(); i++)
      list.AppendBoolean(led_status_[i] ? true : false);

    device_->SetStateProperty("_ledflasher._leds", list, nullptr);
  }

  weave::Device* device_{nullptr};
  weave::provider::TaskRunner* task_runner_{nullptr};

  int counter_{0};

  // Simulate LED status on this device so client app could explore
  // Each bit represents one device, indexing from LSB
  std::bitset<kLedCount> led_status_{0};

  base::WeakPtrFactory<CommandHandler> weak_ptr_factory_{this};
};

void OnRegisterDeviceDone(weave::Device* device, weave::ErrorPtr error) {
  if (error)
    LOG(ERROR) << "Fail to register device: " << error->GetMessage();
  else
    LOG(INFO) << "Device registered: " << device->GetSettings().cloud_id;
}

}  // namespace

int main(int argc, char** argv) {
  bool force_bootstrapping = false;
  bool disable_security = false;
  std::string registration_ticket;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-h" || arg == "--help") {
      ShowUsage(argv[0]);
      return 0;
    } else if (arg == "-b" || arg == "--bootstrapping") {
      force_bootstrapping = true;
    } else if (arg == "-d" || arg == "--disable_security") {
      disable_security = true;
    } else if (arg.find("--registration_ticket") != std::string::npos) {
      auto pos = arg.find("=");
      if (pos == std::string::npos) {
        ShowUsage(argv[0]);
        return 1;
      }
      registration_ticket = arg.substr(pos + 1);
    } else if (arg.find("--v") != std::string::npos) {
      auto pos = arg.find("=");
      if (pos == std::string::npos) {
        ShowUsage(argv[0]);
        return 1;
      }
      logging::SetMinLogLevel(-std::stoi(arg.substr(pos + 1)));
    } else {
      ShowUsage(argv[0]);
      return 1;
    }
  }

  weave::examples::FileConfigStore config_store{disable_security};
  weave::examples::EventTaskRunner task_runner;
  weave::examples::CurlHttpClient http_client{&task_runner};
  weave::examples::NetworkImpl network{&task_runner, force_bootstrapping};
  weave::examples::AvahiClient dns_sd;
  weave::examples::HttpServerImpl http_server{&task_runner};
  weave::examples::BluetoothImpl bluetooth;

  auto device = weave::Device::Create(
      &config_store, &task_runner, &http_client, &network, &dns_sd,
      &http_server,
      weave::examples::NetworkImpl::HasWifiCapability() ? &network : nullptr,
      &bluetooth);

  if (!registration_ticket.empty()) {
    device->Register(registration_ticket,
                     base::Bind(&OnRegisterDeviceDone, device.get()));
  }

  CommandHandler handler(device.get(), &task_runner);
  task_runner.Run();

  LOG(INFO) << "exit";
  return 0;
}
