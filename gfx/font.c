// Copyright (C) strawberryhacker

#include <gfx/font.h>
#include <citrus/lcd.h>
#include <citrus/cache.h>
#include <citrus/print.h>

const u8 mono9_bitmap[] = {
    0xAA, 0xA8, 0x0C, 0xED, 0x24, 0x92, 0x48, 0x24, 0x48, 0x91, 0x2F, 0xE4,
    0x89, 0x7F, 0x28, 0x51, 0x22, 0x40, 0x08, 0x3E, 0x62, 0x40, 0x30, 0x0E,
    0x01, 0x81, 0xC3, 0xBE, 0x08, 0x08, 0x71, 0x12, 0x23, 0x80, 0x23, 0xB8,
    0x0E, 0x22, 0x44, 0x70, 0x38, 0x81, 0x02, 0x06, 0x1A, 0x65, 0x46, 0xC8,
    0xEC, 0xE9, 0x24, 0x5A, 0xAA, 0xA9, 0x40, 0xA9, 0x55, 0x5A, 0x80, 0x10,
    0x22, 0x4B, 0xE3, 0x05, 0x11, 0x00, 0x10, 0x20, 0x47, 0xF1, 0x02, 0x04,
    0x00, 0x6B, 0x48, 0xFF, 0x00, 0xF0, 0x02, 0x08, 0x10, 0x60, 0x81, 0x04,
    0x08, 0x20, 0x41, 0x02, 0x08, 0x00, 0x38, 0x8A, 0x0C, 0x18, 0x30, 0x60,
    0xC1, 0x82, 0x88, 0xE0, 0x27, 0x28, 0x42, 0x10, 0x84, 0x21, 0x3E, 0x38,
    0x8A, 0x08, 0x10, 0x20, 0x82, 0x08, 0x61, 0x03, 0xF8, 0x7C, 0x06, 0x02,
    0x02, 0x1C, 0x06, 0x01, 0x01, 0x01, 0x42, 0x3C, 0x18, 0xA2, 0x92, 0x8A,
    0x28, 0xBF, 0x08, 0x21, 0xC0, 0x7C, 0x81, 0x03, 0xE4, 0x40, 0x40, 0x81,
    0x03, 0x88, 0xE0, 0x1E, 0x41, 0x04, 0x0B, 0x98, 0xB0, 0xC1, 0xC2, 0x88,
    0xE0, 0xFE, 0x04, 0x08, 0x20, 0x40, 0x82, 0x04, 0x08, 0x20, 0x40, 0x38,
    0x8A, 0x0C, 0x14, 0x47, 0x11, 0x41, 0x83, 0x8C, 0xE0, 0x38, 0x8A, 0x1C,
    0x18, 0x68, 0xCE, 0x81, 0x04, 0x13, 0xC0, 0xF0, 0x0F, 0x6C, 0x00, 0xD2,
    0xD2, 0x00, 0x03, 0x04, 0x18, 0x60, 0x60, 0x18, 0x04, 0x03, 0xFF, 0x80,
    0x00, 0x1F, 0xF0, 0x40, 0x18, 0x03, 0x00, 0x60, 0x20, 0x60, 0xC0, 0x80,
    0x3D, 0x84, 0x08, 0x30, 0xC2, 0x00, 0x00, 0x00, 0x30, 0x3C, 0x46, 0x82,
    0x8E, 0xB2, 0xA2, 0xA2, 0x9F, 0x80, 0x80, 0x40, 0x3C, 0x3C, 0x01, 0x40,
    0x28, 0x09, 0x01, 0x10, 0x42, 0x0F, 0xC1, 0x04, 0x40, 0x9E, 0x3C, 0xFE,
    0x21, 0x90, 0x48, 0x67, 0xE2, 0x09, 0x02, 0x81, 0x41, 0xFF, 0x80, 0x3E,
    0xB0, 0xF0, 0x30, 0x08, 0x04, 0x02, 0x00, 0x80, 0x60, 0x8F, 0x80, 0xFE,
    0x21, 0x90, 0x68, 0x14, 0x0A, 0x05, 0x02, 0x83, 0x43, 0x7F, 0x00, 0xFF,
    0x20, 0x90, 0x08, 0x87, 0xC2, 0x21, 0x00, 0x81, 0x40, 0xFF, 0xC0, 0xFF,
    0xA0, 0x50, 0x08, 0x87, 0xC2, 0x21, 0x00, 0x80, 0x40, 0x78, 0x00, 0x1E,
    0x98, 0x6C, 0x0A, 0x00, 0x80, 0x20, 0xF8, 0x0B, 0x02, 0x60, 0x87, 0xC0,
    0xE3, 0xA0, 0x90, 0x48, 0x27, 0xF2, 0x09, 0x04, 0x82, 0x41, 0x71, 0xC0,
    0xF9, 0x08, 0x42, 0x10, 0x84, 0x27, 0xC0, 0x1F, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x82, 0x82, 0xC6, 0x78, 0xE3, 0xA1, 0x11, 0x09, 0x05, 0x83, 0x21,
    0x08, 0x84, 0x41, 0x70, 0xC0, 0xE0, 0x40, 0x40, 0x40, 0x40, 0x40, 0x41,
    0x41, 0x41, 0xFF, 0xE0, 0xEC, 0x19, 0x45, 0x28, 0xA4, 0xA4, 0x94, 0x91,
    0x12, 0x02, 0x40, 0x5C, 0x1C, 0xC3, 0xB0, 0x94, 0x4A, 0x24, 0x92, 0x49,
    0x14, 0x8A, 0x43, 0x70, 0x80, 0x1E, 0x31, 0x90, 0x50, 0x18, 0x0C, 0x06,
    0x02, 0x82, 0x63, 0x0F, 0x00, 0xFE, 0x43, 0x41, 0x41, 0x42, 0x7C, 0x40,
    0x40, 0x40, 0xF0, 0x1C, 0x31, 0x90, 0x50, 0x18, 0x0C, 0x06, 0x02, 0x82,
    0x63, 0x1F, 0x04, 0x07, 0x92, 0x30, 0xFE, 0x21, 0x90, 0x48, 0x24, 0x23,
    0xE1, 0x10, 0x84, 0x41, 0x70, 0xC0, 0x3A, 0xCD, 0x0A, 0x03, 0x01, 0x80,
    0xC1, 0xC7, 0x78, 0xFF, 0xC4, 0x62, 0x21, 0x00, 0x80, 0x40, 0x20, 0x10,
    0x08, 0x1F, 0x00, 0xE3, 0xA0, 0x90, 0x48, 0x24, 0x12, 0x09, 0x04, 0x82,
    0x22, 0x0E, 0x00, 0xF1, 0xE8, 0x10, 0x82, 0x10, 0x42, 0x10, 0x22, 0x04,
    0x80, 0x50, 0x0C, 0x00, 0x80, 0xF1, 0xE8, 0x09, 0x11, 0x25, 0x44, 0xA8,
    0x55, 0x0C, 0xA1, 0x8C, 0x31, 0x84, 0x30, 0xE3, 0xA0, 0x88, 0x82, 0x80,
    0x80, 0xC0, 0x90, 0x44, 0x41, 0x71, 0xC0, 0xE3, 0xA0, 0x88, 0x82, 0x81,
    0x40, 0x40, 0x20, 0x10, 0x08, 0x1F, 0x00, 0xFD, 0x0A, 0x20, 0x81, 0x04,
    0x10, 0x21, 0x83, 0xFC, 0xEA, 0xAA, 0xAA, 0xC0, 0x80, 0x81, 0x03, 0x02,
    0x04, 0x04, 0x08, 0x08, 0x10, 0x10, 0x20, 0x20, 0xD5, 0x55, 0x55, 0xC0,
    0x10, 0x51, 0x22, 0x28, 0x20, 0xFF, 0xE0, 0x88, 0x80, 0x7E, 0x00, 0x80,
    0x47, 0xEC, 0x14, 0x0A, 0x0C, 0xFB, 0xC0, 0x20, 0x10, 0x0B, 0xC6, 0x12,
    0x05, 0x02, 0x81, 0x40, 0xB0, 0xB7, 0x80, 0x3A, 0x8E, 0x0C, 0x08, 0x10,
    0x10, 0x9E, 0x03, 0x00, 0x80, 0x47, 0xA4, 0x34, 0x0A, 0x05, 0x02, 0x81,
    0x21, 0x8F, 0x60, 0x3C, 0x43, 0x81, 0xFF, 0x80, 0x80, 0x61, 0x3E, 0x3D,
    0x04, 0x3E, 0x41, 0x04, 0x10, 0x41, 0x0F, 0x80, 0x3D, 0xA1, 0xA0, 0x50,
    0x28, 0x14, 0x09, 0x0C, 0x7A, 0x01, 0x01, 0x87, 0x80, 0xC0, 0x20, 0x10,
    0x0B, 0xC6, 0x32, 0x09, 0x04, 0x82, 0x41, 0x20, 0xB8, 0xE0, 0x10, 0x01,
    0xC0, 0x81, 0x02, 0x04, 0x08, 0x11, 0xFC, 0x10, 0x3E, 0x10, 0x84, 0x21,
    0x08, 0x42, 0x3F, 0x00, 0xC0, 0x40, 0x40, 0x4F, 0x44, 0x58, 0x70, 0x48,
    0x44, 0x42, 0xC7, 0x70, 0x20, 0x40, 0x81, 0x02, 0x04, 0x08, 0x10, 0x23,
    0xF8, 0xB7, 0x64, 0x62, 0x31, 0x18, 0x8C, 0x46, 0x23, 0x91, 0x5E, 0x31,
    0x90, 0x48, 0x24, 0x12, 0x09, 0x05, 0xC7, 0x3E, 0x31, 0xA0, 0x30, 0x18,
    0x0C, 0x05, 0x8C, 0x7C, 0xDE, 0x30, 0x90, 0x28, 0x14, 0x0A, 0x05, 0x84,
    0xBC, 0x40, 0x20, 0x38, 0x00, 0x3D, 0xA1, 0xA0, 0x50, 0x28, 0x14, 0x09,
    0x0C, 0x7A, 0x01, 0x00, 0x80, 0xE0, 0xCE, 0xA1, 0x82, 0x04, 0x08, 0x10,
    0x7C, 0x3A, 0x8D, 0x0B, 0x80, 0xF0, 0x70, 0xDE, 0x40, 0x40, 0xFC, 0x40,
    0x40, 0x40, 0x40, 0x40, 0x41, 0x3E, 0xC3, 0x41, 0x41, 0x41, 0x41, 0x41,
    0x43, 0x3D, 0xE3, 0xA0, 0x90, 0x84, 0x42, 0x20, 0xA0, 0x50, 0x10, 0xE3,
    0xC0, 0x92, 0x4B, 0x25, 0x92, 0xA9, 0x98, 0x44, 0xE3, 0x31, 0x05, 0x01,
    0x01, 0x41, 0x11, 0x05, 0xC7, 0xE3, 0xA0, 0x90, 0x84, 0x42, 0x40, 0xA0,
    0x60, 0x10, 0x10, 0x08, 0x3E, 0x00, 0xFD, 0x08, 0x20, 0x82, 0x08, 0x10,
    0xBF, 0x29, 0x24, 0xA2, 0x49, 0x26, 0xFF, 0xF8, 0x89, 0x24, 0x8A, 0x49,
    0x2C, 0x61, 0x24, 0x30};

