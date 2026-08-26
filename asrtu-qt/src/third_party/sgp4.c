/*
 * SGP4/SDP4 Satellite Propagation Library - Implementation
 *
 * Copyright (c) 2025 Andrew C. Young <andrew@vaelen.org>
 * SPDX-License-Identifier: MIT
 *
 * Based on the Vallado reference implementation from CelesTrak.
 * See: https://celestrak.org/software/vallado-sw.php
 */

#include "sgp4.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

// ============================================================================
// Internal Helper Functions (static)
// ============================================================================


// Parse double from fixed-width field
static double sgp4__parse_double(const char* str, int len) {
    char buf[64];
    int i = 0;

    // Skip leading spaces
    while (i < len && str[i] == ' ') i++;

    // Copy to buffer
    int j = 0;
    while (i < len && j < 63) {
        buf[j++] = str[i++];
    }
    buf[j] = '\0';

    // Trim trailing spaces
    while (j > 0 && buf[j-1] == ' ') {
        buf[--j] = '\0';
    }

    return atof(buf);
}

// Parse integer from fixed-width field
static int sgp4__parse_int(const char* str, int len) {
    char buf[32];
    int i = 0;

    // Skip leading spaces
    while (i < len && str[i] == ' ') i++;

    // Copy to buffer
    int j = 0;
    while (i < len && j < 31) {
        buf[j++] = str[i++];
    }
    buf[j] = '\0';

    return atoi(buf);
}

// Parse TLE exponential format: " 12345-3" -> 0.00012345
static double sgp4__parse_tle_exp(const char* str, int len) {
    char buf[16];
    int i = 0;

    // Skip leading spaces
    while (i < len && str[i] == ' ') i++;

    // Copy to buffer
    int j = 0;
    while (i < len && j < 15) {
        buf[j++] = str[i++];
    }
    buf[j] = '\0';

    // Find sign position (- or +)
    int sign_neg = 0;
    int exp_sign_pos = -1;
    int exp_neg = 0;

    // Check for leading sign
    int start = 0;
    if (buf[0] == '-') {
        sign_neg = 1;
        start = 1;
    } else if (buf[0] == '+') {
        start = 1;
    }

    // Find exponent sign
    for (i = start; buf[i] != '\0'; i++) {
        if (buf[i] == '-') {
            exp_sign_pos = i;
            exp_neg = 1;
            break;
        } else if (buf[i] == '+') {
            exp_sign_pos = i;
            exp_neg = 0;
            break;
        }
    }

    if (exp_sign_pos < 0) {
        return 0.0;
    }

    // Parse mantissa (as 0.XXXXX)
    char mantissa_buf[16];
    strncpy(mantissa_buf, buf + start, exp_sign_pos - start);
    mantissa_buf[exp_sign_pos - start] = '\0';

    char full_mantissa[20];
    snprintf(full_mantissa, sizeof(full_mantissa), "0.%s", mantissa_buf);
    double mantissa = atof(full_mantissa);

    // Parse exponent
    int exponent = atoi(buf + exp_sign_pos + 1);
    if (exp_neg) exponent = -exponent;

    double value = mantissa * pow(10.0, exponent);
    return sign_neg ? -value : value;
}

// Parse TLE epoch (YYDDD.DDDDDDDD) to Julian Date
static double sgp4__parse_epoch(const char* str, int len) {
    char buf[20];
    int i = 0;

    // Skip leading spaces
    while (i < len && str[i] == ' ') i++;

    // Copy to buffer
    int j = 0;
    while (i < len && j < 19) {
        buf[j++] = str[i++];
    }
    buf[j] = '\0';

    // Parse year (2 digits)
    char year_buf[3] = {buf[0], buf[1], '\0'};
    int year = atoi(year_buf);

    // Convert 2-digit year to 4-digit
    if (year < 57) {
        year += 2000;
    } else {
        year += 1900;
    }

    // Parse day of year (DDD.DDDDDDDD)
    double day_of_year = atof(buf + 2);

    // Convert to Julian Date
    // Jan 1 of year in JD
    int a = (14 - 1) / 12;
    int y = year + 4800 - a;
    int m = 1 + 12 * a - 3;
    int jd_jan1 = 1 + (153 * m + 2) / 5 + 365 * y + y / 4 - y / 100 + y / 400 - 32045;

    return (double)jd_jan1 + day_of_year - 1.0 - 0.5;
}

// Format value in TLE exponential notation
static void sgp4__format_tle_exp(double value, char* buf) {
    if (value == 0.0) {
        strcpy(buf, " 00000+0");
        return;
    }

    char sign = (value >= 0) ? ' ' : '-';
    value = fabs(value);

    int exponent = (int)floor(log10(value));
    double mantissa = value / pow(10.0, exponent + 1);
    int mantissa_int = (int)(mantissa * 100000 + 0.5);

    if (mantissa_int >= 100000) {
        mantissa_int = 10000;
        exponent++;
    }

    char exp_sign = (exponent + 1 >= 0) ? '+' : '-';
    int exp_abs = abs(exponent + 1);

    sprintf(buf, "%c%05d%c%d", sign, mantissa_int, exp_sign, exp_abs);
}

// Format first derivative
static void sgp4__format_first_deriv(double value, char* buf) {
    char sign = (value >= 0) ? ' ' : '-';
    value = fabs(value);
    int mantissa = (int)(value * 100000000 + 0.5);
    sprintf(buf, "%c.%08d", sign, mantissa);
}

// ============================================================================
// Time Functions
// ============================================================================

double sgp4_gstime(double jdut1) {
    double tut1 = (jdut1 - 2451545.0) / 36525.0;
    double temp = -6.2e-6 * tut1 * tut1 * tut1
                  + 0.093104 * tut1 * tut1
                  + (876600.0 * 3600 + 8640184.812866) * tut1
                  + 67310.54841;
    temp = fmod(temp * SGP4_DEG_TO_RAD / 240.0, SGP4_TWO_PI);
    if (temp < 0.0) temp += SGP4_TWO_PI;
    return temp;
}

double sgp4_gmst(double jd) {
    double T = (jd - SGP4_J2000_JD) / SGP4_DAYS_PER_CENTURY;
    double gmst_deg = 280.46061837
                    + 360.98564736629 * (jd - SGP4_J2000_JD)
                    + 0.000387933 * T * T
                    - T * T * T / 38710000.0;
    gmst_deg = fmod(gmst_deg, 360.0);
    if (gmst_deg < 0.0) gmst_deg += 360.0;
    return gmst_deg * SGP4_DEG_TO_RAD;
}

// ============================================================================
// Vector Operations
// ============================================================================

double sgp4_vec3_magnitude(const sgp4_vec3_t* v) {
    return sqrt(v->x * v->x + v->y * v->y + v->z * v->z);
}

