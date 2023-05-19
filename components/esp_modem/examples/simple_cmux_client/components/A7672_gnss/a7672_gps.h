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
    latitude_gps_t latitude;                                       /*!< Latitude (degrees) */
    longitude_gps_t longitude;                                     /*!< Longitude (degrees) */
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
