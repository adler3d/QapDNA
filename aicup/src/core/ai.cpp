#include <iostream>
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <iostream>

using namespace std;
int main( int argc, char * argv[] )
{
  _setmode( _fileno( stdout ),  _O_BINARY );
  _setmode( _fileno( stderr ),  _O_BINARY );
  for(int i=0;;i++){
    //cout<<"CMD"<<i<<endl;
    cerr<<"ERR"<<i<<endl;
    //cout<<"TYPE ANY INT"<<endl;
    Sleep(16);
  }
  int i=0;
  cin>>i;
  cout<<"I="<<i<<endl;
  return 0;
}