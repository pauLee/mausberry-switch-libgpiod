#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>

// Replace these with your configuration values
#define INPUT_PIN 23
#define OUTPUT_PIN 24
#define SHUTDOWN_DELAY 4

static volatile int running = 1;

void handle_signal(int sig) {
    running = 0;
}

int main(void) {
    struct gpiod_chip *chip;
    struct gpiod_line *input_line, *output_line;
    int ret;
    struct timespec ts = {1, 0}; // 1 second timeout

    // Set up signal handlers
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    printf("Mausberry switch daemon starting\n");
    
    // Open GPIO chip
    chip = gpiod_chip_open_by_name("gpiochip0");
    if (!chip) {
        perror("Failed to open GPIO chip");
        return 1;
    }

    // Get the input and output lines
    input_line = gpiod_chip_get_line(chip, INPUT_PIN);
    output_line = gpiod_chip_get_line(chip, OUTPUT_PIN);
    if (!input_line || !output_line) {
        perror("Failed to get GPIO lines");
        gpiod_chip_close(chip);
        return 1;
    }

    // Configure the input line with pull-up and falling edge detection
    ret = gpiod_line_request_both_edges_events(input_line, "mausberry-switch");
    if (ret < 0) {
        perror("Failed to request line events");
        gpiod_chip_close(chip);
        return 1;
    }

    // Configure the output line
    ret = gpiod_line_request_output(output_line, "mausberry-switch", 0);
    if (ret < 0) {
        perror("Failed to request output line");
        gpiod_line_release(input_line);
        gpiod_chip_close(chip);
        return 1;
    }

    // Set output high to enable the switch
    gpiod_line_set_value(output_line, 1);

    printf("Mausberry switch monitoring started\n");

    while (running) {
        struct gpiod_line_event event;
        
        // Wait for an event with timeout
        ret = gpiod_line_event_wait(input_line, &ts);
        
        if (ret < 0) {
            perror("Error waiting for event");
            break;
        } else if (ret == 0) {
            // Timeout - check if we should continue running
            continue;
        }
        
        // Get the event
        ret = gpiod_line_event_read(input_line, &event);
        if (ret < 0) {
            perror("Failed to read line event");
            break;
        }
        
        // Check if it's a rising edge (button pressed)
        if (event.event_type == GPIOD_LINE_EVENT_RISING_EDGE) {
            printf("Switch activated, shutting down in %d seconds\n", SHUTDOWN_DELAY);
            sleep(SHUTDOWN_DELAY);
            if (system("shutdown -h now") == -1) {
                perror("Failed to execute shutdown");
            }
            break;
        }
    }

    // Clean up
    gpiod_line_release(input_line);
    gpiod_line_release(output_line);
    gpiod_chip_close(chip);
    
    printf("Mausberry switch daemon stopping\n");

    return 0;
}
