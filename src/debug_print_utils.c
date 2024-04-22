/* Copyright 2024 Tenera Care
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "debug_print_utils.h"

char* debug_print_ble_addr(ble_addr_t* addr)
{
    // 2 char per byte * 6 bytes + 5 colon + 1 null terminator
    // 12 + 5 + 1 == 18
    static char addr_buf[18];

    if (addr != NULL)
    {
        // NimBLE addresses are stored in reverse order so we print these from index 5 to 0
        snprintf(
            addr_buf,
            18,
            "%02X:%02X:%02X:%02X:%02X:%02X",
            addr->val[5],
            addr->val[4],
            addr->val[3],
            addr->val[2],
            addr->val[1],
            addr->val[0]
        );
    }
    else
    {
        strcpy(addr_buf, "null");
    }

    return addr_buf;
}
