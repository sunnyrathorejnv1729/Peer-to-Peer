#include <bits/stdc++.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <fcntl.h>

using namespace std;

class Tracker {
private:
    int sock_fd;
    sockaddr_in address;
    map<int, string> clients;
    map<string, string> users;
    map<string, set<string>> groups;        
    map<string, string> group_owners; 
    map<int,string>client_login;
    map<string,int> logged_in_users;
    map<string,set<string>> pending_requests;
    map<string,map<string,vector<string>>>files;
    mutex client_mutex;
    bool is_running;

public:
    Tracker(const string &ip, int port) : is_running(true) {
        sock_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (sock_fd == 0) {
            perror("Socket creation failed");
            exit(EXIT_FAILURE);
        }

        int opt = 1;
        if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
            perror("setsockopt failed");
            close(sock_fd);
            exit(EXIT_FAILURE);
        }

        address.sin_family = AF_INET;
        address.sin_port = htons(port);

        if (inet_pton(AF_INET, ip.c_str(), &address.sin_addr) <= 0) {
            cout << "Invalid IP address: " << ip << "\n";
            exit(EXIT_FAILURE);
        }

        if (bind(sock_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
            perror("Bind failed");
            close(sock_fd);
            exit(EXIT_FAILURE);
        }

        if (listen(sock_fd, 10) < 0) {
            perror("Listen failed");
            close(sock_fd);
            exit(EXIT_FAILURE);
        }

        cout << "Tracker listening on port " << port << "..." << "\n";
    }

    void Communicate_with_ClientQueries(int client_socket) {
        char buffer[1024] = {0};

        while (true) {
            memset(buffer, 0, sizeof(buffer));
            int bytes_read = read(client_socket, buffer, sizeof(buffer));
            if (bytes_read <= 0) {
                if (bytes_read == 0) {
                    cout << "Client disconnected.\n";
                } else {
                    perror("Error reading from client");
                }
                break;
            }

            string command(buffer);
            vector<string> tokens;
            stringstream ss(command);
            string token;


            if (tokens.size() == 0) {
                cout << "Empty Query Received.\n";
                continue;
            }

            else if (tokens[0] == "create_user") {
                if (tokens.size() == 3) {
                    createUser(tokens[1], tokens[2], client_socket);
                } else {
                    string response = "Invalid login command\n";
                    send(client_socket, response.c_str(), response.size(), 0);
                }
            } 
            else if (tokens.size()==3 && tokens[0] == "./client") {
                std::string str = tokens[1];   
                char s[1024];                   
                strcpy(s, str.c_str());
                char *tok = strtok(s, ":");
                tok = strtok(NULL, ":");
                cout << "Client with port number " << tok << " is connected successfully...\n";
            }

            else if (tokens[0] == "login") {
                if (tokens.size() >= 3) {
                    login_user(tokens[1], tokens[2], client_socket);
                } else {
                    cout << "Invalid login command\n";
                    string response = "Invalid login command\n";
                    send(client_socket, response.c_str(), response.size(), 0);
                }
                continue;
            }

            else if (tokens[0] == "create_group") {
                if (tokens.size() == 2) {
                    creategroup(tokens[1], client_socket);
                } 
                else {
                    string response = "Invalid Create Group Command\n";
                    send(client_socket, response.c_str(), response.size(), 0);
                }
            }

            else if (tokens[0] == "join_group") {
                if (tokens.size() == 2) {
                    joingroup(tokens[1], client_socket);
                } 
                else {
                    string response = "Invalid Join Group Command\n";
                    send(client_socket, response.c_str(), response.size(), 0);
                }
            }

            else if(tokens[0] == "list_files"){
                if(tokens.size() == 2){
                    Listfiles(tokens[1],client_socket);
                }
                else{
                    string response = "Invalid Join Group Command\n";
                    send(client_socket, response.c_str(), response.size(), 0);
                }
            }

            else if (tokens[0] == "leave_group") {
                if (tokens.size() == 2) {
                    leaveGroup(tokens[1], client_socket);
                } 
                else {
                    string response = "Invalid Join Group Command\n";
                    send(client_socket, response.c_str(), response.size(), 0);
                }
            }



            else if (tokens[0] == "list_requests") {
                if (tokens.size() == 2) {
                    ListRequest(tokens[1],client_socket);
                } 
                else {
                    string response = "Invalid List Request Command\n";
                    send(client_socket, response.c_str(), response.size(), 0);
                }
            }


            else if (tokens[0] == "list_groups") {
                if (tokens.size() == 1) {
                    ListGroups(client_socket);
                } 
                else {
                    string response = "Invalid Pending List Command\n";
                    send(client_socket, response.c_str(), response.size(), 0);
                }
            }

            else if (tokens[0] == "accept_request") {
                if (tokens.size() == 3) {
                    AcceptRequest(tokens[1],tokens[2],client_socket);
                } 
                else {
                    string response = "Invalid Pending List Command\n";
                    send(client_socket, response.c_str(), response.size(), 0);
                }
            }

            else if (tokens[0] == "logout") {
                if (tokens.size() == 1) {
                    Logout(client_socket);
                } 
                else {
                    string response = "Invalid logout Command\n";
                    send(client_socket, response.c_str(), response.size(), 0);
                }
            }

            else if(tokens[0]=="upload_file"){

                if(tokens.size()==3){
                    string filename;
                    size_t pos = tokens[1].find_last_of("/");
                    if (pos != string::npos) 
                    filename = tokens[1].substr(pos + 1);
                    UploadFile(tokens[1],tokens[2],client_socket);
                }
                else{
                    string response = "Invalid upload Command\n";
                    send(client_socket, response.c_str(), response.size(), 0);
                }
            }


            else{
                string response = "Invalid Command\n";
                send(client_socket, response.c_str(), response.size(), 0);
            }

            string client_info = string(buffer);
            {
                lock_guard<mutex> lock(client_mutex);
                clients[client_socket] = client_info;
                string all_clients;
                for (const auto& [sock, info] : clients) {
                    if (sock != client_socket) { 
                        all_clients += info + "\n";
                    }
                }

            }

        }

        close(client_socket);
    }

