pushd ../../QapLR/QapLR/
git pull
g++ -ggdb -DQAP_UNIX -O0 QapLR.cpp -oQapLR
mv QapLR ../../src/core
popd
ls -lht QapLR
