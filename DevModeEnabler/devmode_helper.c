#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <xpc/xpc.h>
#include <spawn.h>
#include <sys/wait.h>

#define AMFI_SERVICE_NAME "com.apple.amfi.xpc"
#define kAMFIActionArm 4

typedef void (*AMFIInjectActionFunc)(uint64_t action, uint64_t flags, uint64_t value);

static int call_amfi(int enable) {
    void *handle = dlopen("/System/Library/PrivateFrameworks/AMFI.framework/AMFI", RTLD_LAZY);
    if (!handle) return -1;
    AMFIInjectActionFunc AMFIInjectAction = (AMFIInjectActionFunc)dlsym(handle, "AMFIInjectAction");
    if (!AMFIInjectAction) { dlclose(handle); return -1; }
    uint64_t value = enable ? 0 : 1;
    AMFIInjectAction(kAMFIActionArm, 0, value);
    dlclose(handle);
    return 0;
}

int enable_developer_mode(void) {
    int ret = call_amfi(1);
    if (ret == 0) {
        FILE *f = fopen("/var/run/developer_mode_enabled", "w");
        if (f) fclose(f);
    }
    return ret;
}

int check_developer_mode(void) {
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

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s [enable|status|respring]\n", argv[0]);
        return 1;
    }
    if (strcmp(argv[1], "enable") == 0) {
        return enable_developer_mode();
    } else if (strcmp(argv[1], "status") == 0) {
        printf("%d\n", check_developer_mode());
        return 0;
    } else if (strcmp(argv[1], "respring") == 0) {
        return respring_device();
    } else {
        return 1;
    }
}
