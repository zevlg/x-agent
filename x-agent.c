/*
 * x-agent.c --- Simple session agent for Xorg.
 *
 * Copyright (C) 2005,2016 Zajcev Evgeny <zevlg@yandex.ru>
 *
 ** Author: Zajcev Evgeny <zevlg@yandex.ru>
 ** Created: Sun Feb 13 01:38:54 MSK 2005
 ** Keywords: Xorg
 *
 ** Comentary:
 *
 * x-agent is handy when developing window manager, or in situations
 * when using unstable window manager.  It allows restarting window
 * manager without restarting Xorg.
 *
 * When x-agent starts it install magic keys (C-Sh-F6 and C-Sh-F11)
 * by default and starts <WM>.  Whenever you want to restart
 * <WM> - press one of the magic keys.  You can also force
 * <WM> restart by killing x-agent with SIGHUP signal.
 *
 *   C-Sh-F6  - Kill <WM> with SIGABORT signal, this will cause
 *              <WM> to dump core for futher investigation.
 *   C-Sh-F9  - Kill <WM> with SIGKILL signal, this will cause
 *              <WM> to always exit.
 *   C-Sh-F11 - Kill <WM> with SIGTERM signal, this will cause
 *              <WM> to terminate normally.
 *   C-Sh-ESC - Exit x-agent.
 *
 ** Usage:
 *
 * Put x-agent in PATH
 *
 * Add into your ~/.xinitrc:
 *
 *     exec x-agent -f <log-file> <yourwm>
 * 
 */
#include <X11/X.h>
#include <X11/Xlib.h>
#define XK_MISCELLANY
#include <X11/keysymdef.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include <time.h>
#include <signal.h>
#include <fcntl.h>

#include <stdarg.h>

#include <sysexits.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

Display *xdpy;
pid_t wmpid = -1;                        /* <WM> pid */

enum {
        VERBOSE_NONE,
        VERBOSE_STDOUT,
        VERBOSE_X,
        VERBOSE_BOTH
};
int verbose = VERBOSE_BOTH;
int autodetect = 1;

char *wm;
char **wm_argv;

char *outfile = NULL;

enum {
        STATE_WAITING,
        STATE_RUNNING,
        STATE_KILLING
};
static int state = STATE_WAITING;      /* x-agent current state */

/*
 * Evil hackery do display verbose logs
 */
int
xverbose(const char *fmt, ...)
{
#define VSTRLEN 256
        static char vstr[VSTRLEN+1];
        int ret;
        va_list ap;

        va_start(ap, fmt);
        ret = vsnprintf(vstr, VSTRLEN, fmt, ap);
        va_end(ap);

        if (verbose == VERBOSE_STDOUT || verbose == VERBOSE_BOTH)
                printf("%s\n", vstr);

        if (verbose == VERBOSE_X || verbose == VERBOSE_BOTH) {
                static int y = 13;
                XDrawImageString(xdpy, RootWindow(xdpy, DefaultScreen(xdpy)),
                                 DefaultGC(xdpy, DefaultScreen(xdpy)),
                                 0, y, vstr, strlen(vstr));
                y += 13;
                if (y > 600) {
                        XClearWindow(xdpy, RootWindow(xdpy, DefaultScreen(xdpy)));
                        y = 13;
                }
                XFlush(xdpy);
        }

        return ret;
}

