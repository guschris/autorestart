#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <curl/curl.h>

#define DEFAULT_RETRY_COUNT 3
#define HEALTH_CHECK_INTERVAL 5

// Perform the health check using libcurl
int perform_health_check(const char *url) {
    CURL *curl;
    CURLcode res;
    long response_code = 0;

    curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Failed to initialize CURL.\n");
        return 0;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L); // Only check the response header
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L); // Timeout for health check

    res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    }

    curl_easy_cleanup(curl);

    return (res == CURLE_OK && response_code >= 200 && response_code < 300);
}

// Print usage instructions
void print_usage(const char *prog_name) {
    printf("Usage: %s [--health <url>] [--retry <count>] -- <child_process> [args...]\n", prog_name);
}

int main(int argc, char *argv[]) {
    const char *health_endpoint = NULL; // NULL means no health check
    int max_retries = DEFAULT_RETRY_COUNT;
    int child_arg_start = 0;
    const char *program_name = argv[0];

    // Parse command-line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--health") == 0 && i + 1 < argc) {
            health_endpoint = argv[++i];
        } else if (strcmp(argv[i], "--retry") == 0 && i + 1 < argc) {
            max_retries = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--") == 0) {
            child_arg_start = i + 1;
            break;
        }
    }

    if (child_arg_start == 0 || child_arg_start >= argc) {
        print_usage(program_name);
        return 1;
    }

    int retries = 0;
    pid_t pid;
    int status;

    while (retries <= max_retries) {
        if (retries > 0) {
            fprintf(stderr, "%s: restarting in %d s...\n", program_name, retries);
            sleep(retries);
        }

        pid = fork();        
        if (pid == 0) { // Child process
            execvp(argv[child_arg_start], &argv[child_arg_start]);
            perror("execvp failed");
            exit(1);
        } else if (pid < 0) { // Fork failed
            perror("fork failed");
            return 1;
        }

        // Parent process

        // if health check is enabled, perform health check in a loop
        while (health_endpoint) {
            sleep(HEALTH_CHECK_INTERVAL);

            // Perform health check
            if (perform_health_check(health_endpoint)) {
                fprintf(stderr, "%s: Health check good.\n", program_name);
                continue;
            } else {
                fprintf(stderr, "%s: Health check FAILED.\n", program_name);
                kill(pid, SIGKILL);
                break;
            }
        }

        // Wait until the child process has terminated
        if (waitpid(pid, &status, 0) >= 0) {
            if (WIFEXITED(status)) {
                int exit_code = WEXITSTATUS(status);
                fprintf(stderr, "%s: Child process exited with code %d\n", program_name, exit_code);

                // Restart only if the child exited with a non-zero code
                if (exit_code == 0) {
                    return 0;
                }
            } else if (WIFSIGNALED(status)) {
                fprintf(stderr, "%s: Child process terminated by signal %d\n", program_name, WTERMSIG(status));
            }
        }

        retries++;
    }

    fprintf(stderr, "%s: Exceeded maximum retries (%d).\n", program_name, max_retries);
    return 1;
}
