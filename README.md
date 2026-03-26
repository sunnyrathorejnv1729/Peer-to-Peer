## Run the program using commands

cd tracker
g++ tracker.cpp -o tracker
./tracker tracker_info.txt 1

cd client
g++ client.cpp -o client -lssl -lcrypto
./client ip:port tracker_info.txt

## key features implemented

- Create user
- Login user
- Create group
- Join group
- Accept group joining request
- Leave group
- List all groups
- List of pending request
- List all files in a group
- Logout
- Upload file


