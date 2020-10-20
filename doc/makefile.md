# Makefile

The project uses a flat makefile build structure. The main makefile is located in the root directory. Files and include directories is included uses separate makefiles.

## Top makefile

The compiler used in this project is the arm-none-eabi cross-compiler. The following compiler flags is useful

- -x explicitly specifies the input language
- -O? specifies the optimisation level (0, 1, 2, 3, s/size, g/debugging)
