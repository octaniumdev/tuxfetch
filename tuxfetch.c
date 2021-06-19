//usr/bin/cc tuxfetch.c -lpthread -lpci -O3 -g3 -o tuxfetch; exec ./tuxfetch

#define DIRENT_SIZE (100 * 1024 * 4)

// replace cpuid.h with this for tcc support
int __get_cpuid(unsigned int leaf, unsigned int *a, unsigned int *b,
                unsigned int *c, unsigned int *d) {

  __asm__ __volatile__("cpuid\n\t"
                       : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
                       : "0"(leaf));

  return 1;
}

//#include <cpuid.h>

#include <ctype.h>

#define _GNU_SOURCE
#define _LARGEFILE64_SOURCE
#define __USE_LARGEFILE64
#include <dirent.h> // pacman package count and wm fetch

#include <fcntl.h> // open

#include <stdio.h> // printf

#include <stdlib.h> // malloc

#include <string.h>

#include <syscall.h>

#include <threads.h>
//#include "tuxthread/tuxthread.h"

#include <unistd.h> // read

#include <linux/fb.h>
#include <sys/ioctl.h> // fb0 resolution fetch

#include <sys/mman.h> // mmap

#include <sys/time.h> // benchmarking

#include <sys/utsname.h> // kernel fetch

#include <sys/stat.h> // get size of file

#include <sys/sysinfo.h> // uptime fetch

#include <pci/pci.h> // gpu fetch

#define MAX_LENGTH 50

#define TIME_INIT struct timeval start, end
#define TIME_START gettimeofday(&start, NULL)
#define TIME_END                                                               \
  gettimeofday(&end, NULL);                                                    \
  printf("\e[90mtime %lu us\e[0m\n",                                           \
         (end.tv_sec - start.tv_sec) * 1000000 + end.tv_usec - start.tv_usec)

char tux[] = "\e[1m\n\
\e[37m       ,--.   \n\
\e[37m      |0\e[33m_\e[37m0 |\n\
\e[37m      |\e[33mL_/\e[37m |\n\
\e[37m     //   \\ \\  \n\
\e[37m    ((     ) )    \n\
\e[33m   /`\\     /`\\ \e[37m \n\
\e[33m   \\__)\e[37m===\e[33m(__/\e[37m ";

char *read_first_line(char *path) {
  int file = open(path, O_RDONLY);
  char *string = malloc(256);
  size_t length = read(file, string, 256);
  string[length - 1] = '\0'; // remove newline
  close(file);
  return string;
}

char *read_file(char *path) {
  size_t chunk_size = 65536;
  int file = open(path, O_RDONLY);
  char *buffer = malloc(0);
  size_t total_length = 0;
  while (1) {
    buffer = realloc(buffer, total_length + chunk_size);
    if (!buffer)
      break;
    size_t length = read(file, buffer + total_length, chunk_size);
    if (!length)
      break;
    total_length += length;
  }
  close(file);
  return buffer;
}

int ppid_from_pid(int pid) {
  char path[256];
  snprintf(path, sizeof(path), "/proc/%d/stat", pid);
  FILE *file = fopen(path, "r");
  int ppid;
  fscanf(file, "%*d %*s %*c %d", &ppid);
  fclose(file);
  return ppid;
}

char *name_from_pid(int pid) {
  char path[256];
  snprintf(path, sizeof(path), "/proc/%d/comm", pid);
  return read_first_line(path);
}

#define WM_COUNT 32
static char wms[WM_COUNT][16] = {
    "9wm",      "awesome",      "blackbox",    "bspwm",
    "compiz",   "dwl",          "dwm",         "enlightenment",
    "evilwm",   "fluxbox",      "gnome-shell", "herbstluftwm",
    "i3",       "icewm",        "leftwm",      "jwm",
    "kwin_x11", "kwin_wayland", "matchbox",    "metacity",
    "mutter",   "openbox",      "pekwm",       "qtile",
    "spectrwm", "sowm",         "sway",        "twm",
    "waymonad", "weston",       "xfwm",        "xmonad"};

