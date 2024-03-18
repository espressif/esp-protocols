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
 * @brief GPS N/S Indicator, N=north or S=south.
 *
 */
typedef enum {
    GPS_NS_INVALID, /*!< Not fixed */
    GPS_N,     /*!< N=north */
    GPS_S,    /*!< S=south */
} gps_ns_t;

/**
 * @brief GPS E/W Indicator, E=east or W=west.
 *
 */
typedef enum {
    GPS_EW_INVALID, /*!< Not fixed */
    GPS_E,     /*!< E=east */
    GPS_W,    /*!< W=west */
} gps_ew_t;


/**
 * @brief GPS fix type
 *
 */
typedef enum {
    GPS_FIX_INVALID, /*!< Not fixed */
    GPS_FIX_GPS,     /*!< GPS */
    GPS_FIX_DGPS,    /*!< Differential GPS */
} gps_fix_t;

/**
 * @brief GPS run type
 *
 */
typedef enum {
    GPS_RUN_INVALID, /*!< Not fixed */
    GPS_RUN_GPS,     /*!< GPS */
} gps_run_t;

/**
 * @brief GPS fix mode
 *
 */
typedef enum {
    GPS_MODE_INVALID,     /*!< Not fixed */
    GPS_MODE_2D,          /*!< 2D GPS */
    GPS_MODE_3D           /*!< 3D GPS */
} gps_fix_mode_t;

/**
 * @brief GPS satellite information
 *
 */
typedef struct {
    uint8_t num;       /*!< Satellite number */
} gps_satellite_t;

/**
 * @brief GPS time
 *
 */
typedef struct {
    uint8_t hour;      /*!< Hour */
    uint8_t minute;    /*!< Minute */
    uint8_t second;    /*!< Second */
    uint16_t thousand; /*!< Thousand */
} gps_time_t;

/**
 * @brief GPS date
 *
 */
typedef struct {
    uint8_t day;   /*!< Day (start from 1) */
    uint8_t month; /*!< Month (start from 1) */
    uint16_t year; /*!< Year (start from 2000) */
} gps_date_t;



/**
 * @brief GPS Latitude for GNSS
 *
 */
typedef struct {
    float degrees;                                                /*!< Latitude (degrees) */
    gps_ns_t latitude_ns;                                         /*!< Latitude N/S Indicator, N=north or S=south. */
} latitude_gnss_t;

/**
 * @brief GPS Latitude for GPS
 *
 */

typedef struct {
    uint8_t degrees;                                              /*!< Latitude (degrees) */
    float minutes;                                                /*!< Latitude (minutes) */
    gps_ns_t latitude_ns;                                         /*!< Latitude N/S Indicator, N=north or S=south. */
} latitude_gps_t;

/**
 * @brief GPS Longitude  for GNSS
 *
 */
typedef struct {
    float degrees;                                               /*!< Longitude (degrees) */
    gps_ew_t longitude_ew;                                       /*!< Longitude E/W Indicator, E=east or W=west. */
} longitude_gnss_t;

/**
 * @brief GPS Longitude  for GPS
 *
 */
typedef struct {
    uint8_t degrees;                                             /*!< Longitude (degrees) */
    float minutes;                                               /*!< Longitude (minutes) */
    gps_ew_t longitude_ew;                                       /*!< Longitude E/W Indicator, E=east or W=west. */
} longitude_gps_t;


#ifdef __cplusplus
}
#endif
