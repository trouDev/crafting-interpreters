#include <stdio.h>
#include <string.h>
#include "memory.h"
#include "object.h"

static Obj *objects;

static uint32_t hashString(const char *key, int length)
{
  uint32_t hash = 2166136261u;
  for (int i = 0; i < length; i++)
  {
    hash ^= (uint8_t)key[i];
    hash *= 16777619;
  }
  return hash;
}

static ObjString *allocateString(const char *chars, int length,
                                 uint32_t hash)
{
  ObjString *string = (ObjString *)reallocate(
      NULL, 0, sizeof(ObjString) + (size_t)length + 1);
  string->obj.type = OBJ_STRING;
  string->obj.next = objects;
  objects = (Obj *)string;
  string->length = length;
  string->hash = hash;
  memcpy(string->chars, chars, length);
  string->chars[length] = '\0';
  return string;
}

ObjString *copyString(const char *chars, int length)
{
  return allocateString(chars, length, hashString(chars, length));
}

ObjString *takeString(char *chars, int length)
{
  ObjString *string = allocateString(chars, length, hashString(chars, length));
  FREE_ARRAY(char, chars, length + 1);
  return string;
}

void printObject(Value value) { printf("%s", AS_STRING(value)->chars); }

void freeObjects(Obj *object)
{
  while (object != NULL)
  {
    Obj *next = object->next;
    FREE(ObjString, (ObjString *)object);
    object = next;
  }
  objects = NULL;
}
