#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <signal.h>
#include <time.h>

#include "bluetooth.h"
#include "hci.h"
#include "hci_lib.h"

static int numSamples = -1;
static int maxTime = -1;
static char debug = 0;

static char *filename = NULL;
static volatile int signal_received = 0;

static unsigned long* timeTemperature;
static unsigned long* timeHumidity;
static unsigned long* timeBattery;
static int* temperature;
static int* humidity;
static int* battery;
static int* ntSamples = 0;
static int* nhSamples = 0;
static int nDevs;
static bdaddr_t *devsBtAddr;

static void sigint_handler(int sig)
{
  signal_received = sig;
}

static int getVal16(uint8_t* data) {
  int t = (data[1]&0xff)<<8;
  t += (data[0]&0xff);
  return t;
}

static void update_data(int devIndex, uint8_t* data, uint8_t length) {
  //printf("ZZZ: update_data: %02X,", length);
  //for (int i = 0; i < length; ++i) {
  //  printf(" %02X", data[i]);
  //}
  //printf("\n");
  if (length!=0x16 && length!=0x17 && length!=0x19)
    return;
  if (data[4]!=0x16 || data[5]!=0x95 || data[6]!=0xFE)
    return;

  time_t currentTime = time(NULL);

  bool has_temperature = false;
  bool has_humidity = false;
  bool has_battery = false;
  float temperature_celsius = 0.0f;
  float relative_humidity = 0.0f;
  int battery_percentage = 0;
  switch (data[18]) {
    case 0x0D:
      temperature_celsius = static_cast<float>(getVal16(&data[21])) * 0.1f;
      relative_humidity = static_cast<float>(getVal16(&data[23])) * 0.1f;
      temperature[devIndex] += getVal16(&data[21]);
      ntSamples[devIndex]++;
      humidity[devIndex] += getVal16(&data[23]);
      nhSamples[devIndex]++;
      timeTemperature[devIndex] = currentTime;
      timeHumidity[devIndex] = currentTime;
      has_temperature = true;
      has_humidity = true;
      break;
    case 0x0A:
      battery_percentage = data[21];
      battery[devIndex] = data[21]&0xff;
      timeBattery[devIndex] = currentTime;
      has_battery = true;
      break;
    case 0x04:
      temperature_celsius = static_cast<float>(getVal16(&data[21])) * 0.1f;
      temperature[devIndex] += getVal16(&data[21]);
      ntSamples[devIndex]++;
      timeTemperature[devIndex] = currentTime;
      has_temperature = true;
      break;
    case 0x06:
      relative_humidity = static_cast<float>(getVal16(&data[21])) * 0.1f;
      humidity[devIndex] += getVal16(&data[21]);
      nhSamples[devIndex]++;
      timeHumidity[devIndex] = currentTime;
      has_humidity = true;
      break;
  }

  if (has_temperature) {
    printf("T %lu %.1f\n", currentTime, temperature_celsius);
  }
  if (has_humidity) {
    printf("H %lu %.1f\n", currentTime, relative_humidity);
  }
  if (has_battery) {
    printf("B %lu %d\n", currentTime, battery_percentage);
  }
}

