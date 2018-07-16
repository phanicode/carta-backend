#include <vector>
#include <fmt/format.h>
#include <uWS/uWS.h>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/program_options.hpp>
#include <proto/fileLoadRequest.pb.h>
#include <proto/regionReadRequest.pb.h>
#include <proto/profileRequest.pb.h>
#include <regex>
#include <fstream>
#include <iostream>
#include <carta-protobuf/close_file.pb.h>
#include "ctpl.h"
#include "Session.h"

#define MAX_THREADS 4

using namespace std;
using namespace uWS;
namespace po = boost::program_options;

map<WebSocket<SERVER>*, Session*> sessions;
map<string, vector<string>> permissionsMap;
boost::uuids::random_generator uuid_gen;

string baseFolder = "./";
bool verbose = false;
ctpl::thread_pool threadPool;

// Reads a permissions file to determine which API keys are required to access various subdirectories
void readPermissions(string filename) {
    ifstream permissionsFile(filename);
    if (permissionsFile.good()) {
        fmt::print("Reading permissions file\n");
        string line;
        regex commentRegex("\\s*#.*");
        regex folderRegex("\\s*(\\S+):\\s*");
        regex keyRegex("\\s*(\\S{4,}|\\*)\\s*");
        string currentFolder;
        while (getline(permissionsFile, line)) {
            smatch matches;
            if (regex_match(line, commentRegex)) {
                continue;
            } else if (regex_match(line, matches, folderRegex) && matches.size() == 2) {
                currentFolder = matches[1].str();
            } else if (currentFolder.length() && regex_match(line, matches, keyRegex) && matches.size() == 2) {
                string key = matches[1].str();
                permissionsMap[currentFolder].push_back(key);
            }
        }
    } else {
        fmt::print("Missing permissions file\n");
    }
}

// Looks for null termination in a char array to determine event names from message payloads
string getEventName(char* rawMessage) {
    int nullIndex = 0;
    for (auto i = 0; i < 32; i++) {
        if (!rawMessage[i]) {
            nullIndex = i;
            break;
        }
    }
    return string(rawMessage, nullIndex);
}

// Called on connection. Creates session object and assigns UUID and API keys to it
void onConnect(WebSocket<SERVER>* ws, HttpRequest httpRequest) {
    sessions[ws] = new Session(ws, uuid_gen(), permissionsMap, baseFolder, threadPool, verbose);
    time_t time = chrono::system_clock::to_time_t(chrono::system_clock::now());
    string timeString = ctime(&time);
    timeString = timeString.substr(0, timeString.length() - 1);

    fmt::print("Client {} [{}] Connected ({}). Clients: {}\n", boost::uuids::to_string(sessions[ws]->uuid), ws->getAddress().address, timeString, sessions.size());
}

// Called on disconnect. Cleans up sessions. In future, we may want to delay this (in case of unintentional disconnects)
void onDisconnect(WebSocket<SERVER>* ws, int code, char* message, size_t length) {
    auto uuid = sessions[ws]->uuid;
    auto session = sessions[ws];
    if (session) {
        delete session;
        sessions.erase(ws);
    }
    time_t time = chrono::system_clock::to_time_t(chrono::system_clock::now());
    string timeString = ctime(&time);
    timeString = timeString.substr(0, timeString.length() - 1);
    fmt::print("Client {} [{}] Disconnected ({}). Remaining clients: {}\n", boost::uuids::to_string(uuid), ws->getAddress().address, timeString, sessions.size());
}

// Forward message requests to session callbacks after parsing message into relevant ProtoBuf message
void onMessage(WebSocket<SERVER>* ws, char* rawMessage, size_t length, OpCode opCode) {
    auto session = sessions[ws];

    if (!session) {
        fmt::print("Missing session!\n");
        return;
    }

    if (opCode == OpCode::BINARY) {
        if (length > 36) {
            string eventName = getEventName(rawMessage);
            uint32_t requestId = *((uint32_t*) (rawMessage + 32));
            void* eventPayload = rawMessage + 36;
            int payloadSize = (int) length - 36;

            //CARTA ICD
            if (eventName == "REGISTER_VIEWER") {
                CARTA::RegisterViewer message;
                if (message.ParseFromArray(eventPayload, payloadSize)) {
                    session->onRegisterViewer(message, requestId);
                }
            }
            else if (eventName == "FILE_LIST_REQUEST") {
                CARTA::FileListRequest message;
                if (message.ParseFromArray(eventPayload, payloadSize)) {
                    session->onFileListRequest(message, requestId);
                }
            }
            else if (eventName == "FILE_INFO_REQUEST") {
                CARTA::FileInfoRequest message;
                if (message.ParseFromArray(eventPayload, payloadSize)) {
                    session->onFileInfoRequest(message, requestId);
                }
            }
            else if (eventName == "OPEN_FILE") {
                CARTA::OpenFile message;
                if (message.ParseFromArray(eventPayload, payloadSize)) {
                    session->onOpenFile(message, requestId);
                }
            }
            else if (eventName == "SET_IMAGE_VIEW") {
                CARTA::SetImageView message;
                if (message.ParseFromArray(eventPayload, payloadSize)) {
                    session->onSetImageView(message, requestId);
                }
            }
            else if (eventName == "CLOSE_FILE") {
                CARTA::CloseFile message;
                if (message.ParseFromArray(eventPayload, payloadSize)) {
                    session->onCloseFile(message, requestId);
                }
            }
            else {
                fmt::print("Unknown event type {}\n", eventName);
            }
        }
    } else {
        fmt::print("Invalid event type\n");
    }
};

// Entry point. Parses command line arguments and starts server listening
int main(int argc, const char* argv[]) {
    try {
        po::options_description desc("Allowed options");
        desc.add_options()
            ("help", "produce help message")
            ("verbose", "display verbose logging")
            ("port", po::value<int>(), "set server port")
            ("threads", po::value<int>(), "set thread pool count")
            ("folder", po::value<string>(), "set folder for data files");

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);

        if (vm.count("help")) {
            cout << desc << "\n";
            return 0;
        }

        verbose = vm.count("verbose");

        int port = 3002;
        if (vm.count("port")) {
            port = vm["port"].as<int>();
        }

        int threadCount = MAX_THREADS;
        if (vm.count("threads")) {
            threadCount = vm["threads"].as<int>();
        }
        threadPool.resize(threadCount);

        if (vm.count("folder")) {
            baseFolder = vm["folder"].as<string>();
        }

        readPermissions("permissions.txt");

        Hub h;
        h.onMessage(&onMessage);
        h.onConnection(&onConnect);
        h.onDisconnection(&onDisconnect);
        if (h.listen(port)) {
            h.getDefaultGroup<uWS::SERVER>().startAutoPing(5000);
            fmt::print("Listening on port {} with data folder {} and {} threads in thread pool\n", port, baseFolder, threadCount);
            h.run();
        } else {
            fmt::print("Error listening on port {}\n", port);
            return 1;
        }
    }
    catch (exception& e) {
        fmt::print("Error: {}\n", e.what());
        return 1;
    }
    catch (...) {
        fmt::print("Unknown error\n");
        return 1;
    }
    return 0;
}