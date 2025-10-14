echo ./QapLR.exe t_splinter 4 9 0 -p 31500 -o replay.cmds &
./socket_adapter.exe -e 127.0.0.1:31500 ./gpt5.exe&
./socket_adapter.exe -e 127.0.0.1:31501 ./gpt5.exe&
./socket_adapter.exe -e 127.0.0.1:31502 ./gpt5.exe&
./socket_adapter.exe -e 127.0.0.1:31503 ./gpt5.exe&

echo ./socket_adapter.exe -e 127.0.0.1:31500 ./adler20250907.exe&
