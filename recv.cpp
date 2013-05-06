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
  
  double rate = 3e6;
  //set the tx sample rate
  std::cout << boost::format("Setting TX Rate: %f Msps...") % (rate/1e6) << std::endl;
  usrp->set_tx_rate(rate);
  std::cout << boost::format("Actual TX Rate: %f Msps...") % (usrp->get_tx_rate()/1e6) << std::endl << std::endl;
  
  double freq = 2.4e9;
  // set the tx frequency
  std::cout << boost::format("Setting TX Freq: %f MHz...") % (freq/1e6) << std::endl;
  for(size_t i=0; i < usrp->get_tx_num_channels(); i++) usrp->set_tx_freq(freq, i);
  std::cout << boost::format("Actual TX Freq: %f MHz...") % (usrp->get_tx_freq()/1e6) << std::endl << std::endl;

  double gain = 31;
  // set the tx gain
  std::cout << boost::format("Setting TX Gain: %f...") % (gain) << std::endl;
  for(size_t i=0; i < usrp->get_tx_num_channels(); i++) usrp->set_tx_gain(gain, i);
  std::cout << boost::format("Actual TX Gain: %f...") % (usrp->get_tx_gain()) << std::endl << std::endl;

  //create a transmit streamer
  uhd::stream_args_t stream_args("fc32"); //complex floats
  uhd::tx_streamer::sptr tx_stream = usrp->get_tx_stream(stream_args);

  float ampl = float(0.3);
  //allocate buffer with data to send
  const size_t spb = tx_stream->get_max_num_samps();
  std::cout << "buffer size: " << spb << std::endl;

  std::vector<std::complex<float> > buff(spb, std::complex<float>(ampl, ampl));
  std::vector<std::complex<float> *> buffs(usrp->get_tx_num_channels(), &buff.front());

  //setup metadata for the first packet
  uhd::tx_metadata_t md;
  md.start_of_burst = true;
  md.end_of_burst = false;
  md.has_time_spec = true;
  md.time_spec = uhd::time_spec_t(0.0);
  
  size_t samps_to_send = 30;
  size_t num_tx_samps = tx_stream->send(buffs, samps_to_send, md, 0.1);
  
  md.start_of_burst = false;
  
  for (int i = 0; i < 100; i++) {
    //send a single packet
    size_t num_tx_samps = tx_stream->send(buffs, samps_to_send, md, 0.1);
    std::cout << "number of sent samples: " << num_tx_samps << std::endl;
  }


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

    
    
    