const u8 mono12_bitmap[] = {
    0x49, 0x24, 0x92, 0x48, 0x01, 0xF8, 0xE7, 0xE7, 0x67, 0x42, 0x42, 0x42,
    0x42, 0x09, 0x02, 0x41, 0x10, 0x44, 0x11, 0x1F, 0xF1, 0x10, 0x4C, 0x12,
    0x3F, 0xE1, 0x20, 0x48, 0x12, 0x04, 0x81, 0x20, 0x48, 0x04, 0x07, 0xA2,
    0x19, 0x02, 0x40, 0x10, 0x03, 0x00, 0x3C, 0x00, 0x80, 0x10, 0x06, 0x01,
    0xE0, 0xA7, 0xC0, 0x40, 0x10, 0x04, 0x00, 0x3C, 0x19, 0x84, 0x21, 0x08,
    0x66, 0x0F, 0x00, 0x0C, 0x1C, 0x78, 0x01, 0xE0, 0xCC, 0x21, 0x08, 0x43,
    0x30, 0x78, 0x3E, 0x30, 0x10, 0x08, 0x02, 0x03, 0x03, 0x47, 0x14, 0x8A,
    0x43, 0x11, 0x8F, 0x60, 0xFD, 0xA4, 0x90, 0x05, 0x25, 0x24, 0x92, 0x48,
    0x92, 0x24, 0x11, 0x24, 0x89, 0x24, 0x92, 0x92, 0x90, 0x00, 0x04, 0x02,
    0x11, 0x07, 0xF0, 0xC0, 0x50, 0x48, 0x42, 0x00, 0x08, 0x04, 0x02, 0x01,
    0x00, 0x87, 0xFC, 0x20, 0x10, 0x08, 0x04, 0x02, 0x00, 0x3B, 0x9C, 0xCE,
    0x62, 0x00, 0xFF, 0xE0, 0xFF, 0x80, 0x00, 0x80, 0xC0, 0x40, 0x20, 0x20,
    0x10, 0x10, 0x08, 0x08, 0x04, 0x04, 0x02, 0x02, 0x01, 0x01, 0x00, 0x80,
    0x80, 0x40, 0x00, 0x1C, 0x31, 0x90, 0x58, 0x38, 0x0C, 0x06, 0x03, 0x01,
    0x80, 0xC0, 0x60, 0x30, 0x34, 0x13, 0x18, 0x70, 0x30, 0xE1, 0x44, 0x81,
    0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x81, 0x1F, 0xC0, 0x1E, 0x10, 0x90,
    0x68, 0x10, 0x08, 0x0C, 0x04, 0x04, 0x04, 0x06, 0x06, 0x06, 0x06, 0x0E,
    0x07, 0xFE, 0x3E, 0x10, 0x40, 0x08, 0x02, 0x00, 0x80, 0x40, 0xE0, 0x04,
    0x00, 0x80, 0x10, 0x04, 0x01, 0x00, 0xD8, 0x63, 0xE0, 0x06, 0x0A, 0x0A,
    0x12, 0x22, 0x22, 0x42, 0x42, 0x82, 0x82, 0xFF, 0x02, 0x02, 0x02, 0x0F,
    0x7F, 0x20, 0x10, 0x08, 0x04, 0x02, 0xF1, 0x8C, 0x03, 0x00, 0x80, 0x40,
    0x20, 0x18, 0x16, 0x18, 0xF0, 0x0F, 0x8C, 0x08, 0x08, 0x04, 0x04, 0x02,
    0x79, 0x46, 0xC1, 0xE0, 0x60, 0x28, 0x14, 0x19, 0x08, 0x78, 0xFF, 0x81,
    0x81, 0x02, 0x02, 0x02, 0x02, 0x04, 0x04, 0x04, 0x04, 0x08, 0x08, 0x08,
    0x08, 0x3E, 0x31, 0xB0, 0x70, 0x18, 0x0C, 0x05, 0x8C, 0x38, 0x63, 0x40,
    0x60, 0x30, 0x18, 0x1B, 0x18, 0xF8, 0x3C, 0x31, 0x30, 0x50, 0x28, 0x0C,
    0x0F, 0x06, 0x85, 0x3C, 0x80, 0x40, 0x40, 0x20, 0x20, 0x63, 0xE0, 0xFF,
    0x80, 0x07, 0xFC, 0x39, 0xCE, 0x00, 0x00, 0x06, 0x33, 0x98, 0xC4, 0x00,
    0x00, 0xC0, 0x60, 0x18, 0x0C, 0x06, 0x01, 0x80, 0x0C, 0x00, 0x60, 0x03,
    0x00, 0x30, 0x01, 0x00, 0xFF, 0xF0, 0x00, 0x00, 0x0F, 0xFF, 0xC0, 0x06,
    0x00, 0x30, 0x01, 0x80, 0x18, 0x01, 0x80, 0xC0, 0x30, 0x18, 0x0C, 0x02,
    0x00, 0x00, 0x3E, 0x60, 0xA0, 0x20, 0x10, 0x08, 0x08, 0x18, 0x10, 0x08,
    0x00, 0x00, 0x00, 0x01, 0xC0, 0xE0, 0x1C, 0x31, 0x10, 0x50, 0x28, 0x14,
    0x3A, 0x25, 0x22, 0x91, 0x4C, 0xA3, 0xF0, 0x08, 0x02, 0x01, 0x80, 0x7C,
    0x3F, 0x00, 0x0C, 0x00, 0x48, 0x01, 0x20, 0x04, 0x40, 0x21, 0x00, 0x84,
    0x04, 0x08, 0x1F, 0xE0, 0x40, 0x82, 0x01, 0x08, 0x04, 0x20, 0x13, 0xE1,
    0xF0, 0xFF, 0x08, 0x11, 0x01, 0x20, 0x24, 0x04, 0x81, 0x1F, 0xC2, 0x06,
    0x40, 0x68, 0x05, 0x00, 0xA0, 0x14, 0x05, 0xFF, 0x00, 0x1E, 0x48, 0x74,
    0x05, 0x01, 0x80, 0x20, 0x08, 0x02, 0x00, 0x80, 0x20, 0x04, 0x01, 0x01,
    0x30, 0x87, 0xC0, 0xFE, 0x10, 0x44, 0x09, 0x02, 0x40, 0x50, 0x14, 0x05,
    0x01, 0x40, 0x50, 0x14, 0x0D, 0x02, 0x41, 0x3F, 0x80, 0xFF, 0xC8, 0x09,
    0x01, 0x20, 0x04, 0x00, 0x88, 0x1F, 0x02, 0x20, 0x40, 0x08, 0x01, 0x00,
    0xA0, 0x14, 0x03, 0xFF, 0xC0, 0xFF, 0xE8, 0x05, 0x00, 0xA0, 0x04, 0x00,
    0x88, 0x1F, 0x02, 0x20, 0x40, 0x08, 0x01, 0x00, 0x20, 0x04, 0x01, 0xF0,
    0x00, 0x1F, 0x46, 0x19, 0x01, 0x60, 0x28, 0x01, 0x00, 0x20, 0x04, 0x00,
    0x83, 0xF0, 0x0B, 0x01, 0x20, 0x23, 0x0C, 0x3E, 0x00, 0xE1, 0xD0, 0x24,
    0x09, 0x02, 0x40, 0x90, 0x27, 0xF9, 0x02, 0x40, 0x90, 0x24, 0x09, 0x02,
    0x40, 0xB8, 0x70, 0xFE, 0x20, 0x40, 0x81, 0x02, 0x04, 0x08, 0x10, 0x20,
    0x40, 0x81, 0x1F, 0xC0, 0x0F, 0xE0, 0x10, 0x02, 0x00, 0x40, 0x08, 0x01,
    0x00, 0x20, 0x04, 0x80, 0x90, 0x12, 0x02, 0x40, 0xC6, 0x30, 0x7C, 0x00,
    0xF1, 0xE4, 0x0C, 0x41, 0x04, 0x20, 0x44, 0x04, 0x80, 0x5C, 0x06, 0x60,
    0x43, 0x04, 0x10, 0x40, 0x84, 0x08, 0x40, 0xCF, 0x07, 0xF8, 0x04, 0x00,
    0x80, 0x10, 0x02, 0x00, 0x40, 0x08, 0x01, 0x00, 0x20, 0x04, 0x04, 0x80,
    0x90, 0x12, 0x03, 0xFF, 0xC0, 0xE0, 0x3B, 0x01, 0x94, 0x14, 0xA0, 0xA4,
    0x89, 0x24, 0x49, 0x14, 0x48, 0xA2, 0x45, 0x12, 0x10, 0x90, 0x04, 0x80,
    0x24, 0x01, 0x78, 0x3C, 0xE0, 0xF6, 0x02, 0x50, 0x25, 0x02, 0x48, 0x24,
    0xC2, 0x44, 0x24, 0x22, 0x43, 0x24, 0x12, 0x40, 0xA4, 0x0A, 0x40, 0x6F,
    0x06, 0x0F, 0x03, 0x0C, 0x60, 0x64, 0x02, 0x80, 0x18, 0x01, 0x80, 0x18,
    0x01, 0x80, 0x18, 0x01, 0x40, 0x26, 0x06, 0x30, 0xC0, 0xF0, 0xFF, 0x10,
    0x64, 0x05, 0x01, 0x40, 0x50, 0x34, 0x19, 0xFC, 0x40, 0x10, 0x04, 0x01,
    0x00, 0x40, 0x3E, 0x00, 0x0F, 0x03, 0x0C, 0x60, 0x64, 0x02, 0x80, 0x18,
    0x01, 0x80, 0x18, 0x01, 0x80, 0x18, 0x01, 0x40, 0x26, 0x06, 0x30, 0xC1,
    0xF0, 0x0C, 0x01, 0xF1, 0x30, 0xE0, 0xFF, 0x04, 0x18, 0x40, 0xC4, 0x04,
    0x40, 0x44, 0x0C, 0x41, 0x87, 0xE0, 0x43, 0x04, 0x10, 0x40, 0x84, 0x04,
    0x40, 0x4F, 0x03, 0x1F, 0x48, 0x34, 0x05, 0x01, 0x40, 0x08, 0x01, 0xC0,
    0x0E, 0x00, 0x40, 0x18, 0x06, 0x01, 0xE1, 0xA7, 0xC0, 0xFF, 0xF0, 0x86,
    0x10, 0x82, 0x00, 0x40, 0x08, 0x01, 0x00, 0x20, 0x04, 0x00, 0x80, 0x10,
    0x02, 0x00, 0x40, 0x7F, 0x00, 0xF0, 0xF4, 0x02, 0x40, 0x24, 0x02, 0x40,
    0x24, 0x02, 0x40, 0x24, 0x02, 0x40, 0x24, 0x02, 0x40, 0x22, 0x04, 0x30,
    0xC0, 0xF0, 0xF8, 0x7C, 0x80, 0x22, 0x01, 0x04, 0x04, 0x10, 0x20, 0x40,
    0x80, 0x82, 0x02, 0x10, 0x08, 0x40, 0x11, 0x00, 0x48, 0x01, 0xA0, 0x03,
    0x00, 0x0C, 0x00, 0xF8, 0x7C, 0x80, 0x22, 0x00, 0x88, 0xC2, 0x23, 0x10,
    0x8E, 0x42, 0x29, 0x09, 0x24, 0x24, 0x90, 0x91, 0x41, 0x85, 0x06, 0x14,
    0x18, 0x70, 0x60, 0x80, 0xF0, 0xF2, 0x06, 0x30, 0x41, 0x08, 0x09, 0x80,
    0x50, 0x06, 0x00, 0x60, 0x0D, 0x00, 0x88, 0x10, 0xC2, 0x04, 0x60, 0x2F,
    0x0F, 0xF0, 0xF2, 0x02, 0x10, 0x41, 0x04, 0x08, 0x80, 0x50, 0x05, 0x00,
    0x20, 0x02, 0x00, 0x20, 0x02, 0x00, 0x20, 0x02, 0x01, 0xFC, 0xFF, 0x40,
    0xA0, 0x90, 0x40, 0x40, 0x40, 0x20, 0x20, 0x20, 0x10, 0x50, 0x30, 0x18,
    0x0F, 0xFC, 0xF2, 0x49, 0x24, 0x92, 0x49, 0x24, 0x9C, 0x80, 0x60, 0x10,
    0x08, 0x02, 0x01, 0x00, 0x40, 0x20, 0x08, 0x04, 0x01, 0x00, 0x80, 0x20,
    0x10, 0x04, 0x02, 0x00, 0x80, 0x40, 0xE4, 0x92, 0x49, 0x24, 0x92, 0x49,
    0x3C, 0x08, 0x0C, 0x09, 0x0C, 0x4C, 0x14, 0x04, 0xFF, 0xFC, 0x84, 0x21,
    0x3E, 0x00, 0x60, 0x08, 0x02, 0x3F, 0x98, 0x28, 0x0A, 0x02, 0xC3, 0x9F,
    0x30, 0xE0, 0x01, 0x00, 0x08, 0x00, 0x40, 0x02, 0x00, 0x13, 0xE0, 0xA0,
    0x86, 0x02, 0x20, 0x09, 0x00, 0x48, 0x02, 0x40, 0x13, 0x01, 0x14, 0x1B,
    0x9F, 0x00, 0x1F, 0x4C, 0x19, 0x01, 0x40, 0x28, 0x01, 0x00, 0x20, 0x02,
    0x00, 0x60, 0x43, 0xF0, 0x00, 0xC0, 0x08, 0x01, 0x00, 0x20, 0x04, 0x3C,
    0x98, 0x52, 0x06, 0x80, 0x50, 0x0A, 0x01, 0x40, 0x24, 0x0C, 0xC2, 0x87,
    0x98, 0x3F, 0x18, 0x68, 0x06, 0x01, 0xFF, 0xE0, 0x08, 0x03, 0x00, 0x60,
    0xC7, 0xC0, 0x0F, 0x98, 0x08, 0x04, 0x02, 0x07, 0xF8, 0x80, 0x40, 0x20,
    0x10, 0x08, 0x04, 0x02, 0x01, 0x03, 0xF8, 0x1E, 0x6C, 0x39, 0x03, 0x40,
    0x28, 0x05, 0x00, 0xA0, 0x12, 0x06, 0x61, 0x43, 0xC8, 0x01, 0x00, 0x20,
    0x08, 0x3E, 0x00, 0xC0, 0x10, 0x04, 0x01, 0x00, 0x40, 0x13, 0x87, 0x11,
    0x82, 0x40, 0x90, 0x24, 0x09, 0x02, 0x40, 0x90, 0x2E, 0x1C, 0x08, 0x04,
    0x02, 0x00, 0x00, 0x03, 0xC0, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01, 0x00,
    0x80, 0x43, 0xFE, 0x04, 0x08, 0x10, 0x00, 0x1F, 0xC0, 0x81, 0x02, 0x04,
    0x08, 0x10, 0x20, 0x40, 0x81, 0x02, 0x0B, 0xE0, 0xE0, 0x02, 0x00, 0x20,
    0x02, 0x00, 0x20, 0x02, 0x3C, 0x21, 0x02, 0x60, 0x2C, 0x03, 0x80, 0x24,
    0x02, 0x20, 0x21, 0x02, 0x08, 0xE1, 0xF0, 0x78, 0x04, 0x02, 0x01, 0x00,
    0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01, 0x00, 0x80, 0x43, 0xFE,
    0xDC, 0xE3, 0x19, 0x90, 0x84, 0x84, 0x24, 0x21, 0x21, 0x09, 0x08, 0x48,
    0x42, 0x42, 0x17, 0x18, 0xC0, 0x67, 0x83, 0x84, 0x20, 0x22, 0x02, 0x20,
    0x22, 0x02, 0x20, 0x22, 0x02, 0x20, 0x2F, 0x07, 0x1F, 0x04, 0x11, 0x01,
    0x40, 0x18, 0x03, 0x00, 0x60, 0x0A, 0x02, 0x20, 0x83, 0xE0, 0xCF, 0x85,
    0x06, 0x60, 0x24, 0x01, 0x40, 0x14, 0x01, 0x40, 0x16, 0x02, 0x50, 0x44,
    0xF8, 0x40, 0x04, 0x00, 0x40, 0x0F, 0x00, 0x1E, 0x6C, 0x3B, 0x03, 0x40,
    0x28, 0x05, 0x00, 0xA0, 0x12, 0x06, 0x61, 0x43, 0xC8, 0x01, 0x00, 0x20,
    0x04, 0x03, 0xC0, 0xE3, 0x8B, 0x13, 0x80, 0x80, 0x20, 0x08, 0x02, 0x00,
    0x80, 0x20, 0x3F, 0x80, 0x1F, 0x58, 0x34, 0x05, 0x80, 0x1E, 0x00, 0x60,
    0x06, 0x01, 0xC0, 0xAF, 0xC0, 0x20, 0x04, 0x00, 0x80, 0x10, 0x0F, 0xF0,
    0x40, 0x08, 0x01, 0x00, 0x20, 0x04, 0x00, 0x80, 0x10, 0x03, 0x04, 0x3F,
    0x00, 0xC1, 0xC8, 0x09, 0x01, 0x20, 0x24, 0x04, 0x80, 0x90, 0x12, 0x02,
    0x61, 0xC7, 0xCC, 0xF8, 0xF9, 0x01, 0x08, 0x10, 0x60, 0x81, 0x08, 0x08,
    0x40, 0x22, 0x01, 0x20, 0x05, 0x00, 0x30, 0x00, 0xF0, 0x7A, 0x01, 0x10,
    0x08, 0x8C, 0x42, 0x62, 0x12, 0x90, 0xA5, 0x05, 0x18, 0x28, 0xC0, 0x86,
    0x00, 0x78, 0xF3, 0x04, 0x18, 0x80, 0xD0, 0x06, 0x00, 0x70, 0x09, 0x81,
    0x0C, 0x20, 0x6F, 0x8F, 0xF0, 0xF2, 0x02, 0x20, 0x41, 0x04, 0x10, 0x80,
    0x88, 0x09, 0x00, 0x50, 0x06, 0x00, 0x20, 0x04, 0x00, 0x40, 0x08, 0x0F,
    0xE0, 0xFF, 0x41, 0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x40, 0xBF,
    0xC0, 0x19, 0x08, 0x42, 0x10, 0x84, 0x64, 0x18, 0x42, 0x10, 0x84, 0x20,
    0xC0, 0xFF, 0xFF, 0xC0, 0xC1, 0x08, 0x42, 0x10, 0x84, 0x10, 0x4C, 0x42,
    0x10, 0x84, 0x26, 0x00, 0x38, 0x13, 0x38, 0x38};

