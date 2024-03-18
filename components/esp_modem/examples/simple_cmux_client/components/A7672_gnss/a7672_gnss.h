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
struct a7672_gnss {
    gps_fix_mode_t fix_mode;                                       /*!< Fix mode */
    gps_satellite_t sat_gps;                                       /*!< GPS satellite valid numbers scope: 00-12 */
    gps_satellite_t sat_glonass;                                   /*!< GLONASS satellite valid numbers scope: 00-12 */
    gps_satellite_t sat_beidou;                                    /*!< BEIDOU satellite valid numbers scope: 00-12 */
    latitude_gnss_t latitude;                                      /*!< Latitude (degrees) */
    longitude_gnss_t longitude;                                    /*!< Longitude (degrees) */
    gps_date_t date;                                               /*!< Fix date */
    gps_time_t tim;                                                /*!< time in UTC */
    float altitude;                                                /*!< Altitude (meters) */
    float speed;                                                   /*!< Speed Over Ground knots */
    float cog;                                                     /*!< Course over ground */
    float dop_h;                                                   /*!< Horizontal dilution of precision */
    float dop_p;                                                   /*!< Position dilution of precision  */
    float dop_v;                                                   /*!< Vertical dilution of precision  */
};

typedef struct a7672_gnss a7672_gnss_t;


#ifdef __cplusplus
}
#endif
