#define _GNU_SOURCE

#include <errno.h>
#include <error.h>
#include <getopt.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/capability.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <unistd.h>
#include <wait.h>
#include <stdbool.h>

const char *RESOLV_CONF = "/etc/resolv.conf";
char *NEW_RESOLV_CONF = NULL;
bool DO_UNMOUNT = true;

void help() {
    char* h = "dns-overlay -f <resolv file> -e <env var> -c <command> -v"
                "\n\n-f file to overlay /etc/resolv.conf with (required)\n"
                "-c command to execute (defaults to bash)\n"
                "-e optional env var set in new process\n"
                "-v be verbose.\n";

    printf("%s\n", h);
}

void umount_rconf() {
    if(!DO_UNMOUNT)
        return;
    int r = 0;
    if((r = umount(RESOLV_CONF)) != 0)
        fprintf(stderr, "umount of %s failed: %d\n", RESOLV_CONF, r);
    if((r = umount(NEW_RESOLV_CONF)) != 0)
        fprintf(stderr, "umount of %s failed: %d\n", NEW_RESOLV_CONF, r);
}

int drop_caps(cap_t caps) {
    int r, rv = 0;
    if((r = cap_clear(caps)) != 0) {
        fprintf(stderr, "cap_clear failed: %d\n", r);
        rv++;
    }
    if((r = cap_set_proc(caps)) != 0) {
        fprintf(stderr, "cap_set_proc failed: %d\n", r);
        rv++;
    }
    if((r = cap_free(caps)) != 0) {
        fprintf(stderr, "cap_free failed: %d\n", r);
        rv++;
    }
    return rv;
}

int main(int argc, char *argv[]) {
    int verbose = 0;
    char *command = NULL;
    char *env_var = NULL;

    struct option long_options[] = {
        {"verbose", no_argument, 0, 'v'},
        {"env", required_argument, 0, 'e'},
        {"command", required_argument, NULL, 'c'},
        {"resolv-file", required_argument, NULL, 'f'},
        {NULL, 0, NULL, 0}
    };
    int c;
    int long_index = 0;
    while ((c = getopt_long (argc, argv, "c:f:e:vh", long_options, &long_index)) != -1) {
        switch (c) {
        case 'c':
            command = optarg;
            break;
        case 'f':
            NEW_RESOLV_CONF = optarg;
            break;
        case 'v':
            verbose = 1;
            break;
        case 'e':
            env_var = optarg;
            break;
        case 'h':
            help();
            exit(0);
            break;
        default:
            exit(1);
        }
    }

    if(command == NULL)
        command = "bash";

    if(NEW_RESOLV_CONF == NULL) {
        fprintf(stderr, "error: no source file specified\n");
        exit(1);
    }

    if(access(NEW_RESOLV_CONF, R_OK) != 0) {
        fprintf(stderr, "error: cannot read file '%s': %d\n", NEW_RESOLV_CONF, errno);
        exit(1);
    }

    int r;
    cap_t caps = cap_get_proc();
    cap_value_t capflag = CAP_SYS_ADMIN;
    r = cap_set_flag(caps, CAP_EFFECTIVE, 1, &capflag, CAP_SET);
    if(r != 0)
        error(r, errno, "unable to cap_set_flag");

    r = cap_set_proc(caps);
    if(r != 0)
        error(r, errno, "unable to cap_set_proc (does this executable have CAP_SYS_ADMIN set?)");

    /*
    This is needed to make the NEW_RESOLV_CONF "filesystem" private.
    */
    if((r = mount(NEW_RESOLV_CONF, NEW_RESOLV_CONF, "\0", MS_MGC_VAL|MS_BIND, "\0")) != 0)
        error(r, errno, "failed initial bind mount");

    int pid = fork();
    if(pid == 0) {
        if((r = mount("none", NEW_RESOLV_CONF, "\0", MS_PRIVATE, "\0")) != 0)
            error(r, errno, "failed to bind mount (private)");

        if((r = unshare(CLONE_NEWNS)) != 0)
            error(r, errno, "failed to unshare");

        if((r = mount("none", "/", "\0", MS_REC|MS_PRIVATE, "\0")) != 0)
            error(r, errno, "failed to mount / after unshare");

        if((r = mount(NEW_RESOLV_CONF, RESOLV_CONF, "\0", MS_BIND, "\0")) != 0)
            error(r, errno, "failed to mount new resolv.conf");

        pid = fork();
        if(pid == 0) {
            DO_UNMOUNT = false;
            if((r = drop_caps(caps)) != 0)
                exit(r);
            if(env_var != NULL)
                setenv(env_var, "1", 1);
            if(verbose)
                printf("calling: \"%s\"\n", command);
            exit(system(command));
        } else {
            atexit(umount_rconf);
            int status;
            waitpid(pid, &status, 0);
            cap_free(caps);
            exit(status);
        }
    } else {
        int status;
        waitpid(pid, &status, 0);
        int r;
        if((r = umount(NEW_RESOLV_CONF)) != 0)
            fprintf(stderr, "final umount failed: %d\n", r);
        cap_free(caps);
        exit(status);
    }
}
