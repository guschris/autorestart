# autorestart

`autorestart` is a utility program designed to manage and monitor the execution of a child process. It provides mechanisms to restart the child process if it fails, exceeds a specified runtime, or fails a periodic health check. This tool is useful for maintaining long-running processes with built-in fault tolerance.

## Features

- Automatically restart a child process if it exits with a non-zero error code.
- Optionally perform periodic health checks against a configurable HTTP endpoint.
- Restart the child process if it exceeds a specified timeout.
- Configurable retry limits to control how many times the process can be restarted.
- Outputs the child process's `stdout` and `stderr` to the parent process.

## Building

1. Install `libcurl-dev`, e.g. `sudo apt install libcurl3-gnutls-dev` on Debian
2. run `./build`

The script will build a 17K executable called `autorestart`.

## Command-Line Arguments

### General Syntax

```bash
autorestart [--health <url>] [--retry <count>] [--timeout <duration>] -- <command> [args...]
```

### Arguments
| Argument	| Description |
|-----------|-------------|
| --health <url>	| Optional. A URL to an HTTP health check endpoint. The program will periodically send HEAD requests to this URL while the child process is running. If the health check fails, the child process is restarted. Example: --health http://localhost/up. |
| --retry <count>	| Optional. The maximum number of times the child process will be restarted if it fails (exits with a non-zero code), exceeds the timeout, or fails the health check. Defaults to 3. Example: --retry 5. |
|--timeout <duration>	| Optional. The maximum runtime for the child process. If the child process runs longer than the specified duration, it is terminated and restarted. Durations can be specified in: <br> - Seconds: `20s`<br> - Minutes: `10min`<br> - Hours: `5h`<br>If no unit is provided, seconds are assumed. Example: `--timeout 1h`. |
| --	| Required. Indicates the end of autorestart options and the start of the command to run as the child process. |
| command	| The command and its arguments that autorestart will execute and monitor. |

## Examples

### Basic Usage
Run a process without health checks or timeout:

```bash
autorestart -- ./my_program arg1 arg2
```
### With Health Check
Run a process with periodic health checks:

```bash
autorestart --health http://localhost/up -- ./my_program arg1 arg2
```
### With Timeout
Run a process that will be terminated and restarted if it exceeds 10 minutes:

```bash
autorestart --timeout 10min -- ./my_program arg1 arg2
```

### With Retry Limit
Run a process that can be restarted up to 5 times upon failure:

```bash
autorestart --retry 5 -- ./my_program arg1 arg2
```

### Combining Options
Run a process with all features enabled:

```bash
autorestart --health http://localhost/up --retry 3 --timeout 1h -- ./my_program arg1 arg2
```

## Exit Codes
| Exit Code	| Description |
|-----------|-------------|
| 0	| The child process exited successfully, and no retries were needed. |
| 1	| The child process exceeded the retry limit, failed health checks, or was terminated due to timeout. |

### Notes
 - If the `--health` option is not provided, health checks are disabled.
 - If the `--timeout` option is not provided, no timeout monitoring is performed.
 - If the `--retry` option is not provided, the default retry limit is `3`.