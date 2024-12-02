#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <curl/curl.h>
#include <errno.h>
#include <time.h>

#define DEFAULT_RETRIES 3
#define DEFAULT_HEALTH_URL "http://localhost/up"

typedef struct {
    char *health_url;
    int retries;
    int timeout;
    char **child_args;
} Config;

void print_usage(void) {
    fprintf(stderr, "Usage: autorestart [--health <url>] [--retry <count>] [--timeout <duration>] -- <command> [args...]\n");
    fprintf(stderr, "Duration format: <number>[s|min|h] (e.g., 20s, 10min, 5h). Default unit is seconds.\n");
    exit(1);
}

int parse_duration(const char *duration_str) {
    int len = strlen(duration_str);
    if (len == 0) return -1;

    char unit = duration_str[len - 1];
    int multiplier = 1;  // Default to seconds

    if (unit == 's') {
        multiplier = 1;  // Seconds
        len--;           // Exclude the 's'
    } else if (len > 3 && strncmp(&duration_str[len - 3], "min", 3) == 0) {
        multiplier = 60;  // Minutes
        len -= 3;         // Exclude "min"
    } else if (unit == 'h') {
        multiplier = 3600;  // Hours
        len--;              // Exclude the 'h'
    }

    char number_part[len + 1];
    strncpy(number_part, duration_str, len);
    number_part[len] = '\0';

    int value = atoi(number_part);
    return value > 0 ? value * multiplier : -1;
}

void parse_arguments(int argc, char *argv[], Config *config) {
    config->health_url = NULL;
    config->retries = DEFAULT_RETRIES;
    config->timeout = -1;  // Disabled by default
    config->child_args = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--health") == 0) {
            if (++i >= argc) print_usage();
            config->health_url = argv[i];
        } else if (strcmp(argv[i], "--retry") == 0) {
            if (++i >= argc) print_usage();
            config->retries = atoi(argv[i]);
        } else if (strcmp(argv[i], "--timeout") == 0) {
            if (++i >= argc) print_usage();
            config->timeout = parse_duration(argv[i]);
            if (config->timeout < 0) {
                fprintf(stderr, "Invalid timeout format: %s\n", argv[i]);
                print_usage();
            }
        } else if (strcmp(argv[i], "--") == 0) {
            config->child_args = &argv[i + 1];
            break;
        } else {
            print_usage();
        }
    }

    if (config->child_args == NULL) {
        print_usage();
    }
}

int health_check(const char *url) {
    if (!url) return 1;  // Skip health check if no URL is provided
    CURL *curl = curl_easy_init();
    if (!curl) return 0;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);  // 5-second timeout

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_easy_cleanup(curl);

    return (res == CURLE_OK && http_code == 200);
}

void terminate_process(pid_t pid) {
    // First, attempt a graceful termination
    kill(pid, SIGTERM);
    sleep(1);  // Allow the process to terminate

    // Force kill if still running
    if (kill(pid, 0) == 0) {
        kill(pid, SIGKILL);
    }

    // Reap the child process to avoid zombie state
    int status;
    waitpid(pid, &status, 0);
}


int monitor_child_process(pid_t pid, int timeout, const char *health_url) {
    time_t start_time = time(NULL);

    while (1) {
        int status;
        pid_t result = waitpid(pid, &status, WNOHANG);

        if (result == pid) {
            // Process has exited
            if (WIFEXITED(status)) {
                return WEXITSTATUS(status);  // Child exited normally
            } else if (WIFSIGNALED(status)) {
                fprintf(stderr, "Child process terminated by signal %d\n", WTERMSIG(status));
                return -1;  // Termination due to signal
            }
        } else if (result == -1) {
            // Error in waitpid
            perror("waitpid");
            return -1;
        }

        // Check if timeout has been exceeded
        if (timeout > 0 && difftime(time(NULL), start_time) > timeout) {
            fprintf(stderr, "Child process exceeded timeout of %d seconds.\n", timeout);
            return -1;  // Indicate timeout
        }

        // Perform health check if enabled
        if (health_url && !health_check(health_url)) {
            fprintf(stderr, "Health check failed for URL: %s\n", health_url);
            return -1;  // Indicate health check failure
        }

        // Sleep for a short period before next iteration
        sleep(1);
    }
}

int main(int argc, char *argv[]) {
    Config config;
    parse_arguments(argc, argv, &config);

    int retries_left = config.retries;

    while (retries_left > 0) {
        fprintf(stderr, "Starting child process...\n");
        pid_t pid = fork();

        if (pid == 0) {
            // Child process
            execvp(config.child_args[0], config.child_args);
            perror("execvp failed");
            exit(1);
        }

        // Parent process: monitor the child process
        int result = monitor_child_process(pid, config.timeout, config.health_url);

        // If child exited successfully, stop retrying
        if (result == 0) {
            fprintf(stderr, "Child process exited successfully.\n");
            return 0;
        }

        // Restart the process if necessary
        terminate_process(pid);
        fprintf(stderr, "Restarting child process (retries left: %d)...\n", --retries_left);
    }

    fprintf(stderr, "Child process exceeded retry limit (%d retries). Exiting...\n", config.retries);
    return 1;
}
