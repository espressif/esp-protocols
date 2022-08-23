/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

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
