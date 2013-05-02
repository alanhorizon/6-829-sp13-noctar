
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>

int main() {
  //const std::string file_name = "/dev/langford";
  int fd_read = open("/dev/langford", O_RDONLY);
  int fd_write = open("./written", 
                                O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR 
                                  | S_IROTH | S_IWOTH );
  char buf[32];
  ssize_t num_read = read(fd_read, buf, 32);
  close(fd_read);
  
  for (int i=0; i<32; i++) {
    std::cout << buf[i];
  }
  write(fd_write, buf, num_read);
  close(fd_write);
  
 
}

    
    
    



