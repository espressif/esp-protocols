//
// Created by david on 3/17/21.
//

#include "console_helper.hpp"
#include "esp_log.h"

static const char *TAG = "modem_console_helper";

ConsoleCommand::ConsoleCommand(const char* command, const char* help, std::vector<CommandArgs>& args, std::function<bool(ConsoleCommand *)> f):
        func(std::move(f))
{
    RegisterCommand(command, help, args);
}

void ConsoleCommand::RegisterCommand(const char* command, const char* help, std::vector<CommandArgs>& args)
{
    assert(last_command <= MAX_REPEAT_NR);
    void * common_arg = nullptr;
    for (auto it: args) {
        switch(it.type) {
            case ARG_END:
                break;
            case STR0:
                common_arg = arg_str0(it.shortopts, it.longopts, it.datatype, it.glossary);
                break;
            case STR1:
                common_arg = arg_str1(it.shortopts, it.longopts, it.datatype, it.glossary);
                break;
            case INT0:
                common_arg = arg_int0(it.shortopts, it.longopts, it.datatype, it.glossary);
                break;
            case INT1:
                common_arg = arg_int1(it.shortopts, it.longopts, it.datatype, it.glossary);
                break;
            case LIT0:
                common_arg = arg_lit0(it.shortopts, it.longopts, it.glossary);
                break;
        }
        if (common_arg) {
            arg_table.emplace_back(common_arg);
        } else {
            ESP_LOGE(TAG, "Creating argument parser failed for %s", it.glossary);
            abort();
        }
    }
    arg_table.emplace_back( arg_end(1));
    const esp_console_cmd_t command_def = {
            .command = command,
            .help = help,
            .hint = nullptr,
            .func = command_func_pts[last_command],
            .argtable = &arg_table[0]
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&command_def));
    last_command++;
    console_commands.emplace_back(this);
}

int ConsoleCommand::get_count(int index)
{
    return ((struct arg_str *)arg_table[index])->count;
}

std::string ConsoleCommand::get_string(int index)
{
    if (get_count(index) > 0) {
        return std::string(((struct arg_str *)arg_table[index])->sval[0]);
    }
    return std::string();
}

int ConsoleCommand::get_int(int index)
{
    if (get_count(index) > 0) {
        return *((struct arg_int *)arg_table[index])->ival;
    }
    return -1;
}


std::vector<ConsoleCommand*> ConsoleCommand::console_commands;
int ConsoleCommand::last_command = 0;



const esp_console_cmd_func_t ConsoleCommand::command_func_pts[] = {

#define TEMPLATE(index) ConsoleCommand::command_func_ ## index ,
    REPEAT_TEMPLATE_DEF(list of function pointers to command_func_XX() )

#undef  TEMPLATE
};


int ConsoleCommand::command_func(int argc, char **argv) {
    void * plain_arg_array = &arg_table[0];
    int nerrors = arg_parse(argc, argv, (void **)plain_arg_array);
    if (nerrors != 0) {
        arg_print_errors(stderr, (struct arg_end *) arg_table.back(), argv[0]);
        return 1;
    }
    if (func) {
        return func(this);
    }
    return 0;
}
