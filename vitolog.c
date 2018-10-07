/*--------------------------------------------------------------------*/
/* Date: 2018-10-05          Name: VitoLog      Author: Karsten Muuss */
/* compile: gcc -O2 -l pthread -o vitolog vitolog.c                   */
/*--------------------------------------------------------------------*/

#include <pthread.h>
#include <semaphore.h> 

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>

/*--------------------------------------------------------------------*/

static
struct {
  int  fd;
  char buff[1024];
} tt_dev[2] = {{ -1, ""}, {-1, ""}};


int tt_stop(int id) {
  tcdrain(tt_dev[id].fd);

  return EXIT_SUCCESS;
}


int tt_open(int id, char* name) {

  if ((tt_dev[id].fd = open(name, O_RDWR)) < 0) {
    fprintf(stderr, "can't open tty %s.\r\n", name);

    return EXIT_FAILURE;
  }

  fprintf(stderr, "tty%d: %s\n", id, name);

#if 1
  { struct termios term;
 
    tcgetattr(tt_dev[id].fd, &term);
 
    term.c_cflag = CLOCAL | CREAD | B4800 | PARENB | CS8 | CSTOPB;
    term.c_iflag = 0;
    term.c_oflag = 0;
    term.c_lflag = 0;
    term.c_cc[VMIN]  = 1;
    term.c_cc[VTIME] = 0;
 
    tcsetattr(tt_dev[id].fd, TCSANOW, &term);
  }
#endif

  tcflush(tt_dev[id].fd, TCIOFLUSH);
  tcdrain(tt_dev[id].fd);

  return EXIT_SUCCESS;
}

void tt_close(int id) {
  close(tt_dev[id].fd);
}

int channel = -1, mode = 1, alive = 1;
int count[2] = {0, 0};

static
pthread_t thread[2];

static
sem_t mutex;

static
void report(int id, char ch) {
  sem_wait(&mutex);
  count[id]++;

  if (id == channel) {
    printf(" %02X", ch);
  } else {
    printf("\n%d: %02X", id, ch);
  }

  fflush(stdout);

  channel = id;
  sem_post(&mutex); 
}

static
void listen0(void) {
  char ch;

  fprintf(stderr, "listen0 started\n");

  while (alive && read(tt_dev[0].fd, &ch, 1) == 1) {
    report(0, ch); 
    if (mode) write(tt_dev[1].fd, &ch, 1);
  }

  fprintf(stderr, "listen0 terminated\n");
}

static
void listen1(void) {
  char ch;

  fprintf(stderr, "listen1 started\n");

  while (alive && read(tt_dev[1].fd, &ch, 1) == 1) {
    report(1, ch);
    if (mode) write(tt_dev[0].fd, &ch, 1);
  }

  fprintf(stderr, "listen1 terminated\n");
}

static
void cmdline(void) {
  char *p, buff[256];
  int  n;

  while (alive && 0 < (n = read(0, buff, 255))) {
    buff[n] = 0; 

    if (strncmp(buff, "stat", 4) == 0) {
      fprintf(stderr, "status: %d, %6d %6d bytes\n", mode, count[0], count[1]);
    } else if (strncmp(buff, "on", 2) == 0) {
      mode = 1;
    } else if (strncmp(buff, "off", 3) == 0) {
      mode = 0;
    } else if (strncmp(buff, "quit", 4) == 0) {
      fprintf(stderr, "quit:   %6d %6d bytes\n", count[0], count[1]);
      alive = 0;
    }
  }

  pthread_cancel(thread[0]);
  pthread_cancel(thread[1]);

  fprintf(stderr, "session terminated\n");
}


int main(int argc, char* argv[]) {

  fprintf(stdout, "\n#-- vitolog --\n"); fflush(stdout);

  tt_open(0, (argc > 1) ? argv[1] : "/dev/ttyAMA0");
  tt_open(1, (argc > 2) ? argv[2] : "/dev/ttyUSB0");

  sem_init(&mutex, 0, 1);
  pthread_create(&thread[0], NULL, (void*(*)(void*))listen0, NULL);
  pthread_create(&thread[1], NULL, (void*(*)(void*))listen1, NULL);

  cmdline();

  pthread_join(thread[0], NULL);
  pthread_join(thread[1], NULL);
  sem_destroy(&mutex);

  tt_close(1);
  tt_close(0);

  return 0;
}

/*--------------------------------------------------------------------*/
