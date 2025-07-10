/*
 * settings.h
 *
 * This file contains the declarations for the settings management functions and constants.
 * It provides functions to initialize settings, get and set various configuration parameters,
 * and manage thresholds for voltage, pressure, and current.
 *
 * Author:  David Hoy
 * Date:    Feb 2025
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <nvs_flash.h>

#define DEBUG_SHOW_TASK_STATS   0x0001


// Functions
void settings_init(bool reset_defaults);
esp_err_t get_setting(const char *key, void *out_value, size_t *size, bool is_string);
esp_err_t set_setting(const char *key, const void *value, size_t size, bool is_string);

// Geter/setter for NMEA node address
unsigned char   get_node_address(void);
void            set_node_address(unsigned char value);

// Getter/setter for device serial number
uint32_t        get_serial_nbr(void);
void            set_serial_nbr(uint32_t serial_nbr);

// Getter/setter for NMEA device and installation labels
char *          get_device_label(char *label, size_t max_length);
void            set_device_label(const char *label);
char *          get_installation_1(char *label, size_t max_length);
void            set_installation_1(const char *label);
char *          get_installation_2(char *label, size_t max_length);
void            set_installation_2(const char *label);

// Getter/setter for NMEA instance number
unsigned char   get_instance(void);
void            set_instance(unsigned char value);

// Getters/setters for flush times, value in seconds
unsigned short  get_short_flush_time(void);
void            set_short_flush_time(unsigned short value);
unsigned short  get_long_flush_time(void);
void            set_long_flush_time(unsigned short value);
unsigned short  get_mini_flush_time(void);
void            set_mini_flush_time(unsigned short value);

// Getters/setters for voltage thresholds, in 0.01 volts
unsigned short  get_low_voltage_threshold(void);
unsigned short  get_high_voltage_threshold(void);
void            set_low_voltage_threshold(unsigned short value);
void            set_high_voltage_threshold(unsigned short value);

// Getters/setters for pressure thresholds, in 0.01 psi
unsigned short  get_low_pressure_threshold(void);
unsigned short  get_high_pressure_threshold(void); 
void            set_low_pressure_threshold(unsigned short value);
void            set_high_pressure_threshold(unsigned short value);
unsigned short  get_pressure_check_interval(void);
void            set_pressure_check_interval(unsigned short value);

// Getters/setters for current thresholds, in 0.01 amps
unsigned short  get_low_current_threshold(void); 
unsigned short  get_high_current_threshold(void);
void            set_low_current_threshold(unsigned short value); 
void            set_high_current_threshold(unsigned short value);

unsigned short  get_flush_timeout(void);
void            set_flush_timeout(unsigned short value);

// Getter/setter for debug flags
unsigned short  get_debug_flags(void); 
void            set_debug_flags(unsigned short value); 

#ifdef __cplusplus
}   
#endif

#endif





