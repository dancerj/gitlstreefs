#ifndef DISALLOW_H_
#define DISALLOW_H_

#define DISALLOW_COPY_AND_ASSIGN(Type) \
  Type(const Type&) = delete;          \
  void operator=(const Type&) = delete;

#endif