    void wait_for_connections() {
        while (is_running) {
            int client_socket;
            sockaddr_in client_addr;
            socklen_t client_addr_len = sizeof(client_addr);
            client_socket = accept(sock_fd, (struct sockaddr *)&client_addr, &client_addr_len);
            if (client_socket < 0) {
                cout<<"Accept failed"<<"\n";
                continue;
            }
            else{
                int tok = ntohs(client_addr.sin_port);
                cout << "Client with port number " << tok << " is connected successfully...\n";
            }
            thread client_thread(&Tracker::Communicate_with_ClientQueries, this, client_socket);
            client_thread.detach();
        }
    }

    void stop() {
        is_running = false;
        close(sock_fd);
        cout << "Tracker stopped.\n";
    }


    void createUser(const string &user_id, const string &passwd, int client_socket) {
        if (users.find(user_id) != users.end()) {
            string response = "User already exists\n";
            send(client_socket, response.c_str(), response.size(), 0);
            return;
        }
        users[user_id] = passwd;
        string response = "User created successfully\n";
        send(client_socket, response.c_str(), response.size(), 0);
    }





     void login_user(const string &user_id, const string &passwd, int client_socket) {
        if (users.find(user_id) == users.end()) {
            string response = "User does not exist\n";
            send(client_socket, response.c_str(), response.size(), 0);
        }

        else if (users[user_id] != passwd) {
            string response = "Incorrect password\n";
            send(client_socket, response.c_str(), response.size(), 0);
        }

        else if (logged_in_users[user_id]!=0) {
            string response = "User already logged in\n";
            send(client_socket, response.c_str(), response.size(), 0);
        }
        else{
        logged_in_users[user_id] = client_socket;
        client_login[client_socket] = user_id;
        string response = "Login successful\n";
        send(client_socket, response.c_str(), response.size(), 0);
        }
    }


    void creategroup(const string &group_id, int client_socket){
        lock_guard<mutex> lock(client_mutex);

        if(client_login[client_socket]==""){
            string response = "No user is currently login with this client\n";
            send(client_socket, response.c_str(), response.size(), 0);
        }

        else if (groups[group_id].empty()) {
            groups[group_id].insert(client_login[client_socket]); 
            group_owners[group_id] = client_login[client_socket]; 
            cout<<"Group created successfully with user id "<< client_login[client_socket]<<"\n";
            string response = "Group created successfully\n";
            send(client_socket, response.c_str(), response.size(), 0);
        }
        else {
            string response = "Group already exists\n";
            send(client_socket, response.c_str(), response.size(), 0);
        }
    }


    void joingroup(const string &group_id, int client_socket) {
        if(client_login[client_socket]==""){
            string response = "No user is currently login with this client\n";
            send(client_socket, response.c_str(), response.size(), 0);
            return;
        }

        else if (groups[group_id].empty()){
            string response = "Group does not exist\n";
            send(client_socket, response.c_str(), response.size(), 0);
            return;
        }

        else if (find(groups[group_id].begin(),groups[group_id].end(),client_login[client_socket]) != groups[group_id].end()) {
            string response = "User is already in the group\n";
            send(client_socket, response.c_str(), response.size(), 0);
            return;
        }

        pending_requests[group_id].insert(client_login[client_socket]); 
        string response = "Join Group reguest is sent successfully\n";
        send(client_socket, response.c_str(), response.size(), 0);
    }


    void ListRequest(const string &group_id, int client_socket){

        if(pending_requests[group_id].empty()){
            string response = "Currently there is no pending request for this group\n";
            send(client_socket, response.c_str(), response.size(), 0);
        }
        else{
            string response = "---------------------Pending List---------------------\n";
            int n = pending_requests[group_id].size();
            for(auto it: pending_requests[group_id]){
                response+=it + "\n";
            }
            send(client_socket, response.c_str(), response.size(), 0);
        }
    }

    void ListGroups(int client_socket){

        if(group_owners.size()==0){
            string response = "Currently there are no groups formed\n";
            send(client_socket, response.c_str(), response.size(), 0);
        }
        else{
            string response = "---------------------Group List---------------------\n";
            for(auto it: group_owners){
                response+=it.first + "\n";
            }
            send(client_socket, response.c_str(), response.size(), 0);
        }
    }

