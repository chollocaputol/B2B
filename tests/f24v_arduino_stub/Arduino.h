#pragma once
// Host-only compatibility shim used to COMPILE the captured real FS.cpp.
// NOT an Arduino board header, not a substitute for installed source.
#include <cstdint>
#include <cstddef>
#include <ctime>
#include <string>
using boolean=bool;
class String {
 public:
  String()=default;
  String(const char* t):v_(t?t:""){}
  const char* c_str()const{return v_.c_str();}
 private: std::string v_;
};
class Print {
 public:
  virtual ~Print()=default;
  virtual size_t write(uint8_t)=0;
  virtual size_t write(const uint8_t*,size_t)=0;
};
class Stream:public Print {
 public:
  unsigned long _timeout=0;
  virtual int available()=0;
  virtual int read()=0;
  virtual int peek()=0;
  virtual void flush()=0;
};
#ifndef log_w
#define log_w(...) ((void)0)
#endif