const struct glyph mono12_glyph[] = {
    {0, 0, 0, 14, 0, 1},        // 0x20 ' '
    {0, 3, 15, 14, 6, -14},     // 0x21 '!'
    {6, 8, 7, 14, 3, -14},      // 0x22 '"'
    {13, 10, 16, 14, 2, -14},   // 0x23 '#'
    {33, 10, 17, 14, 2, -14},   // 0x24 '$'
    {55, 10, 15, 14, 2, -14},   // 0x25 '%'
    {74, 9, 12, 14, 3, -11},    // 0x26 '&'
    {88, 3, 7, 14, 5, -14},     // 0x27 '''
    {91, 3, 18, 14, 7, -14},    // 0x28 '('
    {98, 3, 18, 14, 4, -14},    // 0x29 ')'
    {105, 9, 9, 14, 3, -14},    // 0x2A '*'
    {116, 9, 11, 14, 3, -11},   // 0x2B '+'
    {129, 5, 7, 14, 3, -3},     // 0x2C ','
    {134, 11, 1, 14, 2, -6},    // 0x2D '-'
    {136, 3, 3, 14, 5, -2},     // 0x2E '.'
    {138, 9, 18, 14, 3, -15},   // 0x2F '/'
    {159, 9, 15, 14, 3, -14},   // 0x30 '0'
    {176, 7, 14, 14, 4, -13},   // 0x31 '1'
    {189, 9, 15, 14, 2, -14},   // 0x32 '2'
    {206, 10, 15, 14, 2, -14},  // 0x33 '3'
    {225, 8, 15, 14, 3, -14},   // 0x34 '4'
    {240, 9, 15, 14, 3, -14},   // 0x35 '5'
    {257, 9, 15, 14, 3, -14},   // 0x36 '6'
    {274, 8, 15, 14, 3, -14},   // 0x37 '7'
    {289, 9, 15, 14, 3, -14},   // 0x38 '8'
    {306, 9, 15, 14, 3, -14},   // 0x39 '9'
    {323, 3, 10, 14, 5, -9},    // 0x3A ':'
    {327, 5, 13, 14, 3, -9},    // 0x3B ';'
    {336, 11, 11, 14, 2, -11},  // 0x3C '<'
    {352, 12, 4, 14, 1, -8},    // 0x3D '='
    {358, 11, 11, 14, 2, -11},  // 0x3E '>'
    {374, 9, 14, 14, 3, -13},   // 0x3F '?'
    {390, 9, 16, 14, 3, -14},   // 0x40 '@'
    {408, 14, 14, 14, 0, -13},  // 0x41 'A'
    {433, 11, 14, 14, 2, -13},  // 0x42 'B'
    {453, 10, 14, 14, 2, -13},  // 0x43 'C'
    {471, 10, 14, 14, 2, -13},  // 0x44 'D'
    {489, 11, 14, 14, 2, -13},  // 0x45 'E'
    {509, 11, 14, 14, 2, -13},  // 0x46 'F'
    {529, 11, 14, 14, 2, -13},  // 0x47 'G'
    {549, 10, 14, 14, 2, -13},  // 0x48 'H'
    {567, 7, 14, 14, 4, -13},   // 0x49 'I'
    {580, 11, 14, 14, 2, -13},  // 0x4A 'J'
    {600, 12, 14, 14, 2, -13},  // 0x4B 'K'
    {621, 11, 14, 14, 2, -13},  // 0x4C 'L'
    {641, 13, 14, 14, 1, -13},  // 0x4D 'M'
    {664, 12, 14, 14, 1, -13},  // 0x4E 'N'
    {685, 12, 14, 14, 1, -13},  // 0x4F 'O'
    {706, 10, 14, 14, 2, -13},  // 0x50 'P'
    {724, 12, 17, 14, 1, -13},  // 0x51 'Q'
    {750, 12, 14, 14, 2, -13},  // 0x52 'R'
    {771, 10, 14, 14, 2, -13},  // 0x53 'S'
    {789, 11, 14, 14, 2, -13},  // 0x54 'T'
    {809, 12, 14, 14, 1, -13},  // 0x55 'U'
    {830, 14, 14, 14, 0, -13},  // 0x56 'V'
    {855, 14, 14, 14, 0, -13},  // 0x57 'W'
    {880, 12, 14, 14, 1, -13},  // 0x58 'X'
    {901, 12, 14, 14, 1, -13},  // 0x59 'Y'
    {922, 9, 14, 14, 3, -13},   // 0x5A 'Z'
    {938, 3, 18, 14, 7, -14},   // 0x5B '['
    {945, 9, 18, 14, 3, -15},   // 0x5C '\'
    {966, 3, 18, 14, 5, -14},   // 0x5D ']'
    {973, 9, 6, 14, 3, -14},    // 0x5E '^'
    {980, 14, 1, 14, 0, 3},     // 0x5F '_'
    {982, 4, 4, 14, 4, -15},    // 0x60 '`'
    {984, 10, 10, 14, 2, -9},   // 0x61 'a'
    {997, 13, 15, 14, 0, -14},  // 0x62 'b'
    {1022, 11, 10, 14, 2, -9},  // 0x63 'c'
    {1036, 11, 15, 14, 2, -14}, // 0x64 'd'
    {1057, 10, 10, 14, 2, -9},  // 0x65 'e'
    {1070, 9, 15, 14, 4, -14},  // 0x66 'f'
    {1087, 11, 14, 14, 2, -9},  // 0x67 'g'
    {1107, 10, 15, 14, 2, -14}, // 0x68 'h'
    {1126, 9, 15, 14, 3, -14},  // 0x69 'i'
    {1143, 7, 19, 14, 3, -14},  // 0x6A 'j'
    {1160, 12, 15, 14, 1, -14}, // 0x6B 'k'
    {1183, 9, 15, 14, 3, -14},  // 0x6C 'l'
    {1200, 13, 10, 14, 1, -9},  // 0x6D 'm'
    {1217, 12, 10, 14, 1, -9},  // 0x6E 'n'
    {1232, 11, 10, 14, 2, -9},  // 0x6F 'o'
    {1246, 12, 14, 14, 1, -9},  // 0x70 'p'
    {1267, 11, 14, 14, 2, -9},  // 0x71 'q'
    {1287, 10, 10, 14, 3, -9},  // 0x72 'r'
    {1300, 10, 10, 14, 2, -9},  // 0x73 's'
    {1313, 11, 14, 14, 1, -13}, // 0x74 't'
    {1333, 11, 10, 14, 2, -9},  // 0x75 'u'
    {1347, 13, 10, 14, 1, -9},  // 0x76 'v'
    {1364, 13, 10, 14, 1, -9},  // 0x77 'w'
    {1381, 12, 10, 14, 1, -9},  // 0x78 'x'
    {1396, 12, 14, 14, 1, -9},  // 0x79 'y'
    {1417, 9, 10, 14, 3, -9},   // 0x7A 'z'
    {1429, 5, 18, 14, 5, -14},  // 0x7B '{'
    {1441, 1, 18, 14, 7, -14},  // 0x7C '|'
    {1444, 5, 18, 14, 5, -14},  // 0x7D '}'
    {1456, 10, 3, 14, 2, -7}};  // 0x7E '~'

