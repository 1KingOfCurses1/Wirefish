/*
 * File: log.c
 * Implements printf-style logging to stderr with level filtering.
 */

#include <stdio.h>
#include <stdarg.h>
#include "log.h"


//Starting level
static LogLevel currentLevel = LOG_INFO;


/*
 * This is what sets what level of log messages we want to show
 * chosenLevel = the level to use
 * no return
 */
void log_set_level(LogLevel chosenLevel){
    currentLevel = chosenLevel;
}


/*
 * helper so we dont type the same code 4 times
 * messageLevel = level for the message
 * tag = the text like it can be something like "debug"
 * message = printf-style message text
 * args = extra stuff for the format
 */
static void write_log_message(LogLevel messageLevel, char *tag, char *message, va_list args){

    //dont print if it's below the level we set
    if(messageLevel < currentLevel){
        return;
    }

    //print tag first
    fprintf(stderr,"[%s] ", tag);

    //print the message like printf
    vfprintf(stderr, message, args);
    
    //newline so logs dont stick together
    fprintf(stderr,"\n");
}



/*
 * prints a debug log message
 */
void log_debug(char *message, ...){
    va_list args;
    va_start(args, message);
    write_log_message(LOG_DEBUG, "debug : ", message, args);
    va_end(args);
}



/*
 * prints an info log message
 */
void log_info(char *message, ...){
    va_list args;
    va_start(args, message);
    write_log_message(LOG_INFO, "Info : ", message, args);
    va_end(args);
}



/*
 * prints a warning message
 */
void log_warn(char *message, ...){
    va_list args;
    va_start(args, message);
    write_log_message(LOG_WARN, "warn : ", message, args);
    va_end(args);
}



/*
 * prints an error message
 */
void log_error(char *message, ...){
    va_list args;
    va_start(args, message);
    write_log_message(LOG_ERROR, "error : ", message, args);
    va_end(args);
}
