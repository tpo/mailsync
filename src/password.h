#ifndef __PASSWORD__

#include <string>

using namespace std;

//////////////////////////////////////////////////////////////////////////
//
class Password
//
// Structure that holds the password
//
//////////////////////////////////////////////////////////////////////////
{
  public:
    bool nopasswd;
    string text;

    void clear();
    void set_passwd(string passwd);
};

#define __PASSWORD__
#endif