const struct glyph mono9_glyph[] = {
    {0, 0, 0, 11, 0, 1},      // 0x20 ' '
    {0, 2, 11, 11, 4, -10},   // 0x21 '!'
    {3, 6, 5, 11, 2, -10},    // 0x22 '"'
    {7, 7, 12, 11, 2, -10},   // 0x23 '#'
    {18, 8, 12, 11, 1, -10},  // 0x24 '$'
    {30, 7, 11, 11, 2, -10},  // 0x25 '%'
    {40, 7, 10, 11, 2, -9},   // 0x26 '&'
    {49, 3, 5, 11, 4, -10},   // 0x27 '''
    {51, 2, 13, 11, 5, -10},  // 0x28 '('
    {55, 2, 13, 11, 4, -10},  // 0x29 ')'
    {59, 7, 7, 11, 2, -10},   // 0x2A '*'
    {66, 7, 7, 11, 2, -8},    // 0x2B '+'
    {73, 3, 5, 11, 2, -1},    // 0x2C ','
    {75, 9, 1, 11, 1, -5},    // 0x2D '-'
    {77, 2, 2, 11, 4, -1},    // 0x2E '.'
    {78, 7, 13, 11, 2, -11},  // 0x2F '/'
    {90, 7, 11, 11, 2, -10},  // 0x30 '0'
    {100, 5, 11, 11, 3, -10}, // 0x31 '1'
    {107, 7, 11, 11, 2, -10}, // 0x32 '2'
    {117, 8, 11, 11, 1, -10}, // 0x33 '3'
    {128, 6, 11, 11, 3, -10}, // 0x34 '4'
    {137, 7, 11, 11, 2, -10}, // 0x35 '5'
    {147, 7, 11, 11, 2, -10}, // 0x36 '6'
    {157, 7, 11, 11, 2, -10}, // 0x37 '7'
    {167, 7, 11, 11, 2, -10}, // 0x38 '8'
    {177, 7, 11, 11, 2, -10}, // 0x39 '9'
    {187, 2, 8, 11, 4, -7},   // 0x3A ':'
    {189, 3, 11, 11, 3, -7},  // 0x3B ';'
    {194, 8, 8, 11, 1, -8},   // 0x3C '<'
    {202, 9, 4, 11, 1, -6},   // 0x3D '='
    {207, 9, 8, 11, 1, -8},   // 0x3E '>'
    {216, 7, 10, 11, 2, -9},  // 0x3F '?'
    {225, 8, 12, 11, 2, -10}, // 0x40 '@'
    {237, 11, 10, 11, 0, -9}, // 0x41 'A'
    {251, 9, 10, 11, 1, -9},  // 0x42 'B'
    {263, 9, 10, 11, 1, -9},  // 0x43 'C'
    {275, 9, 10, 11, 1, -9},  // 0x44 'D'
    {287, 9, 10, 11, 1, -9},  // 0x45 'E'
    {299, 9, 10, 11, 1, -9},  // 0x46 'F'
    {311, 10, 10, 11, 1, -9}, // 0x47 'G'
    {324, 9, 10, 11, 1, -9},  // 0x48 'H'
    {336, 5, 10, 11, 3, -9},  // 0x49 'I'
    {343, 8, 10, 11, 2, -9},  // 0x4A 'J'
    {353, 9, 10, 11, 1, -9},  // 0x4B 'K'
    {365, 8, 10, 11, 2, -9},  // 0x4C 'L'
    {375, 11, 10, 11, 0, -9}, // 0x4D 'M'
    {389, 9, 10, 11, 1, -9},  // 0x4E 'N'
    {401, 9, 10, 11, 1, -9},  // 0x4F 'O'
    {413, 8, 10, 11, 1, -9},  // 0x50 'P'
    {423, 9, 13, 11, 1, -9},  // 0x51 'Q'
    {438, 9, 10, 11, 1, -9},  // 0x52 'R'
    {450, 7, 10, 11, 2, -9},  // 0x53 'S'
    {459, 9, 10, 11, 1, -9},  // 0x54 'T'
    {471, 9, 10, 11, 1, -9},  // 0x55 'U'
    {483, 11, 10, 11, 0, -9}, // 0x56 'V'
    {497, 11, 10, 11, 0, -9}, // 0x57 'W'
    {511, 9, 10, 11, 1, -9},  // 0x58 'X'
    {523, 9, 10, 11, 1, -9},  // 0x59 'Y'
    {535, 7, 10, 11, 2, -9},  // 0x5A 'Z'
    {544, 2, 13, 11, 5, -10}, // 0x5B '['
    {548, 7, 13, 11, 2, -11}, // 0x5C '\'
    {560, 2, 13, 11, 4, -10}, // 0x5D ']'
    {564, 7, 5, 11, 2, -10},  // 0x5E '^'
    {569, 11, 1, 11, 0, 2},   // 0x5F '_'
    {571, 3, 3, 11, 3, -11},  // 0x60 '`'
    {573, 9, 8, 11, 1, -7},   // 0x61 'a'
    {582, 9, 11, 11, 1, -10}, // 0x62 'b'
    {595, 7, 8, 11, 2, -7},   // 0x63 'c'
    {602, 9, 11, 11, 1, -10}, // 0x64 'd'
    {615, 8, 8, 11, 1, -7},   // 0x65 'e'
    {623, 6, 11, 11, 3, -10}, // 0x66 'f'
    {632, 9, 11, 11, 1, -7},  // 0x67 'g'
    {645, 9, 11, 11, 1, -10}, // 0x68 'h'
    {658, 7, 10, 11, 2, -9},  // 0x69 'i'
    {667, 5, 13, 11, 3, -9},  // 0x6A 'j'
    {676, 8, 11, 11, 2, -10}, // 0x6B 'k'
    {687, 7, 11, 11, 2, -10}, // 0x6C 'l'
    {697, 9, 8, 11, 1, -7},   // 0x6D 'm'
    {706, 9, 8, 11, 1, -7},   // 0x6E 'n'
    {715, 9, 8, 11, 1, -7},   // 0x6F 'o'
    {724, 9, 11, 11, 1, -7},  // 0x70 'p'
    {737, 9, 11, 11, 1, -7},  // 0x71 'q'
    {750, 7, 8, 11, 3, -7},   // 0x72 'r'
    {757, 7, 8, 11, 2, -7},   // 0x73 's'
    {764, 8, 10, 11, 2, -9},  // 0x74 't'
    {774, 8, 8, 11, 1, -7},   // 0x75 'u'
    {782, 9, 8, 11, 1, -7},   // 0x76 'v'
    {791, 9, 8, 11, 1, -7},   // 0x77 'w'
    {800, 9, 8, 11, 1, -7},   // 0x78 'x'
    {809, 9, 11, 11, 1, -7},  // 0x79 'y'
    {822, 7, 8, 11, 2, -7},   // 0x7A 'z'
    {829, 3, 13, 11, 4, -10}, // 0x7B '{'
    {834, 1, 13, 11, 5, -10}, // 0x7C '|'
    {836, 3, 13, 11, 4, -10}, // 0x7D '}'
    {841, 7, 3, 11, 2, -6}};  // 0x7E '~'

