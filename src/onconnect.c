#include <sys/signalfd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <poll.h>

/*****************************************************************************
 * This program monitors for new client connections, as well as signals.
 * When a client connects, this program accepts the connection and forks
 * off the process specified in the command line options to handle it.
 * Signals to this program are accepted using signalfd. 
 *
 * By default, the forked process will acquire the newly-accepted connection
 * as file descriptor 3
 *
 ****************************************************************************/

struct {
  in_addr_t addr;    /* local IP or INADDR_ANY   */
  int port;          /* local port to listen on  */
  int listener_fd;   /* listener descriptor      */
  int signal_fd;     /* used to receive signals  */
  int epoll_fd;      /* used for all notification*/
  int verbose;
  int ticks;         /* uptime in seconds        */
  int pid;           /* our own pid              */
  char *prog;
  char **subprocess_argv;
  int subprocess_argc;
} cfg = {
  .addr = INADDR_ANY, /* by default, listen on all local IP's   */
  .listener_fd = -1,
  .signal_fd = -1,
  .epoll_fd = -1,
};

void usage() {
  fprintf(stderr,"usage: %s [-v] [-a <ip>] -p <port>\n", cfg.prog);
  exit(-1);
}

/* do periodic work here */
void periodic() {
  if (cfg.verbose) fprintf(stderr,"up %d seconds\n", cfg.ticks);
}

int add_epoll(int events, int fd) {
  int rc;
  struct epoll_event ev;
  memset(&ev,0,sizeof(ev)); // placate valgrind
  ev.events = events;
  ev.data.fd= fd;
  if (cfg.verbose) fprintf(stderr,"adding fd %d to epoll\n", fd);
  rc = epoll_ctl(cfg.epoll_fd, EPOLL_CTL_ADD, fd, &ev);
  if (rc == -1) {
    fprintf(stderr,"epoll_ctl: %s\n", strerror(errno));
  }
  return rc;
}

int del_epoll(int fd) {
  int rc;
  struct epoll_event ev;
  rc = epoll_ctl(cfg.epoll_fd, EPOLL_CTL_DEL, fd, &ev);
  if (rc == -1) {
    fprintf(stderr,"epoll_ctl: %s\n", strerror(errno));
  }
  return rc;
}

/* signals that we'll accept synchronously via signalfd */
int sigs[] = {SIGIO,SIGHUP,SIGTERM,SIGINT,SIGQUIT,SIGALRM,SIGCHLD};

int setup_listener() {
  int rc = -1, one=1;

  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd == -1) {
    fprintf(stderr,"socket: %s\n", strerror(errno));
    goto done;
  }

  /**********************************************************
   * internet socket address structure: our address and port
   *********************************************************/
  struct sockaddr_in sin;
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = cfg.addr;
  sin.sin_port = htons(cfg.port);

  /**********************************************************
   * bind socket to address and port 
   *********************************************************/
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  if (bind(fd, (struct sockaddr*)&sin, sizeof(sin)) == -1) {
    fprintf(stderr,"bind: %s\n", strerror(errno));
    goto done;
  }

  /**********************************************************
   * put socket into listening state
   *********************************************************/
  if (listen(fd,1) == -1) {
    fprintf(stderr,"listen: %s\n", strerror(errno));
    goto done;
  }

  cfg.listener_fd = fd;
  rc=0;

 done:
  if ((rc < 0) && (fd != -1)) close(fd);
  return rc;
}

/* accept a new client connection to the listening socket */
int accept_client() {
  int fd, n, sc, rc = -1;
  pid_t pid;
  struct sockaddr_in in;
  socklen_t sz = sizeof(in);

  fd = accept(cfg.listener_fd,(struct sockaddr*)&in, &sz);
  if (fd == -1) {
    fprintf(stderr,"accept: %s\n", strerror(errno)); 
    goto done;
  }

  if (cfg.verbose && (sizeof(in)==sz)) {
    fprintf(stderr,"connection fd %d from %s:%d\n", fd,
    inet_ntoa(in.sin_addr), (int)ntohs(in.sin_port));
  }

  /* fork subprocess */
  pid = fork();

  if (pid == -1) {
    fprintf(stderr, "fork: %s\n", strerror(errno));
    goto done;
  }

  if (pid > 0) { /* parent */
    close(fd);
    fprintf(stderr, "forked pid %d\n", pid);
  } else {  /* child */
    assert(pid == 0);

    /* in the child, close the parent's descriptors except
     * for stdin, stdout, stderr and the new connection */
    close(cfg.listener_fd);
    close(cfg.signal_fd);
    close(cfg.epoll_fd);

    /* make the new connection fd 3, close original fd */
    if (fd != STDERR_FILENO+1) {      /* 3 */
      sc = dup2(fd, STDERR_FILENO+1);
      if (sc < 0) {
        fprintf(stderr, "dup2: %s\n", strerror(errno));
        goto done;
      }
      close(fd);
    }

    /* restore/unblock default signal handlers */
    for(n = 0; n < sizeof(sigs)/sizeof(*sigs); n++) {
      signal(sigs[n], SIG_DFL);
    }
    sigset_t none;
    sigemptyset(&none);
    sigprocmask(SIG_SETMASK, &none, NULL);

    sc = execv(cfg.subprocess_argv[0], cfg.subprocess_argv);
    if (sc == -1) {
      fprintf(stderr, "execv: %s\n", strerror(errno));
      goto done;
    }

    /* NOT REACHED */
    assert(0);
    goto done;

  }

  rc = 0;

 done:
  return rc;
}

