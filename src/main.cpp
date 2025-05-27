// check if pendrive / disk / blockdevice / etc. is ok
// made by kofi<45hx1w89h@mozmail.com> (she/her)

#include <unistd.h> // read() write()
#include <fcntl.h> // open() close()
#include <sys/stat.h>
#include <string.h> // strerror
#include <cerrno>
#include <cstdlib>
#include <stdexcept>
#include <sys/random.h>
#include <sys/ioctl.h>
#include <linux/fs.h>

#include <iostream>

constexpr int sd_blocksize = 512;
constexpr int blocksize_coefficient = 128;
const int global_open_flags = O_NOCTTY | O_NOATIME | O_DIRECT | O_SYNC; // see man open(2)
int write_data = O_RDONLY; /* this is the default value */
bool should_fail = false;
int to_string(int num, char* const buff, size_t buffsize);

void handle_file(const int fd, const blksize_t blocksize, const off_t size);

int main(int argc, char*argv[])
{
  if(argc == 1)
  { // help
    constexpr char const*const help_00 = "[\033[36mHELP\033[0m]\n";
    constexpr char const*const help_01 = "All files after the -rw options are handled in read write mode\n";
    constexpr char const*const help_02 = "All files after the -ro options are handled in read only mode\n";
    constexpr char const*const help_03 = "All files open() that fail after --fail option exit the program with status -1\n";
    constexpr char const*const help_04 = "All files open() that fail after --no-fail option continue to next device\n";
    constexpr char const*const help_05 = "--fail and --no-fail don't affect exit(-1) when device failure is detected\n";
    (void) write(2, help_00,  strlen(help_00) );
    (void) write(2, help_01,  strlen(help_01) );
    (void) write(2, help_02,  strlen(help_02) );
    (void) write(2, help_03,  strlen(help_03) );
    (void) write(2, help_04,  strlen(help_04) );
    (void) write(2, help_05,  strlen(help_05) );
    return 1;
  }
  for(int i = 1; i < argc; i++)
  {
    if(strcmp(argv[i], "-ro") == 0)
    {
      write_data = O_RDONLY;
    }
    else if(strcmp(argv[i], "-rw") == 0)
    {
      write_data = O_RDWR;
    }
    else if(strcmp(argv[i], "--fail") == 0)
    {
      should_fail = true;
    }
    else if(strcmp(argv[i], "--no-fail") == 0)
    {
      should_fail = false;
    } 
    else
    {
      const int fd = open(argv[i], global_open_flags | write_data);
      (void) write(1, argv[i], strlen(argv[i]));
      (void) write(1, ": ", 2);
      struct stat file_stat;
      if(fstat(fd, &file_stat) == -1)
      { // stat failed
        constexpr char const*const error_msg = "[\033[31mFATAL\033[0m] Failed to stat: ";
        (void) write(2, error_msg, strlen(error_msg));
        write(2, strerror(errno), strlen(strerror(errno)));
        (void) write(2, "\n", 1);
        if(should_fail)
          exit(-1);
      }
      else
      {
        size_t size = file_stat.st_size;
        if(size == 0)
        {
          long iosize = 0;
          if(ioctl(fd, BLKGETSIZE, &iosize) != 0)
            exit(-1);
          size = iosize*sd_blocksize;
        }

        char * size_str = new char[32];
        if(to_string(size, size_str, 32) == (-1))
          throw std::out_of_range("size number too big");
        (void) write(1, size_str, 32);
        (void) write(1, "\n", 1);

        handle_file(fd, file_stat.st_blksize, size);
      }
      close(fd);
    }
  }
  return 0;
}

void handle_file_ro(const int fd, const blksize_t blocksize, const off_t size) noexcept;
void handle_file_rw(const int fd, const blksize_t blocksize, const off_t size) noexcept;

void handle_file(const int fd, const blksize_t blocksize, const off_t size)
{
  if(write_data == O_RDONLY)
    handle_file_ro(fd, blocksize, size);
  else if(write_data == O_RDWR)
    handle_file_rw(fd, blocksize, size);
  else
    throw std::runtime_error("bad write mode flag");
  return;
}

void error_encounter(char const*const what)
{
  (void) write(2, "\n", 1);
  (void) write(2, what, strlen(what));
  (void) write(2, ": ", 2);
  (void) write(2, strerror(errno), strlen(strerror(errno)));
  (void) write(2, "\n", 1);
  exit(-1);
}