const struct simple_font mono9_font = {
    .bitmap = (u8 *)mono9_bitmap,
    .glyph = (struct glyph *)mono9_glyph,
    .first = 0x20,
    .last = 0x7E,
    .y_advance = 18
};

const struct simple_font mono12_font = {
    .bitmap = (u8 *)mono12_bitmap,
    .glyph = (struct glyph *)mono12_glyph,
    .first = 0x20,
    .last = 0x7E,
    .y_advance = 24
};

void draw_char(char c, const struct simple_font* font, struct rgba (*fb)[800], u16 x, u16 y)
{
    struct glyph* g = font->glyph + (c - font->first);

    u8 counter = 0;
    u8 bits = 0;
    u16 bit_off = g->off;
    for (u16 i = 0; i < g->h; i++) {
        for (u16 j = 0; j < g->w; j++) {
            if (!(counter++ & 7)) {
                bits = font->bitmap[bit_off++];
            }
            if (bits & 0x80) {
                fb[y + i + g->y_off][x + j + g->x_off] = (struct rgba){0xFF, 0xFF, 0, 0};
            }
            bits <<= 1;
        }
    }
    
}

void print_screen(const char* text, u16 x, u16 y, const struct simple_font* font, struct rgba (*fb)[800])
{
    while (*text) {
        draw_char(*text, font, fb, x, y);
        struct glyph* g = font->glyph + *text - font->first;
        x += g->x_advance;
        text++;
    }
    dcache_clean();
}

