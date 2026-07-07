#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <xpc/xpc.h>
#include <spawn.h>
#include <sys/wait.h>

// AMFI XPC service name
#define AMFI_SERVICE_NAME "com.apple.amfi.xpc"

// Action constants (from amfi private headers)
#define kAMFIActionArm 4

// Function prototype for AMFIInjectAction
typedef void (*AMFIInjectActionFunc)(uint64_t action, uint64_t flags, uint64_t value);

static int call_amfi(int enable) {
    // Dynamically load the AMFI framework
    void *handle = dlopen("/System/Library/PrivateFrameworks/AMFI.framework/AMFI", RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "Failed to load AMFI.framework\n");
        return -1;
    }

    AMFIInjectActionFunc AMFIInjectAction = (AMFIInjectActionFunc)dlsym(handle, "AMFIInjectAction");
    if (!AMFIInjectAction) {
        fprintf(stderr, "Failed to find AMFIInjectAction\n");
        dlclose(handle);
        return -1;
    }

    // enable = 1 -> set value 0 (enabled), enable = 0 -> set value 1 (disabled)
    uint64_t value = enable ? 0 : 1;
    AMFIInjectAction(kAMFIActionArm, 0, value);

    dlclose(handle);
    return 0;
}

int enable_developer_mode(void) {
    return call_amfi(1);
}

int check_developer_mode(void) {
    // There's no direct API to read the current state, so we try to read from
    // a known file or use a heuristic. For simplicity, we'll check if
    // /var/mobile/Library/Preferences/com.apple.developer-mode.plist exists
    // with a certain key. But we can also attempt to enable and see if it fails.
    // Actually, we can use a different approach: run "amfidebug" if available,
    // but we want to be self-contained. Since this is a proof-of-concept,
    // we'll just return 0 (disabled) by default and let the user enable.
    // A better method would be to query AMFI via XPC, but that's more complex.
    // For simplicity, we'll check the output of the AMFI service via a private
    // method. Since that's too deep, we'll assume the user will use the app
    // to enable and then the state is remembered.
    // For now, we'll check if the file /var/run/developer_mode_enabled exists.
    // This is just a placeholder; real apps would use a proper method.
    if (access("/var/run/developer_mode_enabled", F_OK) == 0) {
        return 1;
    }
    return 0;
}

int respring_device(void) {
    pid_t pid;
    const char *argv[] = {"killall", "-9", "SpringBoard", NULL};
    int status = posix_spawn(&pid, "/usr/bin/killall", NULL, NULL, (char * const *)argv, NULL);
    if (status != 0) return -1;
    waitpid(pid, NULL, 0);
    return 0;
}

// Main function for command-line usage
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s [enable|status|respring]\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "enable") == 0) {
        int ret = enable_developer_mode();
        if (ret == 0) {
            // Mark that we enabled it (for status check)
            FILE *f = fopen("/var/run/developer_mode_enabled", "w");
            if (f) fclose(f);
        }
        return ret;
    } else if (strcmp(argv[1], "status") == 0) {
        int enabled = check_developer_mode();
        printf("%d\n", enabled);
        return 0;
    } else if (strcmp(argv[1], "respring") == 0) {
        return respring_device();
    } else {
        fprintf(stderr, "Unknown command: %s\n", argv[1]);
        return 1;
    }
}