pid_t
start_wm()
{
        if (wmpid > 0) {
                /* Already running WM */
                xverbose("  - %s already running, not starting", wm);
                return -1;
        }

        if ((wmpid = fork()) == 0) {
                /* Direct <WM>'s stdout/stderr to file */
                if (outfile) {
                        int fd = open(outfile, O_CREAT|O_WRONLY|O_APPEND,
                                      S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
                        if (fd > 0) {
                                time_t clock = time(NULL);
                                char *ts = ctime(&clock);

                                write(fd, "------------[ ", 14);
                                write(fd, ts, strlen(ts));

                                close(STDOUT_FILENO);
                                close(STDERR_FILENO);
                                dup2(fd, STDOUT_FILENO);
                                dup2(fd, STDERR_FILENO);
                        }
                }

                /* Remove signal handlers */
                signal(SIGCHLD, SIG_DFL);
                signal(SIGHUP, SIG_DFL);

                execvp(wm, wm_argv);

                /* Execve failed :( */
                fprintf(stderr, "    - execve failed: %s\n", strerror(errno));
                exit(EX_UNAVAILABLE);
                /* NOT REACHED */
        }
        state = STATE_RUNNING;
        xverbose("  + Starting %s pid=%d ..", wm, wmpid);

        return wmpid;
}

void
restart_wm(int sig)
{
        xverbose("+ Restarting %s ..", wm);

        /* Kill wm */
        if (wmpid > 0) {
                xverbose("  + Killing %s pid=%d sig=%d ..", wm, wmpid, sig);

                state = STATE_KILLING;
                kill(wmpid, sig);
        } else {
                start_wm();
        }
}

void
wm_rip(int sig)
{
        pid_t pid;

        xverbose("+ SIGCHLD received ..");

        while ((pid = waitpid(wmpid, NULL, WNOHANG)) > 0) {
                xverbose("  + Riping %s pid=%d ..", wm, pid);
                if (pid == wmpid)
                        wmpid = -1;
        }

        XSetInputFocus(xdpy, PointerRoot, RevertToPointerRoot, CurrentTime);
        xverbose("  + InputFocus set to PointerRoot ..");

        if ((state == STATE_KILLING) || autodetect) {
                if (start_wm() < 0)
                        state = STATE_WAITING;
        } else
                state = STATE_WAITING;
}

void
wm_hup(int sig)
{
        xverbose("+ SIGHUP received ..");
        restart_wm(SIGTERM);
}

void
usage(const char *prog)
{
        printf("usage: %s [-on] [-f <outfile>] [-v X|stdout|both|none] "
               "-- <WM> [wm arguments]\n"
               "\t-o\tOmit starting <WM> on start.\n"
               "\t-n\tDo not autostart new <WM> when old exits.\n"
               "\t-f\tSpecify output file."
               " <WM> will direct its output to this file.\n"
               "\t-v\tEnable/disable verbosity type. Default is 'both'\n",
               prog);
        exit(EX_USAGE);
}

int
main(int argc, char **argv)
{
        const char *prog = argv[0];
        int o_flag = 0;
        int ch;
        KeyCode exit_kc, abort_kc, term_kc, kill_kc;

        while ((ch = getopt(argc, argv, "onv:f:")) != -1) {
                switch (ch) {
                case 'o':
                        o_flag = 1;
                        break;
                case 'v':
                        if (!strcmp(optarg, "X"))
                                verbose = VERBOSE_X;
                        else if (!strcmp(optarg, "stdout"))
                                verbose = VERBOSE_STDOUT;
                        else if (!strcmp(optarg, "both"))
                                verbose = VERBOSE_BOTH;
                        else if (!strcmp(optarg, "none"))
                                verbose = VERBOSE_NONE;
                        else
                                usage(prog);
                        break;
                case 'n':
                        autodetect = 0;
                        break;
                case 'f':
                        outfile = optarg;
                        break;
                case '?':
                default:
                        usage(prog);
                }
        }
        argc -= optind;
        argv += optind;
        
        if (argc < 1)
                usage(prog);

        /* Setup wm arguments */
        wm = argv[0];
        wm_argv = argv;

        xdpy = XOpenDisplay(NULL);
        if (xdpy == NULL) {
                fprintf(stderr, "- Can't open display\n");
                exit(EX_NOINPUT);
        }

        /* Install signal handler to rip childs */
        signal(SIGCHLD, wm_rip);
        signal(SIGHUP, wm_hup);

        xverbose("+ X Initialisation ..");

        XSetInputFocus(xdpy, RootWindow(xdpy, DefaultScreen(xdpy)),
                       RevertToPointerRoot, CurrentTime);
        xverbose("  + InputFocus set to root window ..");

        /* Grab magic key */
        exit_kc = XKeysymToKeycode(xdpy, XK_Escape);
        XGrabKey(xdpy, exit_kc, ShiftMask|ControlMask,
                 RootWindow(xdpy, DefaultScreen(xdpy)), True,
                 GrabModeAsync, GrabModeAsync);
        abort_kc = XKeysymToKeycode(xdpy, XK_F6);
        XGrabKey(xdpy, abort_kc, ShiftMask|ControlMask,
                 RootWindow(xdpy, DefaultScreen(xdpy)), True,
                 GrabModeAsync, GrabModeAsync);
        term_kc = XKeysymToKeycode(xdpy, XK_F11);
        XGrabKey(xdpy, term_kc, ShiftMask|ControlMask,
                 RootWindow(xdpy, DefaultScreen(xdpy)), True,
                 GrabModeAsync, GrabModeAsync);
        kill_kc = XKeysymToKeycode(xdpy, XK_F9);
        XGrabKey(xdpy, kill_kc, ShiftMask|ControlMask,
                 RootWindow(xdpy, DefaultScreen(xdpy)), True,
                 GrabModeAsync, GrabModeAsync);
        XFlush(xdpy);
        xverbose("  + Magic keys at C-Sh-ESC(exit), C-Sh-F6(SIGABORT),"
                 " C-Sh-F9(SIGKILL) and C-Sh-F11(SIGTERM) ..");

        state = STATE_WAITING;

        if (o_flag == 0)
                start_wm();

        /* Event loop */
        while (1) {
                XEvent xev;

                XNextEvent(xdpy, &xev);
                switch (xev.type) {
                case KeyPress:
                        xverbose("+ Magic KeyPress %s pid=%d ..", wm, wmpid);

			if (xev.xkey.keycode == exit_kc) {
				xverbose("  + Exiting ..");
				XCloseDisplay(xdpy);
				exit(EX_OK);
                        } else if (xev.xkey.keycode == abort_kc)
                                restart_wm(SIGABRT);
                        else if (xev.xkey.keycode == term_kc)
                                restart_wm(SIGTERM);
                        else if (xev.xkey.keycode == kill_kc)
                                restart_wm(SIGKILL);
                        else
                                xverbose("  - Unrecognized keycode 0x%X ..",
                                         xev.xkey.keycode);
                        break;
                }
        }

        return 0;
}