void status(int percent_complete)
{
  char * status = new char[5];
  status[0] = 0x0D;
  status[1] = 0x30+(percent_complete/100) %10;
  status[2] = 0x30+(percent_complete/10)  %10;
  status[3] = 0x30+(percent_complete)     %10;
  status[4] = '%';
  (void) write(2, status, 5);
}

void handle_file_ro(const int fd, const blksize_t blocksize, const off_t size) noexcept
{
  off_t bytesRead = 0;
  void
    *buf      /*= new char[blocksize * blocksize_coefficient]*/,
    *confirm  /*= new char[blocksize * blocksize_coefficient]*/;

  if(posix_memalign(&buf,     blocksize, blocksize*blocksize_coefficient) != 0)
    error_encounter("posix_memalign");
  if(posix_memalign(&confirm, blocksize, blocksize*blocksize_coefficient) != 0)
    error_encounter("posix_memalign");

  for(off_t i = 0; i < size; i += bytesRead)
  {
    status((i*100)/size);

    bytesRead = read(fd, buf, blocksize*blocksize_coefficient);
    if(bytesRead < 1)
      error_encounter("read");

    // confirm consistency
    off_t new_off = lseek(fd, i, SEEK_SET);
    if(new_off != i)
      error_encounter("seek");

    off_t bytesRead2 = read(fd, confirm, blocksize*blocksize_coefficient);
    if(bytesRead != bytesRead2)
      error_encounter("read");

    if(memcmp(buf, confirm, bytesRead) != 0)
      error_encounter("consistency");
  }

  constexpr char const*const msg_ok = " \033[32mOK\033[0m";
  status(100);
  write(1, msg_ok, strlen(msg_ok));
  write(1, "\n", 1);
}
void handle_file_rw(const int fd, const blksize_t blocksize, const off_t size) noexcept
{
  off_t bytesRead = 0;

  void
    *original        /* = new char[blocksize*blocksize_coefficient]*/,
    *random          /* = new char[blocksize*blocksize_coefficient]*/,
    *confirm_random  /* = new char[blocksize*blocksize_coefficient]*/;

  if(posix_memalign(&original       , blocksize, blocksize*blocksize_coefficient) != 0)
    error_encounter("posix_memalign");
  if(posix_memalign(&random         , blocksize, blocksize*blocksize_coefficient) != 0)
    error_encounter("posix_memalign");
  if(posix_memalign(&confirm_random , blocksize, blocksize*blocksize_coefficient) != 0)
    error_encounter("posix_memalign");

  for(off_t i = 0; i < size; i += bytesRead)
  {
    status((i*100)/size);

    bytesRead = read(fd, original, blocksize*blocksize_coefficient);
    lseek(fd, i, SEEK_SET);
    if( bytesRead < 1)
    { // error
      error_encounter("write");
      exit(-1);
    }

    getrandom(random, blocksize*blocksize_coefficient, 0);
    int bytesWritten = write(fd, random, blocksize*blocksize_coefficient);
    lseek(fd, i, SEEK_SET);
    if(bytesWritten != bytesRead)
    { // error
      error_encounter("write");
      exit(-1);
    }

    // read if bytes have changed
    int &bytesRead2 = bytesWritten; // rename variable - different purpose now
    bytesRead2 = read(fd, confirm_random, blocksize*blocksize_coefficient);
    lseek(fd, i, SEEK_SET);
    if(bytesRead2 != bytesRead)
    { // error
      error_encounter("read");
      exit(-1);
    }

    // restore original data
    bytesWritten = write(fd, original, blocksize*blocksize_coefficient);
    lseek(fd, i, SEEK_SET);
    if(bytesWritten != bytesRead)
    { // error
      error_encounter("write");
      exit(-1);
    }

    // verify if we succeded to restore original data
    void * confirm_original = random;
    bytesRead2 = read(fd, confirm_original, blocksize*blocksize_coefficient);
    // ^ bytesWritten
    if(memcmp(original, confirm_original, blocksize*blocksize_coefficient) != 0)
    { // error
      error_encounter("restore");
      exit(-1);
    }
  }
  constexpr char const*const msg_ok = " \033[32mOK\033[0m";
  status(100);
  write(1, msg_ok, strlen(msg_ok));
  write(1, "\n", 1);
}

// this string is not null terminated:
int to_string(int num, char* const buff, size_t buffsize)
{
  memset(buff, 0, buffsize);
  do
  {
    if(buffsize == 0) return -1;
    buff[--buffsize] = 0x30+num%10;
    num /= 10;
  }while(num>0);
  return 0;
}
