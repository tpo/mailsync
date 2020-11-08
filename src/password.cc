#include "store.h"

void Password::clear() {
  text = "";
  nopasswd = true;
}

void Password::set_passwd(string passwd) {
  nopasswd = false;
  text = passwd;
}

