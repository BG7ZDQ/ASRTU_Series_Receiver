/*
 * SGP4/SDP4 Satellite Propagation Library
 *
 * Copyright (c) 2025 Andrew C. Young <andrew@vaelen.org>
 * SPDX-License-Identifier: MIT
 *
 * Based on the Vallado reference implementation from CelesTrak.
 * See: https://celestrak.org/software/vallado-sw.php
 *
 * Features:
 * - C99 compatible
 * - No dynamic memory allocation
 * - Fixed-size buffers for embedded systems
 * - Error code based error handling (no exceptions)
 * - Full coordinate transforms (ECI, ECEF, Geodetic, ENU)
 * - Look angle calculations for antenna pointing
 *
 * Usage:
 *   #include "sgp4.h"
 *
 *   sgp4_tle_t tle;
 *   sgp4_parse_tle_2line(line1, line2, &tle);
 *
 *   sgp4_elements_t elements;
 *   sgp4_tle_to_elements(&tle, &elements);
 *
 *   sgp4_state_t state;
 *   sgp4_init(&state, &elements);
 *
 *   sgp4_result_t result;
 *   sgp4_propagate(&state, tsince_min, &result);
 */

#ifndef SGP4_H
#define SGP4_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

// ============================================================================
// Constants
// ============================================================================

#define SGP4_PI             3.14159265358979323846
#define SGP4_TWO_PI         (2.0 * SGP4_PI)
#define SGP4_DEG_TO_RAD     (SGP4_PI / 180.0)
#define SGP4_RAD_TO_DEG     (180.0 / SGP4_PI)
#define SGP4_X2O3           (2.0 / 3.0)

// WGS84 / EGM-96 Constants
#define SGP4_MU             398600.8           // Earth gravitational parameter (km^3/s^2)
#define SGP4_RADIUS_EARTH   6378.135           // Earth equatorial radius (km)
#define SGP4_J2             0.001082616        // Second gravitational zonal harmonic
#define SGP4_J3             (-0.00000253881)   // Third gravitational zonal harmonic
#define SGP4_J4             (-0.00000165597)   // Fourth gravitational zonal harmonic
#define SGP4_J3OJ2          (SGP4_J3 / SGP4_J2)
#define SGP4_XKE            0.0743669161331734132   // sqrt(GM) in Earth radii^1.5/min
#define SGP4_TUMIN          13.44683969695931       // Minutes per time unit
#define SGP4_VKMPERSEC      7.905366149846074       // km/s per velocity unit

// WGS84 ellipsoid constants
#define SGP4_WGS84_A        6378.137           // Semi-major axis (km)
#define SGP4_WGS84_F        (1.0 / 298.257223563)  // Flattening
#define SGP4_WGS84_E2       (SGP4_WGS84_F * (2.0 - SGP4_WGS84_F))  // Eccentricity squared

// Time constants
#define SGP4_J2000_JD       2451545.0          // Julian Date of J2000.0 epoch
#define SGP4_DAYS_PER_CENTURY 36525.0          // Days in a Julian century
#define SGP4_UNIX_EPOCH_JD  2440587.5          // Julian Date of Unix epoch (1970-01-01)

// Buffer sizes
#define SGP4_TLE_LINE_LEN   70                 // 69 chars + null
#define SGP4_TLE_NAME_LEN   25                 // Max satellite name length
#define SGP4_TLE_BUFFER_LEN 210                // 3 lines * 70
#define SGP4_LINE_BUFFER_LEN 256               // For file I/O

// ============================================================================
// Error Codes
// ============================================================================

typedef enum sgp4_error {
    SGP4_SUCCESS = 0,
    SGP4_ERROR_NULL_POINTER = -1,
    SGP4_ERROR_NOT_INITIALIZED = -2,
    SGP4_ERROR_SATELLITE_DECAYED = -3,
    SGP4_ERROR_INVALID_ECCENTRICITY = -4,
    SGP4_ERROR_INVALID_ORBIT = -5,
    SGP4_ERROR_BUFFER_TOO_SMALL = -6,
    SGP4_ERROR_PARSE_FAILED = -7,
    SGP4_ERROR_INVALID_TLE_FORMAT = -8,
    SGP4_ERROR_CHECKSUM_MISMATCH = -9,
    SGP4_ERROR_FILE_OPEN_FAILED = -10
} sgp4_error_t;

// ============================================================================
// Data Structures
// ============================================================================

// 3D Vector
typedef struct sgp4_vec3 {
    double x, y, z;
} sgp4_vec3_t;

// Geodetic coordinates
typedef struct sgp4_geodetic {
    double lat_rad;   // Latitude in radians (-pi/2 to pi/2)
    double lon_rad;   // Longitude in radians (-pi to pi)
    double alt_km;    // Altitude in kilometers
} sgp4_geodetic_t;