    void leaveGroup(const string &group_id, int client_socket){
        if(client_login[client_socket]==""){
            string response = "No user is currently login with this client\n";
            send(client_socket, response.c_str(), response.size(), 0);
            return;
        }
        else if (find(groups[group_id].begin(),groups[group_id].end(),client_login[client_socket]) == groups[group_id].end()) {
            string response = "User already not in this group\n";
            send(client_socket, response.c_str(), response.size(), 0);
            return;
        }
        else{
            if(group_owners[group_id] == client_login[client_socket]){
                if(groups[group_id].size()==1){
                    group_owners.erase(group_id);
                    pending_requests.erase(group_id);
                    files.erase(group_id);
                }
                else{
                    auto new_owner = groups[group_id].begin();
                    new_owner++;
                    group_owners[group_id] = *(new_owner);
                }
            }
            groups[group_id].erase(client_login[client_socket]); 
            string response = "User leave the group successfully\n";
            send(client_socket, response.c_str(), response.size(), 0);
        }
        
    }

    void AcceptRequest(const string &group_id, const string &user_id, int client_socket){

        if (groups[group_id].empty()){
            string response = "Group does not exist\n";
            send(client_socket, response.c_str(), response.size(), 0);
            return;
        }

        else if(group_owners[group_id] != client_login[client_socket]){
            string response = "owner is not logged in with this client\n";
            send(client_socket, response.c_str(), response.size(), 0);
            return;
        }
        else if (find(groups[group_id].begin(),groups[group_id].end(),user_id) != groups[group_id].end()) {
            string response = "User is already in the group\n";
            send(client_socket, response.c_str(), response.size(), 0);
            return;
        }

        else{
            if(pending_requests[group_id].size()==1){
                pending_requests.erase(group_id);
                groups[group_id].insert(user_id);
                
            }
            else{
                pending_requests[group_id].erase(user_id);
                groups[group_id].insert(user_id);
            }
            string response = "Accept request is successful\n";
            send(client_socket, response.c_str(), response.size(), 0);
            return;
        }

    }

    void Logout(int client_socket){
        if(client_login[client_socket]==""){
            string response = "Currently there is no user Logged in\n";
            send(client_socket, response.c_str(), response.size(), 0);
            return;
        }
        logged_in_users.erase(client_login[client_socket]);
        client_login[client_socket]="";
        string response = "User logout Successfully\n";
        send(client_socket, response.c_str(), response.size(), 0);
        return;
    }

    void Listfiles(const string &group_id,int client_socket){

        if(client_login[client_socket]==""){
            string response = "No user is currently login with this client\n";
            send(client_socket, response.c_str(), response.size(), 0);
        }

        else if (groups[group_id].empty()){
            string response = "Group does not exist\n";
            send(client_socket, response.c_str(), response.size(), 0);
            return;
        }

        else if(groups[group_id].find(client_login[client_socket]) == groups[group_id].end()){
            string response = "User is not a member of this group\n";
            send(client_socket, response.c_str(), response.size(), 0);
            return;
        }

        else{
            string response = "---------------------List Files---------------------\n";
            for(auto it: files[group_id]){
                response+=it.first + "\n";
            }
            send(client_socket, response.c_str(), response.size(), 0);
        }
    }
    void UploadFile(const string &filename,const string &group_id,int client_socket){
        files[group_id][filename].push_back("1");
    }



};

void wait_for_quit_command(Tracker &tracker) {
    string command;
    while (true) {
        cin >> command;
        if (command == "quit") {
            tracker.stop();
            break;
        }
    }
}

pair<string,int> get_Tracker_Info(const string &filename, vector<pair<string, int>> &tracker_info) {
    int fd = open(filename.c_str(), O_RDONLY);
    if (fd < 0) {
        cout<<"Unable to open the tracker_info file"<<"\n";
        exit(EXIT_FAILURE);
    }

    const size_t buffer_size = 1024;
    char buffer[buffer_size];
    ssize_t bytes_read = read(fd, buffer, buffer_size);

    if (bytes_read < 0) {
        cout<<"Unable to read the tracker_info file"<<"\n";
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

int main(int argc, char const *argv[]) {
    if (argc != 3) {
        cout << "Invalid tracker arguments\n";
        return EXIT_FAILURE;
    }

    string tracker_file = argv[1];
    int tracker_no = stoi(argv[2]);

    vector<pair<string, int>> tracker_info;
    pair<string, int> temp = get_Tracker_Info(tracker_file, tracker_info);

    if (tracker_no < 0 || tracker_no > 2) {
        cout << "Invalid tracker number\n";
        return EXIT_FAILURE;
    }

    string ip = temp.first;
    int port = temp.second;

    Tracker tracker(ip, port);
    thread tracker_thread(&Tracker::wait_for_connections, &tracker);
    thread quit_thread(wait_for_quit_command, ref(tracker));

    quit_thread.join();  
    tracker_thread.join(); 
    return 0;
}
