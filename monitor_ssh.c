#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libssh/libssh.h>
#include <sys/select.h>
#include <time.h>

#define SSH_TIMEOUT 6

void print_usage(const char *prog_name) {
    printf("Usage: %s --server <remote_server> [health_monitor options]\n", prog_name);
}

int start_ssh_session(ssh_session *session, ssh_channel *channel, const char *server, const char *command) {
    int rc;

    *session = ssh_new();
    if (*session == NULL) {
        fprintf(stderr, "Error: Unable to allocate SSH session.\n");
        return -1;
    }

    ssh_options_set(*session, SSH_OPTIONS_HOST, server);

    rc = ssh_connect(*session);
    if (rc != SSH_OK) {
        fprintf(stderr, "Error: Unable to connect to server %s: %s\n", server, ssh_get_error(*session));
        ssh_free(*session);
        return -1;
    }

    // Authenticate the session
    rc = ssh_userauth_publickey_auto(*session, NULL, NULL);
    if (rc != SSH_AUTH_SUCCESS) {
        fprintf(stderr, "Error: Authentication failed: %s\n", ssh_get_error(*session));
        ssh_disconnect(*session);
        ssh_free(*session);
        return -1;
    }

    // Start the remote command
    *channel = ssh_channel_new(*session);
    if (*channel == NULL) {
        fprintf(stderr, "Error: Unable to create SSH channel.\n");
        ssh_disconnect(*session);
        ssh_free(*session);
        return -1;
    }

    rc = ssh_channel_open_session(*channel);
    if (rc != SSH_OK) {
        fprintf(stderr, "Error: Unable to open SSH session: %s\n", ssh_get_error(*session));
        ssh_channel_free(*channel);
        ssh_disconnect(*session);
        ssh_free(*session);
        return -1;
    }

    rc = ssh_channel_request_exec(*channel, command);
    if (rc != SSH_OK) {
        fprintf(stderr, "Error: Unable to execute command: %s\n", ssh_get_error(*session));
        ssh_channel_free(*channel);
        ssh_disconnect(*session);
        ssh_free(*session);
        return -1;
    }

    return 0;
}

void monitor_ssh_session(ssh_session session, ssh_channel channel) {
    fd_set fds;
    struct timeval timeout;
    char buffer[256];
    time_t last_activity = time(NULL);

    while (1) {
        FD_ZERO(&fds);
        FD_SET(ssh_get_fd(session), &fds);
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        if (select(ssh_get_fd(session) + 1, &fds, NULL, NULL, &timeout) > 0) {
            // Read from the SSH channel
            if (ssh_channel_is_open(channel) && !ssh_channel_is_eof(channel)) {
                int nbytes = ssh_channel_read(channel, buffer, sizeof(buffer), 0); // Read stdout
                if (nbytes > 0) {
                    last_activity = time(NULL);
                    fwrite(buffer, 1, nbytes, stdout);
                }

                nbytes = ssh_channel_read(channel, buffer, sizeof(buffer), 1); // Read stderr
                if (nbytes > 0) {
                    last_activity = time(NULL);
                    fwrite(buffer, 1, nbytes, stderr);
                }
            }
        }

        // Check timeout
        if (time(NULL) - last_activity > SSH_TIMEOUT) {
            fprintf(stderr, "No activity detected for %d seconds. Restarting session...\n", SSH_TIMEOUT);
            break;
        }

        // Check if channel has closed
        if (ssh_channel_is_closed(channel) || ssh_channel_is_eof(channel)) {
            fprintf(stderr, "SSH channel closed or reached EOF. Exiting monitoring.\n");
            break;
        }
    }
}

int main(int argc, char *argv[]) {
    const char *server = NULL;
    char command[1024] = "health_monitor"; // Default command to execute remotely
    ssh_session session = NULL;
    ssh_channel channel = NULL;

    // Parse command-line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--server") == 0 && i + 1 < argc) {
            server = argv[++i];
        } else {
            strcat(command, " ");
            strcat(command, argv[i]);
        }
    }

    if (server == NULL) {
        print_usage(argv[0]);
        return 1;
    }

    while (1) {
        fprintf(stderr, "Starting SSH session to %s...\n", server);

        if (start_ssh_session(&session, &channel, server, command) != 0) {
            fprintf(stderr, "Failed to start SSH session. Retrying in %d seconds...\n", SSH_TIMEOUT);
            sleep(SSH_TIMEOUT);
            continue;
        }

        fprintf(stderr, "Monitoring SSH session...\n");
        monitor_ssh_session(session, channel);

        // Cleanup after monitoring
        ssh_channel_close(channel);
        ssh_channel_free(channel);
        ssh_disconnect(session);
        ssh_free(session);

        fprintf(stderr, "Restarting SSH session...\n");
    }

    return 0;
}
