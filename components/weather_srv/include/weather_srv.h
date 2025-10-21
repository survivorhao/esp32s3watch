#pragma once


// Default query: Wuhan weather
extern char weather_position[32];


/**
 * @brief Register weather-related event handlers with the UI event loop.
 *
 * Registers handlers for weather request and weather position change events
 * so the application can trigger weather queries and update the configured
 * location.
 */
void weather_register_weathe_service_handler(void);
