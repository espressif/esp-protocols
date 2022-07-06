/*
 * x.c
 *
 *  Created on: 06.07.2022
 *      Author: franz
 */



static const char esp_unknown_msg[] =  "ERROR";

const char *esp_err_to_name(esp_err_t code)
{
    return esp_unknown_msg;
}
