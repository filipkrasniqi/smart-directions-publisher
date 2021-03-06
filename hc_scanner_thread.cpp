//
// Created by Filip Krasniqi on 03/11/2020.
//

#include <iostream>
#include "hc_scanner_thread.h"
#include <thread>
#include <atomic>
#include <sstream>
#include <cstdio>
#include <vector>
#include <mosquitto.h>
#include <bits/stdc++.h>
#include <unistd.h>
#include "utils/process/shell_process.h"

void HCScannerThread::execute() {
    this->terminate = false;
    while(!this->terminate) {
        /// executing process in s.w.t. I take its stdout; won't work with ShellProcess class
        const char *cmd = "btmon";
        FILE* stream = popen(cmd, "r");
        std::string currentUUID = "";
        std::string rssi = "";
        char delim = ' ';
        const int max_buffer = 256;
        char buffer[max_buffer];
        while (!feof(stream) && !this->terminate) {
            if (fgets(buffer, max_buffer, stream) != NULL) {
                // data.append(buffer);
                std::string current_line = buffer;

                /// defining whether I am in a useless row, taking UUID or RSSI
                std::size_t found_UUID = current_line.find("Data: beac");
                std::size_t found_rssi = current_line.find("RSSI");
                /// this instead tells whether previous group was useless
                std::size_t found_start_group = current_line.find("HCI Event");

                if(found_start_group!=std::string::npos) {
                    // start from beginning
                    rssi = "";
                    currentUUID = "";
                    //std::cout << "Cleaning"<<std::endl;
                }

                if (found_UUID!=std::string::npos || found_rssi!=std::string::npos) {
                    /// something good.
                    /// Text has some spaces at first, then it contains Data: beac<UUID> or RSSI: <val> <junk>
                    std::string sanitizedString = "";
                    int i = 0;
                    bool hasEncounteredChar = false;
                    /// sanitizing string by trimming first part
                    while(i < current_line.length()) {
                        hasEncounteredChar = hasEncounteredChar || (current_line[i] != ' ');
                        if(hasEncounteredChar) {
                            sanitizedString += current_line.at(i);
                        }
                        i++;
                    }

                    i = 0;

                    if(found_UUID!=std::string::npos) {
                        // it means we are in the correct block
                        currentUUID = sanitizedString.substr(10, 32);
                        //std::cout << "Found UUID: "<<currentUUID<<std::endl;
                    }

                    if(found_rssi != std::string::npos) {
                        std::size_t pos_dbm = sanitizedString.find(" dBm");
                        if(pos_dbm == std::string::npos) {
                            //std::cout << "Something wrong here..." << std::endl;
                        } else {
				            rssi = sanitizedString.substr(6, pos_dbm-6);
                            //std::cout << "Found RSSI: "<<std::stoi(rssi)<<std::endl;
                        }

                    }
                }
            }
		
            /// whenever I have both values it means I parsed data for one device. I send them to the broker.
            if(!rssi.empty() && !currentUUID.empty()) {
                std::string current_timestamp = std::to_string(std::time(0));

                std::string msg_string = (currentUUID + "$" + rssi + "$" + current_timestamp);
                this->publisher->publish("directions/anchor/proximity", msg_string);
                rssi = "";
                currentUUID = "";
                // usleep(5000);                         // between every publish, we sleep
            }
        }
        std::cout << "Finished stream!!!" << std::endl; // never enters here
        usleep(200000);
    }
    std::cout << "Scanner thread stopped" << std::endl;
}

void HCScannerThread::end() {
    this->terminate = true;
}

/**
* Runs lescan and starts the thread running btmon. 
* Everytime something relevant is found, it is published via MQTT
*/
std::thread HCScannerThread::start(MQTTPublisher *publisher) {
    this->publisher = publisher;
    ShellProcess *process = new ShellProcess();
	process->startProcess("hciconfig hci0 down && hciconfig hci0 up && hcitool lescan --duplicates");
    return std::thread([=] { this->execute(); });
}