static int print_advertising_devices(int dd, uint8_t filter_type)
{
  unsigned char buf[HCI_MAX_EVENT_SIZE], *ptr;
  struct hci_filter nf, of;
  struct sigaction sa;
  socklen_t olen;
  int len = 0;
  int i;

  //printf("ZZZ: HCI_MAX_EVENT_SIZE: %d\n", HCI_MAX_EVENT_SIZE);

  olen = sizeof(of);
  if (getsockopt(dd, SOL_HCI, HCI_FILTER, &of, &olen) < 0) {
    printf("Could not get socket options\n");
    return -1;
  }

  hci_filter_clear(&nf);
  hci_filter_set_ptype(HCI_EVENT_PKT, &nf);
  hci_filter_set_event(EVT_LE_META_EVENT, &nf);

  if (setsockopt(dd, SOL_HCI, HCI_FILTER, &nf, sizeof(nf)) < 0) {
    printf("Could not set socket options\n");
    return -1;
  }

  memset(&sa, 0, sizeof(sa));
  sa.sa_flags = SA_NOCLDSTOP;
  sa.sa_handler = sigint_handler;
  sigaction(SIGINT, &sa, NULL);
  time_t startTime = time(NULL);

  /*

  typedef struct {
    uint8_t b[6];
  } __attribute__((packed)) bdaddr_t;

  typedef struct {
    uint8_t         subevent;
    uint8_t         data[0];
  } __attribute__ ((packed)) evt_le_meta_event;

  typedef struct {
    uint8_t         evt_type;
    uint8_t         bdaddr_type;
    bdaddr_t        bdaddr;
    uint8_t         length;
    uint8_t         data[0];
  } __attribute__ ((packed)) le_advertising_info;
  */
  evt_le_meta_event *meta;
  le_advertising_info *info;
  fd_set set;
  struct timeval timeout;
  int rv = 1;
  char addr[18];
  while (rv > 0) {
    len = 0;
    FD_ZERO(&set); /* clear the set */
    FD_SET(dd, &set); /* add our file descriptor to the set */
    if (maxTime > 0) {
      timeout.tv_sec = maxTime - (time(NULL) - startTime);
      timeout.tv_usec = 0;
      if (timeout.tv_sec <= 0)
        goto done;
      if (debug) {
        printf("ZZZ: select() ...\n");
      }
      rv = select(dd + 1, &set, NULL, NULL, &timeout);
      if (debug) {
        printf("ZZZ: select(): rv: %d\n", rv);
      }
    } else {
      rv = select(dd + 1, &set, NULL, NULL, NULL);
    }

    if(rv == -1) { /* an error accured */
      perror("select");
      len = -1;
      goto done;
    } else if (rv == 0) { /* a timeout occured */
      goto done;
    }
    len = read(dd, buf, sizeof(buf));
    if (debug) {
      printf("ZZZ: read(): %d\n", len);
    }

    ptr = buf + (1 + HCI_EVENT_HDR_SIZE);
    len -= (1 + HCI_EVENT_HDR_SIZE);

    meta = reinterpret_cast<evt_le_meta_event*>(ptr);

    if (meta->subevent != 0x02)
      goto done;

    /* Ignoring multiple reports */
    info = (le_advertising_info *) (meta->data + 1);

    if (debug) {
      ba2str(&info->bdaddr,addr);
      printf("%s - ",addr);
      for (i=0; i<info->length; i++)
        printf("%02X ", (unsigned int)(info->data[i]& 0xFF));
      printf("\n");
    }

    for (i=0; i<nDevs; i++)
      if (memcmp(info->bdaddr.b, devsBtAddr[i].b, sizeof(bdaddr_t)) == 0) {
        update_data(i, info->data, info->length);
        break; 
      }

    /* check if enough samples or timeout */
    if ((maxTime > 0) && (time(NULL) - startTime) > maxTime)
      goto done;
    int exit = 1;
    if (numSamples == -1) {
      exit = 0;
    } else {
      for (i=0; i<nDevs; i++) {
        if (ntSamples[i] < numSamples || nhSamples[i] < numSamples)
          exit = 0;
      }
    }
    if (exit == 1)
      goto done;
  }

done:
  setsockopt(dd, SOL_HCI, HCI_FILTER, &of, sizeof(of));

  if (len < 0)
    return -1;

  return 0;
}


static void usage(void)
{
  int i;

  printf("scanMijia - ver 1.0\n");
  printf("Usage:\n"
    "\tscanMijia [options] BT adress list\n");
  printf("Options:\n"
    "\t-h\t\tDisplay help\n"
    "\t-i dev\t\tHCI device\n"
    "\t-d\t\tPrint raw data to stdout (debug)\n"
    "\t-f filename\tPrint output values to the file (default stdout)\n"
    "\t-t timeout\tStop after timout seconds (default no timeout)\n"
    "\t-n num\t\tOutput the average and stops after num samples\n"
    "\t\t\t(default 1)\n");
  printf("Output:\n"
    "\tOne line for each value. The format is:\n"
    "\t<TAG> <BT Address> <Timestamp> <Value>\n"
    "\t<TAG>: one of 'T'(temperature) 'H'(humidity) 'B'(battery)\n"
    "\t<Timestamp>: Unix timestamp (seconds since January 1, 1970)\n"
    "\t<Value>: float for 'T' and 'H', integer for 'B' \n");
    
    
}

