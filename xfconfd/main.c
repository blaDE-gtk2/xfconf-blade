/*
 *  xfconf
 *
 *  Copyright (c) 2007 Brian Tarricone <bjt23@cornell.edu>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License ONLY.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <libxfce4util/libxfce4util.h>

#include "xfconf-daemon.h"

#define DEFAULT_BACKEND  "xfce-perchannel-xml"

enum
{
    SIGNAL_NONE = 0,
    SIGNAL_RESTART,
    SIGNAL_QUIT,
};

static int signal_pipe[2] = { -1, -1 };

static void
sighandler(int sig)
{
    guint32 sigstate;
    
    switch(sig) {
        case SIGUSR1:
            sigstate = SIGNAL_RESTART;
            break;
        
        default:
            sigstate = SIGNAL_QUIT;
            break;
    }
    
    write(signal_pipe[1], &sigstate, sizeof(sigstate));
}

static gboolean
signal_pipe_io(GIOChannel *source,
               GIOCondition condition,
               gpointer data)
{
    guint32 sigstate = 0;
    gsize bread = 0;
    
    if(G_IO_ERROR_NONE == g_io_channel_read(source, (gchar *)&sigstate,
                                            sizeof(sigstate), &bread)
       && sizeof(sigstate) == bread)
    {
        switch(sigstate)
        {
            case SIGNAL_RESTART:
                /* FIXME: implement */
                break;
            
            case SIGNAL_QUIT:
                g_main_loop_quit((GMainLoop *)data);
                break;
            
            default:
                break;
        }
        
    }
    
    return TRUE;
}

int
main(int argc,
     char **argv)
{
    GMainLoop *mloop;
    XfconfDaemon *xfconfd;
    GError *error = NULL;
    struct sigaction act;
    GIOChannel *signal_io;
    guint signal_watch = 0;
    
    memset(&act, 0, sizeof(act));
    act.sa_handler = sighandler;
    act.sa_flags = SA_RESTART;
    
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGHUP, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
    sigaction(SIGQUIT, &act, NULL);
    sigaction(SIGUSR1, &act, NULL);
    
    //act.sa_handler = SIG_IGN;
    //sigaction(SIGPIPE, &act, NULL);
    
    xfce_textdomain(PACKAGE, LOCALEDIR, "UTF-8");
    
    g_set_application_name("Xfce Configuration Daemon");
    g_set_prgname("xfconfd");
    g_type_init();
    
    mloop = g_main_loop_new(NULL, FALSE);
    
    if(pipe(signal_pipe))
        g_warning("Unable to create signal-watch pipe: %s.  Signals will be ignored.", strerror(errno));
    else {
        /* set writing end to non-blocking */
        int oldflags = fcntl(signal_pipe[1], F_GETFL);
        
        if(fcntl(signal_pipe[1], F_SETFL, oldflags | O_NONBLOCK)) {
            g_warning("Unable to set signal-watch pipe to non-blocking mode: %s.  Signals will be ignored.", strerror(errno));
            close(signal_pipe[0]);
            close(signal_pipe[1]);
        } else {
            signal_io = g_io_channel_unix_new(signal_pipe[0]);
            g_io_channel_set_encoding(signal_io, NULL, NULL);
            g_io_channel_set_close_on_unref(signal_io, FALSE);
            signal_watch = g_io_add_watch(signal_io, G_IO_IN | G_IO_PRI,
                                          signal_pipe_io, mloop);
            g_io_channel_unref(signal_io);
        }
    }
    
    xfconfd = xfconf_daemon_new_unique(DEFAULT_BACKEND, &error);
    if(!xfconfd) {
        g_printerr("Xfconfd failed to start: %s\n", error->message);
        return 1;
    }
    
    g_main_loop_run(mloop);
    
    g_object_unref(G_OBJECT(xfconfd));
    
    if(signal_watch) {
        g_source_remove(signal_watch);
        close(signal_pipe[0]);
        close(signal_pipe[1]);
    }
    
    g_main_loop_unref(mloop);
    
    return 0;
}
