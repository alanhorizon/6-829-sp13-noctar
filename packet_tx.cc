/*
 * Copyright (c) 2007, 2008, 2009, 2010, 2013 Joseph Gaeddert
 *
 * This file is part of liquid.
 *
 * liquid is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * liquid is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with liquid.  If not, see <http://www.gnu.org/licenses/>.
 */

/* edited by alho
   switched to new streamer api, i.e. change send method call arguments
   got rid of resamp code
   moved frame code to before transmission 
   prepared buffers in advance, then send same buffers repeatedly
*/
 
#include <math.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <complex>
#include <getopt.h>
#include <liquid/liquid.h>
#include <typeinfo>

#include <uhd/usrp/multi_usrp.hpp>

#include <fstream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>


void transmit(unsigned int num_frames, uhd::tx_streamer::sptr tx_stream, std::vector<std::vector<std::complex<float> *> > buffs_vec, uhd::tx_metadata_t md, bool verbose);

void usage() {
    printf("packet_tx -- transmit simple packets\n");
    printf("\n");
    printf("  u,h   : usage/help\n");
    printf("  q/v   : quiet/verbose\n");
    printf("  f     : center frequency [Hz], default: 462 MHz\n");
    printf("  b     : bandwidth [Hz] (62.5kHz min, 8MHz max), default: 250 kHz\n");
    printf("  g     : software tx gain [dB] (default: -6dB)\n");
    printf("  G     : uhd tx gain [dB] (default: 40dB)\n");
    printf("  N     : number of frames, default: 2000\n");
}

