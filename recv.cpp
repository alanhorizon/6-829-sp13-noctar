#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <stdlib.h>

#include <uhd/utils/thread_priority.hpp>
#include <uhd/utils/safe_main.hpp>
#include <uhd/usrp/multi_usrp.hpp>
#include <boost/program_options.hpp>
#include <boost/thread/thread.hpp>
#include <boost/format.hpp>
#include <csignal>
#include <iostream>
#include <complex>

namespace po = boost::program_options;


int main(int argc, char* argv[]) {
  int bytes_to_read;
  if (argc > 1) {
    bytes_to_read = atoi(argv[1]);
  } else {
    bytes_to_read = 32;
  }

  std::string args = "addr=192.168.20.2";

  //create a usrp device
  std::cout << std::endl;
  std::cout << boost::format("Creating the usrp device with: %s...") % args << std::endl;
  uhd::usrp::multi_usrp::sptr usrp = uhd::usrp::multi_usrp::make(args);
  std::cout << boost::format("Using Device: %s") % usrp->get_pp_string() << std::endl;


  //const std::string file_name = "/dev/langford";
  int fd_read = open("/dev/langford", O_RDONLY);
  int fd_write = open("./written", 
                                O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR 
                                  | S_IROTH | S_IWOTH );
  char buf[bytes_to_read];
  ssize_t num_read = read(fd_read, buf, bytes_to_read);
  close(fd_read);
  
  for (int i=0; i<num_read; i++) {
    std::cout << buf[i];
  }
  write(fd_write, buf, num_read);
  close(fd_write);
  
 
}

    
    
    



