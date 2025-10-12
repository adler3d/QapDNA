#include <thread>
#include <iostream>
#include <chrono>
#include <string>
using namespace std;
int main(){
  using namespace std::chrono_literals;
  for(int i=0;i<100;i++){
    std::cout<<i<<std::endl;
    string s;
    cin>>s;
    cerr<<s<<endl;
    std::this_thread::sleep_for(1000ms);
  }
  return 0;
}