/// Copyright (C) strawberryhacker

void* malloc(int size)
{
    size += 5000;
    return (void *)size;
}
