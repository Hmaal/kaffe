/*
 * java.lang.Compiler.c
 *
 * Copyright (c) 1996, 1997
 *	Transvirtual Technologies, Inc.  All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution 
 * of this file. 
 */

#include "config.h"
#include <assert.h>
#include <stdlib.h>
#include <native.h>
#include "../../../kaffe/kaffevm/baseClasses.h"
#include "../../../kaffe/kaffevm/support.h"
#include "java_lang_Compiler.h"

void
java_lang_Compiler_initialize(void)
{
	unimp("java.lang.Compiler:initialize unimplemented");
}

jbool
java_lang_Compiler_compileClass(struct Hjava_lang_Class* class UNUSED)
{
	unimp("java.lang.Compiler:compilerClass unimplemented");
}

jbool
java_lang_Compiler_compileClasses(struct Hjava_lang_String* str UNUSED)
{
	unimp("java.lang.Compiler:compileClasses unimplemented");
}

struct Hjava_lang_Object*
java_lang_Compiler_command(struct Hjava_lang_Object* obj UNUSED)
{
	unimp("java.lang.Compiler:command unimplemented");
}

void
java_lang_Compiler_enable(void)
{
	unimp("java.lang.Compiler:enable unimplemented");
}

void
java_lang_Compiler_disable(void)
{
	unimp("java.lang.Compiler:disable unimplemented");
}