int handle_signal() {
  struct signalfd_siginfo info;
  int es, rc = -1;
  pid_t pid;

  if (read(cfg.signal_fd, &info, sizeof(info)) != sizeof(info)) {
    fprintf(stderr,"failed to read signal fd buffer\n");
    goto done;
  }

  switch (info.ssi_signo) {
    case SIGALRM: 
      if ((++cfg.ticks % 10) == 0) periodic(); 
      alarm(1); 
      break;
    case SIGCHLD:  /* collect children to avoid leaving zombie processes */
      while ( (pid = waitpid(-1, &es, WNOHANG)) > 0) {
        if (WIFSIGNALED(es)) {
          fprintf(stderr, "pid %d exited: signal %d\n", pid, (int)WTERMSIG(es));
        }
        if (WIFEXITED(es)) {
          fprintf(stderr, "pid %d exited: status %d\n", pid, (int)WEXITSTATUS(es));
        }
      }
      break;
    default:  /* exit */
      /* TODO on termination, signal forked processes */
      fprintf(stderr,"got signal %d\n", info.ssi_signo);  
      goto done;
  }

  rc = 0;

 done:
  return rc;
}

int main(int argc, char *argv[]) {
  cfg.prog = argv[0];
  cfg.prog=argv[0];
  cfg.pid = getpid();
  int n, opt, sc;
  struct epoll_event ev;

  while ( (opt=getopt(argc,argv,"vp:a:h")) != -1) {
    switch(opt) {
      case 'v': cfg.verbose++; break;
      case 'p': cfg.port=atoi(optarg); break; 
      case 'a': cfg.addr=inet_addr(optarg); break; 
      case 'h': default: usage(); break;
    }
  }
  if (cfg.addr == INADDR_NONE) usage();
  if (cfg.port==0) usage();
  /* advance argv/argc to subprocess name and arguments */
  cfg.subprocess_argv = argv + optind;
  cfg.subprocess_argc = argc - optind;
  if (cfg.verbose) {
    for(n = 0; n < cfg.subprocess_argc; n++) {
      fprintf(stderr, "subprocess: argv[%d]: %s\n", n, cfg.subprocess_argv[n]);
    }
  }

  /* block all signals. we take signals synchronously via signalfd */
  sigset_t all;
  sigfillset(&all);
  sigprocmask(SIG_SETMASK,&all,NULL);

  /* a few signals we'll accept via our signalfd */
  sigset_t sw;
  sigemptyset(&sw);
  for(n=0; n < sizeof(sigs)/sizeof(*sigs); n++) sigaddset(&sw, sigs[n]);

  if (setup_listener()) goto done;

  /* create the signalfd for receiving signals */
  cfg.signal_fd = signalfd(-1, &sw, 0);
  if (cfg.signal_fd == -1) {
    fprintf(stderr,"signalfd: %s\n", strerror(errno));
    goto done;
  }

  /* set up the epoll instance */
  cfg.epoll_fd = epoll_create(1); 
  if (cfg.epoll_fd == -1) {
    fprintf(stderr,"epoll: %s\n", strerror(errno));
    goto done;
  }

  /* add descriptors of interest */
  if (add_epoll(EPOLLIN, cfg.listener_fd)) goto done;
  if (add_epoll(EPOLLIN, cfg.signal_fd)) goto done;

  /*
   * This is our main loop. epoll for input or signals.
   */
  alarm(1);
  while (epoll_wait(cfg.epoll_fd, &ev, 1, -1) > 0) {

    if (ev.data.fd == cfg.signal_fd) { 
      sc = handle_signal();
      if (sc < 0) goto done;
    }
    else if (ev.data.fd == cfg.listener_fd) { 
      sc = accept_client();
      if (sc < 0) goto done;
    }
    else {
      fprintf(stderr, "Unexpected file descriptor from epoll\n");
      goto done;
    }
  }

  fprintf(stderr, "epoll_wait: %s\n", strerror(errno));

 done:   /* we get here if we got a signal like Ctrl-C */
  if (cfg.listener_fd != -1) close(cfg.listener_fd);
  if (cfg.epoll_fd != -1) close(cfg.epoll_fd);
  if (cfg.signal_fd != -1) close(cfg.signal_fd);
  return 0;
}
