#include <bits/stdc++.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <cstring>
#include <thread>
#include <fcntl.h>
#include <openssl/sha.h>
#include <openssl/crypto.h>


using namespace std;

class Client {
private:
    int socket_fd;
    sockaddr_in server_address;
    bool is_running;

public:
    Client(const string &server_ip, int server_port, const string &client_ip, int client_port) {
        socket_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_fd < 0) {
            perror("Unable to create socket");
            exit(EXIT_FAILURE);
        }

        int opt = 1;
        if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
            perror("setsockopt failed");
            close(socket_fd);
            exit(EXIT_FAILURE);
        }

        server_address.sin_family = AF_INET;
        server_address.sin_port = htons(server_port);
        inet_pton(AF_INET, server_ip.c_str(), &server_address.sin_addr);



        sockaddr_in client_addr;
        client_addr.sin_family = AF_INET;
        client_addr.sin_port = htons(0);
        inet_pton(AF_INET, client_ip.c_str(), &client_addr.sin_addr);

        if (bind(socket_fd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
            perror("Client bind failed");
            close(socket_fd); 
            exit(EXIT_FAILURE);
        }

        is_running = true;
    }

    void connect_with_tracker(int tracker_port,const string &client_ip, int client_port) {
        if (connect(socket_fd, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
            perror("Connecting with the tracker fails");
            close(socket_fd);
            exit(EXIT_FAILURE);
        }
        cout<<"Server listening on port "<<tracker_port<<"...."<<"\n";
        cout << "Connected to server....\n";
    }

    void send_message_to_tracker(const string &request) {
        send(socket_fd, request.c_str(), request.size(), 0);
    }

    void message_from_client() {
        while (is_running) {
            char buffer[1024] = {0};
            int bytes_received = read(socket_fd, buffer, sizeof(buffer));

            if (bytes_received == 0) {
                cout << "Tracker closed the connection.\n";
                is_running = false;
                break;
            } 
            
            else if (bytes_received < 0) {
                perror("Error receiving data from tracker");
                is_running = false;
                break;
            } 
            
            else {
                cout << buffer << "\n";
            }
        }
    }

    void stop() {
        is_running = false;
        close(socket_fd);
    }

    ~Client() {
        if (is_running) {
            close(socket_fd);
        }
    }
};

string divide_and_hash(string filepath, string groupid) {
    size_t size = 512 * 1024;  
    size_t Size;
    unsigned char buffer[size];
    unsigned char hash[20];

    int fd = open(filepath.c_str(), O_RDONLY);
    if (fd < 0) {
        cout<<"Error In Opening the file"<<"\n";
        return "-1";
    }

    struct stat stat_buf;
    if (stat(filepath.c_str(), &stat_buf) == 0) {
        Size = stat_buf.st_size;
    }
    else{
        cout<<"Error in geting file size"<<"\n";
        return "-1";
    }

    ssize_t bytes_read;
    string filename;
    size_t pos = filepath.find_last_of("/");
    string s = "";
    s+="upload";
    s+=" ";
    s+=filepath + ",";

    if (pos != string::npos) 
        filename = filepath.substr(pos + 1);

    s+=filename + ",";
    unsigned char buffer1[Size];
    bytes_read = read(fd, buffer1, Size);
    SHA1(buffer1, bytes_read, hash);
    char *sst = OPENSSL_buf2hexstr(hash, 20);
    s+=string(sst)+",";
    lseek(fd, 0, SEEK_SET);


    while (Size>0) {

        bytes_read = read(fd, buffer, size);
        if (bytes_read < 0) {
        close(fd);
        perror("Error in reading the chunk");
        return "-1";
    }
        SHA1(buffer, bytes_read, hash);
        char *ss = OPENSSL_buf2hexstr(hash, 20);
        s+=string(ss);
        
        Size-=size;
        if(Size>0) s+=",";
    }

    close(fd);
    write(1,s.c_str(),sizeof(s));
    return s;
}


void Send_Loop(Client &client) {
    string input;
    while (true) {
        getline(cin, input);
        stringstream ss(input);
        string x;
        vector<string> word;
        while (ss >> x) {
            word.push_back(x);
        }
                
        if (input == "quit") {
            client.stop();
            break;
        }

        client.send_message_to_tracker(input);
    }
}

void clientThreadFunction(const string &tracker_ip, int tracker_port, const string &client_ip, int client_port) {
    Client client(tracker_ip, tracker_port, client_ip, client_port);
    client.connect_with_tracker(tracker_port,client_ip,client_port);
    thread receive_thread(&Client::message_from_client, &client);
    Send_Loop(client);
    receive_thread.join();
}

pair<string,int> get_tracker_info(const string &filename, vector<pair<string, int>> &tracker_info) {
    int fd = open(filename.c_str(), O_RDONLY);
    if (fd < 0) {
        perror("Unable to open the tracker_info file");
        exit(EXIT_FAILURE);
    }

    const size_t buffer_size = 1024;
    char buffer[buffer_size];
    ssize_t bytes_read = read(fd, buffer, buffer_size);

    if (bytes_read < 0) {
        perror("Error in reading the tracker_info file");
        close(fd);
        exit(EXIT_FAILURE);
    }
    buffer[bytes_read] = '\0';
    string ip;
    int port;
    char *token = strtok(buffer, " \n");
    while (token != nullptr) {
        ip = token;
        token = strtok(nullptr, " \n");
        if (token != nullptr) {
            port = stoi(token);
            tracker_info.push_back({ip, port});
            token = strtok(nullptr, " \n");
        }
    }

    close(fd);
    return {ip, port};
}

pair<string, int> extract_client_info(const string &input) {
    size_t pos = input.find(':');
    if (pos == string::npos) {
        cerr << "Invalid Client Address Format" << endl;
        exit(EXIT_FAILURE);
    }

    string ip = input.substr(0, pos);
    int port;
    try {
        port = stoi(input.substr(pos + 1));
    } catch (const invalid_argument& e) {
        cout << "Invalid port number format.\n";
        exit(EXIT_FAILURE);
    }

    return {ip, port};
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        cout << "Invalid Client Arguments\n";
        return EXIT_FAILURE;
    }

    string tracker_file = argv[2];

    vector<pair<string, int>> tracker_info;
    pair<string, int> temp = get_tracker_info(tracker_file, tracker_info);

    string tracker_ip = temp.first;
    int tracker_port = temp.second;

    pair<string, int> v = extract_client_info(argv[1]);

    string client_ip = v.first;
    int client_port = v.second;

    vector<thread> client_threads;
    client_threads.emplace_back(clientThreadFunction, tracker_ip, tracker_port, client_ip, client_port);

    for (auto &t : client_threads) {
        t.join();
    }

    return 0;
}
