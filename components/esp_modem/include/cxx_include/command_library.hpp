// Copyright 2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once
namespace esp_modem::dce_commands {
command_result generic_command(CommandableIf *t, const std::string &command,
                               const std::list<std::string_view> &pass_phrase,
                               const std::list<std::string_view> &fail_phrase,
                               uint32_t timeout_ms);


command_result generic_command(CommandableIf *t, const std::string &command,
        const std::string &pass_phrase,
        const std::string &fail_phrase, uint32_t timeout_ms);

command_result generic_get_string(CommandableIf *t, const std::string &command, std::string_view &output, uint32_t timeout_ms = 500);

command_result generic_get_string(CommandableIf *t, const std::string &command, std::string &output, uint32_t timeout_ms = 500);

command_result generic_command_common(CommandableIf *t, const std::string &command, uint32_t timeout = 500);


							   
} // esp_modem::dce_commands