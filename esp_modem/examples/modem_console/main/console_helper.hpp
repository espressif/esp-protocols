//
// Created by david on 3/17/21.
//

#ifndef MODEM_CONSOLE_CONSOLE_HELPER_H
#define MODEM_CONSOLE_CONSOLE_HELPER_H

#include <vector>
#include <algorithm>
#include <functional>
#include <esp_console.h>
#include "argtable3/argtable3.h"
#include "repeat_helper.inc"

#define MAX_REPEAT_NR 10

enum arg_type {
    STR0,
    STR1,
    INT0,
    INT1,
    ARG_END,
};

struct CommandArgs {
    CommandArgs(arg_type t, const char * shopts, const char * lopts, const char * data, const char * glos):
    type(t), shortopts(shopts), longopts(lopts), datatype(data), glossary(glos) {}
    arg_type type;
    const char *shortopts;
    const char *longopts;
    const char *datatype;
    const char *glossary;
};

class ConsoleCommand {
public:
    template<typename T> explicit ConsoleCommand(const char* command, const char* help,
                                                 const T *arg_struct, size_t srg_struct_size , std::function<bool(ConsoleCommand *)> f):
                                                    func(std::move(f))
    {
        size_t args_plain_size = srg_struct_size / sizeof(CommandArgs);
        auto first_arg = reinterpret_cast<const CommandArgs *>(arg_struct);
        std::vector<CommandArgs> args(first_arg, first_arg + args_plain_size);
        RegisterCommand(command, help, args);
    }

    explicit ConsoleCommand(const char* command, const char* help, std::vector<CommandArgs>& args, std::function<bool(ConsoleCommand *)> f);
    int get_count(int index);
    template<typename T> int get_count_of(CommandArgs T::*member) { return get_count(index_arg(member)); }
    template<typename T> std::string get_string_of(CommandArgs T::*member) { return get_string(index_arg(member)); }

    std::string get_string(int index);

private:
    void RegisterCommand(const char* command, const char* help, std::vector<CommandArgs>& args);
    template<typename T> static constexpr size_t index_arg(CommandArgs T::*member)
        { return ((uint8_t *)&((T*)nullptr->*member) - (uint8_t *)nullptr)/sizeof(CommandArgs); }
    std::function<bool(ConsoleCommand *)> func;
    std::vector<void*> arg_table;

    int command_func(int argc, char **argv);

#define TEMPLATE(index) \
    static inline int command_func_ ## index(int argc, char **argv) \
        { return console_commands[index]->command_func(argc, argv); }

    REPEAT_TEMPLATE_DEF(definition of command_func_XX() )
#undef TEMPLATE

    static std::vector<ConsoleCommand*> console_commands;
    static int last_command;
    const static esp_console_cmd_func_t command_func_pts[];

};

#endif //MODEM_CONSOLE_CONSOLE_HELPER_H
