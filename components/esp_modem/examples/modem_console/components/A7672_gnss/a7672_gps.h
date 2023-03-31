/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Origin: https://github.com/espressif/esp-idf/blob/master/examples/peripherals/uart/nmea0183_parser/main/nmea_parser.h
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif


#define GPS_MAX_SATELLITES_IN_USE (12)
#define GPS_MAX_SATELLITES_IN_VIEW (16)


/**
 * @brief GPS object
 *
 */
struct a7672_gps {
    float latitude;                                                /*!< Latitude (degrees) */
    gps_ns_t latitude_ns;                                          /*!< Latitude N/S Indicator, N=north or S=south. */
    float longitude;                                               /*!< Longitude (degrees) */
    gps_ew_t longitude_ew;                                         /*!< Longitude E/W Indicator, E=east or W=west. */
    gps_date_t date;                                               /*!< Fix date */
    gps_time_t tim;                                                /*!< time in UTC */
    float altitude;                                                /*!< Altitude (meters) */
    float speed;                                                   /*!< Speed Over Ground knots */
    float cog;                                                     /*!< Course over ground */
};

typedef struct a7672_gps a7672_gps_t;


#ifdef __cplusplus
}
#endif