// Look angles from observer to satellite
typedef struct sgp4_look_angles {
    double azimuth_rad;    // Azimuth in radians (0=North, pi/2=East)
    double elevation_rad;  // Elevation in radians (0=horizon, pi/2=overhead)
    double range_km;       // Range in kilometers
} sgp4_look_angles_t;

// Input orbital elements from TLE
typedef struct sgp4_elements {
    double epoch_jd;       // Epoch as Julian Date
    double bstar;          // BSTAR drag term
    double inclination;    // Inclination (radians)
    double raan;           // Right ascension of ascending node (radians)
    double eccentricity;   // Eccentricity
    double arg_perigee;    // Argument of perigee (radians)
    double mean_anomaly;   // Mean anomaly (radians)
    double mean_motion;    // Mean motion (rad/min)
} sgp4_elements_t;

// Propagation result
typedef struct sgp4_result {
    double r[3];    // Position (km) in TEME frame
    double v[3];    // Velocity (km/s) in TEME frame
    double atime;   // Resonance integration time (for deep space)
    double xli;     // Resonance mean longitude
    double xni;     // Resonance mean motion
} sgp4_result_t;

// Parsed TLE data
typedef struct sgp4_tle {
    char name[SGP4_TLE_NAME_LEN];
    int norad_id;
    char classification;
    char designator[9];
    double epoch_jd;
    double first_deriv;     // First derivative of mean motion
    double second_deriv;    // Second derivative of mean motion
    double bstar;           // BSTAR drag term
    int element_set_number;
    double inclination;     // degrees
    double raan;            // degrees
    double eccentricity;    // 0 to 1
    double arg_perigee;     // degrees
    double mean_anomaly;    // degrees
    double mean_motion;     // revolutions per day
    int revolution_number;
} sgp4_tle_t;

// SGP4 state structure (~800 bytes)
typedef struct sgp4_state {
    int initialized;

    // Epoch in Julian Date (split for precision)
    double jdsatepoch;
    double jdsatepochF;

    // Method flag: 'n' = near-earth (SGP4), 'd' = deep-space (SDP4)
    char method;

    // Initialization flags
    int isimp;    // Simple drag flag
    int irez;     // Resonance flag (0=none, 1=1-day, 2=0.5-day)

    // Common orbital parameters
    double a;           // Semi-major axis (Earth radii)
    double alta;        // Altitude at apogee
    double altp;        // Altitude at perigee
    double argpo;       // Argument of perigee (rad)
    double bstar;       // Drag term
    double ecco;        // Eccentricity
    double inclo;       // Inclination (rad)
    double mo;          // Mean anomaly (rad)
    double no_kozai;    // Mean motion (Kozai, rad/min)
    double no_unkozai;  // Mean motion (un-Kozai'd, rad/min)
    double nodeo;       // Right ascension (rad)
    double gsto;        // Greenwich sidereal time at epoch
    double cosio2;      // cos(inclination)^2
    double eccsq;       // Eccentricity squared

    // Near-earth coefficients
    double aycof;
    double con41;
    double cc1, cc4, cc5;
    double d2, d3, d4;
    double delmo;
    double eta;
    double argpdot;
    double omgcof;
    double sinmao;
    double t2cof, t3cof, t4cof, t5cof;
    double x1mth2;
    double x7thm1;
    double mdot;
    double nodedot;
    double xlcof;
    double xmcof;
    double nodecf;

    // Deep space coefficients
    double e3, ee2;
    double peo, pgho, pho, pinco, plo;
    double se2, se3, sgh2, sgh3, sgh4;
    double sh2, sh3, si2, si3, sl2, sl3, sl4;
    double xgh2, xgh3, xgh4;
    double xh2, xh3;
    double xi2, xi3;
    double xl2, xl3, xl4;
    double xlamo;
    double zmol, zmos;
    double atime;
    double xli, xni;

    // Resonance coefficients
    double d2201, d2211;
    double d3210, d3222;
    double d4410, d4422;
    double d5220, d5232, d5421, d5433;
    double del1, del2, del3;
    double dedt, didt, dmdt;
    double dnodt, domdt;
} sgp4_state_t;

// ============================================================================
// Inline Utility Functions
// ============================================================================

static inline double sgp4_deg_to_rad(double deg) {
    return deg * SGP4_DEG_TO_RAD;
}

static inline double sgp4_rad_to_deg(double rad) {
    return rad * SGP4_RAD_TO_DEG;
}

// ============================================================================
// Time Functions
// ============================================================================

// Convert Unix timestamp to Julian Date
static inline double sgp4_unix_to_jd(double unix_time) {
    return SGP4_UNIX_EPOCH_JD + unix_time / 86400.0;
}