double sgp4_vec3_dot(const sgp4_vec3_t* a, const sgp4_vec3_t* b) {
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

void sgp4_vec3_cross(const sgp4_vec3_t* a, const sgp4_vec3_t* b, sgp4_vec3_t* result) {
    result->x = a->y * b->z - a->z * b->y;
    result->y = a->z * b->x - a->x * b->z;
    result->z = a->x * b->y - a->y * b->x;
}

void sgp4_vec3_normalize(const sgp4_vec3_t* v, sgp4_vec3_t* result) {
    double mag = sgp4_vec3_magnitude(v);
    if (mag > 0.0) {
        result->x = v->x / mag;
        result->y = v->y / mag;
        result->z = v->z / mag;
    } else {
        result->x = result->y = result->z = 0.0;
    }
}

void sgp4_vec3_sub(const sgp4_vec3_t* a, const sgp4_vec3_t* b, sgp4_vec3_t* result) {
    result->x = a->x - b->x;
    result->y = a->y - b->y;
    result->z = a->z - b->z;
}

// ============================================================================
// Checksum Functions
// ============================================================================

int sgp4_calculate_checksum(const char* line) {
    int sum = 0;
    int len = strlen(line);
    // Only sum first 68 characters (checksum is at position 69)
    int max = (len > 68) ? 68 : len;

    for (int i = 0; i < max; i++) {
        char c = line[i];
        if (c >= '0' && c <= '9') {
            sum += (c - '0');
        } else if (c == '-') {
            sum += 1;
        }
    }
    return sum % 10;
}

int sgp4_validate_checksum(const char* line) {
    int len = strlen(line);
    if (len < 69) return 0;

    int expected = line[68] - '0';
    if (expected < 0 || expected > 9) return 0;

    int calculated = sgp4_calculate_checksum(line);
    return (calculated == expected);
}

// ============================================================================
// TLE Parsing
// ============================================================================

sgp4_error_t sgp4_parse_tle_2line(const char* line1, const char* line2, sgp4_tle_t* tle) {
    if (!line1 || !line2 || !tle) {
        return SGP4_ERROR_NULL_POINTER;
    }

    int len1 = strlen(line1);
    int len2 = strlen(line2);

    if (len1 < 69 || len2 < 69) {
        return SGP4_ERROR_INVALID_TLE_FORMAT;
    }

    // Validate line identifiers
    if (line1[0] != '1' || line2[0] != '2') {
        return SGP4_ERROR_INVALID_TLE_FORMAT;
    }

    // Validate checksums
    if (!sgp4_validate_checksum(line1) || !sgp4_validate_checksum(line2)) {
        return SGP4_ERROR_CHECKSUM_MISMATCH;
    }

    // Initialize TLE
    memset(tle, 0, sizeof(sgp4_tle_t));

    // Parse Line 1
    // NORAD ID: columns 3-7 (0-indexed: 2-6)
    tle->norad_id = sgp4__parse_int(line1 + 2, 5);

    // Classification: column 8 (0-indexed: 7)
    tle->classification = line1[7];

    // Designator: columns 10-17 (0-indexed: 9-16)
    strncpy(tle->designator, line1 + 9, 8);
    tle->designator[8] = '\0';
    // Trim trailing spaces
    for (int i = 7; i >= 0 && tle->designator[i] == ' '; i--) {
        tle->designator[i] = '\0';
    }

    // Epoch: columns 19-32 (0-indexed: 18-31)
    tle->epoch_jd = sgp4__parse_epoch(line1 + 18, 14);

    // First derivative: columns 34-43 (0-indexed: 33-42)
    tle->first_deriv = sgp4__parse_double(line1 + 33, 10);

    // Second derivative: columns 45-52 (0-indexed: 44-51)
    tle->second_deriv = sgp4__parse_tle_exp(line1 + 44, 8);

    // BSTAR: columns 54-61 (0-indexed: 53-60)
    tle->bstar = sgp4__parse_tle_exp(line1 + 53, 8);

    // Element set number: columns 65-68 (0-indexed: 64-67)
    tle->element_set_number = sgp4__parse_int(line1 + 64, 4);

    // Parse Line 2
    // Inclination: columns 9-16 (0-indexed: 8-15)
    tle->inclination = sgp4__parse_double(line2 + 8, 8);

    // RAAN: columns 18-25 (0-indexed: 17-24)
    tle->raan = sgp4__parse_double(line2 + 17, 8);

    // Eccentricity: columns 27-33 (0-indexed: 26-32) - implied decimal
    char ecc_buf[16];
    strncpy(ecc_buf, "0.", 3);
    strncpy(ecc_buf + 2, line2 + 26, 7);
    ecc_buf[9] = '\0';
    tle->eccentricity = atof(ecc_buf);

    // Argument of perigee: columns 35-42 (0-indexed: 34-41)
    tle->arg_perigee = sgp4__parse_double(line2 + 34, 8);

    // Mean anomaly: columns 44-51 (0-indexed: 43-50)
    tle->mean_anomaly = sgp4__parse_double(line2 + 43, 8);

    // Mean motion: columns 53-63 (0-indexed: 52-62)
    tle->mean_motion = sgp4__parse_double(line2 + 52, 11);

    // Revolution number: columns 64-68 (0-indexed: 63-67)
    tle->revolution_number = sgp4__parse_int(line2 + 63, 5);

    return SGP4_SUCCESS;
}

sgp4_error_t sgp4_parse_tle_3line(const char* line0, const char* line1, const char* line2, sgp4_tle_t* tle) {
    sgp4_error_t err = sgp4_parse_tle_2line(line1, line2, tle);
    if (err != SGP4_SUCCESS) {
        return err;
    }

    if (line0) {
        // Copy name, trim trailing spaces
        strncpy(tle->name, line0, SGP4_TLE_NAME_LEN - 1);
        tle->name[SGP4_TLE_NAME_LEN - 1] = '\0';

        // Trim trailing whitespace
        int len = strlen(tle->name);
        while (len > 0 && (tle->name[len-1] == ' ' || tle->name[len-1] == '\r' || tle->name[len-1] == '\n')) {
            tle->name[--len] = '\0';
        }
    }

    return SGP4_SUCCESS;
}

sgp4_error_t sgp4_tle_to_elements(const sgp4_tle_t* tle, sgp4_elements_t* elements) {
    if (!tle || !elements) {
        return SGP4_ERROR_NULL_POINTER;
    }

    elements->epoch_jd = tle->epoch_jd;
    elements->bstar = tle->bstar;
    elements->inclination = sgp4_deg_to_rad(tle->inclination);
    elements->raan = sgp4_deg_to_rad(tle->raan);
    elements->eccentricity = tle->eccentricity;
    elements->arg_perigee = sgp4_deg_to_rad(tle->arg_perigee);
    elements->mean_anomaly = sgp4_deg_to_rad(tle->mean_anomaly);
    // Convert rev/day to rad/min
    elements->mean_motion = tle->mean_motion * SGP4_TWO_PI / 1440.0;

    return SGP4_SUCCESS;
}

// ============================================================================
// TLE Formatting
// ============================================================================

sgp4_error_t sgp4_format_tle_line1(const sgp4_tle_t* tle, char* buffer, size_t size) {
    if (!tle || !buffer) return SGP4_ERROR_NULL_POINTER;
    if (size < SGP4_TLE_LINE_LEN) return SGP4_ERROR_BUFFER_TOO_SMALL;

    // Convert epoch JD back to YYDDD.DDDDDDDD
    double jd = tle->epoch_jd + 0.5;
    int z = (int)jd;
    int a = (int)((z - 1867216.25) / 36524.25);
    a = z + 1 + a - a / 4;
    int b = a + 1524;
    int c = (int)((b - 122.1) / 365.25);
    int d = (int)(365.25 * c);
    int e = (int)((b - d) / 30.6001);

    int day = b - d - (int)(30.6001 * e);
    int month = (e < 14) ? e - 1 : e - 13;
    int year = (month > 2) ? c - 4716 : c - 4715;

    // Day of year
    int days_in_months[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    int doy = days_in_months[month - 1] + day;
    if (month > 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) {
        doy++; // Leap year
    }

    double frac_day = fmod(jd + 0.5, 1.0);

    int two_digit_year = year % 100;

    char first_deriv_buf[16];
    char second_deriv_buf[16];
    char bstar_buf[16];

    sgp4__format_first_deriv(tle->first_deriv, first_deriv_buf);
    sgp4__format_tle_exp(tle->second_deriv, second_deriv_buf);
    sgp4__format_tle_exp(tle->bstar, bstar_buf);

    // Build line without checksum
    char line[128];
    snprintf(line, sizeof(line), "1 %05d%c %-8s %02d%03d.%08d %s %s %s 0 %4d",
            tle->norad_id,
            tle->classification,
            tle->designator,
            two_digit_year,
            doy,
            (int)(frac_day * 100000000 + 0.5),
            first_deriv_buf,
            second_deriv_buf,
            bstar_buf,
            tle->element_set_number % 10000);

    // Pad to 68 chars
    int len = strlen(line);
    while (len < 68) {
        line[len++] = ' ';
    }
    line[68] = '\0';

    // Add checksum
    int checksum = sgp4_calculate_checksum(line);
    sprintf(buffer, "%s%d", line, checksum);

    return SGP4_SUCCESS;
}

sgp4_error_t sgp4_format_tle_line2(const sgp4_tle_t* tle, char* buffer, size_t size) {
    if (!tle || !buffer) return SGP4_ERROR_NULL_POINTER;
    if (size < SGP4_TLE_LINE_LEN) return SGP4_ERROR_BUFFER_TOO_SMALL;

    int ecc_int = (int)(tle->eccentricity * 10000000 + 0.5);

    char line[70];
    sprintf(line, "2 %05d %8.4f %8.4f %07d %8.4f %8.4f %11.8f%05d",
            tle->norad_id,
            tle->inclination,
            tle->raan,
            ecc_int,
            tle->arg_perigee,
            tle->mean_anomaly,
            tle->mean_motion,
            tle->revolution_number % 100000);

    // Ensure exactly 68 chars
    int len = strlen(line);
    while (len < 68) {
        line[len++] = ' ';
    }
    line[68] = '\0';

    // Add checksum
    int checksum = sgp4_calculate_checksum(line);
    sprintf(buffer, "%s%d", line, checksum);

    return SGP4_SUCCESS;
}

sgp4_error_t sgp4_format_tle(const sgp4_tle_t* tle, char* buffer, size_t size) {
    if (!tle || !buffer) return SGP4_ERROR_NULL_POINTER;
    if (size < SGP4_TLE_BUFFER_LEN) return SGP4_ERROR_BUFFER_TOO_SMALL;

    char line1[SGP4_TLE_LINE_LEN];
    char line2[SGP4_TLE_LINE_LEN];

    sgp4_error_t err = sgp4_format_tle_line1(tle, line1, sizeof(line1));
    if (err != SGP4_SUCCESS) return err;

    err = sgp4_format_tle_line2(tle, line2, sizeof(line2));
    if (err != SGP4_SUCCESS) return err;

    sprintf(buffer, "%s\n%s\n%s\n", tle->name, line1, line2);

    return SGP4_SUCCESS;
}

// ============================================================================
// File I/O
// ============================================================================

sgp4_error_t sgp4_read_tle_stream(FILE* stream, sgp4_tle_t* tle) {
    if (!stream || !tle) return SGP4_ERROR_NULL_POINTER;

    char line_buf[SGP4_LINE_BUFFER_LEN];
    char name_line[SGP4_LINE_BUFFER_LEN] = "";
    char line1[SGP4_LINE_BUFFER_LEN] = "";
    char line2[SGP4_LINE_BUFFER_LEN] = "";

    int have_line1 = 0;
    int have_line2 = 0;

    while (fgets(line_buf, sizeof(line_buf), stream)) {
        // Remove trailing newline/carriage return
        int len = strlen(line_buf);
        while (len > 0 && (line_buf[len-1] == '\n' || line_buf[len-1] == '\r')) {
            line_buf[--len] = '\0';
        }

        if (len == 0) continue;

        if (line_buf[0] == '1' && line_buf[1] == ' ') {
            strncpy(line1, line_buf, sizeof(line1) - 1);
            line1[sizeof(line1) - 1] = '\0';
            have_line1 = 1;
        } else if (line_buf[0] == '2' && line_buf[1] == ' ') {
            strncpy(line2, line_buf, sizeof(line2) - 1);
            line2[sizeof(line2) - 1] = '\0';
            have_line2 = 1;
        } else if (!have_line1 && !have_line2) {
            strncpy(name_line, line_buf, sizeof(name_line) - 1);
            name_line[sizeof(name_line) - 1] = '\0';
        }

        if (have_line1 && have_line2) {
            break;
        }
    }

    if (!have_line1 || !have_line2) {
        return SGP4_ERROR_INVALID_TLE_FORMAT;
    }

    return sgp4_parse_tle_3line(name_line[0] ? name_line : NULL, line1, line2, tle);
}

sgp4_error_t sgp4_read_tle_file(const char* filepath, sgp4_tle_t* tle) {
    if (!filepath || !tle) return SGP4_ERROR_NULL_POINTER;

    FILE* f = fopen(filepath, "r");
    if (!f) return SGP4_ERROR_FILE_OPEN_FAILED;

    sgp4_error_t err = sgp4_read_tle_stream(f, tle);
    fclose(f);

    return err;
}

sgp4_error_t sgp4_write_tle_stream(FILE* stream, const sgp4_tle_t* tle) {
    if (!stream || !tle) return SGP4_ERROR_NULL_POINTER;

    char buffer[SGP4_TLE_BUFFER_LEN];
    sgp4_error_t err = sgp4_format_tle(tle, buffer, sizeof(buffer));
    if (err != SGP4_SUCCESS) return err;

    if (fputs(buffer, stream) == EOF) {
        return SGP4_ERROR_FILE_OPEN_FAILED;
    }

    return SGP4_SUCCESS;
}

sgp4_error_t sgp4_write_tle_file(const char* filepath, const sgp4_tle_t* tle) {
    if (!filepath || !tle) return SGP4_ERROR_NULL_POINTER;

    FILE* f = fopen(filepath, "w");
    if (!f) return SGP4_ERROR_FILE_OPEN_FAILED;

    sgp4_error_t err = sgp4_write_tle_stream(f, tle);
    fclose(f);

    return err;
}

// ============================================================================
// Deep Space Initialization (internal)
// ============================================================================

static void sgp4__init_deep_space(
    sgp4_state_t* state,
    double tc)
{
    // Solar and lunar constants
    const double zes = 0.01675;
    const double zel = 0.05490;
    const double c1ss = 2.9864797e-6;
    const double c1l = 4.7968065e-7;
    const double zsinis = 0.39785416;
    const double zcosis = 0.91744867;
    const double zcosgs = 0.1945905;
    const double zsings = -0.98088458;

    double nm = state->no_unkozai;
    double em = state->ecco;
    double snodm = sin(state->nodeo);
    double cnodm = cos(state->nodeo);
    double sinomm = sin(state->argpo);
    double cosomm = cos(state->argpo);
    double sinim = sin(state->inclo);
    double cosim = cos(state->inclo);
    double emsq = em * em;
    double betasq = 1.0 - emsq;
    double rtemsq = sqrt(betasq);

    double day = state->jdsatepoch + state->jdsatepochF - 2433281.5 + tc / 1440.0;
    double xnodce = fmod(4.5236020 - 9.2422029e-4 * day, SGP4_TWO_PI);
    double stem = sin(xnodce);
    double ctem = cos(xnodce);
    double zcosil = 0.91375164 - 0.03568096 * ctem;
    double zsinil = sqrt(1.0 - zcosil * zcosil);
    double zsinhl = 0.089683511 * stem / zsinil;
    double zcoshl = sqrt(1.0 - zsinhl * zsinhl);
    double gam = 5.8351514 + 0.0019443680 * day;
    double zx = 0.39785416 * stem / zsinil;
    double zy = zcoshl * ctem + 0.91744867 * zsinhl * stem;
    zx = atan2(zx, zy);
    zx = gam + zx - xnodce;
    double zcosgl = cos(zx);
    double zsingl = sin(zx);

    double zcosg = zcosgs;
    double zsing = zsings;
    double zcosi = zcosis;
    double zsini = zsinis;
    double zcosh = cnodm;
    double zsinh = snodm;
    double cc = c1ss;
    double xnoi = 1.0 / nm;

    double ss1, ss2, ss3, ss4, ss6, ss7;
    double sz1, sz2, sz3, sz11, sz12, sz13, sz21, sz22, sz23, sz31, sz32, sz33;
    double s1, s2, s3, s4, s6, s7;
    double z1, z2, z3, z11, z12, z13, z21, z22, z23, z31, z32, z33;

    for (int lsflg = 1; lsflg <= 2; lsflg++) {
        double a1 = zcosg * zcosh + zsing * zcosi * zsinh;
        double a3 = -zsing * zcosh + zcosg * zcosi * zsinh;
        double a7 = -zcosg * zsinh + zsing * zcosi * zcosh;
        double a8 = zsing * zsini;
        double a9 = zsing * zsinh + zcosg * zcosi * zcosh;
        double a10 = zcosg * zsini;
        double a2 = cosim * a7 + sinim * a8;
        double a4 = cosim * a9 + sinim * a10;
        double a5 = -sinim * a7 + cosim * a8;
        double a6 = -sinim * a9 + cosim * a10;

        double x1 = a1 * cosomm + a2 * sinomm;
        double x2 = a3 * cosomm + a4 * sinomm;
        double x3 = -a1 * sinomm + a2 * cosomm;
        double x4 = -a3 * sinomm + a4 * cosomm;
        double x5 = a5 * sinomm;
        double x6 = a6 * sinomm;
        double x7 = a5 * cosomm;
        double x8 = a6 * cosomm;

        z31 = 12.0 * x1 * x1 - 3.0 * x3 * x3;
        z32 = 24.0 * x1 * x2 - 6.0 * x3 * x4;
        z33 = 12.0 * x2 * x2 - 3.0 * x4 * x4;
        z1 = 3.0 * (a1 * a1 + a2 * a2) + z31 * emsq;
        z2 = 6.0 * (a1 * a3 + a2 * a4) + z32 * emsq;
        z3 = 3.0 * (a3 * a3 + a4 * a4) + z33 * emsq;
        z11 = -6.0 * a1 * a5 + emsq * (-24.0 * x1 * x7 - 6.0 * x3 * x5);
        z12 = -6.0 * (a1 * a6 + a3 * a5) + emsq * (-24.0 * (x2 * x7 + x1 * x8) - 6.0 * (x3 * x6 + x4 * x5));
        z13 = -6.0 * a3 * a6 + emsq * (-24.0 * x2 * x8 - 6.0 * x4 * x6);
        z21 = 6.0 * a2 * a5 + emsq * (24.0 * x1 * x5 - 6.0 * x3 * x7);
        z22 = 6.0 * (a4 * a5 + a2 * a6) + emsq * (24.0 * (x2 * x5 + x1 * x6) - 6.0 * (x4 * x7 + x3 * x8));
        z23 = 6.0 * a4 * a6 + emsq * (24.0 * x2 * x6 - 6.0 * x4 * x8);
        z1 = z1 + z1 + betasq * z31;
        z2 = z2 + z2 + betasq * z32;
        z3 = z3 + z3 + betasq * z33;
        s3 = cc * xnoi;
        s2 = -0.5 * s3 / rtemsq;
        s4 = s3 * rtemsq;
        s1 = -15.0 * em * s4;
        s6 = x2 * x3 + x1 * x4;
        s7 = x2 * x4 - x1 * x3;

        if (lsflg == 1) {
            ss1 = s1; ss2 = s2; ss3 = s3; ss4 = s4;
            ss6 = s6; ss7 = s7;
            sz1 = z1; sz2 = z2; sz3 = z3;
            sz11 = z11; sz12 = z12; sz13 = z13;
            sz21 = z21; sz22 = z22; sz23 = z23;
            sz31 = z31; sz32 = z32; sz33 = z33;
            zcosg = zcosgl;
            zsing = zsingl;
            zcosi = zcosil;
            zsini = zsinil;
            zcosh = zcoshl * cnodm + zsinhl * snodm;
            zsinh = snodm * zcoshl - cnodm * zsinhl;
            cc = c1l;
        }
    }

    state->zmol = fmod(4.7199672 + 0.22997150 * day - gam, SGP4_TWO_PI);
    state->zmos = fmod(6.2565837 + 0.017201977 * day, SGP4_TWO_PI);

    // Solar terms
    state->se2 = 2.0 * ss1 * ss6;
    state->se3 = 2.0 * ss1 * ss7;
    state->si2 = 2.0 * ss2 * sz12;
    state->si3 = 2.0 * ss2 * (sz13 - sz11);
    state->sl2 = -2.0 * ss3 * sz2;
    state->sl3 = -2.0 * ss3 * (sz3 - sz1);
    state->sl4 = -2.0 * ss3 * (-21.0 - 9.0 * emsq) * zes;
    state->sgh2 = 2.0 * ss4 * sz32;
    state->sgh3 = 2.0 * ss4 * (sz33 - sz31);
    state->sgh4 = -18.0 * ss4 * zes;
    state->sh2 = -2.0 * ss2 * sz22;
    state->sh3 = -2.0 * ss2 * (sz23 - sz21);

    // Lunar terms
    state->ee2 = 2.0 * s1 * s6;
    state->e3 = 2.0 * s1 * s7;
    state->xi2 = 2.0 * s2 * z12;
    state->xi3 = 2.0 * s2 * (z13 - z11);
    state->xl2 = -2.0 * s3 * z2;
    state->xl3 = -2.0 * s3 * (z3 - z1);
    state->xl4 = -2.0 * s3 * (-21.0 - 9.0 * emsq) * zel;
    state->xgh2 = 2.0 * s4 * z32;
    state->xgh3 = 2.0 * s4 * (z33 - z31);
    state->xgh4 = -18.0 * s4 * zel;
    state->xh2 = -2.0 * s2 * z22;
    state->xh3 = -2.0 * s2 * (z23 - z21);

    // Apply deep space long period periodics
    double f2 = 0.5 * sinomm * sinomm - 0.25;
    double f3 = -0.5 * sinomm * cosomm;
    double ses = state->se2 * f2 + state->se3 * f3;
    double sis = state->si2 * f2 + state->si3 * f3;
    double sls = state->sl2 * f2 + state->sl3 * f3 + state->sl4 * sinomm;
    double sghs = state->sgh2 * f2 + state->sgh3 * f3 + state->sgh4 * sinomm;
    double shs = state->sh2 * f2 + state->sh3 * f3;

    if (state->inclo < 5.2359877e-2 || state->inclo > SGP4_PI - 5.2359877e-2) {
        shs = 0.0;
    }
    if (sinim != 0.0) {
        shs = shs / sinim;
    }

    double sel = state->ee2 * f2 + state->e3 * f3;
    double sil = state->xi2 * f2 + state->xi3 * f3;
    double sll = state->xl2 * f2 + state->xl3 * f3 + state->xl4 * sinomm;
    double sghl = state->xgh2 * f2 + state->xgh3 * f3 + state->xgh4 * sinomm;
    double shll = state->xh2 * f2 + state->xh3 * f3;

    if (state->inclo < 5.2359877e-2 || state->inclo > SGP4_PI - 5.2359877e-2) {
        shll = 0.0;
    }

    state->peo = ses + sel;
    state->pinco = sis + sil;
    state->plo = sls + sll;
    state->pgho = sghs + sghl;
    state->pho = shs + shll;

    // Initialize resonance terms
    if (state->irez != 0) {
        double aonv = pow(nm / SGP4_XKE, SGP4_X2O3);
        double cosisq = cosim * cosim;
        double emo = em;
        em = state->ecco;
        emsq = state->ecco * state->ecco;
        double eoc = em * emsq;
        double g201 = -0.306 - (em - 0.64) * 0.440;
        double g211, g310, g322, g410, g422, g520, g521, g532, g533;

        if (em <= 0.65) {
            g211 = 3.616 - 13.2470 * em + 16.2900 * emsq;
            g310 = -19.302 + 117.3900 * em - 228.4190 * emsq + 156.5910 * eoc;
            g322 = -18.9068 + 109.7927 * em - 214.6334 * emsq + 146.5816 * eoc;
            g410 = -41.122 + 242.6940 * em - 471.0940 * emsq + 313.9530 * eoc;
            g422 = -146.407 + 841.8800 * em - 1629.014 * emsq + 1083.4350 * eoc;
            g520 = -532.114 + 3017.977 * em - 5740.032 * emsq + 3708.2760 * eoc;
        } else {
            g211 = -72.099 + 331.819 * em - 508.738 * emsq + 266.724 * eoc;
            g310 = -346.844 + 1582.851 * em - 2415.925 * emsq + 1246.113 * eoc;
            g322 = -342.585 + 1554.908 * em - 2366.899 * emsq + 1215.972 * eoc;
            g410 = -1052.797 + 4758.686 * em - 7193.992 * emsq + 3651.957 * eoc;
            g422 = -3581.690 + 16178.11 * em - 24462.77 * emsq + 12422.52 * eoc;
            if (em > 0.715) {
                g520 = -5149.66 + 29936.92 * em - 54087.36 * emsq + 31324.56 * eoc;
            } else {
                g520 = 1464.74 - 4664.75 * em + 3763.64 * emsq;
            }
        }

        if (em < 0.7) {
            g533 = -919.22770 + 4988.6100 * em - 9064.7700 * emsq + 5542.210 * eoc;
            g521 = -822.71072 + 4568.6173 * em - 8491.4146 * emsq + 5337.524 * eoc;
            g532 = -853.66600 + 4690.2500 * em - 8624.7700 * emsq + 5341.400 * eoc;
        } else {
            g533 = -37995.780 + 161616.52 * em - 229838.20 * emsq + 109377.94 * eoc;
            g521 = -51752.104 + 218913.95 * em - 309468.16 * emsq + 146349.42 * eoc;
            g532 = -40023.880 + 170470.89 * em - 242699.48 * emsq + 115605.82 * eoc;
        }

        double sini2 = sinim * sinim;
        double f220 = 0.75 * (1.0 + 2.0 * cosim + cosisq);
        double f221 = 1.5 * sini2;
        double f321 = 1.875 * sinim * (1.0 - 2.0 * cosim - 3.0 * cosisq);
        double f322 = -1.875 * sinim * (1.0 + 2.0 * cosim - 3.0 * cosisq);
        double f441 = 35.0 * sini2 * f220;
        double f442 = 39.375 * sini2 * sini2;
        double f522 = 9.84375 * sinim * (sini2 * (1.0 - 2.0 * cosim - 5.0 * cosisq) +
                      0.33333333 * (-2.0 + 4.0 * cosim + 6.0 * cosisq));
        double f523 = sinim * (4.92187512 * sini2 * (-2.0 - 4.0 * cosim + 10.0 * cosisq) +
                      6.56250012 * (1.0 + 2.0 * cosim - 3.0 * cosisq));
        double f542 = 29.53125 * sinim * (2.0 - 8.0 * cosim + cosisq * (-12.0 + 8.0 * cosim + 10.0 * cosisq));
        double f543 = 29.53125 * sinim * (-2.0 - 8.0 * cosim + cosisq * (12.0 + 8.0 * cosim - 10.0 * cosisq));

        double xno2 = nm * nm;
        double ainv2 = aonv * aonv;
        double temp1_ds = 3.0 * xno2 * ainv2;
        double temp_ds = temp1_ds * SGP4_J2 / 2.0;

        if (state->irez == 2) {
            // Half-day resonance terms
            state->d2201 = temp_ds * f220 * g201;
            state->d2211 = temp_ds * f221 * g211;
            state->d3210 = temp1_ds * 1.5 * SGP4_J2 * f321 * g310 * ainv2;
            state->d3222 = temp1_ds * 1.5 * SGP4_J2 * f322 * g322 * ainv2;
            state->d4410 = temp1_ds * temp_ds * 2.0 * f441 * g410 * ainv2;
            state->d4422 = temp1_ds * temp_ds * 2.0 * f442 * g422 * ainv2;
            state->d5220 = temp1_ds * temp_ds * f522 * g520 * ainv2 * ainv2;
            state->d5232 = temp1_ds * temp_ds * f523 * g532 * ainv2 * ainv2;
            state->d5421 = temp1_ds * temp_ds * f542 * g521 * ainv2 * ainv2;
            state->d5433 = temp1_ds * temp_ds * f543 * g533 * ainv2 * ainv2;
            double theta = fmod(state->gsto + tc * 7.29211514668855e-5, SGP4_TWO_PI);
            state->xlamo = fmod(state->mo + state->nodeo + state->nodeo - theta - theta, SGP4_TWO_PI);
            em = emo;
        }

        if (state->irez == 1) {
            // Synchronous resonance terms
            double g200 = 1.0 + emsq * (-2.5 + 0.8125 * emsq);
            double g310_s = 1.0 + 2.0 * emsq;
            double g300 = 1.0 + emsq * (-6.0 + 6.60937 * emsq);
            double f220_s = 0.75 * (1.0 + cosim) * (1.0 + cosim);
            double f311 = 0.9375 * sinim * sinim * (1.0 + 3.0 * cosim) - 0.75 * (1.0 + cosim);
            double f330 = 1.0 + cosim;
            f330 = 1.875 * f330 * f330 * f330;

            state->del1 = 3.0 * nm * nm * aonv * aonv;
            state->del2 = 2.0 * state->del1 * f220_s * g200 * SGP4_J2;
            state->del3 = 3.0 * state->del1 * f330 * g300 * SGP4_J3OJ2 * aonv;
            state->del1 = state->del1 * f311 * g310_s * SGP4_J2 * aonv;

            double theta_s = fmod(state->gsto + tc * 7.29211514668855e-5, SGP4_TWO_PI);
            state->xlamo = fmod(state->mo + state->nodeo + state->argpo - theta_s, SGP4_TWO_PI);
        }

        // Initialize integrator
        state->xli = state->xlamo;
        state->xni = state->no_unkozai;
        state->atime = 0.0;
    }
}

// ============================================================================
// SGP4 Initialization
// ============================================================================

sgp4_error_t sgp4_init(sgp4_state_t* state, const sgp4_elements_t* elements) {
    if (!state || !elements) {
        return SGP4_ERROR_NULL_POINTER;
    }

    // Initialize state
    memset(state, 0, sizeof(sgp4_state_t));

    // Convert input elements to internal units
    state->ecco = elements->eccentricity;
    state->inclo = elements->inclination;
    state->nodeo = elements->raan;
    state->argpo = elements->arg_perigee;
    state->mo = elements->mean_anomaly;
    state->bstar = elements->bstar;
    state->no_kozai = elements->mean_motion;

    // Compute epoch Julian Date (split for precision)
    state->jdsatepoch = floor(elements->epoch_jd);
    state->jdsatepochF = elements->epoch_jd - state->jdsatepoch;

    // Compute Greenwich sidereal time at epoch
    state->gsto = sgp4_gstime(elements->epoch_jd);

    // WGS-72 Earth constants
    const double radiusearthkm = SGP4_RADIUS_EARTH;
    const double xke = SGP4_XKE;
    const double j2 = SGP4_J2;
    const double j3oj2 = SGP4_J3OJ2;
    const double j4 = SGP4_J4;

    // Recover original mean motion and semimajor axis
    double a1 = pow(xke / state->no_kozai, SGP4_X2O3);
    double cosio = cos(state->inclo);
    double cosio2 = cosio * cosio;
    double eccsq = state->ecco * state->ecco;
    double omeosq = 1.0 - eccsq;
    double rteosq = sqrt(omeosq);
    double d1 = 0.75 * j2 * (3.0 * cosio2 - 1.0) / (rteosq * omeosq);
    double del_ = d1 / (a1 * a1);
    double ao = a1 * (1.0 - del_ * (1.0/3.0 + del_ * (1.0 + 134.0/81.0 * del_)));
    double delo = d1 / (ao * ao);
    state->no_unkozai = state->no_kozai / (1.0 + delo);

    // Compute semi-major axis
    state->a = pow(xke / state->no_unkozai, SGP4_X2O3);

    // Compute perigee and apogee altitudes
    state->altp = state->a * (1.0 - state->ecco) - 1.0;
    state->alta = state->a * (1.0 + state->ecco) - 1.0;

    // Determine if deep space (period >= 225 min)
    double periodearthradii = SGP4_TWO_PI / state->no_unkozai;
    if (periodearthradii >= 225.0) {
        state->method = 'd';  // deep space
    } else {
        state->method = 'n';  // near earth
    }

    // SGP4 initialization
    double ss = 78.0 / radiusearthkm + 1.0;
    double qzms2t = pow((120.0 - 78.0) / radiusearthkm, 4);

    double sinio = sin(state->inclo);
    double x1mth2 = 1.0 - cosio2;
    state->x1mth2 = x1mth2;
    state->cosio2 = cosio2;
    state->eccsq = eccsq;

    // Check for eccentricity out of range
    if (state->ecco >= 1.0 || state->ecco < -0.001) {
        return SGP4_ERROR_INVALID_ECCENTRICITY;
    }
    if (state->ecco < 1.0e-10) {
        state->ecco = 1.0e-10;
    }

    // Compute perigee
    double rp = state->a * (1.0 - state->ecco);

    // Check if satellite has decayed
    if (rp < 1.0) {
        return SGP4_ERROR_SATELLITE_DECAYED;
    }

    double sfour = ss;
    double qzms24 = qzms2t;
    double perige = (rp - 1.0) * radiusearthkm;

    // For perigees below 156 km, adjust s and qoms2t
    if (perige < 156.0) {
        sfour = perige - 78.0;
        if (perige < 98.0) {
            sfour = 20.0;
        }
        qzms24 = pow((120.0 - sfour) / radiusearthkm, 4);
        sfour = sfour / radiusearthkm + 1.0;
    }

    double pinvsq = 1.0 / (state->a * state->a * omeosq * omeosq);
    double tsi = 1.0 / (state->a - sfour);
    state->eta = state->a * state->ecco * tsi;
    double etasq = state->eta * state->eta;
    double eeta = state->ecco * state->eta;
    double psisq = fabs(1.0 - etasq);
    double coef = qzms24 * pow(tsi, 4);
    double coef1 = coef / pow(psisq, 3.5);
    double cc2 = coef1 * state->no_unkozai * (state->a * (1.0 + 1.5 * etasq + eeta * (4.0 + etasq))
                 + 0.375 * j2 * tsi / psisq * (3.0 * (3.0 * cosio2 - 1.0)
                 * (1.0 + 1.5 * etasq) - 0.5 * (3.0 - 7.0 * cosio2) * eeta * cos(2.0 * state->argpo)));
    state->cc1 = state->bstar * cc2;
    double cc3 = 0.0;
    if (state->ecco > 1.0e-4) {
        cc3 = -2.0 * coef * tsi * j3oj2 * state->no_unkozai * sinio / state->ecco;
    }
    state->x7thm1 = 7.0 * cosio2 - 1.0;
    state->cc4 = 2.0 * state->no_unkozai * coef1 * state->a * omeosq
           * (state->eta * (2.0 + 0.5 * etasq) + state->ecco * (0.5 + 2.0 * etasq)
           - j2 * tsi / (state->a * psisq) * (-3.0 * (3.0 * (3.0 * cosio2 - 1.0) - 7.0 * state->x7thm1)
           * (1.0 + 1.5 * etasq) / 6.0 + 0.25 * (3.0 - 7.0 * cosio2) * (2.0 * etasq - eeta * (1.0 + etasq)) * cos(2.0 * state->argpo)));
    state->cc5 = 2.0 * coef1 * state->a * omeosq * (1.0 + 2.75 * (etasq + eeta) + eeta * etasq);

    double cosio4 = cosio2 * cosio2;
    double temp1 = 1.5 * j2 * pinvsq * state->no_unkozai;
    double temp2 = 0.5 * temp1 * j2 * pinvsq;
    double temp3 = -0.46875 * j4 * pinvsq * pinvsq * state->no_unkozai;
    state->mdot = state->no_unkozai + 0.5 * temp1 * rteosq * (3.0 * cosio2 - 1.0)
            + 0.0625 * temp2 * rteosq * (13.0 - 78.0 * cosio2 + 137.0 * cosio4);
    state->argpdot = -0.5 * temp1 * (1.0 - 5.0 * cosio2)
               + 0.0625 * temp2 * (7.0 - 114.0 * cosio2 + 395.0 * cosio4)
               + temp3 * (3.0 - 36.0 * cosio2 + 49.0 * cosio4);
    double xhdot1 = -temp1 * cosio;
    state->nodedot = xhdot1 + (0.5 * temp2 * (4.0 - 19.0 * cosio2) + 2.0 * temp3 * (3.0 - 7.0 * cosio2)) * cosio;
    state->omgcof = state->bstar * cc3 * cos(state->argpo);
    state->xmcof = 0.0;
    if (state->ecco > 1.0e-4) {
        state->xmcof = -SGP4_X2O3 * coef * state->bstar / eeta;
    }
    state->nodecf = 3.5 * omeosq * xhdot1 * state->cc1;
    state->t2cof = 1.5 * state->cc1;

    // Set xlcof and aycof
    if (fabs(cosio + 1.0) > 1.5e-12) {
        state->xlcof = -0.25 * j3oj2 * sinio * (3.0 + 5.0 * cosio) / (1.0 + cosio);
    } else {
        state->xlcof = -0.25 * j3oj2 * sinio * (3.0 + 5.0 * cosio) / 1.5e-12;
    }
    state->aycof = -0.5 * j3oj2 * sinio;

    // For SGP4, initialize additional terms
    state->delmo = pow(1.0 + state->eta * cos(state->mo), 3);
    state->sinmao = sin(state->mo);

    // Compute con41 for later use
    state->con41 = 3.0 * cosio2 - 1.0;

    // Set isimp flag for very high drag satellites
    state->isimp = 0;
    if ((omeosq >= 0.0) || (state->no_unkozai >= 0.0)) {
        if ((rp < (220.0 / radiusearthkm + 1.0))) {
            state->isimp = 1;
        }
    }

    // Initialize d2, d3, d4, t3cof, t4cof, t5cof
    state->d2 = 0.0;
    state->d3 = 0.0;
    state->d4 = 0.0;
    state->t3cof = 0.0;
    state->t4cof = 0.0;
    state->t5cof = 0.0;

    if (!state->isimp) {
        double c1sq = state->cc1 * state->cc1;
        state->d2 = 4.0 * state->a * tsi * c1sq;
        double temp = state->d2 * tsi * state->cc1 / 3.0;
        state->d3 = (17.0 * state->a + sfour) * temp;
        state->d4 = 0.5 * temp * state->a * tsi * (221.0 * state->a + 31.0 * sfour) * state->cc1;
        state->t3cof = state->d2 + 2.0 * c1sq;
        state->t4cof = 0.25 * (3.0 * state->d3 + state->cc1 * (12.0 * state->d2 + 10.0 * c1sq));
        state->t5cof = 0.2 * (3.0 * state->d4 + 12.0 * state->cc1 * state->d3 + 6.0 * state->d2 * state->d2 + 15.0 * c1sq * (2.0 * state->d2 + c1sq));
    }

    // Deep space initialization if needed
    if (state->method == 'd') {
        state->irez = 0;
        if ((state->no_unkozai < 0.0052359877) && (state->no_unkozai > 0.0034906585)) {
            state->irez = 1;  // Synchronous resonance
        }
        if ((state->no_unkozai >= 8.26e-3) && (state->no_unkozai <= 9.24e-3) && (state->ecco >= 0.5)) {
            state->irez = 2;  // Half-day resonance
        }

        sgp4__init_deep_space(state, 0.0);
    }

    state->initialized = 1;
    return SGP4_SUCCESS;
}

// ============================================================================
// Deep Space Secular Effects (internal)
// ============================================================================

static void sgp4__deep_space_secular(
    const sgp4_state_t* state,
    double t,
    double* em, double* argpm, double* inclm,
    double* nodem, double* mm, double* nm,
    double* atime, double* xli, double* xni)
{
    const double step = 720.0;
    const double step2 = step * step / 2.0;

    // Initialize from state
    *atime = state->atime;
    *xli = state->xli;
    *xni = state->xni;

    // Apply lunar-solar periodics
    double zm = state->zmos + 0.017201977 * t;
    double zf = zm + 2.0 * 0.01675 * sin(zm);
    double sinzf = sin(zf);
    double f2 = 0.5 * sinzf * sinzf - 0.25;
    double f3 = -0.5 * sinzf * cos(zf);

    double ses = state->se2 * f2 + state->se3 * f3;
    double sis = state->si2 * f2 + state->si3 * f3;
    double sls = state->sl2 * f2 + state->sl3 * f3 + state->sl4 * sinzf;
    double sghs = state->sgh2 * f2 + state->sgh3 * f3 + state->sgh4 * sinzf;
    double shs = state->sh2 * f2 + state->sh3 * f3;

    zm = state->zmol + 0.22997150 * t;
    zf = zm + 2.0 * 0.05490 * sin(zm);
    sinzf = sin(zf);
    f2 = 0.5 * sinzf * sinzf - 0.25;
    f3 = -0.5 * sinzf * cos(zf);

    double sel = state->ee2 * f2 + state->e3 * f3;
    double sil = state->xi2 * f2 + state->xi3 * f3;
    double sll = state->xl2 * f2 + state->xl3 * f3 + state->xl4 * sinzf;
    double sghl = state->xgh2 * f2 + state->xgh3 * f3 + state->xgh4 * sinzf;
    double shll = state->xh2 * f2 + state->xh3 * f3;

    double pe = ses + sel;
    double pinc = sis + sil;
    double pl = sls + sll;
    double pgh = sghs + sghl;
    double ph = shs + shll;

    if (fabs(state->inclo) >= 0.2) {
        ph /= sin(state->inclo);
        *inclm += pinc;
        *nodem += ph;
        *argpm -= pgh;
    } else {
        double siniq = sin(state->inclo);
        double cosiq = cos(state->inclo);
        double temp_mod = ph * cosiq;
        *inclm += pinc;
        *nodem += ph / siniq;
        *argpm -= temp_mod / siniq;
    }

    *em += pe;
    *mm += pl;

    // Handle resonance effects
    if (state->irez != 0) {
        const double earthRotRate = 7.29211514668855e-5;

        if (state->irez == 1) {
            // Synchronous resonance terms
            double xfact = state->mdot + state->argpdot + state->nodedot - earthRotRate - state->no_unkozai;
            double xldot = *xni + xfact;
            double theta = fmod(state->gsto + t * earthRotRate, SGP4_TWO_PI);
            double xndt = state->del1 * sin(state->xlamo - 2.0 * (state->nodeo + state->argpo) + theta) +
                   state->del2 * sin(2.0 * (state->xlamo - state->nodeo - state->argpo)) +
                   state->del3 * sin(3.0 * state->xlamo - state->nodeo - state->argpo + theta);

            if (fabs(t - *atime) >= step) {
                double stepp = step;
                if (t < *atime) stepp = -step;

                while (fabs(t - *atime) >= step) {
                    xldot = *xni + xfact;
                    *xli += xldot * stepp + xndt * step2;
                    *xni += xndt * stepp;
                    *atime += stepp;

                    theta = fmod(state->gsto + *atime * earthRotRate, SGP4_TWO_PI);
                    xndt = state->del1 * sin(*xli - 2.0 * (state->nodeo + state->argpo) + theta) +
                           state->del2 * sin(2.0 * (*xli - state->nodeo - state->argpo)) +
                           state->del3 * sin(3.0 * *xli - state->nodeo - state->argpo + theta);
                }
            }

            double ft = t - *atime;
            xldot = *xni + xfact;
            *nm = *xni + xndt * ft;
            double xl = *xli + xldot * ft + xndt * ft * ft * 0.5;
            *mm = xl - 2.0 * *nodem + 2.0 * theta;
        }

        if (state->irez == 2) {
            // Half-day resonance terms
            const double g22 = 5.7686396;
            const double g32 = 0.95240898;
            const double g44 = 1.8014998;
            const double g52 = 1.0508330;
            const double g54 = 4.4108898;

            double xfact = state->mdot + state->dmdt + 2.0 * (state->nodedot + state->dnodt - earthRotRate) - state->no_unkozai;
            double xldot = *xni + xfact;
            double theta = fmod(state->gsto + t * earthRotRate, SGP4_TWO_PI);
            double xomi = state->argpo + state->argpdot * *atime;
            double x2omi = xomi + xomi;
            double x2li = *xli + *xli;

            double xndt = state->d2201 * sin(x2omi + *xli - g22) +
                   state->d2211 * sin(*xli - g22) +
                   state->d3210 * sin(xomi + *xli - g32) +
                   state->d3222 * sin(-xomi + *xli - g32) +
                   state->d4410 * sin(x2omi + x2li - g44) +
                   state->d4422 * sin(x2li - g44) +
                   state->d5220 * sin(xomi + *xli - g52) +
                   state->d5232 * sin(-xomi + *xli - g52) +
                   state->d5421 * sin(xomi + x2li - g54) +
                   state->d5433 * sin(-xomi + x2li - g54);

            double xnddt = state->d2201 * cos(x2omi + *xli - g22) +
                    state->d2211 * cos(*xli - g22) +
                    state->d3210 * cos(xomi + *xli - g32) +
                    state->d3222 * cos(-xomi + *xli - g32) +
                    state->d5220 * cos(xomi + *xli - g52) +
                    state->d5232 * cos(-xomi + *xli - g52) +
                    2.0 * (state->d4410 * cos(x2omi + x2li - g44) +
                           state->d4422 * cos(x2li - g44) +
                           state->d5421 * cos(xomi + x2li - g54) +
                           state->d5433 * cos(-xomi + x2li - g54));
            xnddt *= xldot;

            if (fabs(t - *atime) >= step) {
                double stepp = step;
                if (t < *atime) stepp = -step;

                while (fabs(t - *atime) >= step) {
                    xldot = *xni + xfact;
                    *xli += xldot * stepp + xndt * step2;
                    *xni += xndt * stepp;
                    *atime += stepp;

                    xomi = state->argpo + state->argpdot * *atime;
                    x2omi = xomi + xomi;
                    x2li = *xli + *xli;

                    xndt = state->d2201 * sin(x2omi + *xli - g22) +
                           state->d2211 * sin(*xli - g22) +
                           state->d3210 * sin(xomi + *xli - g32) +
                           state->d3222 * sin(-xomi + *xli - g32) +
                           state->d4410 * sin(x2omi + x2li - g44) +
                           state->d4422 * sin(x2li - g44) +
                           state->d5220 * sin(xomi + *xli - g52) +
                           state->d5232 * sin(-xomi + *xli - g52) +
                           state->d5421 * sin(xomi + x2li - g54) +
                           state->d5433 * sin(-xomi + x2li - g54);

                    xnddt = state->d2201 * cos(x2omi + *xli - g22) +
                            state->d2211 * cos(*xli - g22) +
                            state->d3210 * cos(xomi + *xli - g32) +
                            state->d3222 * cos(-xomi + *xli - g32) +
                            state->d5220 * cos(xomi + *xli - g52) +
                            state->d5232 * cos(-xomi + *xli - g52) +
                            2.0 * (state->d4410 * cos(x2omi + x2li - g44) +
                                   state->d4422 * cos(x2li - g44) +
                                   state->d5421 * cos(xomi + x2li - g54) +
                                   state->d5433 * cos(-xomi + x2li - g54));
                    xnddt *= xldot;
                }
            }

            double ft = t - *atime;
            xldot = *xni + xfact;
            *nm = *xni + xndt * ft + xnddt * ft * ft * 0.5;
            double xl = *xli + xldot * ft + xndt * ft * ft * 0.5;
            double temp_mm = -*nodem - *nodem + theta + theta;
            *mm = xl - xomi + temp_mm;
        }
    }
}

// ============================================================================
// Deep Space Periodic Effects (internal)
// ============================================================================

static void sgp4__deep_space_periodic(
    const sgp4_state_t* state,
    double t,
    double* em, double* inclm, double* nodem,
    double* argpm, double* mm)
{
    double zm = state->zmos + 0.017201977 * t;
    double zf = zm + 2.0 * 0.01675 * sin(zm);
    double sinzf = sin(zf);
    double f2 = 0.5 * sinzf * sinzf - 0.25;
    double f3 = -0.5 * sinzf * cos(zf);

    double ses = state->se2 * f2 + state->se3 * f3;
    double sis = state->si2 * f2 + state->si3 * f3;
    double sls = state->sl2 * f2 + state->sl3 * f3 + state->sl4 * sinzf;
    double sghs = state->sgh2 * f2 + state->sgh3 * f3 + state->sgh4 * sinzf;
    double shs = state->sh2 * f2 + state->sh3 * f3;

    zm = state->zmol + 0.22997150 * t;
    zf = zm + 2.0 * 0.05490 * sin(zm);
    sinzf = sin(zf);
    f2 = 0.5 * sinzf * sinzf - 0.25;
    f3 = -0.5 * sinzf * cos(zf);

    double sel = state->ee2 * f2 + state->e3 * f3;
    double sil = state->xi2 * f2 + state->xi3 * f3;
    double sll = state->xl2 * f2 + state->xl3 * f3 + state->xl4 * sinzf;
    double sghl = state->xgh2 * f2 + state->xgh3 * f3 + state->xgh4 * sinzf;
    double shll = state->xh2 * f2 + state->xh3 * f3;

    double pe = ses + sel - state->peo;
    double pinc = sis + sil - state->pinco;
    double pl = sls + sll - state->plo;
    double pgh = sghs + sghl - state->pgho;
    double ph = shs + shll - state->pho;

    if (fabs(state->inclo) >= 0.2) {
        ph /= sin(state->inclo);
        *inclm += pinc;
        *em += pe;
        *nodem += ph;
        *argpm -= pgh;
        *mm += pl;
    } else {
        double siniq = sin(state->inclo);
        double cosiq = cos(state->inclo);

        *inclm += pinc;
        *em += pe;

        double sinis = sin(*inclm);

        if (fabs(*inclm) >= 0.2) {
            double temp_per = ph / sinis;
            *nodem += temp_per;
            *argpm -= pgh - cosiq * temp_per;
            *mm += pl;
        } else {
            double temp_per = ph * cosiq;
            *nodem += ph / siniq;
            *argpm -= temp_per / siniq;
            *mm += pl;
        }
    }
}

// ============================================================================
// SGP4 Propagation
// ============================================================================

sgp4_error_t sgp4_propagate(const sgp4_state_t* state, double tsince, sgp4_result_t* result) {
    if (!state || !result) {
        return SGP4_ERROR_NULL_POINTER;
    }

    if (!state->initialized) {
        return SGP4_ERROR_NOT_INITIALIZED;
    }

    const double radiusearthkm = SGP4_RADIUS_EARTH;
    const double xke = SGP4_XKE;
    const double j2 = SGP4_J2;
    const double vkmpersec = SGP4_VKMPERSEC;

    double cosio = cos(state->inclo);
    double sinio = sin(state->inclo);

    // Update for secular gravity and atmospheric drag
    double xmdf = state->mo + state->mdot * tsince;
    double argpdf = state->argpo + state->argpdot * tsince;
    double nodedf = state->nodeo + state->nodedot * tsince;
    double argpm = argpdf;
    double mm = xmdf;
    double t2 = tsince * tsince;
    double nodem = nodedf + state->nodecf * t2;
    double tempa = 1.0 - state->cc1 * tsince;
    double tempe = state->bstar * state->cc4 * tsince;
    double templ = state->t2cof * t2;

    if (!state->isimp) {
        double delomg = state->omgcof * tsince;
        double delm = state->xmcof * (pow(1.0 + state->eta * cos(xmdf), 3) - state->delmo);
        double temp_sgp = delomg + delm;
        mm = xmdf + temp_sgp;
        argpm = argpdf - temp_sgp;
        double t3 = t2 * tsince;
        double t4 = t3 * tsince;
        tempa = tempa - state->d2 * t2 - state->d3 * t3 - state->d4 * t4;
        tempe = tempe + state->bstar * state->cc5 * (sin(mm) - state->sinmao);
        templ = templ + state->t3cof * t3 + t4 * (state->t4cof + tsince * state->t5cof);
    }

    double nm = state->no_unkozai;
    double em = state->ecco;
    double inclm = state->inclo;

    // Initialize resonance state
    double atime = state->atime;
    double xli = state->xli;
    double xni = state->xni;

    // Handle deep space satellites
    if (state->method == 'd') {
        sgp4__deep_space_secular(state, tsince, &em, &argpm, &inclm, &nodem, &mm, &nm, &atime, &xli, &xni);
    }

    double am = pow(xke / nm, SGP4_X2O3) * tempa * tempa;
    nm = xke / pow(am, 1.5);
    em = em - tempe;

    // Check for eccentricity out of range
    if (em >= 1.0 || em < -0.001) {
        return SGP4_ERROR_INVALID_ECCENTRICITY;
    }
    if (em < 1.0e-6) {
        em = 1.0e-6;
    }

    mm = mm + state->no_unkozai * templ;
    double xlm = mm + argpm + nodem;

    nodem = fmod(nodem, SGP4_TWO_PI);
    argpm = fmod(argpm, SGP4_TWO_PI);
    xlm = fmod(xlm, SGP4_TWO_PI);
    mm = fmod(xlm - argpm - nodem, SGP4_TWO_PI);

    // Apply deep space periodic effects
    if (state->method == 'd') {
        sgp4__deep_space_periodic(state, tsince, &em, &inclm, &nodem, &argpm, &mm);
    }

    // Re-compute sini/cosi if inclination changed
    if (inclm != state->inclo) {
        sinio = sin(inclm);
        cosio = cos(inclm);
    }

    if (em < 0.0) {
        em = 1.0e-6;
    }

    double sinim = sin(inclm);
    double cosim = cos(inclm);

    double ep = em;
    double xincp = inclm;
    double argpp = argpm;
    double nodep = nodem;
    double mp = mm;

    // Check if satellite decayed
    if (nm <= 0.0) {
        return SGP4_ERROR_SATELLITE_DECAYED;
    }

    double eccsq = ep * ep;
    double omeosq = 1.0 - eccsq;

    if (omeosq <= 0.0) {
        return SGP4_ERROR_INVALID_ORBIT;
    }

    // Long period periodics
    cosio = cosim;
    sinio = sinim;
    double cosio2 = cosio * cosio;

    double axnl = ep * cos(argpp);
    double temp_lp = 1.0 / (am * omeosq);
    double aynl = ep * sin(argpp) + temp_lp * state->aycof;
    double xl = mp + argpp + nodep + temp_lp * state->xlcof * axnl;

    // Solve Kepler's equation
    double u = fmod(xl - nodep, SGP4_TWO_PI);
    double eo1 = u;
    double tem5 = 9999.9;
    int ktr = 1;
    double sineo1, coseo1;

    while ((fabs(tem5) >= 1.0e-12) && (ktr <= 10)) {
        sineo1 = sin(eo1);
        coseo1 = cos(eo1);
        tem5 = 1.0 - coseo1 * axnl - sineo1 * aynl;
        tem5 = (u - aynl * coseo1 + axnl * sineo1 - eo1) / tem5;
        if (fabs(tem5) >= 0.95) {
            tem5 = tem5 > 0.0 ? 0.95 : -0.95;
        }
        eo1 = eo1 + tem5;
        ktr++;
    }

    // Short period preliminary quantities
    double ecose = axnl * coseo1 + aynl * sineo1;
    double esine = axnl * sineo1 - aynl * coseo1;
    double el2 = axnl * axnl + aynl * aynl;
    double pl = am * (1.0 - el2);

    if (pl < 0.0) {
        return SGP4_ERROR_INVALID_ORBIT;
    }

    double rl = am * (1.0 - ecose);
    double rdotl = sqrt(am) * esine / rl;
    double rvdotl = sqrt(pl) / rl;
    double betal = sqrt(1.0 - el2);
    double temp_sp = esine / (1.0 + betal);
    double sinu = am / rl * (sineo1 - aynl - axnl * temp_sp);
    double cosu = am / rl * (coseo1 - axnl + aynl * temp_sp);
    double su = atan2(sinu, cosu);
    double sin2u = (cosu + cosu) * sinu;
    double cos2u = 1.0 - 2.0 * sinu * sinu;
    double temp_sp2 = 1.0 / pl;
    double temp1_sp = 0.5 * j2 * temp_sp2;
    double temp2_sp = temp1_sp * temp_sp2;

    // Update for short period periodics
    double con41 = 3.0 * cosio2 - 1.0;
    double x1mth2 = 1.0 - cosio2;
    double x7thm1 = 7.0 * cosio2 - 1.0;

    double mrt = rl * (1.0 - 1.5 * temp2_sp * betal * con41) + 0.5 * temp1_sp * x1mth2 * cos2u;
    su = su - 0.25 * temp2_sp * x7thm1 * sin2u;
    double xnode = nodep + 1.5 * temp2_sp * cosio * sin2u;
    double xinc = xincp + 1.5 * temp2_sp * cosio * sinio * cos2u;
    double mvt = rdotl - nm * temp1_sp * x1mth2 * sin2u / xke;
    double rvdot = rvdotl + nm * temp1_sp * (x1mth2 * cos2u + 1.5 * con41) / xke;

    // Orientation vectors
    double sinsu = sin(su);
    double cossu = cos(su);
    double snod = sin(xnode);
    double cnod = cos(xnode);
    double sini = sin(xinc);
    double cosi = cos(xinc);
    double xmx = -snod * cosi;
    double xmy = cnod * cosi;
    double ux = xmx * sinsu + cnod * cossu;
    double uy = xmy * sinsu + snod * cossu;
    double uz = sini * sinsu;
    double vx = xmx * cossu - cnod * sinsu;
    double vy = xmy * cossu - snod * sinsu;
    double vz = sini * cossu;

    // Position and velocity (in km and km/s)
    result->r[0] = mrt * ux * radiusearthkm;
    result->r[1] = mrt * uy * radiusearthkm;
    result->r[2] = mrt * uz * radiusearthkm;
    result->v[0] = (mvt * ux + rvdot * vx) * vkmpersec;
    result->v[1] = (mvt * uy + rvdot * vy) * vkmpersec;
    result->v[2] = (mvt * uz + rvdot * vz) * vkmpersec;

    // Store updated resonance state
    result->atime = atime;
    result->xli = xli;
    result->xni = xni;

    // Check if satellite decayed
    if (mrt < 1.0) {
        return SGP4_ERROR_SATELLITE_DECAYED;
    }

    return SGP4_SUCCESS;
}

// ============================================================================
// Coordinate Transforms
// ============================================================================

void sgp4_eci_to_ecef(const sgp4_vec3_t* eci, double gst, sgp4_vec3_t* ecef) {
    double cosGST = cos(gst);
    double sinGST = sin(gst);

    ecef->x =  eci->x * cosGST + eci->y * sinGST;
    ecef->y = -eci->x * sinGST + eci->y * cosGST;
    ecef->z =  eci->z;
}

void sgp4_ecef_to_eci(const sgp4_vec3_t* ecef, double gst, sgp4_vec3_t* eci) {
    double cosGST = cos(gst);
    double sinGST = sin(gst);

    eci->x = ecef->x * cosGST - ecef->y * sinGST;
    eci->y = ecef->x * sinGST + ecef->y * cosGST;
    eci->z = ecef->z;
}

void sgp4_ecef_to_geodetic(const sgp4_vec3_t* ecef, sgp4_geodetic_t* geo) {
    double x = ecef->x, y = ecef->y, z = ecef->z;
    double lon = atan2(y, x);
    double p = sqrt(x*x + y*y);

    // Iterative latitude calculation (Bowring's method)
    double lat = atan2(z, p * (1 - SGP4_WGS84_E2));  // initial guess
    for (int i = 0; i < 10; ++i) {
        double sinLat = sin(lat);
        double N = SGP4_WGS84_A / sqrt(1 - SGP4_WGS84_E2 * sinLat * sinLat);
        lat = atan2(z + SGP4_WGS84_E2 * N * sinLat, p);
    }

    double sinLat = sin(lat);
    double N = SGP4_WGS84_A / sqrt(1 - SGP4_WGS84_E2 * sinLat * sinLat);
    double alt = p / cos(lat) - N;

    geo->lat_rad = lat;
    geo->lon_rad = lon;
    geo->alt_km = alt;
}

void sgp4_geodetic_to_ecef(const sgp4_geodetic_t* geo, sgp4_vec3_t* ecef) {
    double sinLat = sin(geo->lat_rad);
    double cosLat = cos(geo->lat_rad);
    double sinLon = sin(geo->lon_rad);
    double cosLon = cos(geo->lon_rad);

    // Radius of curvature in the prime vertical
    double N = SGP4_WGS84_A / sqrt(1.0 - SGP4_WGS84_E2 * sinLat * sinLat);

    ecef->x = (N + geo->alt_km) * cosLat * cosLon;
    ecef->y = (N + geo->alt_km) * cosLat * sinLon;
    ecef->z = (N * (1.0 - SGP4_WGS84_E2) + geo->alt_km) * sinLat;
}

void sgp4_ecef_to_enu(const sgp4_vec3_t* target_ecef, const sgp4_geodetic_t* observer, sgp4_vec3_t* enu) {
    // Get observer's ECEF position
    sgp4_vec3_t observer_ecef;
    sgp4_geodetic_to_ecef(observer, &observer_ecef);

    // Compute difference vector
    sgp4_vec3_t diff;
    sgp4_vec3_sub(target_ecef, &observer_ecef, &diff);

    // Rotation matrix
    double sinLat = sin(observer->lat_rad);
    double cosLat = cos(observer->lat_rad);
    double sinLon = sin(observer->lon_rad);
    double cosLon = cos(observer->lon_rad);

    enu->x = -sinLon * diff.x + cosLon * diff.y;  // East
    enu->y = -sinLat * cosLon * diff.x - sinLat * sinLon * diff.y + cosLat * diff.z;  // North
    enu->z =  cosLat * cosLon * diff.x + cosLat * sinLon * diff.y + sinLat * diff.z;  // Up
}

// ============================================================================
// Look Angles
// ============================================================================

void sgp4_look_angles(const sgp4_vec3_t* sat_ecef, const sgp4_geodetic_t* observer, sgp4_look_angles_t* angles) {
    sgp4_vec3_t enu;
    sgp4_ecef_to_enu(sat_ecef, observer, &enu);

    // Range
    angles->range_km = sgp4_vec3_magnitude(&enu);

    // Elevation
    angles->elevation_rad = asin(enu.z / angles->range_km);

    // Azimuth (0 = North, pi/2 = East)
    angles->azimuth_rad = atan2(enu.x, enu.y);
    if (angles->azimuth_rad < 0.0) {
        angles->azimuth_rad += SGP4_TWO_PI;
    }
}
