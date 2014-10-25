#include <iostream>

#include "dpe_base/chromium_base.h"

using namespace std;


void HelloUI()
{
  cerr << "hello UI" << endl;
}

int main()
{
  cerr << base::SysWideToUTF8(L"123") << endl;
  return 0;
}