// Convert Julian Date to Unix timestamp
static inline double sgp4_jd_to_unix(double jd) {
    return (jd - SGP4_UNIX_EPOCH_JD) * 86400.0;
}

// Compute Greenwich Sidereal Time for SGP4 (radians)
double sgp4_gstime(double jdut1);

// Compute Greenwich Mean Sidereal Time (radians)
double sgp4_gmst(double jd);

// ============================================================================
// Vector Operations
// ============================================================================

double sgp4_vec3_magnitude(const sgp4_vec3_t* v);
double sgp4_vec3_dot(const sgp4_vec3_t* a, const sgp4_vec3_t* b);
void sgp4_vec3_cross(const sgp4_vec3_t* a, const sgp4_vec3_t* b, sgp4_vec3_t* result);
void sgp4_vec3_normalize(const sgp4_vec3_t* v, sgp4_vec3_t* result);
void sgp4_vec3_sub(const sgp4_vec3_t* a, const sgp4_vec3_t* b, sgp4_vec3_t* result);

// ============================================================================
// Checksum Functions
// ============================================================================

// Calculate TLE line checksum (mod 10 sum of digits, '-' counts as 1)
int sgp4_calculate_checksum(const char* line);

// Validate TLE line checksum
int sgp4_validate_checksum(const char* line);

// ============================================================================
// TLE Parsing
// ============================================================================

// Parse 2-line TLE
sgp4_error_t sgp4_parse_tle_2line(const char* line1, const char* line2, sgp4_tle_t* tle);

// Parse 3-line TLE (with name)
sgp4_error_t sgp4_parse_tle_3line(const char* line0, const char* line1, const char* line2, sgp4_tle_t* tle);

// Convert TLE to SGP4 elements
sgp4_error_t sgp4_tle_to_elements(const sgp4_tle_t* tle, sgp4_elements_t* elements);

// ============================================================================
// TLE Formatting
// ============================================================================

// Format TLE line 1
sgp4_error_t sgp4_format_tle_line1(const sgp4_tle_t* tle, char* buffer, size_t size);

// Format TLE line 2
sgp4_error_t sgp4_format_tle_line2(const sgp4_tle_t* tle, char* buffer, size_t size);

// Format complete 3-line TLE
sgp4_error_t sgp4_format_tle(const sgp4_tle_t* tle, char* buffer, size_t size);

// ============================================================================
// File I/O
// ============================================================================

// Read TLE from FILE* stream
sgp4_error_t sgp4_read_tle_stream(FILE* stream, sgp4_tle_t* tle);

// Read TLE from file
sgp4_error_t sgp4_read_tle_file(const char* filepath, sgp4_tle_t* tle);

// Write TLE to FILE* stream
sgp4_error_t sgp4_write_tle_stream(FILE* stream, const sgp4_tle_t* tle);

// Write TLE to file
sgp4_error_t sgp4_write_tle_file(const char* filepath, const sgp4_tle_t* tle);

// ============================================================================
// SGP4 Initialization and Propagation
// ============================================================================

// Initialize SGP4 state from orbital elements
sgp4_error_t sgp4_init(sgp4_state_t* state, const sgp4_elements_t* elements);

// Propagate satellite state to time tsince (minutes from epoch)
sgp4_error_t sgp4_propagate(const sgp4_state_t* state, double tsince, sgp4_result_t* result);

// ============================================================================
// Coordinate Transforms
// ============================================================================

// Convert ECI to ECEF using Greenwich Sidereal Time
void sgp4_eci_to_ecef(const sgp4_vec3_t* eci, double gst, sgp4_vec3_t* ecef);

// Convert ECEF to ECI using Greenwich Sidereal Time
void sgp4_ecef_to_eci(const sgp4_vec3_t* ecef, double gst, sgp4_vec3_t* eci);

// Convert ECEF to geodetic coordinates
void sgp4_ecef_to_geodetic(const sgp4_vec3_t* ecef, sgp4_geodetic_t* geo);

// Convert geodetic coordinates to ECEF
void sgp4_geodetic_to_ecef(const sgp4_geodetic_t* geo, sgp4_vec3_t* ecef);

// Convert ECEF to ENU (East-North-Up) local tangent plane
void sgp4_ecef_to_enu(const sgp4_vec3_t* target_ecef, const sgp4_geodetic_t* observer, sgp4_vec3_t* enu);

// ============================================================================
// Look Angles
// ============================================================================

// Compute look angles from observer to satellite
void sgp4_look_angles(const sgp4_vec3_t* sat_ecef, const sgp4_geodetic_t* observer, sgp4_look_angles_t* angles);

// Check if satellite is visible above minimum elevation
static inline int sgp4_is_visible(const sgp4_look_angles_t* angles, double min_elevation_rad) {
    return angles->elevation_rad >= min_elevation_rad;
}

#ifdef __cplusplus
}
#endif

#endif // SGP4_H
