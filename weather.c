#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <getopt.h>

// Constants:
#define API_KEY_LEN 33
static const char * const API_KEY_ERROR = "Please add correct key to environment variable API_KEY_WEATHER\n";

#define MAX_URL_LENGTH 2000
//global variable used both in main and functions
static char *unit_symbol = "°C";

 /* used code generated by the curl command line tool;
 All curl_easy_setopt() options are documented at:
 https://curl.haxx.se/libcurl/c/curl_easy_setopt.html

 libcurl is used to query the OpenWeather HTTP API with the resulting JSON
 parsed by JSON-C.
 *************************************************************************/
static size_t received_data(char *data, size_t size, size_t nmemb, void *userp) {
    // size is always 1 and is not used
    (void) size;
    (void) userp;

    // 'nmemb' may be 0 if the received file is empty. Exit early.
    if (nmemb == 0) {
        return 0;
    }

    // CURL does not guarantee that 'data' is null-terminated. Check this before
    // passing 'data' to json_tokener_parse().
    bool null_char_found = false;
    for (int i = 0; i < nmemb + 1; i++) {
        if (data[i] == '\0') {
            null_char_found = true;
            break;
        }
    }
    if (!null_char_found) {
        return 0;
    }

    // parse returned JSON data as city and weather forecast
    struct json_object *parsed_json;
    parsed_json = json_tokener_parse(data);
    if (!parsed_json) {
        return 0;
    }

    //get name of city
    struct json_object *name;
    json_object_object_get_ex(parsed_json, "name", &name);
    // check error status codes
    if (!name) {
        return 0;
    }

    //get temperature from dict placed in main from parsed json
    struct json_object *main; //string where temperature is stored
    json_object_object_get_ex(parsed_json, "main", &main);
    if (!main) {
        return 0;
    }
    //get temp key/value pair from main to find temperature
    struct json_object *temperature;
    json_object_object_get_ex(main, "temp", &temperature);
        if (!temperature) {
        return 0;
    }
    //grab temperature value and check validity
    int is_double = json_object_is_type(temperature, json_type_double);
    if (is_double == 0){
        return 0;
    }
    double d_temperature = json_object_get_double(temperature);

    printf("The temperature in %s is %.2f %s\n", json_object_get_string(name), d_temperature, unit_symbol);
    return nmemb;
}


int main(int argc, char **argv) {
    //fetch environment variable that contains weather API key
    char *api_key = getenv("API_KEY_WEATHER");
    if (!api_key) {
        fputs("Please add key to environment variable API_KEY_WEATHER", stderr);
        return EXIT_FAILURE;
    }

    // check and validate api key is correct format (check API to see if always same length and has correct characters)
    int count = 0;
    // temporary pointer variable
    char *current_char = api_key;
    while (*current_char != '\0') {
        //check api key is not more than 33 characters long
        if ((current_char - api_key ) >= API_KEY_LEN) {
            fputs(API_KEY_ERROR, stderr);
            return EXIT_FAILURE;
        }
        //check if a hexadecimal digit
        if (!isxdigit(*current_char)) {
            //it is not a hex digit
            fputs(API_KEY_ERROR, stderr);
            return EXIT_FAILURE;
        }
        current_char++;
    }
    //check API key is sufficiently long (previous only checked that it was not too long)
    if (((current_char - api_key) + 1)!= API_KEY_LEN) {
        fputs(API_KEY_ERROR, stderr);
        return EXIT_FAILURE;
    }

    // Choose unit want temperature in, can be imperial(Fahrenheit) or
    // metric(Celsius). Temperature in Kelvin is used by default if not declared
    // this allows override with command line arg (getopt)
    char *units = "metric";
    int c;
    while (1) {
        static struct option long_options[] = {
            {"units", required_argument, 0, 'u'},
            {0, 0, 0, 0}
        };
        /* getopt_long stores the option index here. */
        int option_index = 0;
        c = getopt_long(argc, argv, "u:", long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1)
            break;
        switch (c) {
            case 'u':
                //check "option u with value %s\n", optarg) is correct unit
                units = optarg;
                //if does not match a correct unit leave the default
                int units_length = strlen(units);
                for (int i = 0; i < units_length; i++) {
                  units[i] = tolower(units[i]);
                }
                //if unit exists - change the unit_symbol
                if (strcmp(units, "standard") == 0) {
                    unit_symbol = "K";
                }
                else if (strcmp(units, "metric") == 0) {
                    unit_symbol = "°C";
                }
                else if (strcmp(units, "imperial") == 0) {
                    unit_symbol = "°F";     
                }
                else {
                    fprintf(stderr, "Usage: %s is not a correct unit, metric will be used\n", units);
                    units = "metric";
                }
                break;
            default:
                abort();
        }
    }
    /* Check mandatory arguments are present */
    if ((argc - optind) != 2) {
        fprintf(stderr, "Usage: %s city iso3166_country_code\n\tExample: %s Paris FR\n", argv[0], argv[0]);
        return EXIT_FAILURE;
    }
    char *city = argv[optind];
    char *country_code = argv[optind + 1];

    //sprintf URL together by adding key, city and country code
    char *url_template = "https://api.openweathermap.org/data/2.5/weather?q=%s,%s&appid=%s&units=%s";
    char url[MAX_URL_LENGTH];

    // add command line argv to URL
    int url_length = snprintf(
        url,
        MAX_URL_LENGTH,
        url_template,
        city, country_code, api_key, units
    );
    // Cap maximum URL length for compatibility
    if (url_length >= MAX_URL_LENGTH) {
        fprintf(stderr, "URL too large\n");
        return EXIT_FAILURE;
    }

    //Reach out to server and fetch data from openweathermap data returned is JSON
    CURL *hnd = curl_easy_init();
    if (!hnd) {
        return EXIT_FAILURE;
    }

    curl_easy_setopt(hnd, CURLOPT_URL, url);
    curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, received_data);
    curl_easy_setopt(hnd, CURLOPT_FAILONERROR, 1);
    
    // TODO: differentiate 401 error(key error)/ DNS issues / 404 if city not recognised
    // or if make more than 60 API calls a minute (429 error returned)
    CURLcode ret = curl_easy_perform(hnd);
    if (ret != CURLE_OK) {
        fprintf(stderr, "Error\n");
        curl_easy_cleanup(hnd);
        return EXIT_FAILURE;
    }
    curl_easy_cleanup(hnd);

    return EXIT_SUCCESS;
}
