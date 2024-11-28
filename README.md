# autorestart

`autorestart` is a simple Linux command line tool that:
* starts a child process, redirecting `stdout` and `stderr`
* restarts the child if it exits with non-zero exit code
* optional checks the child is responding by calling a HTTP health endpoint every 5 seconds and writing the results to `stderr`.  If the healthcheck fails then the child process is killed and restarted.

## Building

1. Install `libcurl-dev`, e.g. `sudo apt install libcurl3-gnutls-dev` on Debian
2. run `./build`

The script will build a 17K executable called `autorestart`.

## Usage

```
Usage: ./autorestart [--health <url>] [--retry <count>] -- <child_process> [args...]
```

Where:
* `--health` is the HTTP endpoint to check, defaults to `http://localhost/up`
* `--retry` is the maximum numer of times to retry the fail process, defaults to `3`
* all arguments after `--` is the command to run

If the process fails more than the maximum number of allowed retries than and exit code of `1` is returned.

