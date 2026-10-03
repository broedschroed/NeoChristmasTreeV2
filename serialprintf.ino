#include <stdarg.h>
#define SERIAL_PRINTF_MAX_BUFF      256

/*
 * Self-made replacement for easier debugging
 */
void serialPrintf(const char *fmt, ...) {
  /* Buffer for storing the formatted data */
  char buff[SERIAL_PRINTF_MAX_BUFF];
  /* pointer to the variable arguments list */
  va_list pargs;
  /* Initialise pargs to point to the first optional argument */
  va_start(pargs, fmt);
  /* create the formatted data and store in buff */
  vsnprintf(buff, SERIAL_PRINTF_MAX_BUFF, fmt, pargs);
  va_end(pargs);
  Serial.print(buff);
}