void font_test(void)
{
    

    while (1) {
        for (u32 i = 0; i < 200; i += 1) {
            struct fb_info* info = lcd_get_new_framebuffer(2);
            struct rgba (*buffer)[800] = info->buffer;

            u32* ptr = (u32 *)buffer;
            for (u32 x = 0; x < 480 * 800; x++) {
                *ptr++ = 0;
            }
            for (u32 x = 0; x < 15; x++) {
                print_screen("Hello World This is a small test to see if this display work", i + 50, i + 50 + x * 15, &mono9_font, buffer);
            }
            lcd_switch_framebuffer(2);

            struct fb_info* info1 = lcd_get_new_framebuffer(1);
            struct rgba (*buffer1)[800] = info->buffer;
            for (u32 x = 0; x < 15; x++) {
                print_screen("Hello World This is a small test to see if this display work", i*2, i + 5 + x * 15, &mono9_font, buffer1);
            }
            lcd_switch_framebuffer(1);
            for (u32 x = 0; x < 480 * 800*2; x++) {
                asm ("nop");
            }
            print("o");
        }
        for (u32 i = 200; i != 0; i -= 1) {
            struct fb_info* info = lcd_get_new_framebuffer(2);
            struct rgba (*buffer)[800] = info->buffer;
            u32* ptr = (u32 *)buffer;
            for (u32 x = 0; x < 480 * 800; x++) {
                *ptr++ = 0;
            }
            for (u32 x = 0; x < 15; x++) {
                print_screen("Hello Worggld This is a small test to see if this display work", i + 50, i + 50 + x * 15, &mono9_font, buffer);
            }
            lcd_switch_framebuffer(2);
            for (u32 x = 0; x < 480 * 800*2; x++) {
                asm ("nop");
            }
            print("o");
        }
    }
}