void outputValues() {
  int i;
  char addr[18];
  int fd;
  FILE* fp;
  
  if (filename != NULL) {
    umask(0);
    fd = open(filename,O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
    while (flock(fd, LOCK_EX) != 0)
      sleep(1);
    fp = fdopen(fd, "w");
  } else {
    fp = stdout;
  }
  for (i=0;i<nDevs;i++) {
    if (temperature[i] != 0) {
      ba2str(&devsBtAddr[i],addr);
      fprintf(fp, "T %s %lu %.1f\n", addr, timeTemperature[i], (float)temperature[i]/(float)ntSamples[i]/10.f);
    }
    if (humidity[i] != 0) {
      ba2str(&devsBtAddr[i],addr);
      fprintf(fp, "H %s %lu %.1f\n", addr, timeHumidity[i], (float)humidity[i]/(float)nhSamples[i]/10.f);
    }
    if (battery[i] != 0) {
      ba2str(&devsBtAddr[i],addr);
      fprintf(fp, "B %s %lu %d\n", addr, timeBattery[i], battery[i]);
    }
  }
  if (filename != NULL) {
    int release = flock(fd, LOCK_UN);  // Unlock the file . . .
    fclose(fp);
    close(fd);
  }
}

// Return value will be used as exit code.
int run(int dd, char** bt_addrs, uint8_t filter_dup) {
  int err = 0;
  uint8_t own_type = LE_PUBLIC_ADDRESS; /* (other option LE_RANDOM_ADDRESS) */
  uint8_t scan_type = 0x00; /* Passive (0x01 = normal scan) */ 
  uint8_t filter_type = 0;
  uint8_t filter_policy = 0x01; /* Whitelist (0x00 = normal scan) */
  uint16_t interval = htobs(0x0010);
  uint16_t window = htobs(0x0010);
  int i;

  /* clear white list */
  err = hci_le_clear_white_list(dd, 1000);
  if (err < 0) {
    err = -errno;
    fprintf(stderr, "Can't clear white list: %s(%d)\n",
              strerror(-err), -err);
    return 1;
  }

  /* add BT devices to white list */
  devsBtAddr = new bdaddr_t[nDevs];
  for (i=0; i<nDevs; i++) {
    err = str2ba(bt_addrs[i], &devsBtAddr[i]);
    if (err < 0) {
      printf("Bad BT address\n");
      return 1;
    }

    err = hci_le_add_white_list(dd, &devsBtAddr[i], own_type, 1000);
    if (err < 0) {
      err = -errno;
      fprintf(stderr, "Can't add to white list: %s(%d)\n",
                strerror(-err), -err);
      return 1;
    }
  }

  err = hci_le_set_scan_parameters(dd, scan_type, interval, window,
            own_type, filter_policy, 10000);
  if (err < 0) {
    perror("Set scan parameters failed");
    return 1;
  }

  err = hci_le_set_scan_enable(dd, /*enable=*/0x01, filter_dup, 10000);
  if (err < 0) {
    perror("Enable scan failed");
    return 1;
  }

  temperature = new int[nDevs];
  humidity = new int[nDevs];
  battery = new int[nDevs];
  ntSamples = new int[nDevs];
  nhSamples = new int[nDevs];
  timeTemperature = new unsigned long[nDevs];
  timeHumidity = new unsigned long[nDevs];
  timeBattery = new unsigned long[nDevs];
  for (i = 0; i < nDevs; i++) {
    temperature[i] = 0;
    humidity[i] = 0;
    battery[i] = 0;
    ntSamples[i] = 0;
    nhSamples[i] = 0;
  }

  err = print_advertising_devices(dd, filter_type);
  if (err < 0) {
    printf("Could not receive advertising events\n");
    return 1;
  }

  return 0;
}

int main(int argc, char *argv[])
{
  int opt, dd;
  int i, dev_id = -1;
  // Don't filter duplicates. On Rpi, if I filter duplicates I'll only get the
  // first message.
  uint8_t filter_dup = 0x00;
  char bad_chars[] = "!@%^*~|";
  char invalid_found = 0;
  
  while ((opt=getopt(argc, argv, "+i:hf:t:n:d")) != -1) {
    switch (opt) {
      case 'i':
        dev_id = hci_devid(optarg);
        if (dev_id < 0) {
          printf("Invalid device\n");
          exit(1);
        }
        break;
      case 'n':
        numSamples = atoi(optarg);
        if (numSamples <= 0 || numSamples > 1000) {
          printf("Invalid number of samples\n");
          exit(1);
        }
        break;
      case 't':
        maxTime = atoi(optarg);
        if (maxTime <= 0 || maxTime > 1000) {
          printf("Invalid timeout\n");
          exit(1);
        }
        break;
      case 'f':
        for (i = 0; i < strlen(bad_chars); ++i) {
          if (strchr(optarg, bad_chars[i]) != NULL) {
            invalid_found = 1;
            break;
          }
        }
        if (invalid_found || strlen(optarg)>255) {
          printf("Invalid file name\n");
          exit(1);
        }
        filename = new char[strlen(optarg)+1];
        strcpy(filename,optarg);
        break;
      case 'd':
        debug = 1;
        break;
      case 'h':
      default:
        usage();
        exit(0);
    }
  }
  argc -= optind;
  argv += optind;
  optind = 0;

  if (argc < 1) {
    usage();
    exit(0);
  }
  nDevs = argc;

  if (dev_id < 0)
    dev_id = hci_get_route(NULL);

  dd = hci_open_dev(dev_id);
  if (dd < 0) {
    perror("Could not open device");
    exit(1);
  }

  int ret = run(dd, argv, filter_dup);

  // These need to be called at all times, or otherwise bluetooth device will be
  // locked and needs to be restarted.
  if (hci_le_set_scan_enable(dd, /*enable=*/0x00, filter_dup, 10000) < 0) {
    perror("Disable scan failed");
  }
  hci_close_dev(dd);

  if (ret == 0) {
    outputValues();
  }
  return ret;
}
