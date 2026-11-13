#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "vm.h"

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

static ObjString *allocateString(char *chars, int length, uint32_t hash)
{
  ObjString *string = ALLOCATE(ObjString, 1);
  string->obj.type = OBJ_STRING;
  string->obj.isMarked = false;
  string->obj.next = vm.objects;
  vm.objects = (Obj *)string;
  string->length = length;
  string->chars = chars;
  string->hash = hash;
  return string;
}

ObjString *takeString(char *chars, int length)
{
  return allocateString(chars, length, hashString(chars, length));
}

ObjString *copyString(const char *chars, int length)
{
  char *heapChars = ALLOCATE(char, length + 1);
  memcpy(heapChars, chars, length);
  heapChars[length] = '\0';
  return takeString(heapChars, length);
}

void printObject(Value value)
{
  ObjString *string = AS_STRING(value);
  printf("%s", string->chars);
}

void freeObjects(Obj *object)
{
  while (object != NULL)
  {
    Obj *next = object->next;
    ObjString *string = (ObjString *)object;
    FREE_ARRAY(char, string->chars, string->length + 1);
    FREE(ObjString, string);
    object = next;
  }
  vm.objects = NULL;
}
