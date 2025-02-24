//
// Created by yunarta on 2/24/25.
//

#include <Arduino.h>
#include "aws_utils.h"

String StringPrintF(const char *format, ...) {
    if (!format) return String("");  // Handle null format safely

    va_list args;
    va_start(args, format);

    // Determine the required size of the output string
    va_list args_copy;
    va_copy(args_copy, args);
    int size = vsnprintf(nullptr, 0, format, args_copy);
    va_end(args_copy);

    if (size < 0) {
        va_end(args);
        return String("");  // Handle formatting error
    }

    // Allocate buffer dynamically with the required size (+1 for null terminator)
    char *buffer = new char[size + 1];
    vsnprintf(buffer, size + 1, format, args);
    va_end(args);

    String result(buffer);  // Create result string
    delete[] buffer;        // Free dynamically allocated memory

    return result;
}
