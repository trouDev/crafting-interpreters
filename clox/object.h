#ifndef clox_object_h
#define clox_object_h

#include "value.h"

typedef enum
{
  OBJ_STRING
} ObjType;

struct Obj
{
  ObjType type;
  bool isMarked;
  struct Obj *next;
};

struct ObjString
{
  Obj obj;
  int length;
  char *chars;
  uint32_t hash;
};

ObjString *copyString(const char *chars, int length);
ObjString *takeString(char *chars, int length);
void printObject(Value value);
void freeObjects(Obj *objects);

#endif
