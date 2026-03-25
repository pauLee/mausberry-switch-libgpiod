/*
 * mausberry-switch
 *
 * GPIO monitoring daemon for use with Mausberry switches on the Raspberry Pi.
 * Uses libgpiod for GPIO access.
 */

#include <gpiod.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef SYSCONFDIR
#define SYSCONFDIR "/etc"
#endif

#define CONFIG_PATH SYSCONFDIR "/mausberry-switch.conf"
#define MAX_LINE 256

/* Default configuration */
#define DEFAULT_PIN_OUT 23
#define DEFAULT_PIN_IN 24
#define DEFAULT_DELAY 0
#define DEFAULT_SHUTDOWN_CMD "systemctl poweroff"
#define GPIO_CHIP "gpiochip0"

struct config {
  int pin_out;
  int pin_in;
  int delay;
  char shutdown_cmd[MAX_LINE];
};

static volatile sig_atomic_t running = 1;

static void handle_signal(int sig) {
  (void)sig;
  running = 0;
}

static int install_signal_handlers(void) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = handle_signal;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  if (sigaction(SIGINT, &sa, NULL) == -1) {
    perror("Failed to install SIGINT handler");
    return -1;
  }
  if (sigaction(SIGTERM, &sa, NULL) == -1) {
    perror("Failed to install SIGTERM handler");
    return -1;
  }
  return 0;
}

static void trim(char *s) {
  char *end;
  while (*s == ' ' || *s == '\t')
    memmove(s, s + 1, strlen(s));
  end = s + strlen(s) - 1;
  while (end > s &&
         (*end == '\n' || *end == '\r' || *end == ' ' || *end == '\t'))
    *end-- = '\0';
}

static void load_config(struct config *cfg) {
  FILE *fp;
  char line[MAX_LINE];
  char section[MAX_LINE] = "";

  /* Set defaults */
  cfg->pin_out = DEFAULT_PIN_OUT;
  cfg->pin_in = DEFAULT_PIN_IN;
  cfg->delay = DEFAULT_DELAY;
  snprintf(cfg->shutdown_cmd, MAX_LINE, "%s", DEFAULT_SHUTDOWN_CMD);

  fp = fopen(CONFIG_PATH, "r");
  if (!fp) {
    fprintf(stderr, "Config file '%s' not found, using defaults.\n",
            CONFIG_PATH);
    return;
  }

  while (fgets(line, sizeof(line), fp)) {
    char *p = line;
    trim(p);

    /* Skip empty lines and comments */
    if (*p == '\0' || *p == '#')
      continue;

    /* Section header */
    if (*p == '[') {
      char *end = strchr(p, ']');
      if (end) {
        *end = '\0';
        snprintf(section, MAX_LINE, "%s", p + 1);
      }
      continue;
    }

    /* Key=Value */
    char *eq = strchr(p, '=');
    if (!eq)
      continue;
    *eq = '\0';
    char *key = p;
    char *val = eq + 1;
    trim(key);
    trim(val);

    if (strcmp(section, "Pins") == 0) {
      if (strcmp(key, "Out") == 0)
        cfg->pin_out = atoi(val);
      else if (strcmp(key, "In") == 0)
        cfg->pin_in = atoi(val);
    } else if (strcmp(section, "Config") == 0) {
      if (strcmp(key, "ShutdownCommand") == 0)
        snprintf(cfg->shutdown_cmd, MAX_LINE, "%s", val);
      else if (strcmp(key, "Delay") == 0)
        cfg->delay = atoi(val);
    }
  }

  fclose(fp);
}

static void print_config(const struct config *cfg) {
  printf("\n== Mausberry Switch Configuration ==\n");
  printf("Command:  %s\n", cfg->shutdown_cmd);
  printf("Delay  :  %d\n", cfg->delay);
  printf("Pin OUT:  %d\n", cfg->pin_out);
  printf("Pin IN :  %d\n", cfg->pin_in);
  printf("== End Configuration ==\n\n");
}

int main(void) {
  struct gpiod_chip *chip = NULL;
  struct gpiod_line *input_line = NULL;
  struct gpiod_line *output_line = NULL;
  struct config cfg;
  int ret;
  int exit_code = 0;
  struct timespec ts = {1, 0}; /* 1 second timeout */

  /* Load configuration */
  load_config(&cfg);
  print_config(&cfg);

  /* Install signal handlers */
  if (install_signal_handlers() < 0)
    return 1;

  printf("Mausberry switch daemon starting\n");

  /* Open GPIO chip */
  chip = gpiod_chip_open_by_name(GPIO_CHIP);
  if (!chip) {
    perror("Failed to open GPIO chip");
    return 1;
  }

  /* Get the input and output lines */
  input_line = gpiod_chip_get_line(chip, cfg.pin_out);
  output_line = gpiod_chip_get_line(chip, cfg.pin_in);
  if (!input_line || !output_line) {
    perror("Failed to get GPIO lines");
    exit_code = 1;
    goto cleanup;
  }

  /* Configure the input line with edge detection */
  ret = gpiod_line_request_both_edges_events(input_line, "mausberry-switch");
  if (ret < 0) {
    perror("Failed to request line events");
    exit_code = 1;
    goto cleanup;
  }

  /* Configure the output line */
  ret = gpiod_line_request_output(output_line, "mausberry-switch", 0);
  if (ret < 0) {
    perror("Failed to request output line");
    exit_code = 1;
    goto cleanup;
  }

  /* Set output high to signal the switch that the system is running */
  gpiod_line_set_value(output_line, 1);

  printf("Mausberry switch monitoring started\n");

  while (running) {
    struct gpiod_line_event event;

    /* Wait for an event with timeout */
    ret = gpiod_line_event_wait(input_line, &ts);

    if (ret < 0) {
      if (!running)
        break;
      perror("Error waiting for event");
      exit_code = 1;
      break;
    } else if (ret == 0) {
      /* Timeout - loop and check running flag */
      continue;
    }

    /* Read the event */
    ret = gpiod_line_event_read(input_line, &event);
    if (ret < 0) {
      if (!running)
        break;
      perror("Failed to read line event");
      exit_code = 1;
      break;
    }

    /* Rising edge = switch activated */
    if (event.event_type == GPIOD_LINE_EVENT_RISING_EDGE) {
      printf("Switch activated, shutting down in %d seconds\n", cfg.delay);
      if (cfg.delay > 0)
        sleep((unsigned int)cfg.delay);

      if (!running)
        break;

      printf("Executing: %s\n", cfg.shutdown_cmd);
      if (system(cfg.shutdown_cmd) == -1) {
        perror("Failed to execute shutdown command");
        exit_code = 1;
      }
      break;
    }
  }

cleanup:
  if (input_line)
    gpiod_line_release(input_line);
  if (output_line)
    gpiod_line_release(output_line);
  if (chip)
    gpiod_chip_close(chip);

  printf("Mausberry switch daemon stopping\n");

  return exit_code;
}