int main (int argc, char **argv)
{
    // command-line options
    bool verbose = true;

    unsigned long int DAC_RATE = 64e6;
    double min_bandwidth = 0.25*(DAC_RATE / 512.0);
    double max_bandwidth = 0.25*(DAC_RATE /   4.0);

    double frequency = 462.0e6;
    double bandwidth = 250e3f;
    unsigned int num_frames = 2000;     // number of frames to transmit
    double txgain_dB = -12.0f;          // software tx gain [dB]
    double uhd_txgain = 40.0;           // uhd (hardware) tx gain

    //
    int d;
    while ((d = getopt(argc,argv,"uhqvf:b:g:G:N:")) != EOF) {
        switch (d) {
        case 'u':
        case 'h':   usage();                        return 0;
        case 'q':   verbose     = false;            break;
        case 'v':   verbose     = true;             break;
        case 'f':   frequency   = atof(optarg);     break;
        case 'b':   bandwidth   = atof(optarg);     break;
        case 'g':   txgain_dB   = atof(optarg);     break;
        case 'G':   uhd_txgain  = atof(optarg);     break;
        case 'N':   num_frames  = atoi(optarg);     break;
        default:
            usage();
            return 0;
        }
    }

    if (bandwidth > max_bandwidth) {
        fprintf(stderr,"error: %s, maximum bandwidth exceeded (%8.4f MHz)\n", argv[0], max_bandwidth*1e-6);
        return 0;
    } else if (bandwidth < min_bandwidth) {
        fprintf(stderr,"error: %s, minimum bandwidth exceeded (%8.4f kHz)\n", argv[0], min_bandwidth*1e-3);
        exit(1);
    }

    uhd::device_addr_t dev_addr;
    //dev_addr["addr0"] = "192.168.10.2";
    //dev_addr["addr1"] = "192.168.10.3";
    uhd::usrp::multi_usrp::sptr usrp = uhd::usrp::multi_usrp::make(dev_addr);

    // set properties
    double tx_rate = 4.0*bandwidth;

    // NOTE : the sample rate computation MUST be in double precision so
    //        that the UHD can compute its interpolation rate properly
    unsigned int interp_rate = (unsigned int)(DAC_RATE / tx_rate);
    // ensure multiple of 4
    interp_rate = (interp_rate >> 2) << 2;
    // NOTE : there seems to be a bug where if the interp rate is equal to
    //        240 or 244 we get some weird warning saying that
    //        "The hardware does not support the requested TX sample rate"
    while (interp_rate == 240 || interp_rate == 244)
        interp_rate -= 4;
    // compute usrp sampling rate
    double usrp_tx_rate = DAC_RATE / (double)interp_rate;
    
    // try to set tx rate
    usrp->set_tx_rate(DAC_RATE / interp_rate);

    // get actual tx rate
    usrp_tx_rate = usrp->get_tx_rate();

    //usrp_tx_rate = 262295.081967213;
    // compute arbitrary resampling rate
    double tx_resamp_rate = usrp_tx_rate / tx_rate;

    usrp->set_tx_freq(frequency);
    usrp->set_tx_gain(uhd_txgain);

    printf("frequency   :   %12.8f [MHz]\n", frequency*1e-6f);
    printf("bandwidth   :   %12.8f [kHz]\n", bandwidth*1e-3f);
    printf("verbosity   :   %s\n", (verbose?"enabled":"disabled"));

    printf("sample rate :   %12.8f kHz = %12.8f * %8.6f (interp %u)\n",
            tx_rate * 1e-3f,
            usrp_tx_rate * 1e-3f,
            1.0 / tx_resamp_rate,
            interp_rate);

    // set the IF filter bandwidth
    //usrp->set_tx_bandwidth(2.0f*tx_rate);

    // transmitter gain (linear)
    float g = powf(10.0f, txgain_dB/20.0f);

    // data arrays
        unsigned char header[8];
        unsigned char payload[64];
    
    // create frame generator
    framegen64 fg = framegen64_create();
    framegen64_print(fg);

    // allocate array to hold frame generator samples
    unsigned int frame_len = FRAME64_LEN;   // length of frame64 (defined in liquid.h)
    std::complex<float> frame_samples[frame_len];
    std::cout << frame_len << std::endl;
    /* alho:
       as of may 7 2013, on belinkov-precision-t5600
       value of frame_len is 1340, frame_samples is array of 1340 samples
    */

    // set up the metadta flags
    uhd::tx_metadata_t md;
    md.start_of_burst = false;  // never SOB when continuous

    md.end_of_burst   = false;  // 
    md.has_time_spec  = false;  // set to false to send immediately

    // Streamer API test
    uhd::stream_args_t stream_args("fc32"); //complex floats
    uhd::tx_streamer::sptr tx_stream = usrp->get_tx_stream(stream_args);

    /**/    unsigned int j;


    // create one frame for the entire transmission
        // write header (first two bytes packet ID, remaining are random)
         unsigned int fixed_pid = 0; // try framing a fixed packet
                header[0] = (fixed_pid >> 8) & 0xff;
	        header[1] = (fixed_pid     ) & 0xff;
	        for (j=2; j<8; j++)
	          header[j] = rand() & 0xff;

        /// initialize payload
        for (j=0; j<64; j++)
	payload[j] = rand() & 0xff;

        // generate the entire frame
        framegen64_execute(fg, header, payload, frame_samples);

     //unsigned int num_buffers = frame_len / 256;
     std::vector<std::vector<std::complex<float> *> > buffs_vec;

    unsigned int usrp_sample_counter = 0;

    // prepare buffs
    std::vector<std::complex<float> > cur_usrp_buffer(256);
        for (j=0; j<frame_len; j++) {
                cur_usrp_buffer[usrp_sample_counter++] = g*frame_samples[j];
                if (usrp_sample_counter == 256) {
                    usrp_sample_counter = 0;
                    std::vector<std::complex<float> > usrp_buffer(cur_usrp_buffer);
                    std::vector<std::complex<float> *> buffs(usrp->get_tx_num_channels(), &usrp_buffer.front());    
                    buffs_vec.push_back(buffs);
                }
         }


    ////// START READING NOCTAR ////////

    // open noctar device
    int fd_read = open("/dev/langford", O_RDONLY);
    // open file to write
    int fd_write = open("./noctar_samples", O_WRONLY | O_CREAT, S_IRUSR
                                          | S_IWUSR | S_IROTH | S_IWOTH);

    unsigned int num_samples_to_read = 10;
    unsigned int num_bytes_to_read = 4*num_samples_to_read;
    char buff[num_bytes_to_read];
    //ssize_t num_read_samples = read(fd_read, buff, num_bytes_to_read);
    ssize_t num_read_bytes = 0;
    ssize_t num_read_samples = 0;
    int64_t start_transmit = 0;
    int64_t end_transmit = 0; 

    ///////////// START COUNTER ////////////
    bool transmitted = false;
    bool finished_transmitting = false;
    bool end_transmit_flag = false;
    int64_t receive_sample_counter = 0;
    int64_t delta = 3 * 1e5;// sample rate of noctar (2.4e9)/16



    while(true) {
       
        num_read_bytes = read(fd_read, buff, num_bytes_to_read);
	num_read_samples = num_read_bytes / 4;
        receive_sample_counter += num_read_samples;
        
        if (!transmitted && receive_sample_counter >= 10000) {
           transmitted = true;
           start_transmit = receive_sample_counter;
	   std::cout << "start transmission: " << start_transmit << std::endl;
              
           ////// TODO: transmit with thread /////
	   transmit(num_frames, tx_stream, buffs_vec, md, verbose);
	   finished_transmitting = true;
        }
 
        if (!end_transmit_flag && finished_transmitting) {
            end_transmit_flag = true;
	    std::cout << "finished transmitting: " << receive_sample_counter << std::endl;
	    end_transmit = receive_sample_counter;
	    
        }

	if (end_transmit_flag) {
	    //std::cout << "end_transmit: " << end_transmit << " delta: " << delta << " ended samples: " << receive_sample_counter << std::endl;	
	    if (receive_sample_counter > end_transmit+delta) {
	    	std::cout << "end program: " << receive_sample_counter << std::endl;
	    	break;
	    }
	}

	write(fd_write, buff, num_read_bytes);

        
    }

    // close noctar
    close(fd_read);
    close(fd_write);
    return 0;
}




void transmit(unsigned int num_frames, uhd::tx_streamer::sptr tx_stream, std::vector<std::vector<std::complex<float> *> > buffs_vec, uhd::tx_metadata_t md, bool verbose) {
    unsigned int pid;
    for (pid=0; pid<num_frames; pid++) {
        if (verbose)
            printf("tx packet id: %6u\n", pid);
        
	    // STREAMER API'S SEND METHOD
            for (unsigned int k=0; k<buffs_vec.size(); k++) {
              tx_stream->send(buffs_vec[k], 256, md, 0.1);
            }
		
    } // packet loop
 
    // send a mini EOB packet
    md.start_of_burst = false;
    md.end_of_burst   = true;

    // UPDATED SEND METHOD FROM STREAMER API
    tx_stream->send("", 0, md, 0.1);

    // sleep for a small amount of time to allow USRP buffers
    // to flush
    usleep(100000);

    //finished
    printf("usrp data transfer complete\n");
}

