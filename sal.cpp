#include "sal.h"
#include "Workload.h"
#include "WattsUp.h"
#include "CPUstats.h"
#include <iostream>
#include <cstdlib>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <fstream>
#include <cstring>
#include <unistd.h> // sleep()
#include <time.h>   // clock()
#include <string>
#include <sstream>
#include <iomanip>

using namespace std;

void sigchld_handler(int signum)
{
#ifdef DEBUG
    cout << "child termination detected." << endl;
#endif
    Workload * workload = Workload::get_instance();
    workload->next();
}

string generate_filename()
{
    time_t rawtime = time(NULL); // get UNIX time
    struct tm * timeinfo = localtime(&rawtime); // get broken-down time

    // Now take the time and date info in timeinfo and format it
    // as a string in the way we want it
    stringstream ss (stringstream::in | stringstream::out);
    ss.fill('0'); // leading zero
    ss << setw(2) << 1+timeinfo->tm_mon << "-"
       << setw(2) << timeinfo->tm_mday  << "-"
       << setw(2) << timeinfo->tm_hour  << "-"
       << setw(2) << timeinfo->tm_min   << "-"
       << setw(2) << timeinfo->tm_sec;

    return ss.str();
}

void log_line(WattsUp& wu, int * workload_number, time_t start_time, fstream& log_file)
{
    int disk_utilisation;
    const int NUM_DISKS  = 1;
    int * disk_stats1    = new int[NUM_DISKS];
    int * disk_stats2    = new int[NUM_DISKS];

    CPUstats * cpu_stats  = CPUstats::get_instance();
    int num_cpu_lines  = cpu_stats->get_num_cpu_lines();
    int * cpu_utilisation = new int[num_cpu_lines];

    get_diskstats(NUM_DISKS, disk_stats1);
    sleep(1);  // needed else we don't get Watts readings
    get_diskstats(NUM_DISKS, disk_stats2);
    int watts = wu.getWatts();
    cpu_stats->get_cpu_utilisation(cpu_utilisation);

    cout.fill('0');
    log_file.fill('0');
    cout << "Time=" << setw(4) << time(NULL)-start_time << ",Workload=" << setw(3) << *workload_number << ",deciWatts=" << setw(4) << watts << ",";
    log_file        << setw(4) << time(NULL)-start_time << ","          << setw(3) << *workload_number << ","           << setw(4) << watts << ",";

    for (int cpu_line=0; cpu_line < num_cpu_lines; cpu_line++) {
        if (cpu_line==0) {
            cout     << "CPUav=" << setw(3) << cpu_utilisation[cpu_line];
            log_file <<             setw(3) << cpu_utilisation[cpu_line];
        } else {
            cout     << "," << "CPU" << setw(2) << cpu_line << "=" << setw(3) << cpu_utilisation[cpu_line];
            log_file << "," <<          setw(2) << cpu_line << "," << setw(3) << cpu_utilisation[cpu_line];
        }
    }

    for (int disk=0; disk<NUM_DISKS; disk++) {
        disk_utilisation = (disk_stats2[disk]-disk_stats1[disk]) / 10; // disk stats returns the number of miliseconds the disk has been active over the past second
        //FIXME: We sometimes get disk utilisations >100!  Perhaps try actually timing how much time elapses between each measurement rather than assuming it's exactly 1second
        cout     << "," << "DISK" << setw(2) << disk << "=" << setw(3) << disk_utilisation;
        log_file << "," <<           setw(2) << disk << "," << setw(3) << disk_utilisation;
    }

    cout     << endl;
    log_file << endl;
}

int main(int argc, char* argv[])
{
    // read config file
    // start logging power consumption
    // start logging system workload
    // fire off a sequence of 'stress' workloads

    fstream log_file; ///< Generate base filename for log files
    string filename_base = generate_filename();
    cout << "Base filename = " << filename_base << endl;
    string filename = "stress-log-";
    filename.append( filename_base );
    filename.append( ".csv" );
    log_file.open( filename.c_str(), fstream::out | fstream::app );
    if ( ! log_file.good() ) {
        cerr << "Cannot open " << filename << endl;
        exit(1);
    }

    time_t start_time = time(NULL);

    /* Set a signal handler for SIGCHLD (which we receive
     *  when a child (i.e. "stress") terminates
     *  This code is adapted from
     *  gnu.org/s/hello/manual/libc/Sigaction-Function-Example.html
     */
    struct sigaction sigchld_action;
    sigchld_action.sa_handler = sigchld_handler;
    sigemptyset(&sigchld_action.sa_mask);
    sigchld_action.sa_flags = 0;
    sigaction(SIGCHLD, &sigchld_action, NULL);

    /* Instantiate Workload singleton */
    Workload * workload = Workload::get_instance();

    /**
     * Set workload config
     */
    struct Workload_config workload_config;
    workload_config.cpu=1;
    workload_config.io=0;
    workload_config.vm=0;
    workload_config.vm_bytes=128;
    workload_config.hdd=1;
    workload_config.timeout=10;
    workload_config.filename_base = filename_base;
    workload_config.start_time = start_time;
    int * workload_number = workload->set_workload_config(&workload_config);

    // TODO find the number of physical CPUs http://software.intel.com/en-us/articles/intel-64-architecture-processor-topology-enumeration/
    // TODO figure out where laptop gets power consumption

    cout << "Instantiating WattsUp..." << endl;
    WattsUp wu;

    /* Log until workload finishes */
    while ( ! workload->finished()) {

        log_line(wu, workload_number, start_time, log_file);
        log_file.flush();

        if ((time(NULL)-start_time) == 10) {
            /* Kick off first workload after 10 seconds */
            cout << "Kicking off first workload..." << endl;
            workload->next();
        }
    }

    cout << "parent terminating" << endl;

    log_file.close();

    return 0;
}
