/*
 * RACK - Robotics Application Construction Kit
 * Copyright (C) 2005-2006 University of Hannover
 *                         Institute for Systems Engineering - RTS
 *                         Professor Bernardo Wagner
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Authors
 *      Joerg Langenberg <joerg.langenberg@gmx.net>
 *
 */
#ifndef _ARGOPTS_H_
#define _ARGOPTS_H_

#include <string>
#include <getopt.h>
#include <errno.h>
#include <main/rack_debug.h>

using std::string;

// arg_type values
#define ARGOPT_REQ          1
#define ARGOPT_OPT          2

// has_val values
#define ARGOPT_NOVAL        0
#define ARGOPT_REQVAL       1
#define ARGOPT_OPTVAL       2

// val_type values
#define ARGOPT_VAL_INT      0
#define ARGOPT_VAL_STR      1
#define ARGOPT_VAL_FLT      2

// error codes
#define EARGOPT_BASE        400
#define EARGOPT_INVAL       (EARGOPT_BASE + 1)
#define EARGOPT_MISSED      (EARGOPT_BASE + 2)

typedef union arg_value {
    int i;
    char *s;
    float f;
} arg_value_t;

typedef struct arg_table{
    int arg_type;
    string name;
    int has_val;
    int val_type;
    string help;
    arg_value_t val;
} argTable_t;

typedef struct arg_descriptor{
    argTable_t* tab;
} argDescriptor_t;

/* scans the start-options of the program.
 * nr: argument-number of an option
 * argc: argument counter
 * argv: argument vector
 * argdesc: pointer to an argDescriptor struct
 */
int argScan(int argc, char *argv[], argDescriptor_t *p_ad,
            const char *classname);

void argUsage(argDescriptor_t *p_ad);

arg_value_t __getArg(const char *argname, argTable_t *p_tab);

static inline int getIntArg(const char *argname, argTable_t *p_tab)
{
    return __getArg(argname, p_tab).i;
}

static inline char *getStrArg(const char *argname, argTable_t *p_tab)
{
    return __getArg(argname, p_tab).s;
}

static inline float getFltArg(const char *argname, argTable_t *p_tab)
{
    return __getArg(argname, p_tab).f;
}

void printAllArgs(argDescriptor_t *p_argdesc);

#endif // _ARGOPTS_H_
