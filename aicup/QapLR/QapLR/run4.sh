./QapLR.exe t_splinter 4 9 0 -p 31000 -o replay.cmds &
echo ./socket_adapter.exe -e 127.0.0.1:31000 ./adler20250907.exe&
./socket_adapter.exe -e 127.0.0.1:31000 ./gpt5.exe&
./socket_adapter.exe -e 127.0.0.1:31001 ./gpt5.exe&
./socket_adapter.exe -e 127.0.0.1:31002 ./gpt5.exe&
./socket_adapter.exe -e 127.0.0.1:31003 ./gpt5.exe&