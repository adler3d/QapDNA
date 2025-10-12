#include <thread>
#include <iostream>
#include <chrono>
int main(){
  using namespace std::chrono_literals;
  for(int i=0;i<100;i++){
    std::cout<<i<<std::endl;
    std::this_thread::sleep_for(1000ms);
  }
  return 0;
}