int fetch_wm() {
  int proc_dir = open("/proc/", O_RDONLY | O_DIRECTORY);
  char *buffer = mmap(0, DIRENT_SIZE, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  int entries_length = syscall(SYS_getdents64, proc_dir, buffer, DIRENT_SIZE);

  for (int p = 0; p < entries_length;) {
    struct dirent64 *entry = (struct dirent64 *)(buffer + p);
    p += entry->d_reclen;

    char stat[256];
    char stat_path[256];

    if (isdigit(entry->d_name[0])) {
      snprintf(stat_path, 256, "/proc/%s/stat", entry->d_name);
      int name_fd = open(stat_path, O_RDONLY);
      read(name_fd, stat, 256);
      stat[255] = '\0';
      close(name_fd);

      // find name in parens or continue
      char *name = strchr(stat, '(') + 1;
      char *end = strchr(name, ')');
      if (end) {
        *end = '\0';
      } else {
        continue;
      }

      for (int i = 0; i < WM_COUNT; i++) {
        if (!strcmp(wms[i], name)) {
          printf("\e[18C%13s%s\n", "WM: ", name);
          break;
        }
      }
    }
  }
  close(proc_dir);

  return 0;
}

int fetch_hostname() {

  printf("\e[18C%13s%s\n",
         "host: ", read_first_line("/proc/sys/kernel/hostname"));

  return 0;
}

int fetch_terminal() {
  char *terminal = name_from_pid(ppid_from_pid(getppid()));

  printf("\e[18C%13s%s (%s)\n", "terminal: ", terminal, getenv("TERM"));

  return 0;
}

int fetch_model() {
  char *name = read_first_line("/sys/class/dmi/id/product_name");
  char *version = read_first_line("/sys/class/dmi/id/product_version");

  printf("\e[18C%13s%s %s\n", "model: ", name, version);

  return 0;
}

int fetch_kernel() {
  struct utsname uname_info;
  uname(&uname_info);

  printf("\e[18C%13s%s\n", "kernel: ", uname_info.release);

  return 0;
}

int fetch_os(char *buffer) {
  char *id = strstr(buffer, "NAME=");
  static char name[MAX_LENGTH];
  sscanf(id, "NAME=\"%[^\"]+", name);

  printf("\e[18C%13s%s\n", "OS: ", name);

  return 0;
}

char alpine_logo[] = "\e[1m\n\
\e[31m   .8BBBBBBBBB.  \n\
\e[33m  .888888888888. \n\
\e[32m .8888* *88*8888.\n\
\e[36m 888* ,8, `. *888\n\
\e[36m `* ,88888, `, *`\n\
\e[34m  `888888888888` \n\
\e[35m   `8BBBBBBBBB`   ";

char arch_logo[] = "\e[1m\n\
\e[31m        A \n\
\e[31m       a8s \n\
\e[33m      ao88s \n\
\e[32m     a88888s \n\
\e[36m    a88Y*Y88s \n\
\e[34m   a88H   B8bs \n\
\e[35m  /*`       `*\\ ";

char celos_logo[] = "\e[1m\n\
\e[35m    _~a8B6s~_   \n\
\e[35m  .88888`  `Y6, \n\
\e[35m  J8888:     8l \n\
\e[35m  B88888,  .d88 \n\
\e[35m  V8P* `Y88888P \n\
\e[35m  `9b, .d88888` \n\
\e[35m    `~9BBB8~`   ";

char debian_logo[] = "\e[1m\n\
\e[31m    ,gA88bq.  \n\
\e[33m   dP      `9.\n\
\e[32m  d7  ,*`.  )8\n\
\e[36m  9:  A    ,Q*\n\
\e[34m  *1  `^vsv\" \n\
\e[35m   *b         \n\
\e[35m     \"~. ";

char mint_logo[] = "\e[1m\n\
\e[32m BBBBBBBBBBBs~. \n\
\e[32m BB8  88**88**8,\n\
\e[32m   H  H  a  a  l\n\
\e[32m   H  H  H  H  H\n\
\e[32m   V, *88888* .H\n\
\e[32m   `8,_______.=H\n\
\e[32m     *YBBBBBBBBH ";

char ubuntu_logo[] = "\e[1m\n\
\e[31m          \e[35m($)\n\
\e[31m    \e[33m .\e[31m s88~..\n\
\e[31m    \e[33m.8*  \e[31m `*9.\n\
\e[31m \e[31m($) \e[33m8\n\
\e[31m    \e[33m`6-  \e[35m _-8`\n\
\e[31m     \e[33m`\e[35m ^88*``\n\
\e[31m          \e[33m($) ";

int fetch_art(char *buffer) {
  char *id = strstr(buffer, "\nID=");
  static char os_id[MAX_LENGTH];
  sscanf(id, "\nID=%[^\n]", os_id);

  if (strcmp("alpine", os_id) == 0) {

    write(1, alpine_logo, sizeof(alpine_logo));
  }
  if (strcmp("arch", os_id) == 0) {

    write(1, arch_logo, sizeof(arch_logo));
  }
  if (strcmp("celos", os_id) == 0) {

    write(1, celos_logo, sizeof(celos_logo));
  }
  if (strcmp("debian", os_id) == 0) {

    write(1, debian_logo, sizeof(debian_logo));
  }
  if (strcmp("mint", os_id) == 0) {

    write(1, mint_logo, sizeof(mint_logo));
  }
  if (strcmp("ubuntu", os_id) == 0) {

    write(1, ubuntu_logo, sizeof(ubuntu_logo));
  }
  
  return 0;
}

int fetch_uptime() {
  struct sysinfo a;
  sysinfo(&a);
  unsigned long seconds = a.uptime;
  unsigned long minute = 60;
  unsigned long hour = 60 * minute;
  unsigned long day = 24 * hour;

  printf("\e[18C%13s%lu%c %lu%c %lu%c\n", "uptime: ", seconds / day, 'd',
         (seconds % day) / hour, 'h', (seconds % hour) / minute, 'm');

  return 0;
}

int fetch_packages_apk() {

  int file = open("/lib/apk/db/installed", O_RDONLY);

  struct stat filestat;

  fstat(file, &filestat);

  // the +1 is important
  char *buffer =
      mmap(NULL, filestat.st_size + 1, PROT_READ, MAP_SHARED, file, 0);

  int packages = 0;
  size_t bytes = filestat.st_size;

  if (buffer[0] == 'P')
    packages += 1;

  for (size_t i = 0; i < bytes; i++) {
    packages += (buffer[i] == '\n') & (buffer[i + 1] == 'P');
  }

  close(file);

  return packages;
}

int fetch_packages_dpkg() {

  int file = open("/var/lib/dpkg/status", O_RDONLY);

  struct stat filestat;

  fstat(file, &filestat);

  // the +1 is important
  char *buffer =
      mmap(NULL, filestat.st_size + 2, PROT_READ, MAP_SHARED, file, 0);

  int packages = 0;
  size_t bytes = filestat.st_size;

  if (buffer[0] == 'P' && buffer[0] == 'a')
    packages += 1;

  for (size_t i = 0; i < bytes; i++) {
    packages +=
        (buffer[i] == '\n') & (buffer[i + 1] == 'P') & (buffer[i + 2] == 'a');
  }

  close(file);

  return packages;
}

int fetch_packages_pacman(int pacman_dir, char *buffer) {
  int packages = 0;

  int entries_length = syscall(SYS_getdents64, pacman_dir, buffer, DIRENT_SIZE);

  for (int p = 0; p < entries_length;) {
    struct dirent64 *entry = (struct dirent64 *)(buffer + p);
    p += entry->d_reclen;
    packages += entry->d_type == DT_DIR;
  }

  close(pacman_dir);

  return packages;
}

int fetch_packages() {
  char *buffer = mmap(0, DIRENT_SIZE, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  int packages = 0;
  char *package_manager = "unknown";

  int pacman_dir = open("/var/lib/pacman/local", O_RDONLY | O_DIRECTORY);

  if (pacman_dir != -1) {
    packages += fetch_packages_pacman(pacman_dir, buffer);
    package_manager = "pacman";
  } else if (!access("/lib/apk/db/installed", R_OK)) {
    packages += fetch_packages_apk();
    package_manager = "apk";
  } else if (!access("/var/lib/dpkg/status", R_OK)) {
    packages += fetch_packages_dpkg();
    package_manager = "dpkg";
  }

  int flatpak_app_dir = open("/var/lib/flatpak/app", O_RDONLY | O_DIRECTORY);
  int flatpak_runtime_dir =
      open("/var/lib/flatpak/runtime", O_RDONLY | O_DIRECTORY);

  int flatpaks = 0;

  if (flatpak_app_dir && flatpak_runtime_dir) {

    int entries_length;

    entries_length =
        syscall(SYS_getdents64, flatpak_app_dir, buffer, DIRENT_SIZE);
    for (int p = 0; p < entries_length;) {
      struct dirent64 *entry = (struct dirent64 *)(buffer + p);
      p += entry->d_reclen;
      flatpaks += entry->d_type == DT_DIR;
    }

    entries_length =
        syscall(SYS_getdents64, flatpak_runtime_dir, buffer, DIRENT_SIZE);
    for (int p = 0; p < entries_length;) {
      struct dirent64 *entry = (struct dirent64 *)(buffer + p);
      p += entry->d_reclen;
      flatpaks += entry->d_type == DT_DIR;
    }
    close(flatpak_app_dir);
    close(flatpak_runtime_dir);
  }

  printf("\e[18C%13s%d (%s) %d (flatpak)\n", "packages: ", packages,
         package_manager, flatpaks);

  return 0;
}

int fetch_env() {

  printf("\e[18C%13s%s\n", "shell: ", getenv("SHELL"));
  printf("\e[18C%13s%s\n", "username: ", getenv("USER"));
  char *desktop = getenv("XDG_CURRENT_DESKTOP");
  if (!desktop)
    desktop = "none";
  printf("\e[18C%13s%s\n", "DE: ", desktop);

  return 0;
}

#ifdef __x86_64__
int fetch_cpu() {
  static char cpu[48];
  unsigned int *x = (unsigned int *)cpu;

  __get_cpuid(0x80000002, x + 0, x + 1, x + 2, x + 3);
  __get_cpuid(0x80000003, x + 4, x + 5, x + 6, x + 7);
  __get_cpuid(0x80000004, x + 8, x + 9, x + 10, x + 11);

  // remove duplicate spaces at the end
  for (int i = 47; i--;) {
    if (cpu[i] != ' ') {
      cpu[i + 1] = '\0';
      break;
    }
  }

  printf("\e[18C%13s%s (%ld)\n", "CPU: ", cpu, sysconf(_SC_NPROCESSORS_CONF));
  return 0;
}
#else
int fetch_cpu() {
  char *buffer = read_file("/proc/cpuinfo");
  char *line = strstr(buffer, "model name	: ");
  char cpu[MAX_LENGTH];
  sscanf(line, "model name	: %[^\n]", cpu);

  printf("\e[18C%13s%s (%ld)\n", "CPU: ", cpu, sysconf(_SC_NPROCESSORS_CONF));
  return 0;
}
#endif

int fetch_resolution() {

  int fb_fd = open("/dev/fb0", O_RDONLY);

  if (fb_fd != -1) {
    struct fb_var_screeninfo vinfo;
    ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);
    printf("\e[18C%13s%dx%d\n", "resolution: ", vinfo.xres, vinfo.yres);
    close(fb_fd);
  } else {
    int width, height;
    char *modes = read_first_line("/sys/class/graphics/fb0/virtual_size");
    sscanf(modes, "%d,%d", &width, &height);
    printf("\e[18C%13s%dx%d\n", "resolution: ", width, height);
  }

  return 0;
}

// pull request to add your own gpu
// https://raw.githubusercontent.com/pciutils/pciutils/master/pci.ids
#define GPU_COUNT 5
static struct {
  int key;
  char value[16];
} gpus[GPU_COUNT] = {{5686, "Renoir"},
                     {5592, "Picasso"},
                     {0x1b83, "GTX 1060 6GB"},
                     {0x1b84, "GTX 1060 3GB"},
                     {4487, "GTX 760"}};

int fetch_gpu() {

  char buffer[MAX_LENGTH];

  struct pci_access *pacc = pci_alloc();

  pci_init(pacc);
  pci_scan_bus(pacc);
  struct pci_dev *dev = pacc->devices;

  while (dev) {
    pci_fill_info(dev, PCI_FILL_IDENT | PCI_FILL_CLASS);
    if (dev->device_class == 768) {
      break;
    }
    dev = dev->next;
  }
  if (!dev) {
    printf("\e[18C%13s%s\n", "GPU: ", "none");
    return 0;
  }

  char *vendor = "unknown vendor";

  switch (dev->vendor_id) {
  case 4098:
    vendor = "AMD";
    break;
  case 4318:
    vendor = "NVIDIA";
    break;
  case 32902:
    vendor = "Intel";
    break;
  }

  char *model;

  for (int i = 0; i < GPU_COUNT; i++) {
    if (gpus[i].key == dev->device_id) {
      model = gpus[i].value;
      printf("\e[18C%13s%s %s\n", "GPU: ", vendor, model);
      return 0;
    }
  }

  model = pci_lookup_name(pacc, buffer, sizeof(buffer), PCI_LOOKUP_DEVICE,
                          dev->vendor_id, dev->device_id);

  printf("\e[18C%13s%s %s (id: %d)\n", "GPU: ", vendor, model, dev->device_id);

  pci_cleanup(pacc);

  return 0;
}

int fetch_memory() {
  unsigned long memory = sysconf(_SC_PHYS_PAGES) * sysconf(_SC_PAGESIZE);
  printf("\e[18C%13s%luMiB\n", "memory: ", memory / 1024 / 1024);
  return 0;
}

int main() {
  TIME_INIT;
  TIME_START;

  char *release = read_file("/etc/os-release");

  // draw ascii art
  printf("\n");
  fetch_art(release);
  printf("\n");
  printf("%s\n", tux);
  printf("%s", "\e[16A");

  // create threads for the slowest tasks
  thrd_t gpu_thread;
  thrd_create(&gpu_thread, fetch_gpu, NULL);

  thrd_t packages_thread;
  thrd_create(&packages_thread, fetch_packages, NULL);

  thrd_t wm_thread;
  thrd_create(&wm_thread, fetch_wm, NULL);

  // run quick tasks synchronously in main thread
  fetch_uptime();
  fetch_hostname();
  fetch_terminal();
  fetch_model();
  fetch_kernel();
  fetch_os(release);
  fetch_memory();
  fetch_env();
  fetch_cpu();
  fetch_resolution();

  // wait for threads to end
  int gpu_result;
  int packages_result;
  int wm_result;
  thrd_join(gpu_thread, &gpu_result);
  thrd_join(packages_thread, &packages_result);
  thrd_join(wm_thread, &wm_result);

  printf("\n");
  printf("\e[18C%13s%s\n", "palette: ",
         "\e[1m\e[100m  \e[101m  \e[102m  \e[103m  \e[104m  \e[105m  \e[106m  "
         "\e[107m  \e[0m");
  TIME_END;

  return 0;
}
