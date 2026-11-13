#include <stdlib.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "vm.h"

static Obj **grayStack;
static int grayCount;
static int grayCapacity;

static void markArray(ValueArray *array)
{
  for (int i = 0; i < array->count; i++) markValue(array->values[i]);
}

void markObject(Obj *object)
{
  if (object == NULL || object->isMarked) return;
  object->isMarked = true;

  if (grayCapacity < grayCount + 1)
  {
    grayCapacity = GROW_CAPACITY(grayCapacity);
    grayStack = realloc(grayStack, sizeof(Obj *) * grayCapacity);
    if (grayStack == NULL) exit(1);
  }
  grayStack[grayCount++] = object;
}

void markValue(Value value)
{
  if (IS_OBJ(value)) markObject(AS_OBJ(value));
}

static void markRoots(void)
{
  for (Value *slot = vm.stack; slot < vm.stackTop; slot++) markValue(*slot);
  for (int i = 0; i < vm.switchTop; i++) markValue(vm.switchStack[i]);
  markTable(&vm.globals);
  if (vm.chunk != NULL) markArray(&vm.chunk->constants);
}

static void traceReferences(void)
{
  while (grayCount > 0)
  {
    Obj *object = grayStack[--grayCount];
    // Strings do not contain references to other managed objects.
    (void)object;
  }
}

static void sweep(void)
{
  Obj **object = &vm.objects;
  while (*object != NULL)
  {
    Obj *unreached = *object;
    if (unreached->isMarked)
    {
      unreached->isMarked = false;
      object = &unreached->next;
    }
    else
    {
      *object = unreached->next;
      ObjString *string = (ObjString *)unreached;
      FREE_ARRAY(char, string->chars, string->length + 1);
      FREE(ObjString, string);
    }
  }
}

void collectGarbage(void)
{
  markRoots();
  traceReferences();
  tableRemoveWhite(&vm.globals);
  sweep();
  vm.nextGC = vm.bytesAllocated * 2;
  if (vm.nextGC < 1024 * 1024) vm.nextGC = 1024 * 1024;
}

void *reallocate(void *pointer, size_t oldSize, size_t newSize)
{
  vm.bytesAllocated += newSize - oldSize;

  if (newSize > oldSize && vm.bytesAllocated > vm.nextGC)
    collectGarbage();

  if (newSize == 0)
  {
    free(pointer);
    return NULL;
  }

  void *result = realloc(pointer, newSize);
  if (result == NULL) exit(1);
  return result;
}
